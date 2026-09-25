# AGD-0180: XR Support

- **Scope:** How the engine runs in a PC-tethered headset through OpenXR: the session and its lifetime, the frame loop and its clock, stereo rendering and the desktop mirror, tracked controllers, and the engine's hands — pointing, grabbing, and the in-headset pause panel. Does not cover standalone headsets, which have no XR build, nor how the interface lays out and draws on a panel.

## Overview

- OpenXR sits entirely behind `XrManager`. No OpenXR type crosses out of the XR subsystem; poses arrive as engine vectors and quaternions.
- The desktop build finds a headset at startup and starts a session straight away. Without one it runs flat, and a headset connected later can be entered from the pause menu.
- One frame loop drives every frame, whether or not a session is live. A running session paces the frame and supplies its clock; otherwise the platform does.
- Stereo is multi-pass: the scene is submitted once per eye through the ordinary view loop. The desktop window mirrors the left eye.
- Controller buttons go through the ordinary input layers. Poses, sticks and analogue pulls are read from `XrManager` directly.
- The engine owns the hands. An XR scene adds one component and gets tracked hands, aim rays that operate interface panels, and grabbing of registered objects.

## Concepts

- **Instance** — the connection to the OpenXR runtime and the system (headset) it found. Outlives every session.
- **Session** — one period in the headset. Owns the swapchains, eye targets, eye cameras and action set. Created and destroyed repeatedly.
- **Running** — the runtime has begun the session and frames are being paced by it. Taking the headset off stops it without ending it.
- **Focused** — the session delivers button and axis input. Poses locate while visible but unfocused; buttons read nothing.
- **Eye view** — one eye's pose, its asymmetric field of view, and the targets it renders into for this frame.
- **Mirror** — the left eye's final image drawn to the desktop window.
- **Headset domain** — the input domain for everything arriving through the controllers. Its input target decides what the hands can reach.
- **Grabbable** — a scene object marked as something a hand can pick up.

## Architecture

`XrManager` owns the instance, the session, the per-eye swapchains and targets, the eye cameras and the frame loop. It is created only in a build with XR enabled, before the application initialises, so any state's initialisation can see whether XR exists. `Init` creates only the instance and finds the system; `StartSession` and `EndSession` build and tear down everything a session owns. Starting and ending are requested rather than called, and applied between frames, so nothing is destroyed under an open render pass.

`XrInput` owns the action set and the suggested bindings for the Oculus Touch, Valve Index and Khronos simple controllers. Each action is declared for both hands. Each frame it reads the action state, pushes button states into the input system as XR triggers, and holds poses, thumbsticks and analogue values for `XrManager` to republish. The action set is recreated with each session.

`CameraXrEye` builds its view from a pose and its projection from four field-of-view angles. There is one per eye, created with the session.

`RenderManager` renders a list of views. With no session the list is its own single view of the window. `XrManager` replaces it each frame with the two eye views, and the manager then mirrors the first eye to the window after the eye passes.

Three scene components carry the engine's hand behaviour, and none of them is specific to any application state:

- `XrHands` — creates a hand and an aim ray for each controller as children of its owner, submits both aim rays to the interface's pointer router every frame, and runs grab.
- `XrGrabbable` — marks its owner as grabbable. Registers itself while initialised, as a world panel registers with the pointer router, so the hands find grabbables without knowing the scene.
- `XrPausePanel` — shows and hides a world panel on the same owner as the in-headset pause menu, placing it in front of the head each time it is shown.

`XrHands` and `XrPausePanel` never pause, so pointing keeps working while the world is frozen.

## Runtime flow

**Startup.** The engine initialises, then `XrManager` looks for a headset, then the application initialises, then — if a headset was found — a session starts. The frame loop then replaces the engine's own loop.

**Each iteration of the frame loop**, in order:

1. Pending session start and end requests are applied.
2. Runtime events are polled. Session state changes drive the session: ready begins it, stopping ends the running state, exiting ends the session and returns to the desktop.
3. The input system is told whether a session is running, and the window's vsync is suspended or restored on a change.
4. With a running session: the loop waits on the runtime's frame pacing, hands the predicted display time to the frame timer, begins the frame, locates the views and publishes them to the renderer. A frame the runtime says not to render renders flat to the window instead.
5. The engine ticks one frame: input, application, scene, interface, render.
6. With a running session: swapchain images are released and the frame is submitted, with a projection layer only if every eye was drawn.

**Each eye pass** renders the scene into the engine's own multisampled, half-float target, which resolves when the pass ends. The post-process pass then tone maps and encodes that into the swapchain image the runtime handed out for this frame. Swapchain images are single-sample sRGB, registered with the graphics abstraction once when the session starts, each with a render target built up front.

**Each frame of the hands**, per controller: the hand follows the grip pose and the ray follows the aim pose; the aim ray is submitted to the router with the select trigger as its pressed state; grab runs if the Headset target is the scene. A hand pulses the controller when its ray moves onto a target and when it picks up or drops something.

**Pause**, in an application state that pauses in XR, shows the pause panel in front of the head and points the Headset target at the panel's surface. Rays then reach that panel alone, and grab stops. Anything held is let go where it is the moment the target leaves the scene.

## Working with it

**Put hands in a scene.** Add `XrHands` to one scene object at the origin. Without a running session the hands hide themselves, so the same scene runs flat.

**Make something grabbable.** Add `XrGrabbable` to it. Only an object with no parent can be picked up, so grabbables are root objects.

**Add an in-headset pause menu.** Give a scene object a world panel and an `XrPausePanel`, and register a screen on the panel's surface. On pause, show the panel and set the Headset target to its surface; on resume, hide it and set the target back to the scene.

**Bind a controller button.** Bind an action to an XR trigger in a context, as any action is bound. The engine already binds select to each trigger, grab to each squeeze, and headset navigation to thumbstick directions in every context.

**Read a pose or a stick.** Ask `XrManager` for the hand pose, thumbstick, trigger or squeeze value, or the head pose. A pose carries a tracked flag; an untracked pose holds the last good value and should not be treated as current.

**Tell whether XR is live.** Ask `XrManager` whether the session is running, and watch for it rather than sampling once: the runtime reports the session ready some frames after it is started.

## Design decisions

### OpenXR stays behind one boundary

The same rule the graphics abstraction holds for native handles. Engine types convert at the subsystem's edge, so nothing above it depends on the loader's headers and a build without XR compiles the rest of the engine unchanged.

### The instance outlives the session

Detecting a headset is separate from entering it. Detection runs once at startup, and a session is built and torn down as XR is entered and left, as often as that happens. Starting a session also retries detection, so a headset connected after launch can still be entered.

The action set is recreated with each session rather than reattached. Whether one action set may be attached to a succession of sessions is unclear in the specification, and recreating is correct either way.

### One loop drives every frame

The loop ticks the engine whether or not a session is running, using runtime pacing while one is and platform pacing otherwise.

Choosing a loop once at startup is what made entering and leaving XR at runtime impossible. A loop that ticked only while a session ran froze the window whenever the headset was set down, leaving no way to act on it.

### The frame clock comes from the runtime while a session runs

Animation has to advance to where the frame will be displayed, not to where the processor happens to be when it starts building it. The frame timer takes the predicted display time as its frame clock for as long as the session runs, and releases it when the runtime stops the session. Left in place, a clock that no longer advances would freeze everything driven by frame time for as long as the headset is off.

Profiling timestamps stay on the platform clock throughout, so the profiler never subtracts one clock from the other.

### The window's vsync is suspended while a session runs

The runtime paces a running session. A window swap that also waits for the monitor's refresh adds a second pacer at a different rate, and the frame misses the headset's deadline and halves its rate — measured at alternating single and double frame intervals on a 72 Hz headset against a 60 Hz monitor. With vsync suspended the headset holds its rate. The cost is that the mirror can tear.

### Stereo is multi-pass

Each eye is a full pass through the ordinary view loop, whose one-view case is the desktop frame exactly. That kept the change to a loop over views rather than a change to the render target model, the uniform layout and every shader.

The cost is that draw submission, state changes and anything culled run twice. Single-pass stereo would roughly halve submission cost; it needs a layered target, which multisampling does not cover today, per-view matrices moved out of the per-draw uniforms, and every vertex shader changed. Draw submission would need to be shown to be the bottleneck first.

### Buttons go through the input layers; poses and axes do not

Buttons are remappable, so they are pushed into the trigger map as XR triggers and bound to actions like any other device. A hand position has no sense in which it is remappable, and the action layer carries no value type for one, so poses, sticks and analogue pulls are read directly.

XR triggers carry hand-neutral names — primary and secondary rather than the letters printed on one controller — because the runtime does the rebinding and a binding that named a letter would be wrong on half the hardware. The menu button is left unbound for the runtime's own dashboard.

### A session not being focused is a success code

The runtime reports an unfocused session as success when actions are synchronised. A bare success test walks past it and reads actions the runtime has already declared inactive, so every button reads zero while poses keep locating — which looks exactly like a binding that never arrived. Focus is tracked explicitly and logged on change, and activity is signalled only on deliberate input, not on tracking, so a tracked hand cannot claim the active input type for the life of a session.

### The engine owns pointing and grab

Hand visuals, rays, pointer submission and grab are engine components rather than code in an application state, so every XR state gets them by adding one component. An application state decides only where the hands can reach, through the Headset input target.

**Rays are submitted every frame, and the router decides what they may reach.** Under a Scene target every registered target is offered each ray; under a Surface target only that surface's. Pointing therefore survives a pause without the hands knowing about pause.

**Grab follows the target, not the pause flag.** It is live only under a Scene target. A held object is released where it visibly is when the target leaves the scene, and resuming does not re-attach it; carrying it through a pause would move one object while the rest of the world was frozen.

**A hand is an unscaled anchor with a scaled visual beneath it.** A held object is parented to the anchor, so it keeps its own size.

### The pause panel places itself once, when shown

It appears one metre in front of the head along the head's yaw, at head height, upright and facing the player. Placing along yaw alone keeps it level when the player pauses while looking down. It stays where it was placed rather than following the head.

## Limitations

- Standalone headsets have no XR build. The Android target compiles XR out.
- What the headset shows while an application state without XR support runs is not handled. Leaving the demo state for the main menu during a session puts the menu on the window only, with nothing to point at in the headset.
- An application state is not told when a session starts or stops; it has to watch. The window's input target is held on the window surface while a session runs, standing in for that.
- Ending a session hands the display back to the runtime, which shows its own home environment. What appears in the headset after leaving XR is not the engine's to choose.
- Interface objects are reached by ray only. Direct touch — a contact test at the controller, press by depth — does not exist.
- Headset navigation actions exist but no surface consumes them; there is no confirm action for the headset.
- Multi-pass stereo doubles draw submission, state changes and culling.
- The mirror shows the left eye only, and can tear while vsync is suspended.
- The pause panel appears only if a session is running when pause begins. Entering XR while already paused does not show it.
- The loader is linked against the static runtime library like everything else; a stock loader built for the dynamic runtime does not link.
