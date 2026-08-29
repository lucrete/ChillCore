# Build Pipeline Plan

**Status:** The pipeline builds a self-contained output directory and runs it. What remains is distribution, automated testing, and Android.
**Current state:** AGD-0040 (Build and Targets) describes the pipeline as it exists. This plan covers only what does not.
**Index:** the Build pipeline section of `01_TechBacklog.md`.

---

## Packaged distribution

**Not started.** The output directory is now self-contained, so there is something worth archiving.

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
