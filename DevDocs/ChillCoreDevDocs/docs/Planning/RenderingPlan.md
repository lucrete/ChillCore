# Rendering Plan

**Status:** The graphics abstraction and rendering frontend are built and shipping. What remains is a camera-control defect, three unimplemented optimisations, and the compute feature.
**Current state:** AGD-0070 (Rendering Pipeline) and AGD-0080 (Graphics API Abstraction) describe what exists. This plan covers only what does not.
**Scope:** the rendering work that is in plan, in the order below. Rendering work that is not in plan — photometric light units, specular antialiasing, camera registration, the open design questions — is not covered here.

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

---

## Frustum culling

Every enabled renderable is drawn. Nothing asks whether it is on screen, so an object behind the camera costs a pipeline bind, a uniform upload and vertex processing for all of its vertices before the clipper throws the result away.

**Why it matters.** Rejecting an object costs a handful of plane tests. Drawing an invisible one costs a draw call and its full vertex load. That ratio is why culling is the first optimisation in every renderer, and it is the one whose payoff grows fastest with scene size: a camera in a dense scene sees a small fraction of it at a time.

### What is missing

**Bounds. Nothing in the engine has any.** No renderable, mesh or scene object carries a bounding volume, so there is nothing to test. All mesh geometry passes through `RenderableMesh::Initialize` and `InitializeWithTangents`, and position is attribute 0 at offset 0 in both vertex layouts, so a local bound is computable in those two places at load time and covers primitives, OBJ and glTF alike. glTF also declares min and max on its position accessor; computing from the vertices keeps one path rather than two that can disagree.

**World bounds, per frame.** `SceneObject::GetWorldMatrix` composes parent transforms, and `Transform` carries a quaternion rotation and non-uniform scale, so a rotated local box does not become an axis-aligned one by transforming its min and max corners. Two shapes are worth considering, and they trade against each other rather than one being correct:

- **Sphere** — centre through the world matrix, radius scaled by the largest scale component. One dot product and a compare per plane. Loose on anything not roughly round, so more invisible objects survive the test.
- **Box** — the rotated local box expanded by the absolute values of the world matrix's basis vectors. Tighter, and several times the arithmetic per test.

**Frustum planes.** `CameraBase` stores the combined view-projection only: view and projection are locals inside `UpdateViewProjectionMatrix`, and field of view, near and far are literals there. Six planes extract from the combined matrix by adding and subtracting its rows, which needs no camera API change and works unaltered for an orthographic projection.

**An exemption for screen-space renderables.** `RenderableFullscreenQuad` submits itself through the same path as everything else, and its geometry is not in world space. A world-space test culls it on the first frame the camera looks anywhere. Screen-space renderables need a flag that skips the test, not bounds that lie about where they are.

**A counter that shows it working.** Objects submitted against objects drawn, per frame, alongside the existing per-pass timings. Without it a culling bug reads as a rendering bug, and a culling win cannot be quantified.

### Where the test goes

The one real decision, and it is a frame-sequencing question rather than a maths one. Renderables submit themselves during the scene update. The camera is updated afterwards, at the top of `RenderManager::Render`, deliberately — so that camera-follow components and transform changes made during the update are reflected in the frame's uniforms.

- **At submission.** Rejected objects never take a slot in the fixed-capacity submission array. But the newest frustum available at that point is the previous frame's, so fast camera motion pops objects in at the screen edge a frame late.
- **After submission, before the sorts.** Tests against the current frame's camera and cannot pop. Every object has already occupied an array slot and paid its submission cost by then.

Culling after the camera update is the correct-by-default choice, and array pressure is better treated as its own problem than paid for in visible popping. Moving the camera update ahead of the scene update would give submission-time culling a current frustum, but it makes camera-follow lag a frame — a worse trade in the other direction.

### Watch out

- **A bound that is too small is worse than no culling.** Geometry disappears at the screen edge, intermittently, and only from certain angles. Bounds must be conservative everywhere they are approximated, and a debug draw of the volumes is what makes a wrong one obvious.
- **World bounds are a per-frame result, not a cached one.** A parent moving changes every child's world bound. Caching them across frames requires something that knows a transform changed, and nothing tracks that today.
- **Cost is not zero at low object counts.** A handful of objects pays the tests and rejects nothing. Like the pre-pass, this wants measuring against a dense scene rather than the current ones.
- **The far plane is a hardcoded 100 units.** Objects beyond it are already clipped — after being drawn. Culling makes that rejection cheap, but the literal itself stays a camera concern rather than something the culler should reach into.
- **This rejects what is outside the view, not what is hidden behind something else.** Occlusion is a different mechanism. For shading cost the depth pre-pass below covers it; for draw-call cost nothing does, and that is not covered here.

**Done when:** objects outside the view frustum are not drawn, screen-space renderables are unaffected, a submitted-against-drawn counter is visible with the frame timings, and camera motion at speed produces no popping at the screen edges.

---

## Depth pre-pass for opaque geometry

Opaque geometry is shaded in the order it is submitted. Nothing populates depth ahead of shading, so a fragment hidden by a nearer one drawn later is fully shaded and then discarded.

**The sort cannot fix this.** The opaque list is sorted by pipeline, then material, which is what lets the layer below skip a redundant pipeline bind. Ordering the same list front to back would break that grouping. One list has one order, so a single pass buys batching or overdraw rejection, never both.

**A pre-pass removes the choice.** Opaque geometry is drawn twice: once writing depth and no colour, then again shaded with the depth test set to equal and depth writes off. Occluded fragments fail the test before the fragment shader runs, in whatever order the shading pass submits. The shading pass keeps its pipeline sort untouched.

**Why it matters.** Overdraw costs fragment-shader time multiplied by how many surfaces cover a pixel. The pre-pass costs a second submission of the same geometry with a position-only shader. That trade goes one way as shading gets more expensive and scenes get denser — a physically-based material samples several textures and evaluates a BRDF per light, and dense geometry covers a pixel several times over. It is the standard structure for that reason.

### What is missing

**A colour write mask on pipelines.** `Gfx::BlendState` carries blend factors and operations, and nothing else. There is no way to describe a draw that writes depth and no colour, which is the pre-pass's defining state. Both backends need it, and the pipeline bind-skip cache has to track it alongside the state it already compares.

**Depth-only pipelines.** Keyed by vertex layout rather than by material, since the pass does not shade: all opaque geometry sharing a layout shares one pipeline. Masked materials are the exception below. Far fewer pipelines than the shading pass, so the pre-pass binds little.

**A way to draw a renderable with a pipeline other than its own.** `Renderable::PreRender` binds `pipelineHandle` unconditionally and then uploads the material's standard uniforms with the model matrix. The pre-pass needs the per-draw transform and nothing else — not the material block, not the textures. Splitting the pipeline bind out of that method, or giving the depth path its own, is the frontend change.

**Nothing else in the frame sequence.** The scene pass is already open when the draw loops run, so both loops sit inside it and share the multisampled depth attachment. No second pass bracket, no depth clear between them, no change to the transparent pass or the post-process stack.

### Watch out

- **Both passes must compute position identically.** An equal depth compare demands bit-identical results from two different shaders. A compiler is free to reassociate the transform differently in each, and the failure is not subtle — surfaces drop out across the whole frame where the two disagree in the last bit. Declaring `gl_Position` invariant in both, and computing it through one shared include, is what holds this.
- **Masked materials cannot use the position-only shader.** `AlphaBlendMode::Mask` discards on base-colour alpha. A pre-pass ignoring that writes depth across the holes, and the shading pass then rejects what should have shown through them. Masked geometry needs its base-colour texture and cutoff in the pre-pass, which is why it needs pipelines keyed per material rather than per layout.
- **Every opaque renderable is submitted twice.** The pre-pass doubles opaque draw calls and vertex work outright, and a scene bound by draw calls or vertex processing rather than shading loses on that trade. Culling is what changes the arithmetic, by halving what this doubles — the two compound, and this pass is worth measuring with culling in place rather than without it.
- **It does not pay at every scene size.** Cheap fragment shaders or little overlap means paying the second submission for nothing. It wants a switch and a measurement, not unconditional enablement.
- **Front-to-back ordering belongs in the pre-pass, not the shading pass.** The pre-pass has early-z of its own, and it has no batching to protect — its pipelines are few and its state near-identical across draws. A distance sort there costs nothing that matters; in the shading pass it costs the thing the pre-pass exists to preserve.
- **A complete opaque depth buffer falls out of this.** Depth-aware post-process effects would need exactly that, and this produces it as a by-product rather than as a goal. That work is not in plan and is not covered here.
- **The fullscreen-quad path is unaffected.** It bypasses the scene passes entirely and has no depth to pre-populate.

**Done when:** opaque shading runs against a fully populated depth buffer, occluded fragments are not shaded, the shading pass keeps its pipeline-then-material sort, masked materials cut out correctly in both passes, and a dense test scene shows a frame-time win over the pass disabled.
