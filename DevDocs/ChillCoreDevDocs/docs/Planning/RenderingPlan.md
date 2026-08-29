# Rendering Plan

**Status:** The graphics abstraction and rendering frontend are built and shipping. What remains is two unimplemented capabilities and the compute feature.
**Current state:** AGD-0070 (Rendering Pipeline) and AGD-0080 (Graphics API Abstraction) describe what exists. This plan covers only what does not.
**Sequence:** rows 1–3 of `02_Roadmap.md`. Rendering work not yet scheduled (camera registration, the open design questions) is in `03_TechBacklog.md`.

---

## Offscreen render targets

`CreateRenderTarget` / `DestroyRenderTarget` assert-and-fail in both backends (`GfxRenderApiOpenGl.cpp:1100`, `GfxRenderApiGles.cpp:927`). Only the implicit backbuffer works.

**Why it matters.** The largest capability gap in the abstraction. It blocks a class of features rather than one: shadow maps, reflection probes, and any post-process chain beyond the fixed resolve.

### What already exists

- `RenderTargetDescription` / `RenderTargetAttachment` with `LoadOp` / `StoreOp` / clear colour, up to `MAX_COLOR_ATTACHMENTS` colour attachments plus optional depth-stencil (`GfxDescriptions.h:164`).
- `TextureDescription::isRenderTarget` — the attachment texture path.
- `RenderTargetHandle`, and a `GlRenderTarget` pool (`fbo`, `width`, `height`, `description`, `isAlive`) with `renderTargets` / `freeRenderTargetSlots` members in both backend headers. Nothing writes to the pool.
- `BeginRenderPass(RenderTargetHandle)` / `EndRenderPass()` brackets. `BeginRenderPass` currently ignores the handle and always binds the scene framebuffer; `EndRenderPass` always resolves MSAA and blits to the default framebuffer.
- `GetBackbuffer()` returns a null handle as a placeholder for "the backbuffer as a render target".

### Frontend

- `RenderManager::StartFrame` calls `BeginRenderPass(GetBackbuffer())` (`RenderManager.cpp:151`); `EndRenderPass` is paired in `RenderManager.cpp`. A consumer that wants an offscreen pass needs a way to bracket one **inside** the frame, before the backbuffer pass.
- Add a minimal frontend affordance: `RenderManager::BeginOffscreenPass(RenderTargetHandle)` / `EndOffscreenPass()`, or expose the `Gfx` brackets directly for demo-state use. This is the first real caller that wants pass control, so it forces the *RenderManager versus Gfx::RenderApi access levels* open question — resolve that into AGD-0080 rather than deferring again.
- Texture handle for the colour attachment is sampled in a later pass through the existing `BindTexture` path; no new sampling API.

### Backend work (OpenGl, then Gles)

- `CreateRenderTarget`: allocate an FBO, attach each colour attachment's texture (`glFramebufferTexture2D` at `mipLevel` / `arrayLayer`), attach depth-stencil if `hasDepthStencil`, set `glDrawBuffers` for the colour count, check completeness, store in the pool, return a handle. No MSAA on offscreen targets in this pass — single-sample only.
- `DestroyRenderTarget`: delete the FBO, free the slot, clear it if currently bound.
- `BeginRenderPass`: branch on the handle. Null / backbuffer handle keeps the current scene-framebuffer path. A pool handle binds that FBO, sets the viewport to its dimensions, and applies each attachment's `LoadOp` (clear vs. load) and clear colour.
- `EndRenderPass`: for an offscreen target, apply `StoreOp` and skip the MSAA-resolve-and-blit — that is backbuffer-only. Invalidate the dirty-state cache on pass boundaries (the raw `glBindFramebuffer` / `glClear` bypass the tracked binds).
- Gles mirrors the above. No MSAA scene FB there, so the offscreen path is closer to a straight FBO bind. Watch GLES 3.1 attachment-format and `glDrawBuffers` constraints.

### Capability notes

- Depth-texture sampling and multiple render targets are core in GL 4.3 and GLES 3.1; no capability gate needed for the desktop and Android backends.
- A future WebGL backend will need `WEBGL_draw_buffers` / format checks — leave a `TODO` marker at the `glDrawBuffers` call rather than building the gate now.

### Demo state

- New `AppState` that renders the scene to an offscreen colour target, then draws a fullscreen pass sampling it with a trivial effect (invert or blur) into the backbuffer.
- Doubles as the manual regression check: disabling the effect must produce output identical to the direct path.

**Done when:** the demo state renders to an offscreen target and samples it in a later pass, on both desktop and Android, and `DestroyRenderTarget` leaks no GL objects across a state re-enter.

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
