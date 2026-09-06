# Rendering Plan

**Status:** The graphics abstraction and rendering frontend are built and shipping. What remains is a camera-control defect, three unimplemented optimisations, and three features — compute, shadows and particles.
**Current state:** AGD-0070 (Rendering Pipeline) and AGD-0080 (Graphics API Abstraction) describe what exists. This plan covers only what does not.
**Scope:** the rendering work that is in plan, in the order below. Rendering work that is not in plan — occlusion culling, photometric light units, specular antialiasing, camera registration, the open design questions — is not covered here.

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
- **Visibility belongs to a view, not to an object.** A single flag on a renderable answers "is it on screen" and nothing else. A shadow pass asks what the light sees, and an object behind the camera can still cast into the frame — culled against the camera frustum, its shadow disappears. Producing a per-view result costs nothing extra now and is expensive to retrofit later.
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

---

## Shadow maps

Nothing casts a shadow. Lighting is evaluated per fragment with no test for whether the light reaches it, so geometry is lit through anything in the way and every object appears to float.

**Why it matters.** Contact between objects and the ground reads through shadow before it reads through anything else. Without it a scene has no depth cue tying an object to the surface under it, and the lighting rig can be neither judged nor tuned — no arrangement of intensities substitutes for occlusion.

### The frame sequence is the problem, not the shadow map

A shadow map is opaque geometry drawn depth-only from the light's point of view, into a target of its own, before the scene pass shades anything. The engine has render targets, `TextureFormat::Depth32Float`, and — once the pre-pass lands — depth-only pipelines and a colour write mask. What it does not have is anywhere to put the pass.

- `RenderManager::StartFrame` opens the scene render pass.
- Renderables submit themselves afterwards, during the scene update.
- Passes cannot nest.

So at the moment the scene pass opens there is no geometry list to render a shadow map from, and once it is open no other pass can start. Both halves have to move: the scene pass opens after submission rather than before it, which puts `BeginRenderPass` in `Render` alongside the draw loops it brackets. That touches the fullscreen-quad branch and the post-process target decision, both currently made in `StartFrame`.

The alternative — rendering the shadow map from the previous frame's submission list — keeps the sequence intact and is wrong in a way that shows: a moving object's shadow trails it by a frame, most visible on exactly the fast motion that draws the eye.

### What is missing

**A comparison sampler.** `Gfx::SamplerDescription` describes filtering, addressing and anisotropy. It cannot express a depth comparison, so a shadow map can only be sampled as an ordinary texture and compared in the shader by hand. That gives up the hardware's own comparison and the free bilinear filtering of the comparison result that every GPU provides for it — the cheapest percentage-closer filtering available, discarded before any is written. A `compareEnable` and `compareOp` pair on the description, in both backends.

**Depth bias.** `Gfx::RasterizerState` carries a cull mode and a winding order, and nothing else. Slope-scaled depth bias is the standard answer to shadow acne, and it is rasterizer state on every graphics interface. Doing it in the shader instead is possible and worse: the bias belongs to the pass that writes the map, not to the material that reads it.

**Light-space transform, and room for it.** `FrameUniforms` is 128 bytes of std140, scalars packed into the unused `w` components of surrounding vectors, matching `Include/frameUniforms.glinc` member for member. A light view-projection is another 64 bytes, plus the map's texel size and bias parameters. The layout and its shader declaration change together and must agree exactly, and a mismatch produces wrong values rather than a compile error.

**Something to fit the projection to.** A directional light has no position — it needs an orthographic volume fitted to what the camera can see, and there is nothing describing the extent of a scene to fit against. Per-object bounds arrive with culling; a scene-wide extent is their union.

**Shadow state on the light.** `LightDirectional` holds a direction, a colour and an intensity. Whether it casts, at what resolution, and with what bias are all new.

**One light, deliberately.** `LightManager` holds a map of directional lights while `FrameUniforms` carries exactly one, so the shader has only ever seen one light. Shadowing the one light the shader reads is the whole of the first cut. A second shadow-casting light is a second pass and a second map, and it is not worth designing for until the lighting model carries more than one light at all.

### Watch out

- **Culling has to be per-view, and it is cheaper to build it that way than to retrofit it.** The shadow pass needs what the *light* sees, which is not what the camera sees: an object behind the camera can cast into the frame, and culling it against the camera frustum deletes its shadow. A single `isVisible` flag on a renderable cannot express this. Visibility is a property of a view, not of an object.
- **Acne and peter-panning are one dial with two failure modes.** Too little bias and surfaces self-shadow in stripes; too much and shadows detach from the objects casting them. Slope-scaled bias plus front-face culling in the shadow pass is the usual pairing, and neither is a substitute for the other.
- **Resolution is a fixed budget spent unevenly.** A single map fitted to the whole view spends most of its texels far away where nothing looks closely. Cascades are the production answer and multiply the pass count; a single fitted map is the honest first cut, and it is worth knowing at the outset which one is being built.
- **The map is a full extra submission of opaque geometry per casting light.** With the pre-pass, opaque geometry is already submitted twice. Shadows make it three times, and every one of those passes wants the light's own culling result rather than the camera's.
- **Masked materials cut out in the shadow map too.** The same discard the pre-pass needs, for the same reason: a leaf that is a hole in the base-colour texture must be a hole in the shadow it casts.
- **Transparent geometry does not participate.** It neither casts nor receives correctly, and making it do so is a separate problem. Excluding it explicitly is better than leaving it to fall out of the pass structure by accident.

**Done when:** the directional light casts a shadow from opaque geometry onto opaque geometry, masked materials cut out in the map, moving objects' shadows track them within the frame rather than lagging one, and acne and detachment are both absent at the scene's working scale.

---

## Particle system

Nothing emits particles. There is no renderable that draws many small quads from one submission and no component that simulates them.

**Why it matters.** Smoke, fire, sparks, dust and impacts are the vocabulary a scene uses to look inhabited rather than modelled, and none of it is reachable today. It is also the first thing in the engine that produces geometry per frame rather than loading it once, which is why it costs more than its output suggests: every renderable currently creates its buffers at construction and never writes them again.

### What is missing

**Per-instance vertex attributes.** `DrawIndexed` already takes an instance count, so the draw call itself is there. `VertexAttribute` carries a location, an offset, a type and a component count — there is no divisor, so a buffer that advances once per instance rather than once per vertex cannot be described. Without it the fallback is writing four vertices per particle into a dynamic buffer every frame: four times the bandwidth and four times the CPU work for the same image.

**Additive blending a material can ask for.** `PipelineCache` sets `srcColorFactor` to `SrcAlpha` for every transparent material. `BlendFactor::One` exists in the abstraction and nothing above it can reach it, so additive is unreachable through the material path. Fire and sparks want it for how they look; the first cut wants it for a second reason, below.

**A renderable that writes its geometry each frame.** `BufferMemory::CpuToGpu` and `UpdateBuffer` both exist, so per-frame vertex data is already expressible. Nothing uses them. This is the piece with no precedent to copy rather than the piece with a gap beneath it.

**An emitter component.** Spawn rate, lifetime, initial velocity and spread, gravity, size and colour over life, and the texture. An ordinary component that submits a renderable, so the frame sequence does not change.

### Sorting is the design decision

Transparent objects are sorted per object, by squared distance from the camera. A particle system is one object with one transform, so every particle in it carries the same sort key. Correct blending between the particles of a single system is not something that sort can express, and sorting per particle means sorting every live particle every frame.

**Make the first cut additive only.** Additive blending does not depend on draw order, so the sorting problem does not arise rather than being solved badly. It covers fire, sparks, embers and energy effects outright. Alpha-blended particles — smoke, dust — then arrive as their own decision with the sorting question isolated, rather than as a correctness bug discovered after the fact.

### Watch out

- **Particles are the worst case for overdraw, and the pre-pass does not help.** Transparent geometry writes no depth and takes no part in the pre-pass, so a screenful of smoke is a screenful of blended fragments no matter how cheap each one is. The budget is fill rate, not particle count, and a system that looks free at a thousand particles can cost the frame at the same count drawn larger.
- **Simulate on the CPU first.** A GPU-driven system is the eventual answer, and it only pays when simulation, sorting and the draw all stay on the GPU — a partial version reads results back and loses to the CPU it replaced. Build the CPU one and let compute replace it whole.
- **Culling is per system, and its bounds move.** A particle system is one renderable, so it is culled as a unit. Its extent is the extent of its live particles, which changes every frame and cannot be computed once at load the way a mesh's can. A conservative bound derived from emitter shape, maximum lifetime and maximum speed costs nothing per frame and does not require touching the particles.
- **Scene authoring cannot reach blend modes.** Alpha mode is not authorable in scene YAML; it arrives only through glTF import. An emitter declared in a scene file needs its blend mode to come from somewhere, and that is a scene-format question rather than a rendering one.
- **Soft particles need depth the shader cannot sample.** Particles will intersect geometry in a hard line until scene depth is readable. That is a known and accepted look here, and the work to change it is not covered here.

**Done when:** an emitter declared in a scene spawns additive particles that simulate, draw in one instanced submission, and cull as a unit, with the live particle count and the system's fill cost both visible in the profiler.
