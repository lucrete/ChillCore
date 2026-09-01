# AGD-0010: Engine Architecture and Lifecycle

- **Scope:** How the engine starts, what owns what, and the order of work inside a frame. Covers the engine and application split, subsystem ownership, the frame loop, and the drawing-surface and graphics-context lifecycle hooks. Does not cover any individual subsystem's internals.

## Overview

- The engine is split in two. `CC::CoreMain` owns every engine subsystem; the application supplies one object implementing `CC::IAppMain` and nothing else.
- A per-platform entry shell constructs both, drives the loop, and shuts them down. The shell is the only code that differs between desktop and Android.
- Subsystems are singletons, reached through a static accessor rather than passed as arguments.
- One frame runs a fixed sequence of phases. The ordering is load-bearing, not incidental.
- The engine survives losing its drawing surface, and can be told to rebuild when it loses its graphics context outright.

## Concepts

- **Engine versus application** — engine code lives in the `CC` namespace; application code has no namespace. The boundary is a contract, not a convention: engine code never names an application type.
- **Subsystem** — a long-lived manager owned by `CoreMain` for the process lifetime, reached by a static accessor.
- **Frame phase** — a named span of the frame loop. Phases are timestamped, so the profiler's breakdown and the loop's structure are the same thing.
- **Drawing-surface loss versus graphics-context loss** — losing the drawing surface means the window is gone but graphics resources survive. Losing the graphics context means every graphics handle is invalid and resources must be rebuilt.

## Architecture

`CoreMain` constructs, owns, and destroys every engine subsystem: platform file system and window, input, printing, frame timing, audio, scene hierarchy, rendering, developer UI, fonts, text and UI rendering, the string database, the UI manager, and the UI screen system. It also owns the frame loop and the quit flag.

The application reaches the engine through `IAppMain`, an interface of three methods — initialise, update, shut down. `CoreMain` calls them and knows nothing else about the application. `AppMain` implements it and owns a `StateMachine`; everything application-specific lives below that line.

The platform entry shell is per target and deliberately thin. It constructs `CoreMain`, initialises it, constructs and initialises the application, runs the loop, then shuts both down in reverse order. Desktop drives the loop directly; Android drives it from the platform event loop, calling a single frame tick rather than surrendering control. Both call the same engine methods.

Subsystem construction order in `CoreMain` is significant. The platform file system and window come first, because the window creates the graphics context that rendering subsystems need at construction, and because asset loading depends on the file system. Everything after that is ordered by dependency.

## Runtime flow

A frame runs five phases in fixed order.

1. **Frame start.** Poll the operating system for fresh input, start the developer UI's frame so it can consume that input, then update the input manager. Rendering begins its frame.
2. **Update.** The application updates, then the scene hierarchy updates. Application logic runs before scene traversal so that a change made this frame is reflected in the same frame's world transforms.
3. **UI update.** Screen system, then UI manager, then developer UI. Audio updates last in this phase, so that sounds triggered by UI interactions this frame are accounted for before finished sounds are reaped.
4. **Render.** The scene renders, then UI and text, then the developer UI overlay.
5. **Swap.** Rendering ends the frame and presents.

Three ordering constraints are worth knowing before rearranging anything:

- Operating-system input must be polled before the developer UI starts its frame, because the developer UI consumes that input to decide what it is hovering. The input manager then queries that same-frame hover state to decide whether the application should see the input at all.
- Audio's update follows UI and developer UI, so a sound started by a button press this frame is not reaped in the same frame it began.
- The developer UI draws after everything else, and the screen fade overlay draws after that, inside the render subsystem's end-of-frame. The fade must cover the developer UI, so nothing may draw later.

Each phase boundary records a timestamp. The profiler's CPU breakdown is generated from these, so adding a phase to the loop adds it to the profiler with no further work.

The GPU breakdown works the same way but is built from a separate set of markers, because GPU work does not align with the CPU phases that submitted it. A render pass opens a timing scope named by its caller, and a phase runs from one marker to the next, so each phase is named for the work it contains rather than for the boundary that precedes it. A frame reads as seven phases plus the swap wait: idle, opaque, transparent, the multisample resolve, post-processing, UI, and the final overlay.

Three properties hold this together, and all three are load-bearing:

- **A name is written down once, at the marker.** Anything displaying or recording phases reads the names back from the frame, so the profiler panel and the CSV export cannot drift from what the frame actually did. A panel holding its own list of phase names silently mislabels every phase after the first one that moves.
- **Passes group into phases.** A scope named `Group/Detail` folds together with its neighbours in the same group, so post-processing is one phase whether it ran one pass or four. A capture tool still sees each pass separately. Without this the breakdown would restructure itself whenever an effect was switched on, which is exactly when it is being read.
- **A pass's resolve is charged to the pass.** Ending a pass resolves and presents what it drew, so that cost is named after the pass rather than landing on whichever phase happens to follow.

Grouping keeps the phase set fixed in normal use, but it is not guaranteed: a future phase could appear or disappear. A frame whose phase set differs from the recorded one cannot be compared against it, because the same index would mean two different things, so adopting a new set discards the history rather than blending the two.

## Design decisions

### The application reaches the engine through a three-method interface

`CoreMain` depends on `IAppMain`, not on `AppMain`. The engine can be compiled and reasoned about without any application present, and the application can be replaced wholesale without touching engine code.

The interface is deliberately minimal — initialise, update, shut down. Anything richer would tempt the engine into knowing about application concepts. Application structure, including the state machine and every app state, lives entirely behind it.

The cost is that the engine cannot call into the application for anything it has not already been given. Where the engine needs application participation, it goes through a subsystem the application registers with, not through a widened interface.

### Subsystems are singletons rather than injected dependencies

Every manager exposes a static accessor and asserts on construction that no second instance exists. Callers reach subsystems directly rather than receiving them.

This was chosen for reach: rendering, input, and audio are needed at arbitrary depths — inside components, UI controllers, and app states — and threading references through every intermediate layer would dominate the code. Construction and destruction order remain explicit and centralised in `CoreMain`, which is where the real lifetime risk lives.

The cost is the usual one. Dependencies are invisible in signatures, and a subsystem used before `CoreMain` has constructed it fails at runtime rather than at compile time. The assert in each accessor is what turns that into a loud failure instead of a silent one.

### The frame loop lives in the engine, but the platform drives it

`CoreMain` exposes both a complete loop and a single frame tick. Desktop calls the loop and lets it run to completion. Android cannot — its platform requires the application to service an event queue and to survive the window disappearing — so it calls the frame tick from its own outer loop.

Both paths execute the same phase sequence. Splitting the tick out of the loop, rather than giving Android its own loop, keeps one definition of what a frame is.

### Quit is owned by the engine, not the renderer

Quitting sets a flag on `CoreMain`, which the loop tests. The window's close button routes to the same request.

Quit is a lifecycle concern, and `CoreMain` owns the lifecycle. Routing it through the renderer — the obvious alternative, since the renderer owns the window on desktop — would create a dependency that misrepresents what quitting is, and would not survive the window moving into the platform layer.

### Drawing-surface loss and graphics-context loss are separate hooks

The engine exposes two pairs of lifecycle callbacks. One pair handles the drawing surface going away and coming back; graphics resources survive, and the engine simply stops presenting. The other pair handles the graphics context being destroyed, which invalidates every handle the engine holds.

They are separate because their cost is separate. Drawing-surface loss is common — backgrounding an application on mobile — and must be cheap. Graphics-context loss is rare and expensive, requiring every GPU resource to be rebuilt from its source. Collapsing them into one hook would force the expensive path on the common case.

The graphics-context hooks exist and are called, but recovery is only partly built: the platform layer tears down cleanly, while rebuilding depends on a persistence layer that does not exist yet.

## Limitations

- Subsystem dependencies are not expressed anywhere except construction order in `CoreMain`. Reordering that list can produce a failure far from the cause.
- Application initialisation is synchronous. A slow start blocks the first frame with no opportunity to display progress.
- Graphics-context-loss recovery is incomplete. The hooks are in place, but restoring application state after a rebuild requires persistence that has not been implemented.
- The developer UI is desktop-only. Anything built on it is unavailable on Android.
