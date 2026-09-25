# AGD-0110: Input System

- **Scope:** How physical input becomes application-meaningful actions. Covers the layered model, contexts, the gamepad-first design, input domains and targets, and developer-only bindings. Does not cover how any individual platform captures input.

## Overview

- Input passes through three layers: physical devices, logical triggers, and named actions. Each layer is remappable independently of the ones around it.
- Triggers are gamepad-shaped even when driven by a keyboard, so application code never encounters a device-specific concept.
- Actions are grouped into contexts. Switching context re-points every action at different triggers without the querying code changing.
- Every device is read every frame. An action fires from whichever device holds its trigger; the device most recently used only decides what prompts and on-screen controls show.
- Input is split into domains — the window's devices and the headset's controllers — and each domain has one input target: the scene, or one interface surface.
- Continuous input is either a rate or a displacement, and the two are paced differently against frame time. Both are presented through the same stick surface.
- Developer bindings are keyboard-driven and separate from application bindings.

## Concepts

- **Physical input** — an actual key, mouse button, gamepad button, or stick axis.
- **Trigger** — a logical input in gamepad vocabulary: a direction, a face button, a shoulder. Bound to one or more physical inputs per platform.
- **Action** — something the application means: jump, confirm, move forward. Bound to a trigger.
- **Context** — a named set of action-to-trigger bindings. One is active at a time.
- **Input domain** — a group of devices with one consumer at a time. The window domain is everything arriving through the platform: keyboard, mouse, gamepad, touch. The headset domain is everything arriving through the XR controllers.
- **Input target** — what a domain's input reaches: the scene, or one named surface.
- **Active input type** — the kind of device used most recently. Decides prompts and which on-screen controls show; never decides which device is listened to.
- **Edge** — the transition into or out of a pressed state, as distinct from the state itself.
- **Rate input** — a held position standing for a speed: a stick's deflection, or a touch drag treated as one.
- **Displacement input** — a reading standing for a distance already travelled: the mouse's movement since the previous frame.

## Architecture

`InputTriggerMap` sits closest to the hardware. Each frame it reads physical state through the platform layer, evaluates every trigger's keyboard and gamepad bindings, takes up the XR controller state pushed into it, and keeps this frame's and last frame's results so edges can be derived. A trigger is held if any device holds it. A trigger's gamepad binding is a list of alternatives, each a set of inputs held together, so the D-pad and a stick direction can fire one trigger. It supports modifier combinations but not ordered combinations or timing windows.

`InputActionMap` maps actions to triggers within contexts. Actions are integers, so an application defines its own enumerations and registers them; the map knows nothing about what any action means. Contexts are created and populated at startup and selected at runtime. Creating a context also binds the engine's own actions into it: developer actions, interface navigation for the window and for the headset, and the XR hands' select and grab.

`InputManager` is the application-facing surface, and the only one application code should use. It answers whether an action is pressed and whether it changed this frame, exposes mouse and stick state, owns the input target of each domain and the input-blocked flag, and provides stick overrides for driving input synthetically.

The input target is the only gate left on where input goes. The camera reads devices directly only while the window's target is the scene. A surface takes pointers and navigation only while it is its domain's target. The interface's pointer router offers headset rays only to what the headset's target allows. Stick overrides are read whatever the target, because they are only ever set by an on-screen control that holds it.

Every trigger and action carries a display label, used by the developer overlay to show current bindings.

## Runtime flow

Each frame, in order:

1. The platform's event queue is pumped, bringing fresh device state in.
2. The developer overlay starts its frame and consumes that state, deciding what it is over.
3. Triggers are evaluated from physical state, and current results are compared against last frame's to produce edges.
4. Actions resolve through the active context to their triggers.

The ordering between the second and third steps is load-bearing: the input system asks the overlay whether it is capturing input before deciding whether the application should see it. Reversing them would let clicks pass through the overlay into the world behind it.

**Queries fold their conditions into the result** rather than returning early. When input is blocked the query answers false rather than being skipped. Asking whether the scene or a surface receives the window's input also answers false while the developer overlay has the mouse.

## Working with it

**Define actions.** Declare an enumeration for the context's actions, create the context, and register each action against a trigger. Query by the enumerator.

**Switch context.** Set the active context by name. Every subsequent query resolves through the new bindings, with no change to the querying code.

**Check for a press versus a hold.** Ask for the pressed state for a hold, or for the positive edge for a single activation. Using pressed state where an edge is meant produces an action that repeats every frame.

**Route input between world and interface.** Set the domain's input target: the scene, or a surface. Systems ask whether the scene or their surface receives the domain's input rather than reading the target directly. The cursor follows the window's target: a surface frees it, and the scene puts back whatever lock it had before.

**Drive input synthetically.** Override a stick's values to feed input from a source other than a device, and clear the override to return control.

## Design decisions

### Three layers, each remappable independently

Physical input, triggers, and actions are separate stages rather than a direct binding from key to action.

Each boundary absorbs a different kind of change. The physical-to-trigger boundary absorbs platform differences: the same trigger is a key on one platform and a button on another, and nothing above notices. The trigger-to-action boundary absorbs application differences: the same trigger means different things in different contexts.

Collapsing to two layers would force one of those to be handled by duplication — either per-platform action tables or per-context physical bindings.

The cost is indirection. Tracing why a key did nothing means checking a binding at each of two stages, and a trigger bound to nothing on the current platform is silently inert.

### Triggers use gamepad vocabulary regardless of device

A trigger is a direction, a face button, or a shoulder — never a letter key — even when a keyboard is providing it.

This is a design commitment rather than a technical necessity: it means application code cannot accidentally assume a keyboard, because the vocabulary offers no way to express one. Designing against a gamepad and mapping a keyboard onto it produces schemes that work on both. The reverse — designing against a keyboard and mapping a gamepad on afterwards — reliably produces schemes needing more inputs than a gamepad has.

The cost is that genuinely keyboard-shaped input, such as text entry, does not fit the trigger model and needs a separate path.

### Actions are integers the application defines

The action layer stores integers and knows nothing about their meaning. Applications declare their own enumerations per context.

An engine-owned action enumeration would require editing a shared engine header to add an application concept, which inverts the dependency. Integers keep the engine ignorant of application vocabulary while still allowing direct-indexed lookup.

The cost is that type safety stops at the boundary: two contexts' enumerations are both integers, so querying with the wrong context's enumerator is not a compile error. It resolves to whatever action shares that index.

### Bindings are grouped into contexts, selected as a set

Bindings change wholesale rather than individually. A context is created and populated once, then activated.

Rebinding individually would leave the input system in a partially-updated state mid-change, and every screen or mode would have to remember to undo whatever the last one did. Selecting a whole set makes the change atomic and makes each context's bindings independently readable.

The cost is that contexts are static once built — there is no per-user rebinding at runtime, which a player-facing settings screen would need.

### Blocking checks fold into results, not early returns

Where a query would be denied, it returns false as part of evaluating its expression rather than returning early.

The reason is uniformity: every query answers the same question — is this action active for this caller right now — and blocking is one term in that answer rather than a separate control path. It also means adding a new condition changes one expression instead of adding a branch to every query.

### Rate and displacement inputs are paced differently against frame time

A stick reports a position that is held. Deflection means a speed, so what it contributes has to be multiplied by the elapsed time of the frame, or the same physical stick position turns the view twenty times faster at 1200 frames per second than at 60.

A mouse reports movement. The cursor is re-centred every frame, so the offset read is already the distance travelled since the previous frame and is frame-rate independent by construction. Multiplying it by elapsed time would be a second scaling of a value that has one, making a gesture of a given physical distance mean less on a fast machine — the opposite of the intent.

The two meet in one surface because the mouse is presented as a virtual right stick, which is the gamepad-first rule applied to look input: a consumer reads one stick regardless of device. Nothing in the value distinguishes them, so the consumer scales by elapsed time on the paths it knows are rates and leaves the mouse path alone. This is the one place where the layered model's device-independence is deliberately incomplete, and it is incomplete because the distinction is real rather than presentational.

What is frame-rate dependent about the mouse is its ceiling. The offset is clamped so that one erratic reading cannot whip the view around, and a clamp stated per frame is a different physical limit at every frame rate — a fast flick loses motion at 60 frames per second and loses none at 1200. The limit is therefore stated as a speed and multiplied by the frame's elapsed time, so the cap is the same physical distance per second everywhere.

Two costs follow. Deflection derived from the mouse is no longer bounded to the stick range: a long frame legitimately yields a value above one, standing for more distance covered rather than a stick pushed past its limit, and a consumer that treats it as a stick position will overshoot. And the rate constants are now expressed per second rather than per frame, so any value tuned against the old per-frame pacing means something different and has to be re-tuned rather than carried across.

### Every device is read every frame

Keyboard, gamepad and XR triggers are all evaluated each frame, and an action fires from whichever device holds its trigger.

Listening only to the most recently used device took input away from everything else. A tracked hand once claimed the active type for the life of a session, and every keyboard key — the developer escape hatch included — read nothing. Two domains in use at once, a keyboard pausing while a headset is on, need every device live.

The active input type survives for what genuinely depends on the device in hand: which button a prompt names, which on-screen controls show, and whether a stick reading is a rate or a displacement.

### A domain's input target names what receives it

Each domain has exactly one target, set by the application state: the scene, or one surface.

This replaced a single interaction mode — interface, world, or none — that answered three different questions: whether the world takes the mouse and sticks, whether the interface takes navigation, and whether the interface is alive at all. It could not say "the interface is alive while the world has input", which is what a heads-up display is, and needed a per-element opt-in to cover the gap. It also described only the desktop while being named as though it described everything, and panels in the world already sat outside it.

Naming the receiver instead makes each consumer's question direct, and makes a panel an ordinary target rather than a special case. The cost is that an application state now sets targets itself, including back again on resume.

**The window's target is held on the window surface while an XR session runs.** The desktop is then a mirror: a click only gives the window focus, and never captures the cursor into a window the player cannot see. This stands in for application states being told when a session starts and stops.

### The cursor lock follows the window's target

A surface target frees the cursor and remembers whether it was locked; a scene target puts that back. Every pausing state used to save and restore the lock itself, and a state that forgot resumed with the pointer free. Resetting the targets at a state swap discards the remembered lock, because the outgoing state's cursor means nothing to the incoming one.

### Headset actions are distinct from window actions

Navigation from the controllers binds to its own actions rather than sharing the window's. Which action fired then says which domain it came from, with no separate mechanism for the origin. Code that does not care checks both.

### Developer bindings are keyboard-based and separate

Free camera, cursor release, and similar affordances are bound to keys and registered separately from application bindings, automatically when a context is created.

They are deliberately outside the gamepad-first rule because they are not application input — they are tools, they are not shipped, and a developer always has a keyboard. Registering them automatically means every context gets them without remembering to ask.

The cost is that they occupy keys applications might otherwise want, and nothing prevents the collision.

## Limitations

- Modifier combinations are supported; ordered combinations and timing windows are not. There is no sequence or chord system.
- Contexts are fixed once created. There is no runtime rebinding and no persistence of user bindings.
- Actions are integers, so using one context's enumerator against another resolves silently rather than failing.
- Several touch pointers reach the interface at once, but there is no gesture recognition.
- Stick directions used for navigation have no hysteresis and no repeat when held, and only the left gamepad stick navigates.
- Headset navigation actions are bound but nothing consumes them yet.
- Text entry does not fit the trigger model and has no first-class path.
- Developer bindings are always registered, including in builds where they are not wanted.
- Mouse-derived stick deflection can exceed the nominal stick range on a long frame, so a consumer assuming a bounded stick position is wrong for the mouse.
- Whether a stick is carrying a rate or a displacement is not expressed in the surface. The consumer has to know from the active input type, and getting it wrong is a pacing bug rather than an error.
