# 01 — Tech Backlog

Single index of outstanding engineering work. Per `Guidelines/CodingGuidelines.md`, future work is captured here rather than in `// Todo` comments.

- **Source of truth for status.** Statuses are verified against code, not against the plan docs. Where a plan doc disagrees, this doc wins and the plan doc is corrected.
- **Scope.** Every workstream with unfinished work, plus open design questions, verification debt, and documentation debt.
- **Detail lives in the plan docs.** Each section links to the plan that carries the design rationale.

Last verified against the source code: 2026-08-28.

## Status legend

Nothing is in flight. Every actionable item is **Ready**; the backlog does not track assignment, and priority is expressed in *Suggested ordering* rather than per item.

| Marker | Meaning |
| --- | --- |
| **Ready** | Designed, unblocked, not started. |
| **Blocked** | Waiting on another item. |
| **Deferred** | Deliberately postponed; design exists. |

---

## Workstream status at a glance

| Workstream | Plan | State |
| --- | --- | --- |
| Rendering | `RenderingPlan.md` | Material-to-pipelines, render targets, dirty-state cache, compute |
| AudioTracker | `AudioTrackerPlan.md` | Phases 0–5 landed; velocity editing and project handling open |
| Platform and build | `PlatformAndBuildPlan.md` | Android ships; CMake, WebGL, further backends open |
| Persistence | `PersistencePlan.md` | Not started |

---

## Rendering

### Material to pipelines (**Ready**)

Last unfinished piece of the graphics abstraction migration. `UiRenderer` and `TextRenderer` already bind pipelines; `Material` is the only holdout.

- `Material::Bind` calls `Gfx::RenderApi::BindShaderProgram` (`Material.cpp:132`). Replace with `BindPipeline`.
- Add a `PipelineHandle` member to `Material` for standalone (non-renderable) use.
- Remove the transitional shims once Material migrates: `BindShaderProgram`, `GetUniformLocation`, `SetUniformMat4` / `Vec4` / `Vec3` / `Vec2` / `Float` / `Int` (`GfxRenderApi.h:102-109`), the matching `Material::SetUniformInternal` overloads, and both backend implementations.
- **Ship:** no transitional shim methods remain on `Gfx::RenderApi`.

### Render target creation (**Ready**)

- `CreateRenderTarget` / `DestroyRenderTarget` assert-and-fail in both backends (`GfxRenderApiOpenGl.cpp:1099`, `GfxRenderApiGles.cpp:927`). Only the implicit backbuffer works.
- Blocks any offscreen pass: shadow maps, post-process chains beyond the fixed blit, reflection probes.

### Dirty-state cache completion (**Ready**)

- Only `BindPipeline` short-circuits redundant binds.
- Extend the same skip logic to `BindVertexBuffer`, `BindTexture`, `BindUniformBuffer`.

### Compute shader path (**Ready**)

Backend surface exists and is implemented for GL — `DispatchCompute`, `DispatchComputeIndirect`, `BindStorageBuffer`, `BindImage`, `MemoryBarrier` — and `CreateShader` accepts `description.computeSource`. Everything above the backend is missing.

- `ShaderManager` does not parse a `#shader compute` section. Nothing can author a compute shader today.
- `CC::ComputeShaderDefinition`, `CC::ComputeBuffer` (SSBO wrapper), `CC::ComputePass` do not exist.
- Atomic counters / append buffers unexercised.
- No compute demo AppState — the plan's ship criterion.
- Capability-gate on `supportsComputeShaders` / `supportsStorageBuffers`; GLES 3.1 reports both true.

---

## AudioTracker

Phases 0–5 are landed. `AudioTrackerPlan.md` carries the per-phase design.

### Velocity editing (**Ready**)

The plan's primary editing affordance; the data model supports it, the editor does not.

- `SetStepVelocityCommand` does not exist. Named as pending in `ToggleStepCommand.h:17`.
- Pattern cells render binary on/off (`AudioTrackerController.cpp:713`). Add brightness / fill-height encoding of velocity.
- Add vertical-drag adjustment on cells. May require drag support in the UI input layer.
- Secondary affordances — scroll wheel, keyboard nudge — belong with editor polish.

### Missing commands (**Ready**)

- `SetPatternTrackGainCommand`, `RenamePatternCommand`, `ClearSongCellCommand`, `RemoveSongTrackCommand`, `SetSongTrackGainCommand`.
- No concrete command overrides `TryCoalesceWith`. Drag-paint and slider-drag push one undo entry per event until they do.

### Project file handling (**Ready**)

- Save and Load use one fixed `GetDefaultProjectPath()` (`AudioTrackerController.cpp:238,253`). Add New / Open / Save As.
- In-engine file browser scoped to `Projects/`, showing BPM, song length, and last-edited timestamp inline.
- Auto-save to `Projects/.last-session.cctrack` on exit; restore offer on next launch.
- Surface load errors on schema-version mismatch in the UI rather than failing silently.
- Native `IFileOpenDialog` for sample import. Confirmed as a Phase 2 decision; only `glfwSetDropCallback` drag-drop is implemented.

### Editor first-cuts to replace (**Ready**)

- Track source assignment is a "Cycle Source" button (`AudioTrackerController.cpp:583`). The intended dropdown waits on `UiDropdown` supporting dynamic option lists.
- No playhead in either grid. `TrackerClock` publishes the position for exactly this; nothing reads it for UI.
- `TrackerPaths.h:14` — POSIX branch is a placeholder until a non-Windows desktop target exists.

### Editor polish (**Deferred**)

- Per-track gain sliders and solo. Mute is done in both views; gain and solo are not.
- Variable step subdivision — 8th, 16th, 32nd, triplets.
- Pattern length editing after creation.
- Waveform display in the sample panel.
- Keyboard shortcuts — space play/stop, 1–9 track select.
- Mid-song BPM and time-signature automation lane.
- Export to WAV via offline render through `TrackerEngine`.

---

## Platform and build

### Persistence and lifecycle hooks (**Ready**)

Nothing in the source code: no `Persistable`, no `PersistenceManager`, no `AppMain::OnPause` / `OnResume`, no `APP_CMD_SAVE_STATE` wiring. Design in `PersistencePlan.md`.

- C1 — `AppMain::OnPause` / `OnResume`, routed from `APP_CMD_PAUSE` / `APP_CMD_RESUME`.
- C2 — `Persistable` interface, `PersistenceManager` singleton, two backends: Android `savedState` blob, on-disk `internalDataPath` / desktop working directory.
- C3 — `StateMachine` as first consumer, snapshotting the active state name.
- C4 — foundation for settings, save games, and later cloud sync.

Wave B's context-loss policy is a cold restart on the premise that persistence recovers the state. Until Wave C lands, that recovery does not exist.

### Android gaps (**Ready**)

- `PlatformFileSystemAndroid::CopyFile` and `ListDirectoryEntries` return `false` (`PlatformFileSystemAndroid.cpp:158-172`). Deliberate — the only consumer is desktop-only AudioTracker — but they block any Android feature needing directory enumeration.
- `PlatformFileSystemAndroid::WriteFileTextAtomic` forwards to `WriteFileText` and is not atomic. Needs scoped-storage work before anything on Android relies on the guarantee.

### CMake unification (**Ready**)

`Code/Targets/Desktop/Code.vcxproj` is still the Windows source of truth. Only the Android `app/src/main/cpp/CMakeLists.txt` exists.

- Root `Code/CMakeLists.txt` describing `chillcore_engine`.
- `CMakePresets.json` with desktop-debug / desktop-release / android-arm64-debug / android-arm64-release.
- Per-target `CMakeLists.txt` under `Targets/Desktop/` and `Targets/Android/`.
- `source_group(TREE …)` replaces the hand-maintained `Code.vcxproj.filters`.
- `.gitignore` gains `Build-CMake/`, `CMakeCache.txt`, `CMakeFiles/`, `cmake_install.cmake`, `CMakeUserPresets.json`.
- Windows binary parity test, then retire the vcxproj and sln.
- Gates the WebGL backend, further backends, and the scripted build system.

### WebGL backend (**Deferred**)

- `Gfx::RenderApiWebGl` under emscripten. Capability gating for compute, storage buffers, indirect draw.
- Platform backends over the emscripten runtime.

### Scripted build system (**Deferred**)

- `scripts/build.py` plus per-target wrappers. Drives CMake presets and Gradle, asset preprocessing, signing, artifact collection into `Artifacts/<platform>/<config>/`.

### Shader cross-compilation (**Deferred**)

- AGD-0080 defers cross-compilation until a second shader target ships. GLES currently uses `ShaderManager` preamble injection instead.
- Revisit when WebGL or Vulkan lands: glslang to SPIR-V, SPIRV-Cross to targets.

---

## Open design questions

Unresolved design choices, not scheduled work. Each is a decision nobody has needed to make yet; they are recorded so the question is not rediscovered from scratch. Resolving one means writing the decision into the relevant AGD, not adding an entry here.

### Material versus material instance (**Open**)

Materials are currently shared objects. There is no distinction between changing a material for every user of it and changing it for one renderable.

- Affects batching: objects sharing a material share a pipeline and sort together, which a per-object override would break.
- Affects authoring: setting a colour on one object currently sets it on every object using that material.
- The usual answer is a material instance holding per-object overrides over a shared base. Nothing has needed it yet.

### RenderManager versus Gfx::RenderApi access levels (**Open**)

`RenderManager` owns the frame and pass brackets, while renderables and materials call `Gfx::RenderApi` directly.

- The split works but is not expressed anywhere: nothing prevents a caller from beginning a render pass or reconfiguring the backbuffer.
- Whether to formalise it — a restricted interface for general callers and a fuller one for `RenderManager` — is undecided.
- Relevant to AGD-0070 and AGD-0080, which describe the current split without endorsing it.

### Resolved

- ~~Engine wrappers for textures and meshes.~~ Delivered by the `CC::Gfx` handle types.
- ~~Forward and deferred rendering paths and lighting.~~
- ~~Transparent rendering path.~~ Recorded in AGD-0090.

---

## Verification debt

Milestones defined in the plans with no recorded run.

- **Gfx Phase 1 acceptance measurements.** Per-frame GL call count before and after on a multi-object scene (target: ~280 uniform/bind calls down to under 40 for ten PBR spheres), frame-time parity via `GetGpuFrameDurationMs()`, a RenderDoc capture showing the labelled scope hierarchy, visual parity across the four reference states, and MSAA still toggling through `ConfigureBackbuffer`. Draw-call reduction is currently an assumption.
- **Android Phase F — v1 ship gate.** `AppStateShowcase` on the reference device for at least one minute, surviving at least one rotation and one task-switch.
- **Android Phase H.** 100+ rotation cycles with no perceptible resume stutter. Forced context loss finishes the activity and relaunches cold without crashing or leaking GL resources.
- **AudioTracker Phase 1 soak.** 10-minute loop, zero sample-frame drift, no seam artefacts, under 2% audio-thread CPU.
- **AudioTracker Phase 5 crash safety.** Kill the process between temp-file write and rename; confirm the previous on-disk project survives intact.

---

## Documentation debt

- **Rendering pipeline getting-started doc.** Requested in `DevBlogChillCore.blog`. Covers the frame and material UBO tiers, the pipeline-first opaque sort, and the render-pass brackets. Superseded by AGD-0070 (Rendering Pipeline); track it there rather than as separate work.
- **AGD-0120 (UI Layout and Styling) inherited a provisional design.** Needs a pass to confirm or revise the layout and styling decisions.
- **`mkdocs.yml` has no `nav`.** Deliberate — navigation is derived from the folder structure. Recorded so it is not "fixed".

---

## Ordering

Implementation order, with the dependencies behind it, is in `02_Roadmap.md`. This document is the inventory; the roadmap is the sequence.
