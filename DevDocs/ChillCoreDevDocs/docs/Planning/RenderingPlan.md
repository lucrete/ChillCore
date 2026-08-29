# Rendering Plan

**Status:** The graphics abstraction and rendering frontend are built and shipping. What remains is two unimplemented capabilities, an unrun acceptance pass, and the compute feature.
**Current state:** AGD-0070 (Rendering Pipeline) and AGD-0080 (Graphics API Abstraction) describe what exists. This plan covers only what does not.
**Index:** the Rendering section of `01_TechBacklog.md`.

---

## Offscreen render targets

Render target creation and destruction assert and fail in both backends. The descriptor and handle types exist; the implementation does not. Only the implicit backbuffer works.

**Why it matters.** This is the largest capability gap in the abstraction, and it blocks a class of features rather than one feature: shadow maps, reflection probes, and any post-process chain beyond the fixed resolve. The render-pass bracket was designed for it and already accepts a render target handle, so the frontend needs no change — the work is confined to the two backends.

**Done when:** a demo state renders to an offscreen target and samples it in a later pass, on both desktop and Android.

---

## Dirty-state cache beyond pipelines

Binding a pipeline compares against the last one bound and returns early. Binding a vertex buffer, a texture, or a uniform buffer does not.

**Why it matters.** It is the cheapest remaining call-count win, and the invalidation discipline that makes caching safe already exists. It also closes a real gap: the opaque list is sorted so same-material draws are adjacent, but nothing exploits that below the pipeline level, so same-material objects still re-bind their uniform block and textures every draw.

**Watch out.** Any newly cached bind inherits the raw-graphics-API hazard. Developer-overlay drawing, the render-pass clear, and the resolve blit all bypass `Gfx::RenderApi`. Anything cached must be reset by the cache-invalidation call, or stale state shows up as a wrong-texture or black-frame bug.

**Done when:** redundant vertex-buffer, texture, and uniform-buffer binds are skipped and frame output is unchanged.

---

## Acceptance measurements

The abstraction work defined quantitative gates. No run is recorded against any of them.

- Per-frame graphics call count before and after, on a multi-object scene. The target was roughly 280 uniform and bind calls down to under 40 for ten physically-based spheres.
- Frame duration stable or improved — the rework must not have regressed frame time.
- A capture showing the labelled scope hierarchy on a debug build.
- Visual parity across the showcase, boot, procedural art, and physically-based scenes.
- Multisampling still taking effect when the sample count is changed through the backbuffer descriptor.

**Why it matters.** Draw-call reduction was half the justification for the whole abstraction, and it is currently an assumption. If the numbers do not show the expected drop, something in the uniform tiering or the sort is not doing its job, and that is worth knowing before more work is built on top. The measurement is also the only remaining check on whether the migration regressed rendering behaviour anywhere.

---

## Compute shaders

The backend surface is built. `Gfx::RenderApi` provides compute dispatch, indirect dispatch, storage-buffer and image binding, and memory barriers; shader creation accepts a compute stage; both backends implement all of it and report compute and storage buffers as available. Nothing above the backend can reach it.

### What is missing

**Shader authoring.** Shader compilation does not recognise a compute stage. This is the single blocking change — until it lands, no compute shader can exist.

- A compute shader definition type, parallel to the existing one but with a single source stream and no vertex or fragment split.
- A compute entry in the shader manager, with its own lookup and compile path routing through the existing backend shader creation.
- Include resolution is unchanged and reuses the existing mechanism.
- Shader parsing gains a third stream. A file declaring only a compute stage registers as compute; a file declaring vertex and fragment stages registers as raster. No file mixes all three.
- Hot reload extends to compute shader files, and anything caching a compiled handle re-fetches on reload.

**Storage buffer wrapper.** A typed wrapper over a buffer used for compute input and output, covering allocation, upload, and readback. The underlying buffer usage and binding already exist in the abstraction.

**Dispatch unit.** The compute-side analogue of a renderable: it holds a compute shader reference, its bound buffers and images, and dispatches. It should offer dispatch by total thread count as well as by workgroup count, rounding up internally, so callers think in the terms the problem is stated in rather than in workgroup arithmetic.

### Capability gating

Compute and storage buffers are already reported in the capability set, and both current backends report them available. Any consumer must branch on those values rather than assuming, because a future web backend will report both unavailable and will need a non-compute path.

### Areas needing design before use

- **Atomic operations on storage buffers** are available but unexercised.
- **Atomic counters and append buffers** need a readback and reset story before anything depends on them.
- **Image atomics** are more constrained than buffer atomics and need their supported formats confirmed per backend.

**Done when:** a compute demo state runs on desktop and on Android, and reports itself unsupported rather than failing where the capability is absent.
