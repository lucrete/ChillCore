# Rendering Plan

**Status:** The graphics abstraction and rendering frontend are built and shipping. What remains is one set of follow-ups left by work that has landed, a camera-control defect, one unimplemented optimisation, and the compute feature.
**Current state:** AGD-0070 (Rendering Pipeline) and AGD-0080 (Graphics API Abstraction) describe what exists. This plan covers only what does not.
**Scope:** the rendering work that is in plan, in the order below. Rendering work that is not in plan — specular antialiasing, camera registration, the open design questions — is not covered here.

---

## Post-processing follow-ups

Gaps left open when the post-process stack landed. Ordered by what they unlock, not by cost.

The effect chain and the procedural-art question each need a decision recorded before code is written; the notes below say which.

### A caller-defined effect chain

Effects fuse into one pass in an order fixed by the shader. A caller cannot reorder them or insert its own.

**Why it matters.** It is the ceiling on the whole stack. Any effect that does not fit the fused pass — depth of field, motion blur, anything needing its own targets — has nowhere to go, and the fused-pass shader grows a block per effect whether or not it is enabled.

**Shape of the work.** The open question is what a chain entry is: a shader plus parameters, or something that can also declare intermediate targets. Bloom is the existing example of an effect that needs its own targets and its own passes, so it is the case to design against rather than an exception to it. Resolve that before writing anything.

**Watch out.** The fused pass exists for a reason — a chain of separate passes is bandwidth bound and costs a full read and write of the frame per effect, which is the dominant cost on a tiled mobile GPU. A chain must still fuse what it can rather than becoming a pass per effect.

**Done when:** a caller can declare an ordered chain including an effect of its own, and effects that can fuse still share one pass.

### Baking grade and tone map into a LUT

Both commercial engines evaluate the grade and tone curve once into a lookup table and sample it per pixel, rather than evaluating the maths per pixel.

**Why it matters.** It moves the whole grade off the per-pixel path, and it is what makes an arbitrarily expensive grade cost the same as a cheap one.

**Shape of the work.** Render to a 3D texture, or to the 2D strip that mobile paths use where 3D render targets are unavailable. The 3D route needs array-layer attachments, so the render-target follow-up above blocks it; the 2D strip route does not and is the sensible first target.

**Watch out.** LUT resolution trades against banding in smooth gradients. Decide the resolution against a test gradient rather than a scene.

**Done when:** grade and tone map are sampled from a baked table, the result matches the per-pixel path within a visually indistinguishable margin, and a capability-poor backend still has a path.

### Procedural art through the tone curve

Procedural art authors display-referred colour and converts to linear on output. With the tone map on, the curve then compresses it, so the art reads softer than it was picked. With the tone map off the round trip is exact.

**Why it matters.** The art is finished pixels, not scene light, so applying a tone curve to it is a category error. It is cosmetic today because the art is the only content of its kind, and it stops being cosmetic as soon as anything else authors finished pixels.

**Shape of the work.** Either the art is retuned against the curve, which is cheap and keeps the pipeline uniform, or the path gains a way to mark content as already display-referred and skip the output transform. The stack has no concept of the second. Pick one deliberately rather than drifting into the first by inaction.

**Done when:** procedural art reads as authored with the tone map in its default state.

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
