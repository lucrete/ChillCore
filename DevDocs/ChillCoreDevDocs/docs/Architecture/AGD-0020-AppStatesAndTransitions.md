# AGD-0020: App States and Transitions

- **Scope:** How the application is divided into states, how it moves between them, and how a state pauses without disappearing. Covers the state machine, the fade transition, input blocking, selective pause, and the direct-boot policy. Does not cover what any individual state does.

## Overview

- The application is a set of named states. Exactly one is active, and it owns its scene, cameras, screens, and input contexts.
- Any state can be the first state. Booting straight into a work-in-progress state is a one-line change.
- Transitions fade the whole frame to black, swap states while nothing is visible, and fade back in.
- Input is blocked for the duration of a transition, so nothing interacts with a state that is being torn down.
- Pausing freezes gameplay while the scene keeps rendering. A state says whether it pauses at all; components say whether they freeze.

## Concepts

- **App state** — a named unit of application behaviour with an initialise, update, and shut down lifecycle. Registered once, activated by name.
- **Transition** — the fade-out, swap, fade-in sequence that separates two states.
- **Pausable state** — a state that has a world to freeze. The default is not pausable; a state such as the audio tracker has no pause.
- **Pauseable** — a property of a component. A pauseable component stops updating while the scene is paused; everything else keeps running.
- **Simulation time** — the world's own clock. It stops while paused and can be scaled; pauseable components read it rather than real time.
- **XR support** — a property of a state. Whether its scene can be entered in a headset, declared by the state and false unless it says otherwise.

## Architecture

`StateMachine` is a singleton owning a name-to-state map, the active state, and the transition sequence. States are registered at application startup and never unregistered.

Because it is a singleton, a state requests a transition by naming the target. It holds no reference to the machine and knows nothing about which state it is handing off to.

`StateMachineState` is the base contract: initialise, update, shut down, whether the state supports XR, and whether it pauses. It is deliberately unaware of transitions and fading — a state cannot observe or interfere with the machinery moving it.

Pausing goes through the base contract. Setting a pausable state paused freezes the Simulation — the scene hierarchy's pause flag and the frame timer's Simulation clock, together — and then calls the state's paused hook; resuming reverses both and calls its resumed hook. The hooks are where the state does everything particular to it: its action context, its input targets, its screens and its panels.

The fade overlay is not owned here. `StateMachine` sets a fade level on the render subsystem, which owns a fullscreen quad drawn after everything else. The state machine carries no knowledge of how the fade is drawn.

The mechanism behind pause is global: one flag on the scene hierarchy and one Simulation clock. Components declare whether they respond to it. The decision to pause is per state, so the machine resets the global parts at a state swap rather than trusting each state to.

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

**Pause** is checked once per scene object per frame. Pauseable components are skipped while paused; the rest update as normal. Renderables are not pauseable, which is what keeps a paused scene visible. The Simulation clock stops at the same moment, so a pauseable component that reads Simulation time resumes from where it stopped rather than jumping to where it would have been, and shaders animating on frame time freeze with it.

## Working with it

**Add a state.** Implement the state contract, register it under a name at application startup, and switch to it by name from anywhere.

**Boot directly into a state.** Change the initial target at application startup. No other change is required — see the direct-boot decision below for what that guarantees.

**Make a component freeze on pause.** Mark it pauseable, normally in its constructor. The default is not pauseable, so a component that should freeze but was never marked will keep running while the rest of the scene is frozen.

**Make a state pausable.** Mark it pausable in its constructor. Poll its own pause action and call the base contract's toggle. In the paused hook, switch to a paused action context, move the input targets to the pause menu's surfaces, and show the menu; in the resumed hook, put back what it changed. Logic the state runs outside components must check whether the state is paused itself; pause only governs component updates.

## Design decisions

### Transition requests are deferred, not immediate

Requesting a transition records the target and returns. The switch happens after the requesting state's update has finished.

Acting immediately would mean shutting down a state from inside its own update, leaving the remainder of that update running against a destroyed object. Deferring makes that impossible by construction rather than by discipline.

The one exception is the first transition, when no state is active. There is nothing to defer, so it executes immediately.

### The state machine resets pause at a swap, without calling the leaving state

Pause is a per-state decision acting on a global mechanism. A state that is left *while paused* would otherwise hand its pause to whatever runs next.

That failure is quiet and lands somewhere else. When pause also forced the input manager into interface mode, a state left through its pause menu handed the next state a paused input manager, and the camera stopped responding to the mouse with nothing in the log and no crash.

At the swap the machine clears the scene's pause flag, restarts the Simulation clock at normal speed, resets every input target to the scene, and marks the leaving state unpaused. It does not call the leaving state's resumed hook: that would bring back its play screen, its panels and anything else it does on resume, all of which its shut-down is about to destroy. The state is reused the next time it is entered, so its paused flag is cleared rather than left for then.

Resetting at the swap rather than in each state's shut-down is deliberate. The states that most need it are the ones whose exit path *is* the pause menu, and the machine already owns the boundary.

The incoming state's initialisation runs after the reset, so it sets its own targets and can begin paused if it wants to.

### Pause is part of the base contract; what it shows belongs to the state

The engine freezes the Simulation; the state decides what pausing looks like. Everything global — the scene's flag and the Simulation clock — is set in one place, together, so they cannot disagree. Everything particular — which context, which targets, which screens, which panels — is the state's own, in its hooks.

Pausing used to be several things every pausing state had to remember: the scene flag, a flag on input, a screen swap, and a saved cursor lock. Each failure was quiet. The cursor now follows the window's input target, and the flags are one call.

The cost is that restoring on resume is still each state's job. Nothing records what a state changed on pause; it puts back what it knows it changed.

### The Simulation clock stops with the scene

Skipping a component's update on pause is not enough on its own. A component that computes its pose from elapsed time jumps on resume to where it would have been, because time kept running while it was skipped. The Simulation clock stops with the pause flag, so anything reading it resumes from where it stopped. It also carries a time scale.

Components that keep running through a pause read real time. Which clock a component reads is fixed by whether it is pauseable, never switched at runtime.

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

### Whether a state supports XR is the state's own answer

A state declares whether its scene can be entered in a headset, and the pause menu offers the XR entry only where it can. The default is no.

Defaulting to no is the point. A state authored for a window — a fullscreen shader, a tracker grid, a flat menu — has not thought about stereo, a tracked head, or a scene measured in metres, and offering XR from it would put the user in a headset looking at something that does not work there. Opting in is a deliberate statement that the scene has been considered for it.

Deriving support instead — from whether a state installs a perspective camera, say — was rejected as a guess about intent from a fact about implementation. The two come apart immediately: a state can have a perfectly ordinary camera and a scene that still makes no sense in a headset.

The cost is that the flag and the XR screen's registration are two separate things a state has to remember. A state answering true without registering the screen fails an assertion the first time someone navigates to it.

### Global setup belongs to application startup, not to a boot state

Any state must be directly bootable as the first state. That requires everything shared — preloaded UI sounds, input contexts spanning states — to be established before the first state activates, in application startup rather than in a boot state.

Screens are not on that list. A screen stack belongs to the scene, so a state registers the screens it uses and clears them on the way out, and a screen used by several states is registered by each. Hoisting shared screens to application startup splits ownership of one registry, and the state's clear on exit then destroys screens it never registered.

The alternative, letting a boot state perform global setup, makes every other state silently depend on having passed through it. A state would work when reached through the menu and fail when booted directly, which is exactly the friction the policy removes.

The initialisation order is therefore fixed: engine subsystems, then application-level globals, then the first state. Anything whose absence would break a state belongs before that last step. A state's own scene, cameras, screens, and contexts stay with the state.

The cost is vigilance: the policy holds only as long as new global setup is put in the right place, and the failure mode when it is not — works from the menu, crashes on direct boot — is precisely what the policy exists to prevent.

## Limitations

- State initialisation is synchronous, and runs during the black frame. A state with heavy loading holds a black screen for as long as it takes, with no progress indication.
- Components default to not pauseable, so a gameplay component that should freeze will keep running unless explicitly marked.
- Pause applies to the whole scene. An object or subtree cannot be paused on its own.
- Which clock a component reads is a convention, not enforced: a pauseable component that reads real time still jumps on resume.
- Pause is cleared at a state swap but not on any other path. Anything else that sets it globally and outlives its setter has the same failure.
- Two independent fade systems exist with different durations and scopes.
- States cannot be unregistered, and there is no mechanism for a state to hand data to its successor.
