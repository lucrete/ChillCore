# AGD-0130: UI Screens and Navigation

- **Scope:** How screens are registered, activated, and navigated between, and how application code attaches behaviour to them. Covers the navigation stack, screen fades, cancel handling, and the controller lifecycle. Does not cover how a screen is laid out or drawn, nor how the interface serves several destinations at once.

## Overview

- A screen pairs an element tree with a controller holding its behaviour. Screens are registered once and activated by name.
- A screen stack belongs to one surface. Every surface has one, and the calls on it are the same everywhere.
- Navigation is a stack. Going forward pushes, going back pops, and a separate flat replacement discards history entirely.
- Forward and back navigation fade the interface only. The scene behind it is untouched.
- Cancelling goes back automatically, without every screen implementing it.
- A controller initialises once, then receives enter, update, and exit calls for each activation.

## Concepts

- **Screen** — a registered element tree with an associated controller, addressed by identifier.
- **Controller** — application code owning a screen's behaviour: callbacks, per-activation state, per-frame logic.
- **Navigation stack** — the history of screens entered, allowing return to the previous one.
- **Flat replacement** — setting the active screen and discarding history, for swaps where returning makes no sense.
- **Fade alpha** — one opacity per surface, applied to that surface's interface drawing before it draws.

## Architecture

`UiScreenSystem` owns registration, the navigation stack, the active screen, and the transition sequence. The stack is a fixed-size array with a small depth limit, so navigation never allocates.

**It is an instance owned by a surface, not a singleton.** A screen definition holds its built element tree, so a screen lives on the one surface it was registered with; two panels showing the same markup register it on each, which is already how registration works. Nothing addresses "the" screen system — code names the surface whose stack it means.

A screen controller is the boundary where application behaviour meets an interface definition. It registers callbacks against the action identifiers in its markup, and handles per-activation setup. It is bound to its surface at registration and never rebound, so it resolves elements and callbacks against that surface rather than against whatever happens to be active. The same controller class then works unchanged wherever it is registered.

Callback registrations live with the screen and persist across activations, because they are configuration rather than state. Per-activation concerns — enabling elements, setting initial selection, refreshing text from current data — belong to the enter and exit calls.

The fade is an opacity applied to interface and text drawing together. Each surface applies its own before drawing, so surfaces fade independently. It does not involve the scene, and does not go through the fullscreen overlay used for application state transitions.

## Runtime flow

**Registration** happens once per screen. The screen's element tree is briefly made active so the controller can find its elements by identifier and register callbacks, then detached again.

**Forward and back navigation** run a three-stage sequence:

1. **Fading out.** Interface opacity falls to zero over the fade duration. The scene continues rendering normally behind it.
2. **At zero opacity.** The outgoing screen is deactivated, the stack is pushed or popped, and the incoming screen is activated.
3. **Fading in.** Opacity returns to full.

**Flat replacement** skips the fade entirely and swaps immediately, because it exists for swaps that should feel instant.

**Cancelling** is handled during the idle phase of the surface that reads platform input. Cancel is one input stream with no spatial origin, so only that surface consumes it; panels are navigated by pointing at them. If the stack has somewhere to return to, cancel navigates back. A controller wanting different behaviour intercepts the action in its own update before the system sees it.

**While a screen is active and not transitioning**, its controller receives a per-frame update.

## Working with it

**Add a screen.** Author the markup and stylesheet, implement a controller, and register it on a surface under an identifier. Register callbacks in the controller's initialisation, since they persist.

**Register the screens a state needs, in that state.** Including sub-screens its menus route to. A state clears the window's registry on the way out, so identifiers are free for the next state to reuse.

**Reach the stack you mean.** Name its surface. A controller asks for its own; application code asks the interface manager for the window's.

**Navigate into a sub-screen.** Transition forward. Cancel will return automatically; no back callback is needed.

**Swap between screens with no history.** Use flat replacement — a heads-up display and a pause menu, for instance, where returning through a stack is meaningless.

**Refresh a screen's content on show.** Do it on enter, not on initialisation. Initialisation runs once ever; enter runs every activation.

**Override cancel.** Handle the action in the controller's update. The system's automatic back only applies if the controller has not already acted.

## Design decisions

### Navigation is a stack, with a flat replacement alongside

A stack models menu hierarchies directly: entering a sub-menu pushes, leaving pops, and the return path is implicit rather than something each screen must remember.

Flat switching alone was rejected because it forces every screen to track where it came from, which duplicates the stack badly across many places. A general navigation graph was rejected as unjustified — the actual navigation is hierarchical, and a graph would add routing machinery for transitions nobody makes.

The flat replacement exists because some swaps genuinely have no history. Swapping a heads-up display for a pause menu is not "going deeper"; treating it as a push would leave a stack entry that back-navigation could stumble into.

The stack is a fixed-size array with a small limit, so navigation never allocates. Exceeding the depth is a hard limit, which is acceptable because menu hierarchies that deep indicate a design problem.

### Screen fades affect the interface only

The fade is an opacity applied to interface and text drawing, held per surface. The scene renders normally throughout.

Reusing the fullscreen overlay that covers application state transitions was rejected because it would black out the scene for what is merely a menu change, and would conflate two unrelated things — one is "the whole application is changing", the other is "this panel is being replaced".

The duration is deliberately shorter than the state-transition fade: fast enough to feel responsive when clicking through menus, slow enough not to flicker.

Note this means two fade systems exist, with different durations and scopes. They are easy to confuse and the distinction is worth knowing before touching either.

### Cancel navigates back at the system level

The system handles cancel when the stack has somewhere to return to, rather than each screen registering its own back handler.

Cancel meaning back is a universal convention, so per-screen handling would be the same code repeated in every sub-screen, easy to omit and producing a screen the user cannot leave. Handling it once means it is always present.

A guard prevents navigating past the root, so cancel at the top level does nothing rather than emptying the stack.

Controllers needing different behaviour intercept the action first. The cost is that this precedence is a convention rather than something enforced.

### A stack belongs to the scene, so screens are registered per state

A state registers its screens on entry and clears them on exit. The next state starts from a clean registry, and identifiers like "Options" or "PauseMenu" are reused freely because no two states hold them at once.

Registering shared screens once at application startup was tried and rejected. It splits ownership of one registry between the application and whichever state is running, so the state's clear on exit destroys screens it did not register — which then needs a lifetime marker on every registration to work around. Screens are content of the scene, and the same rule as the scene itself is simpler: whoever set it up takes it down.

The cost is that a screen used by several states is registered by each of them. That is a few lines per state, and it keeps the question "what is registered right now" answerable from one place.

### Controllers initialise once and then enter and exit repeatedly

Initialisation runs at registration; enter and exit run per activation.

The split follows what the data actually is. Callback registrations are configuration — they never change across activations, so rebuilding them each time would be wasted work whose only effect could be introducing inconsistency. Per-activation state genuinely differs each time a screen is shown, so it belongs where it is redone.

Making initialisation work requires briefly activating the screen's element tree so the controller can find elements and attach callbacks, then detaching it. That is an implementation cost paid once per screen.

The cost is a trap for authors: putting content refresh in initialisation appears to work, because the first activation immediately follows, and then silently shows stale data on every subsequent one.

## Limitations

- Stack depth is fixed and small. Exceeding it is a hard limit.
- Screens cannot pass data to one another through the navigation system; shared state must live elsewhere.
- Controller precedence over automatic cancel is a convention, not an enforced ordering.
- Two independent fade systems exist with different durations and scopes.
- Flat replacement discards the whole stack, so it cannot be used to swap the current screen while preserving history.
- Registration temporarily activates a screen's tree on its surface, so registration order can matter if a controller has side effects beyond attaching callbacks.
- Screen identifiers are unique per surface, not globally. The same identifier on two surfaces names two separate screens.
- A menu routing to a sub-screen its state did not register fails an assertion at the transition. Nothing checks the pairing at registration.
