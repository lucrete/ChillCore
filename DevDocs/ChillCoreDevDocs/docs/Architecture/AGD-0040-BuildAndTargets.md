# AGD-0040: Build and Targets

- **Scope:** How the engine is built for each platform, how the source is organised into shared and per-target parts, and how the graphics backend is selected. Covers both build systems and the constraints they impose. Does not cover platform runtime behaviour.

## Overview

- Engine and application source is shared across every target. Only the entry point and the build description differ.
- Each target owns a directory containing its entry point and its build files. This layout is the design, not an accident of history.
- One CMake description covers every target. Desktop configures it directly through a preset; Android reaches the same description through Gradle, which drives CMake, which drives the native toolchain.
- The graphics backend is selected by a preprocessor definition set by the build, not by swapping source directories.
- A command-line pipeline drives the build, produces a per-build log and report, and compiles a build identifier into the binary.

## Concepts

- **Target** — one platform's buildable product, defined by an entry point and a build description.
- **Shared source** — engine and application code compiled unchanged into every target.
- **Backend definition** — a preprocessor symbol set by the build that selects which graphics implementation compiles in.

## Architecture

The source divides into three parts. Engine code and application code are shared by every target and know nothing about how they are built. Third-party dependencies are vendored and are not modified. Per-target code is a single entry-point file, plus that target's build description.

The per-target split is the part worth understanding:

```
Code/Targets/<Platform>/
```

Everything platform-specific about *building* lives under a target directory; everything platform-specific about *running* lives in the platform layer. A target directory holds an entry point and build files, and nothing else. This is the one place where a directory path is part of the design rather than an implementation detail, which is why it appears here.

**Desktop** configures the shared description through a checked-in preset and builds with the Visual Studio generator, which produces a solution for interactive work. Source files are discovered from the source tree, so adding a file to disk is enough.

**Android** builds through three layers. Gradle configures the application and packaging, invokes CMake for the native build, and CMake drives the toolchain. Native sources are gathered by recursive glob with dependency tracking, so new files are picked up without editing the build description. The result is a shared library loaded by the platform's activity host, linked against the graphics and activity libraries the platform provides.

Assets are not copied or duplicated. The Android build packages the application's existing data directory directly into the archive, so both targets read from one asset tree.

## Design decisions

### One source tree, per-target entry points

The alternative — separate source trees per platform — was never seriously considered, because it guarantees drift and defeats the point of having one engine.

What makes a single tree workable is that the platform-specific runtime behaviour is already abstracted behind the platform layer and the graphics abstraction. With those in place, the only thing a target genuinely needs of its own is the entry point, because process startup differs fundamentally between a console application and a platform-managed activity.

The entry point is a shell, not engine code. It constructs the engine and the application, drives frames, and shuts down. Anything larger appearing in a target directory is a sign that something belongs in the platform layer instead.

### The backend is chosen by definition, not by directory

Both graphics backends live together under the graphics abstraction and share a common helper layer. The build sets a definition that selects which one compiles in.

Selecting by directory — compiling a different source folder per platform — was rejected because the two backends share a substantial amount of code, and separating them by directory would have forced that shared code to be duplicated or awkwardly hoisted. Keeping them adjacent makes the shared layer natural.

The cost is that both backends are visible in every build's source tree, and a change to the shared layer must be considered against both.

### Android is built by Gradle driving CMake

The platform's tooling expects Gradle, and fighting that would mean giving up standard packaging, signing, and deployment. CMake sits underneath because it is what the native toolchain expects, and because it is the build system the desktop target is expected to move to.

Native sources are globbed rather than listed. Explicit lists are more reproducible in principle, but with a shared source tree they become a second place to remember, and the failure mode — a file silently absent from one target — is worse than the reproducibility risk.

### Assets ship from one tree, uncopied

The Android build points its asset packaging at the application's existing data directory rather than copying files into a platform-specific location.

Copying would create a second copy to keep synchronised and a build step that can silently go stale. Pointing at the original means there is exactly one asset tree, and a change to an asset is visible to both targets immediately.

The consequence is that asset paths must be interpreted consistently across platforms, which is handled at the platform file-system boundary rather than by the callers.

### One description, entered from two places

A single description defines the engine; each target adds only its entry point and its platform links. Desktop enters through a preset, Android through Gradle, and both compile the same discovered source closure.

The alternative — a hand-maintained project per platform — was what existed, and its failure mode was drift: a new file added to one build and silently missed by the other. Discovery removes the second place to remember.

The cost is that the build description changes only when a new file appears, so it is rarely exercised, and configuration must be re-run when the file set changes. The build tooling handles that; a developer adding a file does not have to think about it.

### The engine is an object library

Components register themselves through file-scope static objects. Nothing else references symbols in those translation units, so packaging the engine as a static archive lets the linker discard those objects and the registration never runs — the scene loads, the component is reported as an unknown type, and it is simply absent.

An object library has no archive to select from, so every translation unit reaches the link. This is a constraint on how the engine may be packaged, not an implementation detail: any future change here must preserve it.

### Dependencies are vendored and statically linked

Third-party libraries live in the repository rather than being fetched by a package manager, and desktop links them statically.

Vendoring makes a checkout buildable without network access or toolchain-specific package configuration. Static linking removes runtime distribution concerns.

The cost is strict: every dependency must be built with matching runtime settings, and mismatches surface as link errors rather than clear diagnostics. Adding a dependency means confirming that before anything else.

## The build pipeline

A command-line entry point serves two callers from one implementation: it prompts for parameters when launched interactively, and runs unattended when given arguments, which is how automation and continuous integration invoke it.

- Three parameters: whether to label the build, the configuration, and whether to rebuild from scratch.
- Each build writes a folder holding the raw log, a machine-readable diagnostic list, and a report for a person. Unlabelled builds overwrite one fixed location so a consumer never has to discover an identifier.
- Exit codes distinguish a failed compilation from a broken pipeline or a missing toolchain.
- A companion script launches what was built, from the directory its asset paths are relative to, and captures the log — under a debugger when one is available, so a fault yields a stack rather than an exit code.

### Build identity

Builds produced by the pipeline can carry an identifier composed of the date, a counter for that day, and the source revision, marked when the working tree was unclean. It is compiled in through a generated header that the checked-in header falls back for, so a build made outside the pipeline still compiles and reports itself as unlabelled — which is honest, because such a build cannot be reproduced from a record.

The identifier is displayed in both the developer overlay and the application's own interface.

## Limitations

- Android builds one processor architecture only. There is no support for older or alternative architectures.
- Asset loading depends on the working directory being the application directory; assets are addressed relative to it rather than to the executable, so a built executable does not run in place.
- No offline shader processing step exists. Shader variants are produced at load time.
- The pipeline builds the desktop target only. Android is still built through Gradle directly.
- Toolchain resolution prefers the CMake bundled with the installed Visual Studio, because a CMake on PATH is routinely older than the installed toolset and does not know its generator.
