# AGD-0070: Rendering Pipeline

- **Scope:** How a frame is drawn — submission, sorting, passes, materials, shaders, textures, cameras, and lights. Covers the rendering frontend, which speaks only in engine graphics types. Does not cover the graphics abstraction beneath it, which is AGD-0080.

## Overview

- Renderables submit themselves each frame. There is no persistent draw list and no retained scene on the rendering side.
- Submissions split into opaque and transparent lists, drawn as two passes with different rules.
- Uniform data is tiered by how often it changes: once per frame, once per material, once per draw, or baked into the pipeline and never sent at all.
- The opaque list is sorted so that draws sharing a pipeline are adjacent, which lets redundant state changes be dropped.
- Shaders are authored in one file per shader with vertex and fragment sections, support includes, and can be reloaded while the application runs.

## Concepts

- **Renderable** — a component that submits itself to be drawn. Meshes, primitives, and fullscreen quads are all renderables.
- **Material** — a shader plus the parameter values and textures that configure it. Materials decide transparency; shaders do not.
- **Pipeline** — the baked combination of shader, vertex layout, and fixed graphics state for one kind of draw. Created once, bound many times.
- **Uniform tier** — a grouping of shader inputs by update frequency. Frame, material, and draw tiers each have their own buffer.
- **Pass** — a span of the frame drawing one category of geometry under one set of rules.

## Architecture

`RenderManager` owns the frame. It holds the submission lists, the frame-wide uniform buffer, the backbuffer configuration, and the fullscreen fade overlay, and it drives the sequence of passes.

Renderables do not register permanently. Each one submits itself during its component update, into a fixed-capacity array. The list is consumed and effectively reset each frame, so anything that stops updating stops being drawn.

`Material` owns a shader name, parameter values, textures, and the alpha state that determines which pass it belongs to. It uploads its own parameter buffer when a value changes, not every frame.

`PipelineCache` maps a material and vertex layout to a pipeline, so many objects sharing both share one pipeline. Without it the sort below would be meaningless, because every object would carry a distinct pipeline.

`ShaderManager` compiles shaders from source, resolves includes, and can detect that a file has changed and recompile it in place. `MaterialManager` owns named materials; `TextureManager` owns named textures; `CameraManager` owns the active camera and supplies the view and projection; `LightManager` owns the scene's lighting.

## Runtime flow

**Frame start.** Camera state updates, then the frame-wide uniform buffer is filled and bound. It holds the view-projection transform, camera position, time, and the ambient and directional lighting — everything constant for the whole frame.

**Submission.** During the scene update, each renderable adds itself to the opaque or transparent list according to its material's transparency.

**Opaque pass.** The list is sorted by pipeline, then by material. Each renderable binds its pipeline, uploads its per-draw data, binds its textures, and draws. Depth writing is on.

**Transparent pass.** The list is sorted back to front by distance from the camera. Blending is on and depth writing is off — both baked into the pipelines rather than toggled around the pass.

**End of frame.** UI and text draw, then the developer overlay, then the fade overlay if it is not fully transparent, then the frame is presented.

Between these, named timing markers are emitted at each pass boundary, which is what produces the profiler's per-pass breakdown.

**Uniform tiers** carry data according to how often it changes:

- **Frame tier** — view-projection, camera position, time, lighting. Written once per frame.
- **Material tier** — base colour, opacity, tiling, physically-based factors, alpha mode and cutoff, texture presence. Written when a material value changes, not per frame.
- **Draw tier** — the model transform and its combination with the view-projection. Written per draw.
- **Baked** — cull mode, depth comparison, blend state. Never sent; part of the pipeline.

Texture units are declared by the shader rather than assigned at runtime, so binding a texture does not require an accompanying uniform write.

## Working with it

**Add a renderable type.** Derive from the renderable base, describe the vertex layout, create the geometry buffers, and submit during update. The pipeline is obtained from the cache; nothing needs to create one directly.

**Author a shader.** One file holds vertex and fragment sections. Shared declarations, including the uniform tier blocks, are pulled in by include. All shaders target one language version, which is what allows shader-declared texture units and shared include files to work everywhere.

**Add a material parameter.** If it belongs to every material, it goes in the material tier block and its shared shader include. If it belongs to one shader, use the named-uniform path — but see the limitation on that below.

**Iterate on a shader without restarting.** Nominate the shader for hot reload; changes to the file are detected and recompiled in place. This works only where the file system reports modification times, so it is unavailable when assets are packaged.

## Design decisions

### Renderables submit themselves each frame rather than being registered

There is no persistent list of things to draw. A renderable adds itself during its update, every frame.

This ties visibility directly to the scene update, which is what makes disabling or pausing behave sensibly: a component that stops updating stops drawing, with no separate deregistration to remember and no possibility of a stale entry pointing at a destroyed object.

The costs are that visibility cannot be decided independently of updating, and that the submission array has a fixed capacity rather than growing.

### Uniforms are tiered by update frequency

The alternative, which this replaced, was sending every shader input on every draw — the full lighting, camera, material, and transform set for each object, whether or not the previous object had just sent identical values.

Splitting by frequency means frame-constant data is written once, material data is written only when it changes, and only the transform is genuinely per-draw. It also matches how modern graphics interfaces expect resources to be bound, so the structure carries forward rather than needing to be redone.

The cost is that adding a shared parameter means changing a buffer layout and its shared shader declaration together, and the two must agree exactly. A mismatch produces wrong values rather than an error.

### The opaque list is sorted by pipeline, and redundancy is dropped beneath

Sorting groups draws that share a pipeline so they are adjacent, and the layer below skips a pipeline bind that matches the one already bound.

Sorting alone would achieve nothing without the skip, and the skip would rarely trigger without the sort. The pipeline cache is the third necessary part: it makes many objects share one pipeline in the first place.

Transparent geometry is sorted by distance instead, because correct blending order outranks batching. Where distances tie, material grouping is incidental rather than sought.

The redundancy dropping is currently partial. Pipeline binds are skipped; material buffer binds and texture binds are not, so objects sharing a material still repeat that work per draw. Completing it is the natural next step, and it belongs beneath this layer rather than as tracking in the draw loop.

### Materials decide transparency, shaders do not

One shader serves both opaque and transparent draws. Whether a draw blends is a property of the material, and is baked into its pipeline.

Separate shader variants for transparency would double the shader count and require every lighting change to be made twice, for no benefit — a fragment carries alpha in all cases, and what happens to that alpha is fixed state, not shader logic.

### Pass state is baked into pipelines, not set around passes

Blending and depth-write settings are baked into each pipeline when it is created, rather than being enabled before the transparent pass and restored after.

This removes an entire category of bug, where state is left enabled by an early return or leaks into a later pass. It also means the renderer manages no transitional graphics state at all.

The cost is that changing a material's transparency after creation requires recreating its pipeline rather than flipping a flag.

### Overlays are ordinary draws

The fullscreen fade is a normal renderable with a normal material, drawn last. It is not a special capability of the renderer or of anything beneath it.

Anything else needing to cover the finished frame uses the same path with no new support. The cost is one fullscreen draw during transitions, skipped entirely when the overlay is fully transparent.

### Shader hot reload is a development affordance, not a feature

One shader at a time can be nominated for reloading; its file is checked for modification and recompiled in place.

Limiting it to one shader keeps the per-frame check to a single file query. The facility depends on the file system reporting modification times, which is not true when assets are packaged into an application archive, so it is inherently a development-only path rather than something that degrades gracefully.

## Limitations

- The submission list has a fixed capacity. Exceeding it is a hard limit, not a growth.
- Redundant material and texture binds are not eliminated, so objects sharing a material repeat that work per draw.
- Offscreen render targets are unimplemented, so there are no shadow maps, no reflection probes, and no post-processing beyond the fixed resolve.
- The named-uniform path used by procedural-art materials and overlays bypasses the tier system, and is the last part of the frontend still binding shaders directly rather than pipelines.
- Hot reload handles one nominated shader and is unavailable where assets are packaged.
- Visibility cannot be separated from updating; a renderable that must not draw must stop updating.
