# ChillCore Dev Docs

Run `serve-docs.sh` in `DevDocs\ChillCoreDevDocs\` to start the live-reloading docs server, or double-click `serve-docs.bat`. Either creates the Python environment on first run and serves at `http://127.0.0.1:8000`.

ChillCore is a realtime 3D rendering and interaction engine. It provides susbsystems for rendering 3D scenes, processing user input, providing user feedback in the form of UI, audio, and haptics, managing application state, saving global and user settings, and generating installable builds.

# Getting Started
* Clone the repo, then clone submodules:
> git clone repo-url  
> git submodule update --init --recursive

## Desktop
* Build once from the command line, which also generates the solution:
```
bash Tools/Build/Build.sh
```
* Open the generated solution in Visual Studio 2026 Community:
```
ChillCore24\Build\CMake\Desktop\ChillCore.slnx
```
* Build solution: **ctrl-shift-b**
* Run: **ctrl-F5**
* Debug: **F5**

Should open a debug output window, and the app windows opens on a second monitor if one is present, otherwise the main monitor.

## Android
Android builds from `Code\Targets\Android\` via Gradle, which drives CMake and the NDK. The engine and application sources are the same ones the desktop target builds; only the entry point and the build description differ.

### Prerequisites
* Android Studio, or the command-line SDK tools, with the NDK and CMake components installed.
* A JDK. Android Studio supplies one.
* Tell Gradle where the SDK is, by either creating `Code\Targets\Android\local.properties` with a `sdk.dir` entry, or setting the `ANDROID_HOME` environment variable. `local.properties` is machine-specific and not committed.

### Build and install
From `Code\Targets\Android\`:
```
gradlew assembleDebug     Build a debug APK into app\build\outputs\apk\debug\
gradlew installDebug      Build and install onto a connected device
gradlew clean             Remove build outputs
```
Use `gradlew.bat` from a Windows shell and `./gradlew` from a bash shell.

### What the build produces
* One architecture, `arm64-v8a`. Minimum API level 34.
* The GLES graphics backend, and the developer overlay compiled out. Both are set by CMake arguments in the Gradle configuration rather than in the source.
* Assets packaged straight from `Code\App\Data`, with no copy step, so both targets read one asset tree.

### Running and diagnosing
* Launch from the device, or with `adb shell am start`.
* Engine output goes to the system log. `adb logcat` shows it; filtering on the application's tag keeps it readable.
* There is no in-engine developer overlay on Android. Use the platform's own profiling tools.

See [AGD-0040: Build and Targets](Architecture/AGD-0040-BuildAndTargets.md) for why the build is arranged this way, and [AGD-0160: Android Support](Architecture/AGD-0160-AndroidSupport.md) for the runtime behaviour it produces.

## Controls
* F9: to toggle free cam.
* WASD and mouse look to move the free cam.
* F1: unlock the mouse from the free cam.
* F2: toggle full-screen quad.
* F10: toggle full-screen or windowed.
* ESC: exit app.

## Documentation
* **Architecture** — one Architectural Guide Doc per engine system, numbered `AGD-NNNN`. Each is a starting guide to that system, followed by the reasoning behind its current shape. They describe the engine as it is, and are updated when the architecture changes. Start here to understand how something works, or why it works that way.
* **Guidelines** — normative rules for contributors. How to write code, how to write documentation, and the AGD template that governs the Architecture folder.
* **KnowledgeBase** — reference material about external tools and platforms. True regardless of ChillCore, and kept separate so engine documentation does not fill up with third-party setup notes.
* **Planning** — outstanding work only. `01_TechBacklog.md` indexes it, `02_Roadmap.md` gives the implementation order, and a plan per workstream carries the detail. When work lands it moves into an Architecture doc and leaves the plan, so a plan never describes what already exists.

## Project Structure
* Build
* Code
	* App: application-specific implementations and assets
	* Core: common engine files, not specific to the application
	* Targets: per-platform entry points and build files (Desktop, Android)
	* Packages: third-party packages
* DevDocs
	* ChillCoreDevDocs: this documentation site (Architecture, Guidelines, KnowledgeBase, Planning)
	* CodeTemplates

## Coding
* Set Find-all file types to cpp

## Project Setup
Nothing to configure by hand. The build description sets the preprocessor defines, the output directories, the debugger's working directory, and the Solution Explorer tree, which is derived from the folders on disk. Adding a source file to disk is enough; re-run the build to pick it up.

See `KnowledgeBase/BuildPipeline.md` for the build and run scripts and how to read their output.

## Procedural Art
* Set the shader being worked in the ProcArt material created in MaterialManager.
* RenderableFullscreenQuad uses that material.
* Set the shader being worked on as the hotloadShader in ShaderManager.

# Architecture
The code base aims to maintain separation between core engine functionality and application-specific features.

[AGD-0001: Getting Started](Architecture/AGD-0001-GettingStarted.md) is the map: what the subsystems are, who owns whom, and which guide to open for what. The numbered guides under `Architecture/` each cover one system in depth.

The shape in one paragraph: a per-platform entry point creates `CoreMain`, which owns every engine subsystem and the frame loop. An `AppMain` is created behind a three-method interface and passed to it; `AppMain` owns a state machine, and one app state is active at a time. `CoreMain` is then ticked until the application quits. Any app state can be the initial one — see [AGD-0020: App States and Transitions](Architecture/AGD-0020-AppStatesAndTransitions.md).
