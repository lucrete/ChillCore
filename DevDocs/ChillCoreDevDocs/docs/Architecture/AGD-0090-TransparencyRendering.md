# AGD-0090: Transparency Rendering

- **Scope:** How transparent materials are declared, routed, sorted, and drawn. Covers alpha modes, per-material opacity, and the two-pass render split. Does not cover the graphics abstraction itself, shadows, or post-processing.

## Overview

- Transparency is a property of a material, not of a shader. The same shaders serve opaque and transparent draws.
- A material is transparent when its opacity is below one, or when its alpha mode is Blend.
- Transparent renderables are drawn in a second pass, sorted farthest-first, blending against the opaque scene already in the depth buffer.
- Alpha behaviour follows the glTF specification's three modes, so imported assets need no translation layer.

## Concepts

- **Opacity** — a per-material multiplier on fragment alpha. One is fully opaque.
- **Alpha mode** — how a fragment's alpha is interpreted. Three modes, matching glTF:
  - *Opaque* — alpha is ignored, fragments are always solid.
  - *Mask* — fragments below a cutoff threshold are discarded; survivors are fully solid. Suits foliage, fences, and decals.
  - *Blend* — alpha is taken from texture alpha multiplied by material opacity, and the fragment is blended.
- **Transparent routing** — the test that decides which pass a renderable enters. Owned by `Material`, derived from opacity and alpha mode together.

## Architecture

`Material` owns all alpha state — opacity, alpha mode, and cutoff — and answers whether it is transparent. Nothing else decides transparency; both the render split and the pipeline configuration read that one answer.

`RenderManager` keeps two renderable lists and assigns each registered renderable to one of them by asking its material. The lists are drawn as separate passes.

`PipelineCache` bakes the consequences of transparency into the pipeline object at creation: blending on, depth writes off. Because pipelines are keyed on material and vertex layout, transparent and opaque materials naturally produce distinct pipelines.

Shaders read opacity, alpha mode, and cutoff from the per-material uniform block rather than from individual uniforms. A shader that does not declare the block is unaffected, which is why procedural-art and UI shaders need no transparency handling.

`Material` also exposes a depth-test opt-out, used by screen-space overlays such as the fullscreen fade. Overlays have no spatial relationship to scene depth and would otherwise test against undefined default-backbuffer depth values.

## Runtime flow

Each frame:

1. Renderables register with `RenderManager`, which routes each into the opaque or transparent list according to its material.
2. The opaque list is sorted by pipeline, then drawn. Depth writes are on.
3. The transparent list is sorted back-to-front by squared distance from the camera to the object's world position, then drawn.
4. Blending and depth-write state are not toggled between the passes. Each renderable's pipeline already carries the correct state, so binding the pipeline establishes it.

Depth testing stays enabled throughout. Transparent geometry reads the depth the opaque pass wrote, so closer opaque objects correctly occlude it, but writes nothing itself.

## Working with it

**Authoring a transparent material in a scene.** Add an optional opacity field; omitting it means fully opaque.

```yaml
materials:
  - name: TransparentBlue
    shader: LitColour
    baseColor: [0.3, 0.5, 1.0]
    opacity: 0.5
```

Scene-authored materials use Opaque alpha mode with opacity alone driving transparency. Mask and Blend modes are reachable only through glTF import.

**Importing from glTF.** The loader maps the file's alpha mode directly onto the material. Mask mode carries the file's cutoff value across; Blend mode takes opacity from the base colour factor's alpha channel.

**Adding a screen-space overlay.** Disable depth testing on the overlay's material. Leaving it enabled gates the overlay against undefined depth in the default backbuffer, which presents as an overlay that intermittently fails to draw.

## Design decisions

### Transparency is two sorted passes, not order-independent

Opaque geometry is drawn first, then transparent geometry back-to-front. This is correct for non-intersecting transparent objects, which covers essentially all current use, and costs one sort per frame plus a list split.

Order-independent transparency was rejected as disproportionate: it needs additional render targets and a compositing pass, and offscreen render targets are not implemented in the graphics backend at all. Alpha-to-coverage was rejected for visual quality — the dithering it produces is acceptable for foliage and poor for smooth glass.

The cost is that intersecting transparent meshes cannot sort correctly, and no amount of tuning fixes it. Revisit only when content actually demands intersecting transparency.

### Alpha state is material data, not a shader variant

One shader set serves both opaque and transparent materials. Alpha mode, cutoff, and opacity travel to the shader as material data, and the fragment stage branches on the mode.

The alternative, separate transparent shader variants, would double the shader count and require every lighting or PBR change to be made twice. There is no offsetting benefit: fragment output carries alpha in all cases, and whether that alpha blends is a property of the bound pipeline.

Opaque materials pay nothing, because opacity defaults to one and Opaque mode ignores alpha entirely.

### Alpha modes mirror the glTF specification

Three modes rather than a boolean, because a boolean cannot distinguish Mask from Blend, and those differ in kind rather than degree — one discards fragments, the other blends them. Opacity alone is equally insufficient, since Mask geometry is fully solid above its cutoff rather than semi-transparent.

Matching the glTF vocabulary exactly means imported assets need no translation and no lossy mapping.

An earlier implementation carried a separate alpha-blend boolean alongside opacity. It was folded into the mode enum when cutoff support arrived, at which point the boolean was redundant with Blend mode.

### Transparent objects are sorted per object, by squared distance

Sorting compares squared distances from the camera to each object's world position, avoiding a square root per object per frame.

Per-triangle sorting would handle intersecting geometry, but requires splitting meshes and re-uploading vertex data every frame. That cost is not defensible at current object counts. Leaving the list unsorted produces obviously wrong results wherever transparent objects overlap.

The consequence is the central limitation of the whole system: correctness is per object, so intersecting or interleaved transparent meshes will show ordering artefacts.

### Transparent geometry reads depth but does not write it

Depth writes are disabled for the transparent pass. Writing depth would let a nearer transparent surface reject a farther one behind it, which is precisely what back-to-front blending needs to happen.

Reading depth is still required, so that opaque geometry occludes transparent geometry correctly.

### Pass state is baked into pipelines rather than toggled between passes

Blending and depth-write state were originally set by the renderer around the transparent pass and restored afterwards. They are now baked into each pipeline when it is created, derived from the material's transparency.

This followed the move to baked pipeline objects. It removes a class of bug — state left enabled after an early return, or leaking into an unrelated pass — and it means the renderer no longer manages transitional graphics state at all. The trade is that changing a material's transparency after creation must recreate its pipeline rather than flip a flag.

## Limitations

- Sorting is per object. Intersecting transparent meshes render incorrectly, and no ordering of the list fixes it.
- No order-independent transparency.
- Transparent objects do not write depth, so they cannot occlude one another by depth test — only by sort order.
- No separate transparent shadow pass. Transparent geometry casts and receives shadows as though solid, where shadows exist at all.
- Mask and Blend alpha modes cannot be authored in scene YAML; they arrive only through glTF import.
