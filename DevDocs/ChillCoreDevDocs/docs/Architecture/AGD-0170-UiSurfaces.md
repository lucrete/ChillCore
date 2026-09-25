# AGD-0170: UI Surfaces

- **Scope:** How the interface serves several destinations at once — the desktop window and any number of panels standing in the world — and how pointers reach them. Covers surface ownership and lifetime, the world-panel component, pointer routing and multi-pointer interaction, and redraw gating. Does not cover how a screen is laid out or drawn, nor how screens are registered and navigated.

## Overview

- A **surface** is one complete interface: one element tree, one identifier map, one stylesheet, one layout state, one size, one screen stack, and its pointers.
- The engine owns the window's surface. Application code never branches on whether a window exists.
- A panel in the world is a scene component. It owns a surface, the offscreen target behind it, and the quad that shows it.
- A surface accepts several pointers at once, each with its own hover and press. Two hands drive one panel independently.
- A world pointer reaches a surface through a router that takes the nearest hit-testable target. The router holds no geometry.
- A panel redraws only when something changes. A static panel nobody is pointing at submits no work at all.

## Concepts

- **Surface** — one interface and everything that is per-interface. Created with an intent and a size; it allocates and formats its own destination.
- **Surface kind** — window (draws into whatever the frame has bound), offscreen (owns a target and hands out the texture), or null (real, with nothing behind it).
- **Output texture** — an offscreen surface's result. What a panel's quad samples. The target that produced it is the surface's own business and is not exposed.
- **World panel** — a scene component pairing a surface with a quad in the world.
- **Pointer** — a position, a pressed flag, and whether it is on the surface at all, in surface-space pixels.
- **Pointer source** — where a surface's pointers come from: the platform, or whatever is pointing at it.
- **World-focus reachable** — an element a pointer reaches while the world, rather than the interface, has focus. Declared per element and off by default.
- **Pointer target** — something a world ray can reach. Answers whether a ray reaches it and at what distance, then what the hit means.
- **Content dirty** — a surface has something new to draw.

## Architecture

`UiManager` owns every surface and creates them on request. It holds the window's surface from construction and the pointer router. It parses markup into element trees; a tree belongs to no surface until it is loaded onto one.

`UiSurface` holds everything that used to be global: the tree, the identifier map, the active stylesheet, the layout state, the size, the screen stack, the input handler and the target. Its screen stack is an instance rather than a singleton, so every surface has one and the calls on it are identical everywhere.

**Every element carries the surface it is loaded on.** That is how a setter deep inside a controller marks the right panel for redraw without anything having to know which surface is current. The pointer is assigned when a tree is loaded and when a dynamically-built subtree is styled.

**Controllers are bound to their surface at registration and never rebound.** A controller resolves elements and callbacks against it, so the same controller class works on the window and on a panel with no change: a pause menu transitions the window on the window and a panel on a panel.

`UiWorldPanel` is a scene component and a pointer target. It creates its surface, its material, and the quad that shows it, and registers itself with the router. Its size in metres is its owner's transform scale; the surface knows only pixels. It is factory-registered, so panels are authorable in scene files rather than hardcoded per state.

`UiPointerRouter` compares distances and nothing else. Rays are submitted for a frame, then resolved together, so every target has been offered a ray before the nearest is chosen. A pointer that submits nothing releases whatever it was on.

## Runtime flow

Per frame, in order:

1. **Screen stacks update**, every enabled surface, driving its own transition and fade.
2. **Pointers resolve.** Each submitted ray goes to the nearest target that answered it; each target converts its hit into surface pixels and hands the result to that surface's input handler.
3. **Surfaces update.** The window re-reads its size from the platform. Each surface runs its input handler over all its pointers.
4. **Offscreen surfaces render**, each into its own target and only when dirty. This runs before the scene, because the scene samples what they drew.
5. **The scene renders.**
6. **The window surface draws** directly into whatever the frame has bound.

**Pointer handling is per-pointer and then combined.** Each pointer independently hit-tests, presses and releases; a press stays with the pointer that began it. Element state is then derived across all of them: pressed if any pointer presses it, hovered if any pointer hovers it, normal otherwise. A pointer that leaves the surface releases what it was holding rather than leaving it stuck pressed.

**The mouse is pointer zero and extra fingers take the slots above it.** Both go through the same path from there, so multi-touch is not a case of its own.

**Directional navigation and cancel stay on the surface reading the platform.** They are one input stream with no spatial origin, so only one surface can consume them. Panels are pointed at.

**While the world has focus, a pointer reaches only the elements that declare they belong there.** Everything else needs full interface focus. An external pointer is exempt: it exists because something is deliberately aiming at a surface, which is interface focus by definition whatever the desktop is doing.

**Redraw marking is automatic**, in the element setters and in the input handler's hover and press transitions. A transition marks every frame it runs, because a fade changes every frame.

## Working with it

**Put a panel in the world.** Add the world-panel component to a scene object, in the scene file or in code. Give the object the transform that is the panel's position and its size in metres. Register screens on the component's surface exactly as they are registered on the window's.

**Hide a panel.** Disable the component. Its update, hit test and render all stop, and its target stays allocated. Destroy only what is genuinely gone.

**Reach the window's interface from application code.** Ask the manager for the window surface. Everything that used to be a call on the manager is a call on a surface.

**Reach a surface from a controller.** Ask the controller for its own surface. Never assume which one it is.

**Drive a panel from a world pointer.** Submit a ray to the router each frame with an identifier for the hand. The router decides which panel it reaches; nothing on the submitting side knows where a panel is.

**Add a new kind of pointer target.** Implement the target interface. A reactive three-dimensional element implements it against its own geometry and drives its own hover and press with no surface involved.

## Design decisions

### The render target is hidden from whoever creates a surface

Creation states intent — a size, and what kind of surface it is — and the surface allocates, resizes and formats its own destination. What comes back is an output texture, which is the result rather than the target.

Exposing the target would put its format, its clear behaviour and its lifetime in the hands of every caller, and every caller would get them subtly differently. The one thing a caller genuinely needs is the texture a quad samples, and that is what is returned.

### The engine owns the window's surface

The interface is an engine feature rather than something an application assembles, so the window's surface exists from engine startup.

Where no window exists, the engine returns a surface that is real and has nothing behind it: calls succeed and draw nothing. The alternative — returning nothing and making every call site check — puts a branch on the target in application code, which is exactly what the surface abstraction is for.

### The window is one surface, and keeps drawing directly

Main menu, pause menu and heads-up display are screens on it and stay mutually exclusive. Opening a menu hides the display because they share a stack, not because anything hides it.

Caching the window behind a texture was rejected: it would swap its draws for a fullscreen blend costing the same however simple the interface is. That is worse for a sparse display and better only for something as dense as the tracker grid — not a decision worth making before there is something to measure.

### A panel is a scene component

Placement, parenting, lifetime and hiding are then the scene's answers rather than new ones, and being factory-registered makes panels authorable in scene files instead of hardcoded per state.

Its size in metres belongs to its transform, so a panel is moved, turned and resized like anything else in the scene. The surface behind it is described in pixels only, and the two are related solely at the moment a hit is converted.

### What registers with the router is a hit-testable target, not a plane

A ray can reach several panels, so something must take the nearest. That is all the router does; it holds no geometry.

The target interface answers whether a ray reaches it and at what distance, and then what the hit means. This is deliberate and the reason is an interface element that is a reactive three-dimensional mesh rather than a region on a flat panel. Such an element implements the same interface against its own geometry and drives its own hover and press with no surface involved. A router that knew about planes would have to be rebuilt for it. **Do not simplify the target interface back to a plane.**

### Rays resolve together, after every one is submitted

Submission records; resolution dispatches. A ray dispatched the moment it was submitted could only be compared against targets already seen, which makes the nearest depend on registration order.

### Several pointers per surface, with element state derived across them

Each pointer keeps its own hover and press, so two hands can use one panel and a press stays with the hand that began it. An element is hovered if any pointer hovers it.

Tracking a single hovered and pressed element was what made multi-touch a special path: extra fingers could only reach joysticks, because anything else would have fought over the one slot. With per-pointer state that restriction disappears and the platform's fingers, the mouse and a controller ray are all the same thing.

### An element declares whether it is reachable while the world has focus

A heads-up display is drawn over a scene the pointer is also driving — in that mode a click is a camera look as much as a press. So elements are unreachable there unless they say otherwise, and the ones that say otherwise are those whose whole purpose is to be operated during play: an on-screen stick, a pause control on a device with no keyboard.

The alternative, keying it off element type, is what this replaced. Allowing only sticks through made every other on-screen control silently inert — drawn, hovered by nothing, pressed by nothing — and the failure was invisible, because the control looked right and simply never fired. Type is also the wrong question: two buttons in the same display can want opposite answers.

Declaring it per element keeps the guard while making the exception explicit and greppable. The cost is that a new always-available control has to remember to opt in, and the symptom if it does not is a control that draws and does nothing.

### Directional navigation stays on the window

It is one input stream with no spatial origin, so only one surface can consume it. Panels are pointed at.

This forecloses driving a panel without pointing at it, and is deliberately a subset the other answer can grow from.

### Panels redraw only when something changes

A panel's texture persists otherwise, so a static panel nobody is pointing at costs nothing. That is the whole reason panels are affordable in quantity.

**Marking is automatic**, in the element setters and in the input handler's hover and press transitions. A missed mark shows as a panel frozen on stale content, which is quiet and slow to find. Left to callers it would reproduce what explicit layout invalidation already demonstrates, where the call sites that exist remember and anything added later will not.

An offscreen surface renders when dirty even with no screen loaded, because the pass is what clears its target. A panel that had never been cleared would show whatever its texture was allocated over.

### The window is exempt from redraw gating

It draws directly rather than into a target, so skipping a frame's draw would show whatever the scene left behind rather than the previous interface. Gating only applies where there is a texture that persists.

## Profiling

GPU scopes are named for the surface that emitted them and group under one phase, so any number of panels leaves the phase budget alone and adding or removing one does not discard the graph's history, while a capture still names each. **A clean panel emits no scope at all**, so the detail names vary between frames.

CPU timing stays aggregate. The standard phases are a fixed set and the graph holds a small number of them, which a surface count that changes at runtime cannot fit. Interface update and interface render mean all surfaces together.

## Limitations

- The layout engine's scale factor is a single value set at the top of each layout call. It is safe while surfaces lay out one after another, and stops being safe the moment two do so at once.
- Everything that scales against a surface must read its size from the surface. Text sized against the window while its rectangles were laid out against a panel is what made a panel's text vanish.
- Per-surface CPU timings do not exist. When the question becomes "which panel is redrawing when it should not", that is when to add them.
- A world panel's plane is rebuilt from its owner's transform during the scene update, so a panel moved after that point is hit-tested against where it was for one frame.
- A surface's screen stack is its own. Two panels showing the same markup register it on each; there is no shared screen definition.
- A control that opts into world focus but is only shown for one input type has two independent conditions to get right, and the failure of either is a control that draws and does not respond.
- Interface elements are regions on a flat surface. The pointer target interface is shaped so a reactive three-dimensional element does not require the router to change, but no such element exists.
