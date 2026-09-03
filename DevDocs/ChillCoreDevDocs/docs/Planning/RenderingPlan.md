# Rendering Plan

**Status:** The graphics abstraction and rendering frontend are built and shipping. What remains is one unimplemented optimisation and the compute feature.
**Current state:** AGD-0070 (Rendering Pipeline) and AGD-0080 (Graphics API Abstraction) describe what exists. This plan covers only what does not.
**Scope:** the rendering work that is in plan, in the order below. Rendering work that is not in plan — photometric light units, specular antialiasing, camera registration, the open design questions — is not covered here.

---

## Dirty-state cache beyond pipelines

Binding a pipeline compares against the last one bound and returns early. Binding a vertex buffer, a texture, or a uniform buffer does not.

**Why it matters.** It is the cheapest remaining call-count win, and the invalidation discipline that makes caching safe already exists. It also closes a real gap: the opaque list is sorted so same-material draws are adjacent, but nothing exploits that below the pipeline level, so same-material objects still re-bind their uniform block and textures every draw.

**Watch out.** Any newly cached bind inherits the raw-graphics-API hazard. Developer-overlay drawing, the render-pass clear, and the resolve blit all bypass `Gfx::RenderApi`. Anything cached must be reset by the cache-invalidation call, or stale state shows up as a wrong-texture or black-frame bug.

**Done when:** redundant vertex-buffer, texture, and uniform-buffer binds are skipped and frame output is unchanged.

---

## Compute shaders

The backend surface is built and implemented for both backends: `DispatchCompute`, `DispatchComputeIndirect`, `BindStorageBuffer`, `BindImage`, `MemoryBarrier`, and `CreateShader` accepting `description.computeSource`. `supportsComputeShaders` / `supportsStorageBuffers` report true on both, GLES 3.1 included. Nothing above the backend can reach any of it.

### What is missing

**Shader authoring.** `ShaderManager` does not parse a `#shader compute` section. This is the single blocking change — until it lands, no compute shader can exist.

- `CC::ComputeShaderDefinition` — a compute shader definition type, parallel to the existing one but with a single source stream and no vertex or fragment split.
- A compute entry in the shader manager, with its own lookup and compile path routing through the existing backend shader creation.
- Include resolution is unchanged and reuses the existing mechanism.
- Shader parsing gains a third stream. A file declaring only a compute stage registers as compute; a file declaring vertex and fragment stages registers as raster. No file mixes all three.
- Hot reload extends to compute shader files, and anything caching a compiled handle re-fetches on reload.

**Storage buffer wrapper (`CC::ComputeBuffer`).** A typed wrapper over an SSBO used for compute input and output, covering allocation, upload, and readback. The underlying buffer usage and binding already exist in the abstraction.

**Dispatch unit (`CC::ComputePass`).** The compute-side analogue of a renderable: it holds a compute shader reference, its bound buffers and images, and dispatches. It should offer dispatch by total thread count as well as by workgroup count, rounding up internally, so callers think in the terms the problem is stated in rather than in workgroup arithmetic.

### Capability gating

Compute and storage buffers are already reported in the capability set, and both current backends report them available. Any consumer must branch on those values rather than assuming, because a future web backend will report both unavailable and will need a non-compute path.

### Areas needing design before use

- **Atomic operations on storage buffers** are available but unexercised.
- **Atomic counters and append buffers** need a readback and reset story before anything depends on them.
- **Image atomics** are more constrained than buffer atomics and need their supported formats confirmed per backend.

**Done when:** a compute demo state runs on desktop and on Android, and reports itself unsupported rather than failing where the capability is absent.
