# AGD-0001: Getting Started

- **Scope:** A map of the engine and of this folder. What the subsystems are, who owns whom, and which document to open next. Does not describe any system in depth; every section here points at the guide that does.

## Overview

- ChillCore is a realtime 3D rendering and interaction engine, running on Windows and Android from one source tree.
- The engine is a set of subsystems owned by one object; the application is a separate object implementing a three-method interface.
- Almost everything is reached through a static accessor rather than being passed around.
- Platform and graphics differences are absorbed by two abstractions, so application code is written once.
- The numbered guides in this folder each cover one system. This document exists so you know which one to open.

## Concepts

Five terms recur throughout the guides and are worth having before reading any of them.

- **Subsystem** — a long-lived manager created at startup and living for the whole process. Rendering, input, audio, and the scene are subsystems.
- **App state** — a named unit of application behaviour owning its own scene, cameras, screens, and input bindings. One is active at a time.
- **Component** — an attachable behaviour on a scene object. Rendering is a component, not a special case.
- **Handle** — an opaque identifier for a resource owned by the graphics backend. Nothing above the graphics abstraction sees a native graphics type.
- **Frame** — one pass through a fixed sequence of phases. The order of those phases is deliberate and load-bearing.

## Architecture

The engine divides along two lines: engine against application, and portable against platform-specific.

**Engine against application.** `CoreMain` owns every engine subsystem and the frame loop. The application supplies one object implementing `IAppMain` — initialise, update, shut down — and the engine knows nothing else about it. Engine code lives in the `CC` namespace; application code has none. Below that interface, `AppMain` owns a `StateMachine`, which owns the app states.

**Portable against platform-specific.** Two abstractions absorb the differences. The platform layer covers the window, the file system, and physical input. The graphics abstraction covers everything a graphics API does. Between them, application code and the cross-cutting parts of the engine compile unchanged on every target — which was tested when Android was added, and held.

Ownership, top down:

```
platform entry point          per target; owns the engine's lifetime
  └── CoreMain                owns every engine subsystem and the frame loop
        ├── platform layer    window, file system, input backends
        ├── rendering         RenderManager over Gfx::RenderApi over a backend
        ├── scene hierarchy   objects, transforms, components
        ├── input             physical devices to triggers to actions
        ├── UI                screens, layout, text
        ├── audio             playback and mixing
        └── frame timer       delta time, phase timing, profiling
  └── AppMain                 the application, behind IAppMain
        └── StateMachine      one active app state at a time
```

Two things about this shape are worth knowing early, because they explain a lot of the code:

- **Subsystems are singletons.** They are reached by static accessor from arbitrary depth, not threaded through call signatures. Construction and destruction order is centralised in `CoreMain`.
- **Renderables submit themselves each frame.** There is no persistent draw list. A component that stops updating stops drawing, which is why pausing and disabling behave the way they do.

## Design decisions

The decisions below shape every system. Each is argued properly in its own guide; they are collected here because they explain the engine's overall character.

### The engine depends on an interface, never on the application

The engine compiles and is reasoned about without any application present, and the application can be replaced without touching engine code. Everything application-shaped lives behind that boundary.

### Platform differences are confined to the platform layer and the graphics abstraction

Platform-specific code lives in two places. The platform layer provides interfaces for the window and its graphics context, for file access, and for input devices. The graphics abstraction provides the drawing interface. Each has one implementation per target. Where a feature is available on one target and not another, the backend reports its availability as a capability value, and callers branch on that value at runtime. Conditional compilation is not used for platform or feature differences above these two layers, so engine and application code is written once and built for every target.

### Subsystems are reached, not passed

Rendering, input, and audio are needed at arbitrary depth — inside components, screen controllers, app states. Threading references through every intermediate layer would dominate the code, so each subsystem exposes a static accessor instead. The real lifetime risk, construction and destruction order, stays centralised where it can be seen.

### Behaviour is composed, not inherited

A scene object is a node with a list of components rather than a base class to specialise. Rendering is one component among many, so objects with no visual representation need no special case.

### Frequently-changing and rarely-changing data are separated

The recurring pattern in the rendering path: group data by how often it changes, upload each group at its own rate, and bake what never changes into an object created once. It is why pipelines exist, why uniforms are grouped by update frequency, and why the draw loop sorts.

### Any app state can be the first one

Booting directly into a work-in-progress state is a one-line change, which requires that nothing shared is initialised by a particular state. Global setup happens before the first state activates.

