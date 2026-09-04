# XR Plan

**Status:** M1 landed and verified. M2, M3 and M4 are written and build. What can be exercised without a headset is verified: the loader reaches the runtime, the flat fallback is unchanged, and the demo state boots and runs with its world-space panel drawing. Everything downstream of a live session is unverified pending a connected headset.
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

`AppStateXrDemo` under `Code/App/AppStates/`, registered in `AppMain::Init` with one line. Follows the showcase state's shape: camera install, scene init, action context, UI screen, interaction mode.

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

**Done when:** the demo state boots directly as the initial state, objects can be picked up and released with either controller, and a world-space UI panel responds to the controller ray.

**Written, partly verified.** `AppStateXrDemo` is registered and boots directly. Verified flat, with no headset: the scene builds, the panel target is created, the screen renders into it, and the quad in the scene samples it — the whole panel path works without a session, and the state reports itself inactive rather than failing. Unverified: hands, ray, grab, pointer and haptics, all of which are gated on a live session.

**Three engine additions the milestone needed.**

- `UiManager::RenderToTarget` draws the active screen into an offscreen target at that target's size. Layout is cached against one surface size, so it marks layout dirty on the way out and restores the text projection the window pass expects.
- `UiPointerState` on `UiInputHandler` replaces the mouse as the pointer source. Hit testing already worked in panel-space pixels, so the ray only has to convert its hit; a pointer that leaves the surface releases what it was holding rather than leaving it stuck pressed.
- `Material::SetTextureHandleOverride` binds a texture the material does not own at unit 0. `Texture` only ever loads from a file or from memory, and a render target's colour attachment is neither.

**The panel is one frame behind.** It renders during the state's Update, because the scene samples it and the scene draws before the frame's UI pass. At headset refresh this is not perceptible; moving it would mean reordering `CoreMain::TickFrame`.

**Noted, not fixed.** `Vector3`'s arithmetic operators are not const-qualified, so a `const Vector3&` cannot take part in one. The demo copies before use. Worth fixing in the maths header, but not as part of this work.

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

## Risks

- **Loader CRT mismatch** is the likeliest source of a wasted afternoon. Resolve it before any XR code is written. A link failure there invalidates nothing else in the plan, but discovering it late does.
- **Bind cache invalidation.** The runtime and the loader mutate GL state behind the backend's bind cache. Invalidate the cached state after every excursion into the XR API.
- **Multi-pass doubles draw submission.** Draw calls, state changes and any future culling run twice; fragment cost is unchanged. No culling or instancing work is proposed here. If the frame budget misses at headset refresh, measure whether submission is the bottleneck before reaching for single-pass — that is the next piece of work rather than a defect in this one.
