# AGD-0040: Build and Targets

- **Scope:** How the engine is built for each platform, how the source is organised into shared and per-target parts, and how the graphics backend is selected. Covers both build systems and the constraints they impose. Does not cover platform runtime behaviour.

## Overview

- Engine and application source is shared across every target. Only the entry point and the build description differ.
- Each target owns a directory containing its entry point and its build files. This layout is the design, not an accident of history.
- Desktop builds from a hand-maintained Visual Studio project. Android builds through Gradle, which drives CMake, which drives the native toolchain.
- The graphics backend is selected by a preprocessor definition set by the build, not by swapping source directories.
- Two build systems currently describe the same source. That duplication is known and temporary.

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

**Desktop** builds from a Visual Studio solution and project. Source files are enumerated explicitly, so a new file must be added to the project as well as to disk.

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

### Two build descriptions, deliberately temporary

The desktop project and the Android CMake description currently describe the same source in two places. Every new source file must be added to the desktop project by hand; the Android build discovers it automatically.

This is accepted only because the intended end state is a single CMake description generating both. Until then, the desktop project remains authoritative for desktop, and the divergence is a known maintenance cost rather than a design position.

### Dependencies are vendored and statically linked

Third-party libraries live in the repository rather than being fetched by a package manager, and desktop links them statically.

Vendoring makes a checkout buildable without network access or toolchain-specific package configuration. Static linking removes runtime distribution concerns.

The cost is strict: every dependency must be built with matching runtime settings, and mismatches surface as link errors rather than clear diagnostics. Adding a dependency means confirming that before anything else.

## Limitations

- Two build systems describe the same source. Desktop requires manual project maintenance for every new file.
- Android builds one processor architecture only. There is no support for older or alternative architectures.
- Desktop asset loading depends on the debugger's working directory being set correctly; assets are addressed relative to the application directory rather than to the executable.
- There is no scripted or continuous-integration build path. Both targets are built interactively.
- No offline shader processing step exists. Shader variants are produced at load time.
