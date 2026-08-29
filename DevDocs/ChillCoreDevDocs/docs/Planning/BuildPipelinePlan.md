# Build Pipeline Plan

**Status:** Phases 0 to 3 are implemented and building. Debug and Release, clean and incremental, labelled and unlabelled, and the failure path all produce the expected artifacts and exit codes. What remains is behavioural parity — running the produced binary — then Android, then retiring the Visual Studio project.
**Current state:** AGD-0040 (Build and Targets) describes the two hand-driven build systems. This plan covers the command-line pipeline that replaces driving them by hand, and the CMake unification it is built on.
**Index:** the Platform and build section of `01_TechBacklog.md`.

---

## Objective

A command-line build system with two entry paths into one implementation.

- **Double-click from Windows Explorer.** Prompts for build parameters, holds the window open at the end.
- **Invoked with arguments.** Runs unattended, for Claude and for a build runner such as Jenkins.

The first cut compiles the desktop target and reports errors in a form a machine can consume. Everything else — tests, asset bundling, documentation, installers — attaches to the same script later.

### In scope for the first cut

- CMake unification of the desktop build.
- Interactive and non-interactive command-line build of the desktop target.
- A log, a machine-readable diagnostic report, and a human-readable report, in one folder per build.
- A build identifier compiled into the binary and displayed in both the developer interface and the application interface.

### Out of scope, with a place reserved

- Automated testing. A separate `RunTests.sh` alongside `Build.sh`.
- Asset preprocessing and bundling.
- Documentation packaging and installer generation.
- The Android target. Its presets are written in Phase 0; no flag selects it until later.

---

## CMake unification

The pipeline drives CMake presets. It contains no compiler invocation of its own.

**Why it is here rather than in `PlatformAndBuildPlan.md`.** The presets are the script-facing contract, so unification is the pipeline's first phase rather than a prerequisite tracked elsewhere. Nothing else in the pipeline can be designed until it is known what the script calls.

**Why presets rather than the hand-maintained project.** Driving `msbuild` against `Code.vcxproj` would work today and be thrown away at unification, and it would leave the desktop and Android targets described in two places for longer. Building the pipeline on the intended end state costs one phase and no rework.

Shape, unchanged from `PlatformAndBuildPlan.md`:

- Root `Code/CMakeLists.txt` describing the engine as a library, knowing nothing about entry points or platform shells.
- `CMakePresets.json` carrying one `Desktop` configure preset with `DesktopDebug` and `DesktopRelease` build presets. The Visual Studio generator is multi-config, so the configuration is chosen at build time rather than by a second configure preset. Android has no preset: Gradle owns its configuration and passes the same cache variables directly.
- Per-target `CMakeLists.txt` under `Targets/Desktop/` and `Targets/Android/`, each adding only its entry point and its platform links.
- `source_group(TREE …)` replaces the hand-maintained filter file.

**The engine is an object library, not a static one.** Components self-register through file-scope static objects — a `ComponentRegistrar` in `RenderableQuad.cpp` and four siblings. Nothing else references symbols in those translation units, so a static archive lets the linker discard the object and the registration never runs. The failure is quiet and misleading: the scene loads, the component is reported as an unknown type, and the object is simply absent from the world. An object library has no archive to select from, so every translation unit reaches the link — which is what the hand-maintained project did by compiling everything into the executable directly.

This is the one place where unification could silently change behaviour rather than fail loudly, and it is worth remembering before any future change to how the engine is packaged.

**Generator: Visual Studio 2026.** The toolset then matches what interactive builds use today, so parity testing compares like with like, and a solution still exists for the interactive workflow below. Ninja is a later option for build speed; it changes nothing above the preset name.

**CMake is resolved through Visual Studio, not PATH.** A CMake on PATH is routinely older than the installed Visual Studio and does not know its generator — the machine this was written on had 4.0.2 on PATH, which knows nothing past Visual Studio 2022, alongside 4.3.1 inside the Visual Studio installation. The pipeline locates the installation with `vswhere`, prefers its bundled CMake, falls back to PATH, and rejects any CMake that does not advertise the required generator.

**Done when** the binary produced through CMake behaves identically to the one the retired project produced. Both build trees run side by side during verification; the old project is retired only after parity is confirmed.

### The interactive workflow is untouched

- F5 builds and debugs through the generated solution at `Build/CMake/Desktop/ChillCore.slnx`, regenerated by every configure. The Visual Studio 2026 generator emits the XML solution format rather than a `.sln`. It does not call the pipeline.
- Opening `Code/` as a folder instead works through the presets, but the debugger's working directory is a generated-project property and does not carry across, so assets fail to load. The generated solution is the supported route.
- No pre-build event injects a build identifier. An interactive build reports `Unlabelled`, which is honest — it is not a build the pipeline produced and cannot be reproduced from a record.

---

## Entry point

`Tools/Build/Build.sh` is both the double-click target and the command line. Windows associates `.sh` with Git Bash, so no wrapper script is needed.

- **Toolchain validation runs first.** Python 3, CMake, and the Visual Studio 2026 build tools are checked before anything else, and a missing one produces a named message rather than a compiler error.
- **The shell script is the interface.** Prompting, argument parsing, and validation live in `Build.sh`. The work lives in `Build.py`.

### Options

Three, each a pair with a default.

| Option | Default | Prompt |
| --- | --- | --- |
| `--build-id` / `--no-id` | `--no-id` | Label this build? |
| `--debug` / `--release` | `--debug` | Configuration |
| `--incremental` / `--clean` | `--incremental` | Clean rebuild? |

- **No arguments** prompts for all three. Enter accepts the default.
- **Any argument** suppresses every prompt. Unspecified options take their defaults.

Deliberately absent: platform selection, target selection, and run-after-build. One platform ships, one target is supported, and launching the application is not the build system's business.

### Holding the window open

The pause is for the double-click case, where the window would otherwise close before the result could be read.

- Pause when the script prompted — no arguments, and standard input is a terminal.
- Do not pause otherwise. An unattended runner must never block.
- `--pause` and `--no-pause` override the inference, for the case of a flagged build run by hand.

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Compile or link errors |
| 2 | Invalid usage |
| 3 | Missing toolchain |
| 4 | Internal script failure |

Compile failure is separated from script failure so a runner can distinguish a broken build from a broken pipeline.

---

## Output

One folder per build, holding the same three files every time.

```
Build/Logs/<buildId>/      when --build-id
Build/Logs/Unlabelled/     when --no-id, overwritten every run
    Build.log              raw CMake and compiler output, unfiltered
    Diagnostics.json       parsed errors and warnings, timing, resolved configuration
    Report.md              verdict, counts, leading errors, duration, build identifier
```

- **The default lands at a fixed path.** `Build/Logs/Unlabelled/Diagnostics.json` is where an unlabelled build's diagnostics always are, so no consumer has to discover an identifier first. Labelled builds accumulate; unlabelled builds do not.
- **`Build.log` is never filtered.** Parsing can be wrong; the raw record is what settles it.
- **`Diagnostics.json` carries one entry per diagnostic** — file, line, column, code, severity, message — plus the resolved configuration and wall-clock timings. This is the file Claude and a build runner read.
- **`Report.md` is for a person.** Verdict first, then counts, then the leading errors in full.

`Build/Logs/` is gitignored, alongside the CMake artifacts unification adds.

---

## Running and log capture

The pipeline builds; `Tools/Build/Run.sh` runs what it built and collects the log. Same entry-point shape as `Build.sh` — double-clickable, argument-driven, pause only when prompted.

Three things make an unattended run possible at all, and each was found the hard way:

- **The application must start in `Code/App`.** Asset paths are `Data/...`-relative to it. Started anywhere else, shader loading fails and pipeline creation asserts — a failure that looks nothing like a working-directory problem.
- **Standard output is unbuffered.** `printf` is block-buffered whenever stdout is not a console, so a fault discarded the entire log that would have explained it. Set in `PrintManager`'s constructor. The Windows CRT treats line buffering as full buffering, so unbuffered is the only mode that survives.
- **The run happens under the Windows debugger when one is present.** `cdb` from the Windows Kits turns a bare exit code into a symbolised stack. Without it the script still runs the application, and a fault is only an exit code. The debugger has no run-for-n-seconds command, so a clean run ends at the script's own timeout — timing out is success here.

Output goes to `Build/Logs/Run.log`, echoed as it is written.

---

## Build identifier

Format `2026_08_29_4_g1a2b3c4`, with `_dirty` appended when the working tree is unclean.

- Date, then a counter that resets daily, then the short commit hash.
- Counter state in `Build/BuildCounter.json`, gitignored.
- Generated only under `--build-id`. An unlabelled build compiles the literal `Unlabelled`.

**Why a counter and a hash.** The hash identifies the source; the date and counter identify the build, because the same source built twice is two artifacts. The dirty marker states plainly that the hash does not fully describe what was compiled.

### Injection

Two headers, so that an interactive build always compiles and the generated values never appear in a diff.

- `Code/Core/BuildInfo.h` — checked in. Guards on `__has_include` for the generated header and falls back to placeholder values.
- `Code/Core/BuildInfoGenerated.h` — written by `Build.py` before configuring, gitignored.

`CC::BuildInfo` exposes the identifier, the commit hash, the configuration, and the build timestamp.

---

## Displaying the build

The identifier exists to answer "what is this build?" from inside a running application, so both interfaces show it.

- **Developer interface.** A Help → About panel in `DevUi`, following the existing view pattern.
- **Application interface.** An About panel in the application's own interface, reachable without a debug build.

Both read `CC::BuildInfo`. Neither formats the identifier itself.

---

## Phases

Phases 0 to 3 are written and build cleanly. What is outstanding is running the result, Android, then retirement of the old project.

### Phase 0 — CMake unification (building)

`Code/CMakeLists.txt` describes `ChillCoreEngine`; `Code/Targets/Desktop/CMakeLists.txt` and the Android target file add only an entry point and platform links; `Code/CMakePresets.json` carries the desktop presets; the Solution Explorer tree is generated from the on-disk hierarchy.

**Outstanding:**

- Behavioural parity: the produced binary runs and behaves as the retired project's did. Compilation, linkage, and output location are confirmed; nothing has run the executable.
- The Android package parity test, through Gradle.
- Retire `Code.vcxproj`, `Code.vcxproj.filters`, and `ChillCore.sln` once both pass. Until then every new source file still has to be added to the project by hand.

### Phase 1 — Build and log (verified)

`Tools/Build/Build.sh` prompts and parses; `Tools/Build/Build.py` resolves the toolchain, drives `cmake --preset` and `cmake --build`, and captures the full output.

### Phase 2 — Reports (verified)

Diagnostics parsed from the captured output into `Diagnostics.json`, and `Report.md` generated from it. The parser recognises located compiler diagnostics, unlocated linker and command-line diagnostics, and CMake's own errors, and deduplicates the repeats MSBuild emits.

### Phase 3 — Build identifier and display (building, undisplayed)

Identifier generation and daily counter, `BuildInfo.h` with its generated half, `CC::BuildInfo`, a Help → About menu and view in `DevUi`, and an About screen in the application interface reached from the main menu.

### Phase 3.5 — Running and log capture (verified)

`Tools/Build/Run.sh`, unbuffered application output, and debugger-backed crash capture. Not in the original plan; added because verifying anything above it requires being able to run the result and read what it said.

### Phase 4 — Reserved

Named here so the shape is known, not scheduled:

- `RunTests.sh` alongside `Build.sh` and `Run.sh`, reporting into the same per-build folder.
- Asset preprocessing before the build.
- Documentation and runtime data bundling into an installer.
- An Android target option.

**Done when:** a double-click produces a successful desktop build with a readable report and a window that waits to be dismissed, the same script run with arguments produces machine-readable diagnostics and a meaningful exit code, a labelled build displays its identifier in both About panels, and the Visual Studio project has been retired.
