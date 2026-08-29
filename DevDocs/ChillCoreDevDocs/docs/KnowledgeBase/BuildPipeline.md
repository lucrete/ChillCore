## Build Pipeline

How to build, run, and read the results.

### The three scripts

All live in `Tools/Build/` and share one shape: double-click to be prompted, pass arguments to run unattended. The window is held open only when it prompted, so a build runner is never blocked.

| Script | Purpose |
| --- | --- |
| `Build.sh` | Configures and builds the desktop target. |
| `Run.sh` | Launches what was built and captures its log. |
| `Build.py` | The build implementation. Not invoked directly. |

#### Building

```
bash Tools/Build/Build.sh                    # prompts for all three parameters
bash Tools/Build/Build.sh --release --clean  # unattended, nothing prompted
```

Three parameters, each defaulting to the first option:

* `--no-id` / `--build-id` — whether to stamp an identifier into the binary
* `--debug` / `--release`
* `--incremental` / `--clean`

Any argument suppresses every prompt; unspecified parameters take their defaults. There is deliberately no platform, target, or run-after-build option.

Exit codes: `0` success, `1` compile or link errors, `2` invalid usage, `3` missing toolchain, `4` internal script failure. Compilation failure is separate from pipeline failure so automation can tell a broken build from a broken build system.

#### Running

```
bash Tools/Build/Run.sh --seconds 10
```

Starts the application from `Code/App`, because assets are addressed relative to it. Under a debugger when one is installed, so a fault produces a symbolised stack rather than an exit code. The log lands in `Build/Logs/Run.log`.

The debugger has no run-for-n-seconds command, so a clean run is ended by the script's own timeout. **Timing out is success here**, not failure.

### Reading the output

Each build writes one folder:

```
Build/Logs/<buildId>/      when --build-id
Build/Logs/Unlabelled/     when --no-id, overwritten every run
    Build.log              raw, unfiltered
    Diagnostics.json       parsed diagnostics, timings, resolved configuration
    Report.md              verdict, counts, leading errors
```

`Diagnostics.json` is the machine-facing artifact — one entry per diagnostic with file, line, column, code, severity, and message, deduplicated, with repository-relative paths. Because unlabelled is the default, `Build/Logs/Unlabelled/Diagnostics.json` is a fixed path a consumer can read without discovering an identifier first.

`Build.log` holds the complete output of the configure and build, with nothing removed — only a `> <command>` line inserted before each invocation.

The distinction between the two matters. `Diagnostics.json` is *derived*: it comes from running patterns over the compiler's output, which is approximate. A diagnostic in an unanticipated form is missed; an ordinary line that happens to contain something like `error C1234` is picked up as one that is not. So when the report claims no errors and the build clearly failed, or names something that makes no sense, `Build.log` is the authority and the parse is what is wrong.

Read `Diagnostics.json` first because it is quick. Go to `Build.log` the moment it looks suspicious, and never trim the log to save space — it is the only complete record.

### Build identity

`--build-id` produces an identifier like `2026_08_29_4_ga937bd8`, with `_dirty` appended when the working tree is unclean, and compiles it into the binary. It is shown in the developer overlay under **Extras → Views → About**, and in the application's own About screen.

Without `--build-id` the binary reports `Unlabelled`, and so does any build made from the IDE. That is deliberate: such a build cannot be reproduced from a record, and claiming an identifier for it would be a lie.

The counter resets daily and its state lives in `Build/BuildCounter.json`.

### Opening in Visual Studio

There is no checked-in solution. Configuring generates one:

```
Build/CMake/Desktop/ChillCore.slnx
```

Note the extension — the Visual Studio 2026 generator emits the XML solution format. Run `Build.sh` once, then open that. The startup project and the debugger's working directory are already set, and the Solution Explorer tree is derived from the folders on disk rather than hand-maintained, so adding a file to disk is all that is required.

Opening `Code/` as a folder also works through the presets, but the debugger's working directory does not carry across and assets fail to load. Use the generated solution.

### Things that will bite you

**The CMake on PATH is probably too old.** It is routinely older than the installed Visual Studio and does not know its generator. The pipeline locates the Visual Studio installation and prefers the CMake bundled with it, falling back to PATH and rejecting any CMake that does not advertise the required generator. If you invoke CMake by hand, you may not get the same one.

**The engine is an object library, and must stay one.** Components register themselves through file-scope static objects. A static archive lets the linker discard those objects because nothing references them, and the registration silently never runs — the scene loads and the component is reported as an unknown type. This has already happened once.

**Application output is unbuffered on purpose.** `printf` block-buffers whenever stdout is not a console, which discards the entire log when the application faults — exactly when the log matters. The Windows CRT treats line buffering as full buffering, so unbuffered is the only mode that survives. Set once in `PrintManager`'s constructor; do not "optimise" it away.

**The virtual environment.** `Build.sh` creates `Tools/Build/.venv` on first run and uses it thereafter. It costs a few seconds once and nothing measurable per build. The build implementation imports only the standard library today; the environment is there so that adding a dependency later does not mean touching the machine's global interpreter.
