# AGD-0020: App States and Transitions

- **Scope:** How the application is divided into states, how it moves between them, and how a state pauses without disappearing. Covers the state machine, the fade transition, input blocking, selective pause, and the direct-boot policy. Does not cover what any individual state does.

## Overview

- The application is a set of named states. Exactly one is active, and it owns its scene, cameras, screens, and input contexts.
- Any state can be the first state. Booting straight into a work-in-progress state is a one-line change.
- Transitions fade the whole frame to black, swap states while nothing is visible, and fade back in.
- Input is blocked for the duration of a transition, so nothing interacts with a state that is being torn down.
- Pausing freezes gameplay while the scene keeps rendering. It is opt-in per component.

## Concepts

- **App state** — a named unit of application behaviour with an initialise, update, and shut down lifecycle. Registered once, activated by name.
- **Transition** — the fade-out, swap, fade-in sequence that separates two states.
- **Pauseable** — a property of a component, not of a state. A pauseable component stops updating while the scene is paused; everything else keeps running.

## Architecture

`StateMachine` is a singleton owning a name-to-state map, the active state, and the transition sequence. States are registered at application startup and never unregistered.

Because it is a singleton, a state requests a transition by naming the target. It holds no reference to the machine and knows nothing about which state it is handing off to.

`StateMachineState` is the base contract: initialise, update, shut down. It is deliberately unaware of transitions, fading, and pause — a state cannot observe or interfere with the machinery moving it.

The fade overlay is not owned here. `StateMachine` sets a fade level on the render subsystem, which owns a fullscreen quad drawn after everything else. The state machine carries no knowledge of how the fade is drawn.

Pause is owned by the scene hierarchy as a single flag. Components declare whether they respond to it. The decision to pause is per state; the mechanism is global.

## Runtime flow

**A transition** runs as a small sequence of stages.

1. **Idle.** The active state updates normally.
2. A transition is requested. The request is recorded but not acted on, so the requesting state finishes its update uninterrupted.
3. **Fading out.** The fade level rises to fully opaque over the fade duration. The active state stops updating. Input is blocked.
4. **Fully faded.** A completely black frame is presented before anything changes. Only then is the old state shut down, the new state made active, and the new state initialised.
5. **Initialisation complete.** One frame passes without fading. This exists to absorb the frame-time spike that a state's initialisation causes — without it, the accumulated delta would consume a large part of the fade-in on its first frame.
6. **Fading in.** The fade level falls to transparent. The new state does not update yet.
7. **Idle.** Input is unblocked and normal updates resume.

The scene hierarchy keeps updating throughout, even while the active state does not. That is deliberate: renderables must keep submitting themselves or the scene would vanish behind the fade rather than being covered by it.

**Pause** is checked once per scene object per frame. Pauseable components are skipped while paused; the rest update as normal. Renderables are not pauseable, which is what keeps a paused scene visible.

## Working with it

**Add a state.** Implement the state contract, register it under a name at application startup, and switch to it by name from anywhere.

**Boot directly into a state.** Change the initial target at application startup. No other change is required — see the direct-boot decision below for what that guarantees.

**Make a component freeze on pause.** Mark it pauseable, normally in its constructor. The default is not pauseable, so a component that should freeze but was never marked will keep running while the rest of the scene is frozen.

**Pause from a state.** Set the paused flag on the scene hierarchy. States that also run their own logic outside components must guard that logic themselves; the flag only governs component updates.

## Design decisions

### Transition requests are deferred, not immediate

Requesting a transition records the target and returns. The switch happens after the requesting state's update has finished.

Acting immediately would mean shutting down a state from inside its own update, leaving the remainder of that update running against a destroyed object. Deferring makes that impossible by construction rather than by discipline.

The one exception is the first transition, when no state is active. There is nothing to defer, so it executes immediately.

### The fade is a full-frame overlay, not a UI element

The overlay covers the three-dimensional scene, the UI, text, and the developer UI, because it is drawn after all of them.

A UI-based fade was rejected: it cannot cover the scene or the developer UI, and would need permanent top-of-z-order placement, entangling it with ordinary UI layering. Making it the last draw of the frame is both simpler and more complete.

The state machine only sets a level. The overlay lives with rendering, so the transition logic has no rendering knowledge and the renderer has no transition knowledge.

Note there are two independent fades in the engine — this one, covering everything during a state change, and a shorter UI-only fade used when swapping screens within a state. They serve different purposes and are easy to confuse.

### A black frame is presented before the swap

The state swap happens only after a fully opaque frame has actually reached the screen, not merely after the fade level reaches its maximum.

Swapping a frame earlier would let the last frame of the outgoing state and the first frame of the incoming one appear through a not-quite-opaque overlay. The extra frame is imperceptible and removes the possibility entirely.

### Input is blocked by flag as well as by skipping updates

The active state stops updating during a transition, which stops application input handling. That alone is not enough: the UI manager updates from the engine loop independently of the application, so UI callbacks could still fire mid-transition.

Blocking is therefore also a flag on the input manager, set for the whole transition, which folds into the result of every input query. Both mechanisms together mean no path reaches a state that is being torn down.

### Pause is a component property, defaulting to off

Pausing had to freeze gameplay while leaving the scene visible. Disabling the scene hierarchy fails that outright, because renderables submit themselves during their update — disabling them makes the scene disappear rather than freeze.

Marking individual components as pauseable solves it directly. Renderables are never marked, so they keep submitting; gameplay components opt in and freeze.

The cost is the default. Not-pauseable is the safe default for the engine but the wrong default for gameplay, so a new gameplay component that nobody marked will keep animating through a pause. Defaulting the other way would have required every always-on component to opt out, which is a larger change to the component base.

### Global setup belongs to application startup, not to a boot state

Any state must be directly bootable as the first state. That requires everything shared — preloaded UI sounds, screens used by more than one state, input contexts spanning states — to be established before the first state activates, in application startup rather than in a boot state.

The alternative, letting a boot state perform global setup, makes every other state silently depend on having passed through it. A state would work when reached through the menu and fail when booted directly, which is exactly the friction the policy removes.

The initialisation order is therefore fixed: engine subsystems, then application-level globals, then the first state. Anything whose absence would break a state belongs before that last step. A state's own scene, cameras, screens, and contexts stay with the state.

The cost is vigilance: the policy holds only as long as new global setup is put in the right place, and the failure mode when it is not — works from the menu, crashes on direct boot — is precisely what the policy exists to prevent.

## Limitations

- State initialisation is synchronous, and runs during the black frame. A state with heavy loading holds a black screen for as long as it takes, with no progress indication.
- Components default to not pauseable, so a gameplay component that should freeze will keep running unless explicitly marked.
- Two independent fade systems exist with different durations and scopes.
- States cannot be unregistered, and there is no mechanism for a state to hand data to its successor.
