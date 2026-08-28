# AGD-0160: Android Support

- **Scope:** What running on Android requires beyond the shared engine — the activity shell, the drawing-surface and graphics-context lifecycle, packaged assets, touch input, and what is deliberately absent. Does not cover the platform abstraction itself, which is AGD-0030, nor the build, which is AGD-0040.

## Overview

- The application and the cross-cutting parts of the engine compile unchanged. The cost lands in the platform backends and the graphics backend, which is where the abstractions intended it to land.
- A native entry point owns the engine's lifetime and drives frames from the platform's event loop rather than running its own.
- Backgrounding is survived without tearing anything down. Losing the graphics context outright is survived by restarting cleanly.
- Assets are read directly from the packaged application, with no extraction step.
- Touch is delivered as a pointer, so existing interface hit-testing works unmodified.

## Concepts

- **Activity** — the platform's unit of a running screen. It can be stopped, restarted, and destroyed independently of the process.
- **Drawing surface** — the on-screen drawing target. Taken away when the application is backgrounded, given back on return.
- **Graphics context** — the graphics state holding every resource handle. Losing it invalidates everything.
- **Lifecycle wave** — one of three independently shippable layers of lifecycle handling, ordered by how common the event is and how expensive the response.

## Architecture

The native entry point owns everything. It creates the engine and the application when a drawing surface first arrives, runs the outer event loop, ticks a frame whenever a drawing surface is bound, and tears down on destruction.

Frames are driven from the platform's loop, not from the engine's. The engine exposes a single frame tick for exactly this reason, so the definition of a frame stays shared while control of when to run one does not.

Input arrives as event buffers from the activity shell, drained each frame and forwarded through the platform input backend. Touch, and gamepad were it enabled, arrive through the same buffer with no separate path.

The graphics backend is the mobile variant, sitting beside the desktop one and sharing the common layer. Shader dialect differences are handled by injecting the appropriate preamble at compile time rather than by maintaining separate shader files.

The developer overlay is compiled out and replaced by a stub satisfying the same interface, so call sites elsewhere remain valid.

## Runtime flow

**The outer loop** alternates between draining platform events and ticking a frame. Two details in it are not obvious and both were learned the hard way:

- The event drain must also break when destruction has been requested. Otherwise a blocking poll after the final destroy event waits forever for an event that will never come — the events are drained, the timeout is infinite, and the outer condition is never re-evaluated. This was a real deadlock before the extra check existed.
- The poll timeout flips between not waiting at all when a drawing surface is bound, and waiting indefinitely when there is none. Never waiting burns battery while backgrounded; always waiting blocks rendering. Switching between them gives each behaviour the situation it suits.

**Losing the drawing surface** binds the graphics context to a minimal offscreen target instead, so the graphics context and every resource handle stay alive. Nothing is torn down and nothing is rebuilt on return. Rotation, locking, and task-switching resume seamlessly.

**Losing the graphics context** is detected when presentation reports it. The response is to tear down the dead graphics state and ask the framework to finish the activity, which then relaunches cold.

**Rotation** is declared as a configuration change the application handles itself, so the framework does not recreate the activity. Without that declaration, every rotation would destroy and recreate the activity, bypassing drawing-surface preservation entirely.

## Design decisions

### The modern activity shell, not the deprecated one or a custom bridge

The platform's current native shell is used rather than its deprecated predecessor or a hand-written bridge from managed code.

It is what the platform vendor recommends and what the profiling and development tooling targets. It handles the cross-thread lifecycle plumbing — event polling, lifecycle command delivery, input event buffers — which reduces the entry point to event routing rather than a large amount of interop code. Input of every kind arrives through one buffer, so there is no separate path per device class.

Writing a custom bridge would reimplement all of that for no gain. The deprecated shell would mean building on something new platform features no longer target.

### Lifecycle handling is layered into three independent waves

The three lifecycle problems have very different frequencies and costs, so they are solved separately and can ship separately.

Drawing-surface preservation is constant and cheap, and is mandatory — without it every rotation and lock visibly stutters. Graphics-context-loss handling is rare and is mandatory only if graphics-context loss is to be supported at all; without detection and a clean exit, the next graphics call fails and the user sees a crash. State persistence across activity destruction is genuinely optional; without it the user returns to the boot screen after rare destruction events, but the process restarts cleanly.

Layering means each delivers value when it lands, rather than gating everything on one large milestone.

### Graphics-context loss triggers a clean restart, not an in-process rebuild

When the graphics context is lost, the activity finishes and relaunches from cold. There is no attempt to rebuild resources in place.

An in-process rebuild was designed and rejected. It would require every subsystem owning graphics resources to gain a rebuild path, and — more damagingly — would require keeping processor-side copies of all mesh and texture data for the whole session, potentially hundreds of megabytes for a realistic asset set, purely to service an event that fires perhaps once a month per user.

The costs were disproportionate on both axes. The memory is paid constantly for a rare benefit, and the code surface is many subsystems' worth of rarely-exercised paths, which is where dormant bugs accumulate. Restarting is also what comparable engines do.

The restart's user experience is a few seconds of cold launch, and it depends on persistence to return to the same place. Until that persistence exists, the restart is clean but lands at the boot screen, and transient state is lost regardless.

### Packaged assets are read in place

Assets are read directly out of the packaged application rather than being extracted to private storage on first run.

Extraction wastes storage on a second copy, adds first-launch latency, and complicates updates. Reading in place is fast because packaged assets are memory-mapped.

The build packages the existing asset directory directly, so there is one asset tree rather than a platform-specific copy. Path conventions are translated at the platform boundary, so callers are unaffected.

Two consequences follow from packaged assets being immutable. Writes go to private application storage instead. And modification times are meaningless, so shader hot reload is permanently unavailable here — which is correct, since reloading from a packaged archive is not a coherent idea.

### Text file reads normalise line endings

The packaged asset reader collapses line endings to match what the desktop file reader produces.

Without it, parsers that split on whitespace silently pick up a trailing carriage return on every value, producing lookups that fail against strings that look identical when printed. This is the kind of defect that costs hours, and normalising at the platform boundary removes it for every caller at once.

### The developer overlay is compiled out

The interface is satisfied by a stub with empty implementations, selected by a build definition.

The overlay's implementation is bound to a desktop windowing library, and porting it to a touch-driven device with a soft keyboard is a project in its own right rather than a side effect of this port. Its value on a phone is also doubtful — on-device profiling uses the platform's own tooling, which is better at it.

The stub keeps call sites valid everywhere, so the cross-platform contract holds and only the implementation differs. If an on-device overlay ever becomes worth having, it goes behind the same interface.

### Touch is delivered as a pointer

Single-finger touch is forwarded as a held pointer, which the interface layer already understands for hit-testing.

This makes menus work on a phone with no interface changes at all — existing buttons respond to touch because they respond to pointers. Building a parallel touch input path would have duplicated the trigger and action mapping that already exists.

Multiple pointers are tracked and exposed, so the data for gestures is present. Nothing consumes it, so pinch, drag, and similar gestures do not work. That is deferred deliberately: gesture design should follow what the application actually needs rather than being guessed at during a port.

## Limitations

- Shader hot reload is unavailable, because packaged assets are immutable.
- The developer overlay does not exist. Anything built on it is desktop-only.
- Touch is pointer emulation only. Multi-touch data is captured but unused, so there are no gestures.
- Keyboard and gamepad report as absent.
- One processor architecture is supported.
- Drawing-surface preservation depends on the driver supporting a minimal offscreen target. Where it does not, a fallback applies and the preserved cycle is degraded.
- Graphics-context loss costs a cold restart, and transient state not deliberately saved is lost. Persistence to make that recovery meaningful does not exist yet.
- The first frame after a drawing surface returns is measurably slower than steady state on tile-based hardware.
