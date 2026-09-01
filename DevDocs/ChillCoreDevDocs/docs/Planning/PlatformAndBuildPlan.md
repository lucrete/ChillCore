# Platform and Build Plan

**Status:** Windows and Android both ship, from one build description. What remains in plan is the Android v1 ship-gate verification.
**Current state:** AGD-0040 (Build and Targets) and AGD-0160 (Android Support) describe what exists. This plan covers only what does not.
**Scope:** the platform work that is in plan. Further targets (WebGL, Vulkan and beyond) and shader cross-compilation are not in plan and are not covered here. Persistence has its own plan, `PersistencePlan.md`.

---

## Android v1 ship gate — unverified

The Android port defined a narrow ship criterion that has never been recorded as run: the showcase state on the reference device for at least one minute, surviving at least one rotation and one task-switch, with no crash and no visible corruption.

**Why it matters.** Everything downstream assumes Android v1 works. The port is believed good, but believed is not verified, and this is the cheapest item in the plan.

A second, larger gate also stands unrun: a hundred or more rotation cycles with no perceptible resume stutter, and a forced graphics-context loss that finishes the activity and relaunches cold without crashing or leaking graphics resources.

---

## Android platform gaps

Three file-system operations are unimplemented on Android (`PlatformFileSystemAndroid.cpp:158-172`). Not in plan. The detail is kept here because it is where the fix lands.

- `CopyFile` and `ListDirectoryEntries` both return failure. Deliberate — their only consumer is desktop-only — but it blocks any Android feature needing to enumerate files.
- `WriteFileTextAtomic` forwards to an ordinary write and is not atomic. Anything on Android that comes to depend on the atomicity guarantee needs this addressed first, which requires scoped-storage work.

---

## Persistence and lifecycle hooks

Design in `PersistencePlan.md`.

The context-loss policy is a deliberate cold restart, on the premise that persistence restores the user to where they were. Until persistence exists, that recovery does not, and a context loss returns the user to the boot screen. This is a hard dependency: no Android context-loss hardening before persistence lands.
