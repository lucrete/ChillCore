# Rendering Plan

**Status:** The graphics abstraction and rendering frontend are built and shipping. What remains is one set of follow-ups left by work that has landed, a camera-control defect, one unimplemented optimisation, and the compute feature.
**Current state:** AGD-0070 (Rendering Pipeline) and AGD-0080 (Graphics API Abstraction) describe what exists. This plan covers only what does not.
**Scope:** the rendering work that is in plan, in the order below. Rendering work that is not in plan — specular antialiasing, camera registration, the open design questions — is not covered here.

---

## Post-processing follow-ups

Gaps left open when the post-process stack landed. Ordered by what they unlock, not by cost.

### Baking grade and tone map into a LUT

Both commercial engines evaluate the grade and tone curve once into a lookup table and sample it per pixel, rather than evaluating the maths per pixel.

**Why it matters.** It moves the whole grade off the per-pixel path, and it is what makes an arbitrarily expensive grade cost the same as a cheap one.

**Shape of the work.** Render to a 3D texture, or to the 2D strip that mobile paths use where 3D render targets are unavailable. Texture creation covers 2D, array and cubemap shapes only, so the 3D route needs that gap closed first; the 2D strip route needs nothing new and is the sensible first target.

**Measured before assuming.** The grade and tone curve are not what the fused pass spends its time on. At 800x600 the pass averages 0.598 ms with both enabled and 0.565 ms with both disabled: the ACES curve costs 0.031 ms and the entire per-channel grade a further 0.002 ms, around 0.2% of a frame. A grade doing three logs, three exps and three pows per pixel for 0.002 ms means the pass is bandwidth bound rather than arithmetic bound — which is the same reason the pass is fused in the first place. A lookup table moves arithmetic off the per-pixel path and adds a texture fetch, so on this evidence it would save nothing and could cost. Desktop only; the balance on device is unmeasured.

**So the reason to do it is authoring, not speed.** A baked table can be imported: a colourist grades in a standalone tool and hands over a `.cube` file, which the preset table cannot accept. If this is picked up, it should be framed as that feature.

**Watch out.** LUT resolution trades against banding in smooth gradients. Decide the resolution against a test gradient rather than a scene.

**Done when:** grade and tone map are sampled from a baked table, the result matches the per-pixel path within a visually indistinguishable margin, and a capability-poor backend still has a path.

### Photometric light units

Light intensities are authored numbers, not physical units.

**Why it matters.** Linear lighting makes real units possible, and real units are what let a light be specified once and behave the same in every scene. Nothing requires it yet, which is why it sits at the end of this list.

**Shape of the work.** Light intensity gains a unit, exposure becomes a camera property rather than a post-process parameter, and existing scenes are re-authored. The exposure control already lives in the post-process stack, so this crosses two subsystems.

**Watch out.** Re-authoring every existing scene is the bulk of the work, not the shader change.

**Done when:** lights are specified in physical units, exposure is a camera property, and shipped scenes are re-authored against both.

---

## Frame-rate dependent camera control

`CameraFree::UpdateTransform` paces itself per frame rather than per second, in three separate ways.

- **Movement uses a hardcoded step.** `deltaSeconds` is the literal `0.016f`, so the free camera travels at its nominal speed only at 62.5 FPS and is wrong everywhere else. `FrameTimer::Get()->DeltaTime()` is what the rest of the engine already uses.
- **Rotation is not time-scaled at all.** Yaw and pitch accumulate the raw stick value times a velocity constant every frame. A gamepad or touch stick is a *rate* input — held deflection should mean a constant turn per second — so the same stick position turns twenty times faster at 1200 FPS than at 60. The showcase scene runs at four-figure frame rates in a Debug build, so this is the common case, not the corner.
- **The mouse ceiling moves with the frame rate.** The mouse is a *displacement* input, not a rate one: `InputManager` re-centres the cursor each frame, so the value the camera reads is already the distance moved since the last frame. Summed over a gesture it is frame-rate independent, and multiplying it by delta time would be wrong. What is frame-rate dependent is the clamp — the offset saturates at `MAX_MOUSE_DEFLECTION` pixels *per frame*, so a fast flick loses motion at low frame rates and loses none at high ones.

**Why it matters.** Look sensitivity is the most immediately felt property of a 3D camera, and it currently changes with scene complexity, build configuration, and vsync. It also makes any tuning of the velocity constants meaningless, since the value that feels right on one machine is wrong on another.

**Watch out.** The rate/displacement split is the substance of the work: scaling every input by delta time uniformly would fix the sticks and break the mouse. The two paths meet in `CameraFree::UpdateTransform` because `InputManager` presents the mouse as a virtual right stick, so whatever separates them has to survive that. Clamping the mouse per second rather than per frame is one option; another is to leave the mouse path alone and scale only the true rate inputs.

**Done when:** the free camera moves and turns at the same speed per second across frame rates, verified by comparing a vsync-limited run against an uncapped one, and a mouse gesture of a given physical distance turns the camera by the same angle in both.

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
