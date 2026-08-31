# AGD-0070: Rendering Pipeline

- **Scope:** How a frame is drawn — submission, sorting, passes, materials, shaders, textures, cameras, and lights. Covers the rendering frontend, which speaks only in engine graphics types. Does not cover the graphics abstraction beneath it.

## Overview

- Renderables submit themselves each frame. There is no persistent draw list and no retained scene on the rendering side.
- Submissions split into opaque and transparent lists, drawn as two passes with different rules.
- Uniform data is grouped by update frequency: once per frame, once per material, once per draw, or baked into the pipeline and never sent at all.
- The opaque list is sorted so that draws sharing a pipeline are adjacent, which lets redundant state changes be dropped.
- Shaders are authored in one file per shader with vertex and fragment sections, support includes, and can be reloaded while the application runs.

## Concepts

- **Renderable** — a component that submits itself to be drawn. Meshes, primitives, and fullscreen quads are all renderables.
- **Material** — a shader plus the parameter values and textures that configure it. Materials decide transparency; shaders do not.
- **Pipeline** — the baked combination of shader, vertex layout, and fixed graphics state for one kind of draw. Created once, bound many times.
- **Update frequency** — how often a shader input changes, and the axis uniform data is partitioned along. Frame, Material, Custom, and Draw each own a buffer and a binding slot. It is the same partition Vulkan descriptor sets and D3D12 root signatures are organised along.
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

**Post-process stack (optional).** The scene target carries the backbuffer's sample count and is resolved before any effect samples it, so antialiasing survives post-processing. Enabling any effect redirects the opaque and transparent passes into an offscreen colour target the renderer owns. Bloom then runs its own half-resolution passes, and a single fused pass applies every other enabled effect while resolving that target into the backbuffer. With every effect off the scene renders straight to the backbuffer, as before, and the target is released.

**End of frame.** UI and text draw, then the developer overlay, then the fade overlay if it is not fully transparent, then the frame is presented. These follow the post pass, so UI is never subject to a scene post-process effect.

Between these, named timing markers are emitted at each pass boundary, which is what produces the profiler's per-pass breakdown.

**Update frequency** decides where data lives:

- **Frame** — view-projection, camera position, time, lighting. Written once per frame.
- **Material** — base colour, opacity, tiling, physically-based factors, alpha mode and cutoff, texture presence. Written when a material value changes, not per frame.
- **Custom** — parameters one shader declares for itself: procedural-art controls, the fade colour, the fullscreen aspect ratio. Written when the value changes.
- **Draw** — the model transform and its combination with the view-projection. Written per draw.
- **Baked** — cull mode, depth comparison, blend state. Never sent; part of the pipeline.

Texture units are declared by the shader rather than assigned at runtime, so binding a texture does not require an accompanying uniform write.

## Working with it

**Add a renderable type.** Derive from the renderable base, describe the vertex layout, create the geometry buffers, and submit during update. The pipeline is obtained from the cache; nothing needs to create one directly.

**Author a shader.** One file holds vertex and fragment sections. Shared declarations, including the uniform blocks, are pulled in by include. All shaders target one language version, which is what allows shader-declared texture units and shared include files to work everywhere.

**Add a material parameter.** If it belongs to every material, it goes in the material block and its shared shader include. If it belongs to one shader, declare it in that shader's own parameter block and set it by name on the material; the engine places it in the block by the standard layout rules and uploads the block when a value changes.

**Iterate on a shader without restarting.** Nominate the shader for hot reload; changes to the file are detected and recompiled in place. This works only where the file system reports modification times, so it is unavailable when assets are packaged.

## Design decisions

### Renderables submit themselves each frame rather than being registered

There is no persistent list of things to draw. A renderable adds itself during its update, every frame.

This ties visibility directly to the scene update, which is what makes disabling or pausing behave sensibly: a component that stops updating stops drawing, with no separate deregistration to remember and no possibility of a stale entry pointing at a destroyed object.

The costs are that visibility cannot be decided independently of updating, and that the submission array has a fixed capacity rather than growing.

### Uniforms are grouped by update frequency

The alternative, which this replaced, was sending every shader input on every draw — the full lighting, camera, material, and transform set for each object, whether or not the previous object had just sent identical values.

Splitting by frequency means frame-constant data is written once, material data is written only when it changes, and only the transform is genuinely per-draw. It also matches how modern graphics interfaces expect resources to be bound, so the structure carries forward rather than needing to be redone.

The cost is that adding a shared parameter means changing a buffer layout and its shared shader declaration together, and the two must agree exactly. A mismatch produces wrong values rather than an error.

### One shader's own parameters get a block, not individual uniform writes

Procedural-art effects, the fade overlay, and the fullscreen quad each need parameters no standard frequency covers. The obvious route — write each one individually by name to the bound shader — is the one thing a modern graphics interface cannot do, and it was the last reason the frontend bound shaders rather than pipelines.

Instead the shader author declares a parameter block, and the engine reads that declaration to learn each member's position in it. Setting a parameter writes into the block and uploads it; nothing resolves a name against a compiled program, and no backend has to offer a way to set a uniform on its own.

Two costs. The engine computes member positions from the declaration rather than asking the driver, so a member type it cannot place must be rejected outright rather than half-placed. And a value set for a parameter the current shader does not declare is kept but ignored, which is what lets one caller drive several effects that declare different parameters — at the price of a misspelt name failing silently rather than loudly.

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

### Effects fuse into one pass rather than chaining

Vignette, colour grading and tone mapping are uniform-gated blocks inside a single fragment shader. Only bloom gets passes of its own.

A full-screen pass is bandwidth bound, not arithmetic bound: it reads a frame and writes a frame, and at 1080p that is roughly 16 MB of traffic whatever the shader does in between. Chaining three effects as three passes would triple that to save a handful of instructions per pixel. On a desktop GPU the difference is invisible; on a tiled mobile GPU, where bandwidth is shared with the CPU and every pass is a resolve, it is the dominant cost. Both major commercial engines reached the same structure, and Unity's is named for it.

Bloom is the exception because it cannot be expressed as a block: it needs a downsampled bright pass and a separable blur, each sampling the result of the last. It runs at half resolution — the blur is low frequency, so the detail is not missed, and it quarters the bandwidth of every bloom pass — and hands its result to the fused pass as a second texture.

The cost is that effect order is fixed by the shader rather than chosen by the caller, and that every effect's code is compiled into one shader whether or not it is enabled.

### Grading happens before the tone curve, in log space

The fused pass runs bloom composite, exposure, vignette, log encode, grade, tone map, output — in that order.

Grading after the tone curve would be simpler, and it is what a naive reading suggests, but it makes every grade depend on the exposure it was authored at: the same contrast value lands differently once the tone curve has already compressed the highlights. Encoding to log first and grading there is what makes a preset portable between scenes, and it is what both commercial engines do. Vignette runs earlier still, in linear, because it is a lens effect on incoming light rather than a look applied to a finished image.

Tone mapping is a mode rather than a toggle: off clamps, which is exactly what the direct-to-backbuffer path does, so switching it off stays well defined instead of blowing out everything above white.

### Post-processing is an engine capability, chosen by effect alone

A caller names the effect it wants. It does not create the offscreen target, size it, rebuild it when the window resizes, or open and close the passes.

The frame's scene pass opens before any application code runs, and passes cannot nest, so a caller has no point at which it could bracket a pass of its own. Declaring the effect lets the renderer redirect the pass it already opens, which needs no change to the frame sequence and keeps pass ordering in one place. This is the frontend half of a split that also constrains the graphics abstraction: entering and ending a pass is the frontend's alone, while resource creation, binding, and drawing stay open to any caller.

Effects are described, not coded, at the call site. Each carries its own name, its parameters, and each parameter's range and default. The developer panel builds its sliders from that description and the scene loader resolves yaml keys against it, so adding an effect costs a shader block and a parameter list rather than an edit to the panel and the loader as well.

Target ownership follows the same reasoning. The resolution a post-process target must match is the framebuffer resolution, which the renderer already tracks and the caller only observes; a caller that owned the target would have to watch for resizes and rebuild on its own, and every future caller would repeat that. So the renderer creates the target on demand, rebuilds it when the resolution changes, and releases it when the effect is cleared. Only the material crosses the boundary, and it stays owned by the caller.

The post pass reuses the ordinary fullscreen-quad renderable and an ordinary material, so a post-process effect is authored as a normal shader with normal parameters.

The cost is expressive range: one offscreen pass feeding one post pass, not an arbitrary chain. A multi-stage chain — bloom, depth of field — needs this extended rather than reused as is.

### Overlays are ordinary draws

The fullscreen fade is a normal renderable with a normal material, drawn last. It is not a special capability of the renderer or of anything beneath it.

Anything else needing to cover the finished frame uses the same path with no new support. The cost is one fullscreen draw during transitions, skipped entirely when the overlay is fully transparent.

### Shader hot reload is a development affordance, not a feature

One shader at a time can be nominated for reloading; its file is checked for modification and recompiled in place.

Limiting it to one shader keeps the per-frame check to a single file query. The facility depends on the file system reporting modification times, which is not true when assets are packaged into an application archive, so it is inherently a development-only path rather than something that degrades gracefully.

## Limitations

- The submission list has a fixed capacity. Exceeding it is a hard limit, not a growth.
- Redundant material and texture binds are not eliminated, so objects sharing a material repeat that work per draw.
- Effect order in the fused pass is fixed by the shader. A caller cannot reorder effects or insert one of its own.
- The renderer is not linear end to end: lit shaders write display-referred values with no output transform. Tone mapping therefore operates on values that are not scene-linear, which is why it is off by default and is a look choice rather than a correction.
- Bloom has nothing to act on in a scene whose brightest value is white. Its threshold must be dropped below 1 to show anything until content carries values above white.
- Where the backend cannot render to a half-float target the scene target falls back to 8-bit, so tone mapping and bloom keep working but have no range above white to use.
- Shadow maps and reflection probes are not built. Render targets make them possible; nothing in the frontend produces or consumes one yet.
- A shader's own parameter block is parsed from its fragment source, not queried from the graphics interface. A block declared in a vertex section is not seen. Only scalar, vector, and 4x4 matrix members are placed; anything else drops the whole block rather than risk offsets that disagree with the driver.
- Hot reload handles one nominated shader and is unavailable where assets are packaged.
- Visibility cannot be separated from updating; a renderable that must not draw must stop updating.
