# XR Plan

**Status:** M1 landed and verified. M2 and M3 are written and build. M4 is mostly complete: the demo state, ray, grab and world-space panel are in, and the panel path is verified flat. M5 is written and builds, including the in-headset panel rebuilt on its own surface so it and the desktop pause menu coexist; the flat path and clean shutdown are verified, and everything needing a live session is not. What can be exercised without a headset is verified: the loader reaches the runtime, the flat fallback is unchanged, and the demo state boots and runs with its panel drawing. Everything downstream of a live session is unverified pending a connected headset.
**Current state:** AGD-0070 (Rendering Pipeline), AGD-0080 (Graphics API Abstraction), AGD-0030 (Platform Layer) and AGD-0110 (Input System) describe the seams this work plugs into. None of them cover XR.
**Scope:** PCVR on the desktop target — Valve Index through SteamVR, Meta Quest 2 over Link and Air Link, both through the GL 4.3 backend. Quest standalone over GLES is not in plan and is not covered here.

Delivered as four milestones. M1 is engine work with no XR dependency and stands on its own.

---

## Architecture

### New subsystem

```
Code/Core/Xr/
├── XrManager.cpp/h        # Instance, system, session, session-state machine, frame loop
├── XrSwapchain.cpp/h      # Per-eye swapchain, image registration, target selection
├── XrViews.cpp/h          # xrLocateViews, converted to engine types
├── XrInput.cpp/h          # Action set, interaction profiles, poses, haptics
└── XrTypes.h              # TrackedPose, XrHand, XrPoseKind
```

**No OpenXR type crosses out of `Code/Core/Xr/`.** The same rule the graphics abstraction holds for `GLuint`. `XrPosef` and `XrFovf` convert to `Vector3`, `Quaternion` and tangent floats at this boundary.

### Frame loop

The Android shell already drives `CoreMain::TickFrame` from a foreign loop. XR uses that same seam, so `CoreMain` is untouched.

`Code/Targets/Desktop/MainDesktop.cpp` selects a driver:

- `XrManager::Init()` succeeds — `XrRunLoop(coreMain, appMain)`.
- Otherwise — `coreMain->Run(appMain)`, the existing flat desktop path.

**Chosen once, at startup, which sessions starting and stopping at runtime makes untenable.** The two collapse into a single adaptive loop that uses XR pacing while a session is live and platform pacing otherwise.

`XrRunLoop` per iteration:

1. `xrPollEvent`, then the session state machine.
2. `xrWaitFrame`, `xrBeginFrame`.
3. `xrLocateViews`; publish eye views and hand poses.
4. `coreMain->TickFrame(appMain)`.
5. `xrEndFrame`.

Swapchain image acquire, wait and release go in `RenderApiOpenGl::BeginFrame`, currently a no-op whose comment reserves it for exactly this.

`FrameTimer::FrameStart` reads `glfwGetTime`. It gains an optional external time source so delta time comes from the predicted display time returned by `xrWaitFrame`. Head-pose prediction is only correct against that clock.

---

## M1 — Foundations

No XR dependency. Every item is a standalone engine improvement, and the whole milestone is verified by the desktop app behaving exactly as it does now.

### Camera: separate view and projection, asymmetric frustum

`CameraBase` stores only the composed `viewProjection`. `UpdateViewProjectionMatrix` hardcodes a 75 degree field of view, near 0.1 and far 100, and reads aspect from the window rather than from the target being rendered into.

- `UpdateViewProjectionMatrix` becomes `virtual`.
- `CameraBase` gains protected `view` and `projection` members with const accessors.
- Field of view, near distance and far distance become members with setters, defaulted to the current literals.
- Aspect comes from a per-camera viewport size, defaulting to the window size, so existing cameras are unchanged.
- `Mat4x4` gains `Frustum(left, right, bottom, top, nearDistance, farDistance)` beside the existing `Perspective`. This is what an XR field of view's four angles map onto.

`Mat4x4::FromQuaternion` and `Quaternion::Inverse` already exist, so a pose-driven view matrix needs no new maths beyond the frustum.

### RenderManager: a view loop

Stereo is **multi-pass** — one full submission pass per eye. See "Multi-pass versus single-pass stereo" below for the comparison and the reasoning.

The scene renders exactly once into one target, and the render pass opens in `StartFrame` before renderables have submitted.

- `CC::RenderView` carries a camera, a scene target, an output target, and a size.
- `BeginRenderPass` moves out of `StartFrame` into `Render`. `StartFrame` keeps the backbuffer reconfigure, the list reset, and target sizing.
- `Render` iterates a view list. Per view: activate the camera, upload frame uniforms, begin the pass, draw opaque then transparent, end the pass, run the post-process pass into the view's output target.
- The default view list holds one entry — the backbuffer and the existing post-process target — so behaviour is unchanged.
- `PostProcess::Execute` hardcodes the backbuffer as its destination. The destination becomes a parameter.
- `EnsurePostProcessTarget` becomes per-view and sized from the view rather than from the window framebuffer.

The renderable list is collected once per frame before `Render` and consumed per view at no extra cost. `Material::PreRender` re-reads the active camera's view-projection per draw, so a per-view camera swap propagates with no change there.

### Platform: native graphics binding

`PlatformWindow` exposes no graphics context. The GLFW window handle is private with no accessor. An OpenXR graphics binding needs the device context and the render context.

```cpp
struct NativeGraphicsBinding { void* deviceContext = nullptr; void* renderContext = nullptr; };
virtual bool GetNativeGraphicsBinding(NativeGraphicsBinding& outBinding) const;
```

- Desktop implements it against the vendored `glfw3native.h`.
- The default returns false, so Android compiles unchanged and its EGL fill-in is a later one-file change.
- The window config requests GL 2.0 and relies on the driver returning a compatibility context. OpenXR runtimes check the version reported by the graphics requirements query, so the request rises to 4.3 core to match what the backend claims.

### Platform input: hold the window handle

`PlatformInputGlfw` resolves its window through `glfwGetCurrentContext()` on every query. An OpenXR runtime may make another context current, after which keyboard and mouse fail silently. Store the window pointer at init.

**Done when:** the desktop app builds and runs with no visual or behavioural change, driven through the one-entry view list.

**Landed.** All four items are in. The frame renders through a one-entry view list, `CameraBase` exposes view and projection separately with per-camera field of view, near, far and viewport, `Mat4x4::Frustum` is available, `PlatformWindow::GetNativeGraphicsBinding` supplies the WGL handles, and `PlatformInputGlfw` holds its window rather than resolving it through the current context.

---

## M2 — Session and stereo rendering

### Adopting an externally-owned texture

OpenXR owns its swapchain images. Texture creation always generates its own name, and handles carry no route back to a native id.

Add to `Gfx::RenderApi`:

```cpp
[[nodiscard]] virtual TextureHandle RegisterExternalTexture(const ExternalTextureDescription& description) = 0;
```

- `ExternalTextureDescription` carries a `uint64_t` native handle, format, width, height, layer count, sample count and debug name. A `uint64_t` keeps the native type out of the header, and the same shape serves a Vulkan image or a D3D texture later.
- The backend pool entry gains an ownership flag. Destroying an unowned texture frees the pool slot without deleting the underlying object.
- `supportsExternalTextures` joins the capability set, branched on at runtime rather than compiled around.

Everything downstream already works through handle indirection: render target creation, the pass brackets, the multisample resolve, and per-attachment array layer selection.

### Swapchains and eye targets

- One swapchain per eye. Two swapchains rather than an array swapchain — simpler, and both runtimes support it.
- Register every swapchain image once at creation and build a render target per image up front. Index by the acquired image index each frame; nothing is allocated per frame.
- Swapchain images are single-sample. The scene renders into the engine-owned multisampled offscreen target as it does today, and the post-process pass resolves and encodes into the swapchain image. This is the existing structure, run per eye.
- Eye views take their size from the swapchain. The backbuffer size falls back to the window framebuffer, and eye views must not inherit that.

### Eye cameras

`CameraXrEye` derives from `CameraBase`, takes a pose and four field-of-view tangents, and builds its view from the pose quaternion and inverse translation and its projection from `Mat4x4::Frustum`. Registered with `CameraManager` by name and activated per view pass.

Reference space is stage, falling back to local where a stage is not configured.

### Desktop mirror

After the eye loop, the left eye colour texture blits to the backbuffer through the existing fullscreen blit pipeline. UI, text and the developer overlay then draw over it exactly as they do now, and the existing frame-end swap is untouched.

### Build

- Vendor the OpenXR SDK as `Code/Packages/openxr-<version>/{include,lib-vc2022}`. Lowercase upstream name, matching every existing vendored package rather than the PascalCase folder rule, which governs what ChillCore authors.
- **The loader must be built for the static CRT.** Static CRT linking is mandatory engine-wide and stock loader binaries are built against the dynamic CRT. Build the loader static from the SDK with the runtime library matched and vendor the result, the same treatment the GLFW static-CRT variant already gets. Fallback if that proves awkward: add the SDK loader as a subdirectory with the runtime library inherited.
- The desktop target gains one link directory and the loader library.
- The root build gains a `CC_ENABLE_XR` option, on for desktop and off under the GLES backend, following the existing option-to-compile-definition pattern, plus a source filter for `Core/Xr/` alongside the GLES filter. Sources are globbed, so no per-file build edits.

**Done when:** an existing scene is viewable in stereo with head tracking on both the Index and the Quest 2 over Link, the desktop mirror shows the left eye, and launching with no XR runtime present falls back cleanly to the flat desktop path.

---

## M3 — Controllers

### Poses are a new value type

Nothing in the input system carries a pose — it is bool, float axis and 2D pixel throughout.

```cpp
struct TrackedPose { Vector3 position; Quaternion orientation; bool isTracked = false; };
enum class XrHand { Left, Right, Max };
enum class XrPoseKind { Grip, Aim, Max };
```

`XrInput` owns the action set and exposes hand poses, thumbstick values and haptic output. The precedent to imitate is the joystick override already on `InputManager` — a non-hardware source writing values that consumers read device-agnostically.

### Buttons through the existing three layers

- **Triggers.** The trigger enumeration gains an XR block: trigger, squeeze, thumbstick click, the four face buttons and menu, per hand. An XR evaluation path and a current-state array join the keyboard and gamepad pair. The active input type gains an XR value.
- **Actions.** No change. Actions bind to trigger ids and do not care which device produced one.
- **Poses are not triggers.** Anything needing a transform reads `XrInput` directly.

### Interaction profiles

Suggested bindings for the Valve Index controller, the Oculus Touch controller, and the Khronos simple controller as fallback. The runtime performs the rebinding; the engine holds only action names.

**Done when:** two controller-shaped scene objects track the hands on both headsets, buttons drive engine actions through the existing action map, and haptics fire.

**Written, unverified.** `XrInput` owns the action set: ten actions, each declared for both hands through subaction paths, with suggested bindings for Touch, Index and the simple controller. Buttons are pushed into a staging array on the trigger map and taken up by its Update, so edge detection survives; poses, sticks and analogue pulls are read from `XrManager` directly. `ActiveInputType::Xr` outranks the others while a session holds input.

Nothing yet consumes any of it. The first two criteria need the demo state, so they are verified together with M4 rather than on their own.

**One trap, already hit and fixed.** `xrSuggestInteractionProfileBindings` **replaces** a profile's bindings rather than adding to them. A second call for the same profile silently discards the first — every binding a profile has must go in one call.

---

## M4 — Interaction demo

**Mostly complete.** What remains of it is the input-ownership question M5 answers, not the interaction work itself.

`AppStateXrDemo` under `Code/App/AppStates/`, registered in `AppMain::Init` with one line. Follows the showcase state's shape: camera install, scene init, action context, UI screen, interaction mode.

**Superseded in one respect.** This milestone assumed the world-space panel is the only UI and that the state can simply declare `InteractionMode::World` to get it. That is wrong on the desktop: World mode makes `IsUiInteractable()` false, so the on-screen menu becomes unclickable, while `CameraFree` turns the same click into a cursor lock. One line produces both faults. M5 replaces the assumption — the desktop mode is derived from whether a session is live, not asserted by the state.

### Ray pointer and grab

- A line renderable from each aim pose.
- A trigger press grabs the nearest grabbable scene object within a radius. The object's transform is reparented to the hand for the hold and restored on release.
- Hover and grab each fire a short haptic pulse.

### World-space UI

UI, text and the developer overlay draw to the default framebuffer with no render pass, so they cannot reach an eye swapchain unmodified.

**The UI renders once into an offscreen target and is drawn as a quad in the scene.** Stereo-correct for free, no per-eye cost, and no change to the layout engine.

- `UiRenderer::BeginFrame` already takes a width and height and builds its own orthographic projection, so a panel-resolution target needs no projection change.
- The resulting texture draws on a quad with an unlit material, placed in the scene, so it flows through the ordinary per-view path.
- **Pointer input.** `UiInputHandler` reads the platform mouse directly. It gains a pointer state — position, pressed, active — fed by the mouse by default and by an XR pointer in the headset. Hit testing already works in panel-space pixels, so the XR side only intersects the aim ray with the panel plane and converts the hit to pixels.
- **The developer overlay stays on the desktop mirror only.** Out of scope in-headset.

**Done when:** the demo state boots directly as the initial state, objects can be picked up and released with either controller, and a world-space UI panel responds to the controller ray. Nothing here waits on M5; the two are independent except that M5 removes the interaction-mode line this state currently sets.

**Written, partly verified.** `AppStateXrDemo` is registered and boots directly. Verified flat, with no headset: the scene builds, the panel target is created, the screen renders into it, and the quad in the scene samples it — the whole panel path works without a session, and the state reports itself inactive rather than failing. Unverified: hands, ray, grab, pointer and haptics, all of which are gated on a live session.

**Three engine additions the milestone needed.** Two have since been replaced by the surface work in M5; the third stands.

- `UiManager::RenderToTarget` drew the active screen into an offscreen target. Replaced: a surface owns its target and the UI manager renders every offscreen surface before the scene.
- `UiPointerState` on `UiInputHandler` replaced the mouse as the pointer source. Kept as the pointer type, but there is no longer an override — a surface holds several pointers at once and whoever points at it supplies them.
- `Material::SetTextureHandleOverride` binds a texture the material does not own at unit 0. `Texture` only ever loads from a file or from memory, and a render target's colour attachment is neither. Unchanged, and how a panel's quad samples its surface.

**Noted, not fixed.** `Vector3`'s arithmetic operators are not const-qualified, so a `const Vector3&` cannot take part in one. The demo copies before use. Worth fixing in the maths header, but not as part of this work.

---

## M5 — Session lifecycle and desktop mode

Who owns input, and when. Every fix in the milestones above worked around the absence of an answer: the pointer override that implies UI focus, the panel surface, the state asserting `InteractionMode::World`. This states it once.

### The desktop is neither, while XR has input

`InteractionMode`, `IsUiInteractable` and `IsWorldInteractable` stay desktop concepts. They gain a third state, in which both queries answer false, because a session that owns input leaves the desktop driving nothing.

| XR session | Paused | Desktop input | Desktop shows | Headset |
| --- | --- | --- | --- | --- |
| active | no | inert | eye mirror plus a "Tracking VR" HUD element | live, tracking |
| active | yes | Ui, cursor shown | eye mirror plus the pause menu | live, tracking, world frozen |
| inactive | no | the state's own Ui or World | the application as it is without XR | — |
| inactive | yes | Ui, cursor shown | the pause menu | — |

The desktop mode is **derived** from those two facts rather than stored beside them. A state that sets its own mode unconditionally is what produced the fault M4 records.

Pause never stops XR. The headset keeps rendering and tracking while the desktop becomes interactive, and the mirror stays — it is the only way to see what the headset sees without wearing it.

### Pause stops the world, never the camera

`SceneHierarchy::SetPaused(true)` gates world update, and camera update runs from the rendering frontend independently of it, so the behaviour needs no new machinery.

**Not `SetEnabled(false)`.** Renderables submit to the render list from their own component update, so disabling the hierarchy stops the scene being drawn at all rather than stopping it moving — the window goes blank behind the menu. Pausing skips only the components that declare themselves pauseable, and renderables do not.

**This rule is load-bearing and outlives this milestone.** A frozen scene that still tracks the head is comfortable; a scene that stops tracking while the head moves is the standard way to make someone ill. Anything added later that pauses, including a head-following menu, keeps camera update running.

**Pause is bound to the secondary face button**, either hand — B on the right controller, Y on the left Touch, B on both Index controllers. It matches the platform convention where the second face button is back-or-menu and the first is confirm, and it leaves the first free for interaction. The menu button stays unbound and reserved for the runtime's own dashboard.

### Sessions start and stop at runtime

The largest piece of work, and a restructure rather than an addition.

- **The instance outlives the session.** Setup currently runs instance, system, session, swapchains and input in one call. Detecting XR without entering it means splitting the instance and system from everything a session owns.
- **Sessions are created and destroyed repeatedly.** Swapchains, eye targets and eye cameras are session-owned and rebuilt each time.
- **One adaptive loop.** The entry point currently chooses the XR loop or the flat loop once, at startup, which runtime toggling makes untenable. A single loop uses XR frame pacing while a session is live and platform pacing otherwise. The loop already ticks frames either way, so the decision moves inside it rather than a second loop appearing.

**Verify before designing around it:** action sets are created on the instance but attached per session, and attachment is irreversible for that session. Whether one action set may be attached to a succession of sessions, or must be recreated for each, decides whether the action set is instance-lifetime or session-lifetime.

**Ending a session hands the display back to the runtime**, which then shows its own home environment. What appears in the headset after Exit XR is not ours to choose; the alternative is holding the session open on a blank layer, which keeps the whole XR path alive to display nothing.

### Entering and leaving XR

- **On boot, enter XR if a session is available.** No chooser at launch.
- **The choice lives in the pause menu**, as a submenu beside Options. Reached by pausing and navigating in, or later by booting straight to the menu with that submenu already selected.
- **This is the affordance that must work from any state that supports XR.** A launch-time chooser would conflict with booting directly into any AppState; a pause-menu entry does not, because every state can pause.

- **Whether a state supports XR is the state's own answer**, a flag on `StateMachineState` defaulting to false. A state that has not been authored for stereo, a tracked head and a scene in metres does not offer XR, and the pause menu hides the entry rather than offering something that would put the user in a headset looking at a window's content. The AudioTracker is the clear case; a state answering true also registers the XR screen its pause menu routes to.

The existing screens carry the pattern already: the pause menu routes a button to a separately registered Options screen through `UiScreenSystem`, and its controller takes a callback per button. The XR submenu is one more screen, one more button, one more callback.

### Not in this milestone

A pause menu inside the headset. Until it exists, pausing while wearing the headset shows the menu on the desktop only, so acting on it means taking the headset off. The head-following popup comes later and inherits the camera-update rule above.

**Done when:** the application boots into XR when a headset is present and into the ordinary desktop path when it is not; pause shows the desktop menu with a working cursor from either mode without interrupting the headset; and XR can be left and re-entered from the pause menu repeatedly, with the desktop returning to normal interaction each time it is left.

**Written, partly verified.**

- `InteractionMode` gains `None`, and the effective mode is derived by `InputManager` from a state's preference plus two pushed facts — whether XR owns input and whether the app is paused. Paused outranks XR, so the menu is always reachable. `IsHudInteractable` answers false in `None` too, or the HUD would keep taking input the desktop no longer owns.
- `XrManager::Init` now creates only the instance and system. `StartSession` and `EndSession` build and tear down the session, swapchains, eye targets, cameras and action set, and are driven by requests taken up between frames so nothing is destroyed under an open pass. The action set is recreated per session rather than reattached, which is correct whichever way the spec question falls.
- One loop drives every frame whether or not a session is live, and `StartSession` retries detection, so a headset connected after launch can still be entered.
- The runtime asking to exit now ends the session and returns to the desktop instead of quitting the application.
- Pause is on Escape, gamepad Start, and the secondary face button of either controller. It pauses `SceneHierarchy` and leaves the cameras running.
- The XR submenu is registered in `AppMain::Init` rather than per state, because it carries no per-state callbacks and has to reach any state that can pause. The pause menu routes to it exactly as it routes to Options, so no state wiring changed.

**Verified:** builds clean; boots flat with no headset and drives the frame through the adaptive loop; exits with code 0 on a window close. **Unverified:** everything needing a live session — entering and leaving XR, the paused-in-XR desktop mode, and the controller pause binding.

**One HUD, not two.** The window's screen is the application HUD in every unpaused case, and it carries a tracking label that shows itself whenever the effective mode is `None`. Nothing new was added to the screen system: a second HUD, and an API for showing no screen at all, were both written and then removed once the shared HUD covered the case.

**The world panel and the desktop menu now coexist.** The panel was rebuilt on its own surface rather than repaired in place, once the UI could hold more than one. The panel is a `UiWorldPanel` component on a scene object, owning its surface, its target and its quad; its screen is registered on that surface, and the window's HUD, pause menu and options are registered on the window's. Pausing changes the window's screen and leaves the panel showing what it was showing.

Three workarounds went with it rather than being kept:

- **The pointer override is gone.** Each hand submits its aim ray to the UI's pointer router, which takes the nearest hit-testable target and hands that target the hit. Nothing in the demo knows where a panel is, and the haptic pulse on crossing an edge reads what the router decided rather than intersecting a second time.
- **The panel-surface switch is gone.** There is no "current surface" to switch; the window and the panel each lay out and hit-test against their own size.
- **The render-to-target call is gone.** Offscreen surfaces are rendered by the UI manager before the scene that samples them, so the demo no longer drives a UI render from inside its own update. The panel is no longer a frame behind.

**Found on the desktop pass.** Leaving the demo through its own pause menu left the input manager paused, and paused outranks the interaction mode, so the next state's free camera silently took no mouse input. The state machine now clears both pause flags at a swap, because the exit path for a state that pauses is the pause menu itself.

Two further faults the surface rebuild surfaced rather than caused, both now fixed: pause used `SetEnabled` and blanked the scene behind the menu, and the demo state did not clear the screens it registered, so leaving it asserted on the next state's registration. Clearing them exposed a third — the XR submenu registered once at application startup was destroyed by the first state to leave — settled by moving its registration into the states that support XR, which is where a scene's screens belong.

**Verified flat, with no headset:** builds clean; boots into the demo with the window HUD and the panel surface both live; pausing keeps the scene drawing (32 renderables submitted per frame paused and unpaused, against 0 before the fix) and leaving the state for the main menu registers cleanly with the XR submenu still reachable; the panel draws once and then not again until its content changes, and once per change after that. A second panel authored in a scene file creates its own surface alongside the demo's, so the panel path is not specific to this state. **Still unverified:** everything needing a live session — the ray reaching the panel, two hands on one panel, the haptic edge, entering and leaving XR, and the paused-in-XR desktop mode.

---

## Multi-pass versus single-pass stereo

**This plan is multi-pass.** Single-pass is not in plan.

Both draw the scene twice, once per eye. The difference is how many times the CPU submits the work.

### Multi-pass

Loop over the two eyes. Each iteration sets the eye camera, binds that eye's render target, and walks the whole renderable list issuing draws.

```
for eye in (left, right):
    bind target[eye]
    for each renderable: draw
```

Two full submission passes: twice the draw calls, twice the state changes, twice the culling.

### Single-pass

Submit the draw list once. The GPU expands each draw into two invocations, one per view, writing into two layers of an array render target. The vertex shader reads the view id to select its view-projection matrix.

```
bind layered target (2 layers)
for each renderable: draw          # replayed per view by the GPU
```

### What single-pass buys

CPU submission cost, roughly halved. That is the whole benefit. The GPU saves little: vertex shading still runs per view, and fragment shading is unchanged because both eyes cover the same pixel count either way. It matters when draw submission is the bottleneck, which is the normal state at headset refresh rates, and matters little when the frame is fragment-bound.

### What single-pass costs

- A layered array render target rather than two separate ones. A multisampled target resolves colour attachment 0 only, so **layered plus multisampled is not covered today** and needs shader-resolve work first. This is the real prerequisite.
- Per-view matrices move out of the per-draw path: a two-element view-projection array goes into `FrameUniforms`, and `model` alone stays in `ObjectUniforms`, which shrinks to 64 bytes. The model-view-projection matrix leaves the shader interface, so every vertex shader using it changes.
- Anything view-dependent computed per draw on the CPU moves into the shader.
- Post-processing handles a layered source, or runs twice.

### Why multi-pass first

It is a contained refactor of the rendering frontend — a loop over a view list whose one-entry case is today's behaviour exactly. Single-pass touches the render target model, the uniform layout and every shader. Correctness first, then measure whether draw submission is actually the bottleneck.

### The push-constant budget is not what blocks single-pass

`ObjectUniforms` is capped at 128 bytes with an explicit "no third mat4" rule. Recorded here so the cap is not mistaken for the obstacle. Multi-pass never touches it — the model-view-projection matrix is re-uploaded per draw from whichever eye camera is active. Single-pass wants a view-projection array in `FrameUniforms`, which is uncapped, and shrinks `ObjectUniforms` rather than growing it.

The 128-byte figure is a forward-looking promise, not a GL limit: both backends implement push constants as an ordinary uniform buffer update, and the GL floor is 16 KB. It buys the guarantee that a Vulkan backend maps draw-frequency data to native push constants on any conformant device, and keeps the payload inside D3D12's root signature budget. Raising it is a decision for whoever writes that backend.

---

## Found on the first headset run

Four defects, three understood and fixed, one still open.

- **The keyboard died for the life of the session.** XR activity was signalled every frame a hand was tracked, which pinned the active input type to Xr and repointed the shared trigger array at the XR one. Every keyboard trigger then read false, including the developer keys, and the only way out of the running application was the task manager. Activity is now signalled by deliberate input only — a pull or a stick deflection — and **developer triggers are read from the keyboard whatever device is active**, so the escape hatch cannot be taken away by a device that has claimed input.
- **World-space panel buttons could not be pressed.** Hit testing falls back to joystick-only elements unless `IsUiInteractable()`, which requires `InteractionMode::Ui`; the demo sets `World` for the scene around it. An overridden pointer now counts as UI focus by itself, since it exists because something is deliberately pointing at a UI surface.
- **The panel rendered as a black slab.** Its target cleared to the attachment default of opaque black and its surface drew without blending, so everything the screen did not cover was solid. The target now clears transparent and the surface blends. A second text flush inside the panel pass was also redrawing every batch — `EndFrame` draws what is queued and only `BeginFrame` clears it.
- **The panel flickered and the screen laid out twice per frame.** The screen was drawn to the panel at its own size *and* to the window at the window's, so layout — which is cached against one surface — was recomputed twice per frame at two sizes. Hit testing then met whichever ran last while the pointer arrived in panel pixels, so the two disagreed. A panel surface is now a mode: layout, hit testing and drawing all use the panel size, and the window pass draws nothing, because the mirror already shows the eye image with the panel in it. The diagnostic readout made it worse by rewriting itself every frame — setting text invalidates layout, so the panel reflowed under the ray; it is throttled and given a fixed height.
- **The panel vanished entirely.** The state sampled `IsSessionRunning` once in `Init` to decide whether to move its screen onto a panel, but the runtime only reports the session ready some frames into the loop — and `XrManager` was created after `AppMain`, so at `Init` it did not exist at all. Engine setup now runs before `AppMain::Init`, and the state watches for the session rather than sampling it once.
- **A scene material rendered black.** `ExampleScene` names the procedural material but its parameters are set by the state that loads the scene, not by the loader. Unset, they are zero.
- **No session meant no frame at all.** The loop only ticked while the session was running, so a headset that was asleep or set down left the window frozen with no input — the same trap as the keyboard death, by a different route. The frame is now ticked either way, falling back to the built-in view list and drawing flat to the window.
- **Panel text did not render.** Text sizes itself against `lastScreenHeight`, which tracked the window while layout used the panel, so text and the rects it sits in were scaled against two different surfaces. The UI now has one surface size — the panel where one is set, the window otherwise — and layout, text sizing, hit testing and slider geometry all read it from there.
- **`XR_SESSION_NOT_FOCUSED` is a success code.** `xrSyncActions` returns it as a positive result, so a bare `XR_SUCCEEDED` test walks straight past and reads actions the runtime has already declared inactive. Every button and axis then reads zero with nothing reporting why, while poses keep locating — because poses locate in the visible state and actions do not. Hands and a working ray alongside a dead squeeze is what that looks like from the outside, and it is indistinguishable from a binding that never arrived. The focus state is now tracked, logged on change, and shown on the panel readout, and every session state transition is logged by name.
- **Grabbing does not fire, most likely the above.** Not diagnosable from a desk: it needs a live session. The panel readout reports raw squeeze and trigger values, per-hand tracking flags, and the distance from the nearest grip to the nearest sphere, and is mirrored into the log so it survives the panel not being visible. On the first synced frame the runtime's chosen interaction profile is logged per hand, along with whether each action resolved to a real control — a declined profile leaves poses working through the simple controller while squeeze and thumbstick silently do not exist, which is otherwise indistinguishable from an action that is bound but never pressed.

## Risks

- **Loader CRT mismatch** is the likeliest source of a wasted afternoon. Resolve it before any XR code is written. A link failure there invalidates nothing else in the plan, but discovering it late does.
- **Bind cache invalidation.** The runtime and the loader mutate GL state behind the backend's bind cache. Invalidate the cached state after every excursion into the XR API.
- **Multi-pass doubles draw submission.** Draw calls, state changes and any future culling run twice; fragment cost is unchanged. No culling or instancing work is proposed here. If the frame budget misses at headset refresh, measure whether submission is the bottleneck before reaching for single-pass — that is the next piece of work rather than a defect in this one.
