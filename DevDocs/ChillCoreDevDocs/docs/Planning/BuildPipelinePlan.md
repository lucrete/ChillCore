# Build Pipeline Plan

**Status:** The pipeline builds and runs the desktop target. What remains is packaging — making a built executable runnable in place, then distributable — and automated testing.
**Current state:** AGD-0040 (Build and Targets) describes the pipeline as it exists. This plan covers only what does not.
**Index:** the Build pipeline section of `01_TechBacklog.md`.

---

## Runtime data in the build output

**Not started.** A built executable does not run in place: it looks for `Data/...` relative to the working directory, and the only directory that satisfies that is the application source directory. Anyone who double-clicks the executable, copies it elsewhere, or hands it to someone else gets an immediate asset-load failure.

**Why it is not simply fixed by changing the path.** Resolving assets relative to the executable instead would break the interactive workflow, where the debugger deliberately starts in the application directory so an edited asset is picked up without a build. The answer is to make the output directory a place where both are true.

**Shape.**

- A build step copies the runtime data tree into the output directory beside the executable, after linking.
- The copy is incremental — only files whose source is newer — so an unchanged data tree costs nothing on a rebuild, and a clean build repopulates from scratch.
- Deleted source files are removed from the output. A stale asset that no longer exists in source is worse than a missing one, because it works until it does not.
- The interactive workflow is unchanged. The debugger still starts in the application directory; the copy exists for the executable, not for the developer.

**Open question.** Whether the copy runs as part of the CMake build, so any build including one started from the IDE gets it, or as a step the pipeline performs around the build. Building it into CMake is the more honest answer — a build that produces an unrunnable executable is not finished — but it puts an asset-tree walk into every incremental build, so the incremental behaviour above has to be genuinely cheap.

**Done when:** the executable in the output directory runs correctly with no working directory set, and a rebuild after deleting a source asset removes it from the output.

---

## Packaged distribution

**Not started.** Gated on the data copy above, which is what makes the output directory self-contained.

An archive of the output directory — executable, runtime data, and the build report — named by build identifier, produced as a pipeline step. This is the first form of distribution, and deliberately the simplest: no installer, no signing, no per-platform packaging.

- Only labelled builds are archived. An unlabelled build cannot be reproduced from a record and has no business being handed to anyone.
- The archive is written beside the build's report folder rather than into the output directory, so it never becomes an input to the next build.
- The manifest question — recording what went in, so a bug report can be traced to a build — is worth answering here rather than later.

**Later, and out of scope for this:** an installer, code signing, documentation bundling. Each is a separate decision and none blocks the archive.

**Done when:** a labelled build produces an archive that, extracted anywhere on a machine with no development tooling, runs.

---

## Automated testing

**Not started.** Design belongs with this plan because it shares the pipeline's entry-point shape and its report folder.

- `RunTests.sh` alongside `Build.sh` and `Run.sh`: interactive when double-clicked, argument-driven otherwise, same exit-code contract.
- Results land in the same per-build folder as the build that produced them, in both a machine-readable and a human-readable form, so a build and its test results are one record.
- The first tests are the cheapest useful ones: the engine starts, loads a scene, and shuts down without faulting. The run-and-capture machinery for that already exists.
- What a test failure means for the build's exit code is a decision to make when the first test exists, not before.

---

## Android through the pipeline

**Not started.** The pipeline builds the desktop target only; Android is still built by invoking Gradle directly.

Adding it is a target selection option and a Gradle invocation, reporting into the same folder structure. It is unblocked but low value until something other than a developer's machine builds the Android package.
