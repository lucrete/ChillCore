# 03 — Tech Backlog

Work identified but not yet in plan. Summaries only. When an item is picked up it moves to `02_Roadmap.md` and gets a section in its workstream planning doc — see `01_DevProcess.md`. Per `Guidelines/CodingGuidelines.md`, future work is captured here rather than in `// Todo` comments.

Last reviewed against the source: 2026-08-29.

---

## Rendering

### Render target follow-ups

Gaps left open by the offscreen render-target work (AGD-0080 Limitations). None blocking:

- Array-layer and cubemap-face attachments. Needed before a cascaded shadow map or a cubemap reflection probe.
- The backbuffer as a pool handle rather than an invalid one, which would remove the special case in the pass brackets.

### Post-processing follow-ups

Gaps left open by the post-process stack (AGD-0070 Limitations). None blocking:

- A linear rendering workflow. Lit shaders write display-referred values with no output transform, so tone mapping operates on values that are not scene-linear and bloom has nothing above white to find. This is what would make the tone map a correction rather than a look, and it is the largest of these.
- Baking grade and tone map into a 3D LUT, as both commercial engines do. Needs render-to-3D-texture or the 2D strip fallback mobile paths use; the array-layer attachment gap above blocks the first.
- A caller-defined post-process chain. Effects fuse into one pass in a fixed order; a caller cannot reorder them or insert its own.
- Colour grading works on scalar parameters. Per-channel lift, gamma and gain are what a real grade needs, and would want a colour picker in the panel rather than three sliders each.

### Camera registration by name

`CameraManager` keys its map by `std::string` and asserts rather than storing a null. `GetViewProjectionMatrix` and `GetCameraPosition` dereference `activeCamera` unguarded. No state registers a second camera beyond the manager's own default, so the F9 free-camera toggle is a no-op outside it. No longer crashes; not urgent.

---

## AudioTracker

### Editor first-cuts to replace

- Track source assignment is a cycle button; the intended dropdown is blocked on `UiDropdown` supporting dynamically populated options.
- Neither grid shows a playhead, though the transport publishes its position for exactly this.
- The non-Windows storage path is a placeholder, pending a second desktop target.

### Editor polish

Follow-ups, none blocking anything: per-track gain and solo, variable step subdivision, pattern-length editing after creation, sample waveform display, keyboard shortcuts, mid-song tempo and time-signature automation, offline render to an audio file.

---

## Platform and build

### Android platform gaps

`CopyFile` / `ListDirectoryEntries` return failure and `WriteFileTextAtomic` is not atomic on Android. Deliberate — no Android consumer today — but blocks any Android feature needing directory enumeration or the atomicity guarantee. Detail in `PlatformAndBuildPlan.md`.

### Android through the pipeline

The pipeline builds the desktop target only; Android is still built by invoking Gradle directly. Adding it is a target-selection option and a Gradle invocation reporting into the same folder structure. Unblocked, low value until something other than a developer's machine builds the package.

### WebGL backend

A third graphics backend under emscripten, plus platform backends for window, file access, and input over the web runtime. Capability-gates compute, storage buffers, and indirect draw off — dependent states need a fallback or must report unsupported. Not started.

### Further graphics backends

Vulkan, then Direct3D, then Metal if Apple platforms become a real target. Largely mechanical — the abstraction was designed for them, and they inherit a shim-free `Gfx::RenderApi`. Order open; Vulkan first is likely, being cross-platform and applicable to Android.

### Shader cross-compilation

Author in one language, compile to an intermediate representation, translate to each target (glslang → SPIR-V → SPIRV-Cross). Deferred deliberately: AGD-0080 holds this until a second shader target ships. Android currently gets its dialect differences through `ShaderManager` preamble injection. Revisit when WebGL or Vulkan lands.

---

## Open design questions

Decisions nobody has needed to make yet, recorded so they are not rediscovered from scratch. Resolving one means writing the decision into the relevant AGD.

### Material versus material instance

Materials are shared objects with no per-renderable override. Setting a colour on one object sets it on every object using that material, and a per-object override would break material-based batching and sorting. The usual answer is a material instance holding per-object overrides over a shared base. Nothing has needed it yet. Adjacent to the batching and sort code — worth resolving whenever that is next open.

---

## Documentation debt

- AGD-0120 (UI Layout and Styling) inherited a provisional design; needs a pass to confirm or revise it.
- `mkdocs.yml` has no `nav`. Deliberate — navigation is derived from the folder structure. Recorded so it is not "fixed".
