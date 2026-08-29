# Persistence and Lifecycle Plan

**Status:** Not started. Nothing exists in the source: no persistable interface, no persistence manager, no application pause or resume hooks, no saved-state plumbing.
**Blocks:** the Android graphics-context-loss story, which restarts cold on the premise that persistence restores the user's place.
**Sequence:** row 8 of `02_Roadmap.md`.

Nothing below describes existing code. Where interfaces are sketched, this document is the source of truth until the code exists, at which point the design moves into an AGD and the sketches are deleted.

## Goal

Survive destruction of the entire process, and land the user back where they were.

## Scope

Two interlocking pieces:

1. **Lifecycle hooks** — application pause and resume, plus the platform's saved-state plumbing. Pure plumbing, no domain decisions.
2. **Persistence framework** — a small application-level system for serialising and restoring engine state, with two storage backends: the platform's own small ephemeral saved-state blob, and durable on-disk files.

The deliverable is the framework, the lifecycle integration, and one minimum-viable consumer.

Out of scope here, but designed to consume the same framework: settings, save games, cloud sync, telemetry.

## Why this is a separate problem from the other lifecycle work

The Android port already survives losing its drawing surface, and survives losing its graphics context by restarting. Those preserve or rebuild in-memory state. This preserves state across the destruction of the whole object graph, including the process, and the mechanisms have nothing in common.

| | Drawing surface lost | Graphics context lost | Process destroyed |
| --- | --- | --- | --- |
| What dies | The on-screen target only | The context and every graphics handle | Everything the process holds |
| What survives | Context, handles, all engine state | Engine objects and their source data | Only what reached durable storage |
| Recovery | Reattach | Rebuild from source data | Read storage and reconstruct |

Because process death takes everything, nothing in memory can be relied on. The source of truth must live outside the process, which forces it to be explicit, serialised, and versioned. There is no walk-the-scene-and-rebuild equivalent, because the scene is gone.

This is also why the framework is the natural foundation for the broader save system. The same serialisation infrastructure that survives activity destruction is what save games, settings, and cloud sync need. Building it once with both consumers in mind costs the same as building it once for either.

## Architecture

### A persistable interface

Anything wanting to survive process death implements a small interface: a stable identifier used as its key in the serialised blob, a save method, a load method returning failure when the data is incompatible, and a schema version.

The interface is deliberately minimal. It offers no guidance on what to persist — that is the implementer's call — and no reflection-driven serialisation. Explicit save and load methods give each implementer control over format, validation, and compatibility, and avoid a dependency on a serialisation library.

Reading and writing go through thin byte-stream wrappers offering type-safe primitives and length-prefixed strings.

### A persistence manager

A registry of persistables that orchestrates snapshot and restore. A snapshot walks the registry and produces a byte buffer whose entries carry the identifier, schema version, and payload for each persistable. A restore matches entries by identifier: unknown identifiers are ignored, so a newer blob is safe to read, and missing ones are skipped rather than defaulted implicitly.

### Two storage backends

The platform's saved-state blob is small, ephemeral, and driven by the lifecycle. On-disk files are larger, durable, and written when the application chooses. The framework writes to both through one interface.

## Minimum-viable consumer

The state machine snapshots the active state's registered name, and on launch restores it instead of defaulting to the boot state.

That alone fixes the user-visible symptom. After it lands: switching away with activity retention disabled and returning lands on the same screen; force-stopping and relaunching lands on the same screen; a cold launch with no prior session boots normally.

Per-state data — camera position, scene, transient gameplay state — lands incrementally. Each state that wants more registers itself separately with its own identifier and schema. The framework absorbs them without changes.

## Phasing

Three sub-phases, each independently shippable.

1. **Framework and plumbing.** The interface, the manager, the readers and writers, the application pause and resume hooks, and the platform routing. Nothing registered yet — this verifies the framework builds and runs cleanly as a no-op.
2. **State machine restoration.** The state machine becomes a persistable. Verified by disabling activity retention, navigating away from the default state, switching away, and returning.
3. **On-disk fallback.** Snapshot to and restore from a file, triggered on pause, used when the platform blob is absent. Verified by force-stopping and relaunching.

The first two cover the Android symptom. The third closes the abrupt-kill case and is what the broader save system builds on.

## Open questions

Deliberately deferred to implementation rather than guessed at now.

- **Where the writable root lives on desktop.** Android has a private data directory. Desktop currently writes to the working directory, which is wrong for a shipped application. This is really a platform-layer question, but persistence is the consumer that forces it.
- **Schema-mismatch policy.** Discard everything, restore what matches and default the rest, or require explicit migration. The proposal is per-persistable: a failed load defaults that subsystem only, so minor schema drift in one place does not discard everything.
- **Registration lifetime.** Registered objects must unregister before destruction, which for app states means unregistering on exit. Forgetting leaves a dangling pointer, caught by a debug assertion but otherwise silent. A weak reference would remove the hazard at the cost of complexity. Decide in the first sub-phase.
- **Threading.** Snapshotting runs on the main thread, the only thread the engine has. If saving on pause becomes slow enough to delay the lifecycle event, it moves to a worker. Not an initial concern.
- **Encryption.** Files live in private application storage, which is reasonably protected. Encryption layers on top of the byte format if a need appears. Not an initial concern.
- **Reporting restore failures.** Proposal is to log a warning and default silently, with no telemetry until a telemetry system exists.
