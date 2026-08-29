# Platform and Build Plan

**Status:** Windows and Android both ship. What remains is one verification pass, unifying the two build systems, and the further targets that unification unlocks.
**Current state:** AGD-0040 (Build and Targets) and AGD-0160 (Android Support) describe what exists. This plan covers only what does not.
**Index:** the Platform and build section of `01_TechBacklog.md`.

---

## Android v1 ship gate — unverified

The Android port defined a narrow ship criterion that has never been recorded as run: the showcase state on the reference device for at least one minute, surviving at least one rotation and one task-switch, with no crash and no visible corruption.

**Why it matters.** Everything downstream assumes Android v1 works. The port is believed good, but believed is not verified, and this is the cheapest item in the plan.

A second, larger gate also stands unrun: a hundred or more rotation cycles with no perceptible resume stutter, and a forced graphics-context loss that finishes the activity and relaunches cold without crashing or leaking graphics resources.

---

## Android platform gaps

Three file-system operations are unimplemented on Android.

- Copying a file and listing a directory both return failure. This is deliberate — their only consumer is desktop-only — but it blocks any Android feature needing to enumerate files.
- The atomic text write forwards to an ordinary write and is not atomic. Anything on Android that comes to depend on the atomicity guarantee needs this addressed first, which requires scoped-storage work.

---

## CMake unification

**Moved.** Design, shape, and phasing live in `BuildPipelinePlan.md`, as Phase 0 of the build pipeline. The presets are the script-facing contract, so unification belongs to the plan that consumes them.

Retained here because it is a platform concern as much as a build one:

- **Why it matters.** Every new source file must currently be added to the desktop project by hand, while the Android build discovers it automatically — so the two can silently diverge. Unification also gates the further targets below.
- **Why after Android rather than before.** Doing it first would have meant refactoring the build twice: once to reach parity for Windows alone, again to make it genuinely cross-platform. Doing it now means the cross-platform requirement is concrete, and verification is cheap because the Android build is known-good and the Windows side reduces to a focused parity test.
- **A constraint to honour from the start.** Every per-platform configuration must be expressible as a checked-in preset. No platform may require an interactive step to produce a release artifact.
- **Android's half of the parity test.** The package produced through Gradle after unification must match the current one.

---

## Persistence and lifecycle hooks

Not started, and blocking the Android graphics-context-loss story. Design in `PersistencePlan.md`.

The context-loss policy is a deliberate cold restart, on the premise that persistence restores the user to where they were. Until persistence exists, that recovery does not, and a context loss returns the user to the boot screen.

---

## WebGL backend

**Not started.** Gated on CMake unification, which is what makes the web toolchain's build integration tractable.

- A third graphics backend, alongside the two current ones.
- Capability gating for what the platform lacks: compute, storage buffers, indirect draw. Any state depending on those needs a fallback path or must report itself unsupported.
- Platform backends for the window, file access, and input over the web runtime.

**Done when:** a browser build renders the showcase scene, and compute-dependent states report unsupported through the capability query rather than failing.

---

## Further graphics backends

**Not started.** Vulkan, then Direct3D, then Metal if Apple platforms become a real target.

Largely mechanical, because the abstraction was designed for them: handles map directly onto native objects, pipeline descriptors translate closely, and command recording is already explicit. This is where the abstraction's cost is finally repaid.

Order is open. Vulkan first is the likely choice, being cross-platform and applicable to Android as well.

---

## Scripted build system

**Moved.** Design lives in `BuildPipelinePlan.md`.

A command-line build system with two entry paths into one implementation: interactive when double-clicked from Explorer, unattended when given arguments. It drives the CMake presets, produces a log and a machine-readable diagnostic report per build, and compiles a build identifier into the binary. Asset preprocessing, signing, testing, and installer generation attach to the same script later.

---

## Shader cross-compilation

**Deferred, deliberately.** Shader source is not portable, and the eventual answer is to author in one language, compile to an intermediate representation, and translate to each target.

Android currently gets its dialect differences through preamble injection at compile time instead, which is sufficient for two similar targets and avoids a build-time toolchain dependency.

Revisit when a genuinely different shading language arrives — a web or Vulkan backend — rather than building the pipeline speculatively for targets that do not exist.
