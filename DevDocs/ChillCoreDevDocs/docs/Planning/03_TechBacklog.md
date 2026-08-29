# 03 — Tech Backlog

Work identified but not yet in plan. Summaries only. When an item is picked up it moves to `02_Roadmap.md` and gets a section in its workstream planning doc — see `01_DevProcess.md`. Per `Guidelines/CodingGuidelines.md`, future work is captured here rather than in `// Todo` comments.

Last reviewed against the source: 2026-08-29.

---

## Rendering

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

### RenderManager versus Gfx::RenderApi access levels

`RenderManager` owns the frame and pass brackets; renderables and materials call `Gfx::RenderApi` directly. Nothing prevents a general caller from beginning a render pass or reconfiguring the backbuffer. Whether to formalise the split — a restricted interface for general callers, a fuller one for `RenderManager` — is undecided. Relevant to AGD-0070 and AGD-0080. The render-target frontend work (roadmap 1) is the first caller that forces the question.

---

## Documentation debt

- AGD-0120 (UI Layout and Styling) inherited a provisional design; needs a pass to confirm or revise it.
- `mkdocs.yml` has no `nav`. Deliberate — navigation is derived from the folder structure. Recorded so it is not "fixed".
