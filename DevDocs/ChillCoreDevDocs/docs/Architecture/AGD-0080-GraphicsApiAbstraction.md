# AGD-0080: Graphics API Abstraction

- **Scope:** The boundary between the rendering frontend and a graphics API. Covers opaque handles, descriptors, pipeline objects, render passes, capability querying, and GPU timing, plus how backends implement them. Does not cover how a frame is composed.

## Overview

- Nothing above this layer sees a graphics API type. Resources are opaque handles into backend-owned pools.
- Resources are created from descriptor structures describing intent, which the backend realises however its API requires.
- Fixed graphics state is baked into pipeline objects at creation rather than set piecemeal before each draw.
- Feature differences between platforms are reported as data and branched on at runtime, not compiled around.
- Two backends exist, for desktop and mobile variants of the same graphics API, sharing a common helper layer.

## Concepts

- **Handle** — an opaque identifier for a resource. Carries no API meaning and is only interpretable by the backend that issued it.
- **Descriptor** — a plain structure describing a resource or pipeline to create. Uses engine enumerations exclusively.
- **Pipeline object** — shader, vertex layout, and fixed state, combined and validated once at creation.
- **Render pass** — an explicit bracket around drawing into a target, with defined behaviour at entry and exit.
- **Capability set** — what the current backend and device actually support, queried once at startup.
- **GPU scope** — a named span of GPU work, used for both timing and capture labelling.

## Architecture

`Gfx::RenderApi` is the abstract interface. It covers resource creation and destruction, command recording, render passes, backbuffer configuration, compute dispatch, and timing. Every method operates on handles and descriptors.

Two backends implement it: one for desktop, one for mobile. They live side by side and share a common layer holding the handle pools and the timing ring, because the great majority of their logic is identical and only format tables, feature availability, and shader dialect genuinely differ. Which one compiles in is a build-time definition.

Handles index into backend-owned pools with free-list reuse. A handle value of zero is reserved as invalid, so a default-constructed handle is safely meaningless.

Capabilities are gathered once during initialisation and exposed as a queryable set. Callers branch on them. No conditional compilation for feature differences is permitted above this layer.

The boundary is checkable: no graphics API type or header may appear outside this layer and the vendored dependencies, which a search can confirm.

## Runtime flow

**At startup**, the backend initialises against the platform window, populates its capability set, and creates its pools.

**Per frame**, a frame begins, a render pass is entered against a target, pipelines and resources are bound and draws are issued, the pass ends, and the frame ends with presentation.

A pass targets either the backbuffer or an offscreen render target, selected by the handle passed when entering it. An invalid handle means the backbuffer.

- **Backbuffer pass.** Entering binds the multisampled scene framebuffer and clears it. Ending resolves the multisampled output and presents it into the default target as an ordinary draw, using a pipeline like any other.
- **Offscreen pass.** Entering binds the target's framebuffer, sets the viewport to its dimensions, and applies each attachment's load behaviour — clear, or preserve what is there. Ending applies each attachment's store behaviour, discarding what is not needed, and does no resolve or present. Offscreen targets are single-sample.

Passes do not nest. Both pass boundaries bind framebuffers outside the tracked binds, so both invalidate the cached bind state.

A target's colour attachment is an ordinary texture. A later pass samples it through the normal texture binding path; no separate mechanism exists for reading rendered output.

**Binding a pipeline** compares against the last one bound and returns immediately if unchanged. Because pipelines carry all fixed state, this single check subsumes what would otherwise be many individual state comparisons.

**Timing** works by punctual markers rather than paired brackets. Each marker closes the previous span and opens a new one, so a frame's spans form an ordered sequence. Results are read back by index some frames later, once the GPU has caught up.

## Working with it

**Create a resource.** Fill a descriptor and call the corresponding create method. Destroy it through the matching destroy method; nothing is collected automatically.

**Add a draw.** Obtain a pipeline for the shader and vertex layout, bind it, bind vertex and index buffers, textures, and uniform buffers, then issue the draw.

**Render to an offscreen target.** Create the attachment textures, marking them as render targets. Create the render target from a descriptor naming those attachments and their load and store behaviour. Destroy the target when done; attachment textures are owned by the caller and are not destroyed with it. Do not enter the pass directly — declare it on the rendering frontend, which owns pass ordering.

**Support a feature that not every platform has.** Query the capability at runtime and provide a path for its absence. Do not use conditional compilation — that defeats the point of the abstraction and hides the gap from every other platform.

**Dispatch compute work.** The interface provides compute dispatch, storage-buffer and image binding, and memory barriers, and shader creation accepts a compute stage. Both backends implement them, and both report compute and storage buffers as available. Nothing above the backend can author a compute shader yet, so this surface has no callers.

**Add a backend.** Implement the interface, populate the capability set honestly, and add the build definition selecting it. Nothing above this layer should require changes; if it does, that is a defect in the abstraction rather than in the new backend.

## Design decisions

### Resources are opaque handles over backend-owned pools

The alternative — passing the underlying API's own resource identifiers through public signatures — cannot survive a second backend. A backend for a different API has no honest way to produce the first API's identifier type, and would be forced to fabricate one.

Handles also give the backend freedom to reorganise its storage, and a reserved invalid value so an unset handle is detectable rather than accidentally valid.

The cost is one indirection per resource access. Measurable only in synthetic tests.

### Fixed state is baked into pipeline objects

Shader, vertex layout, culling, depth behaviour, and blending are combined and validated at creation, then bound as one object.

This matches how modern graphics interfaces work, so the structure carries forward rather than needing rework. It also makes redundant-state elimination tractable: one comparison replaces many. And it moves validation to creation time, where a failure is diagnosable, rather than to draw time.

The cost is a real loss of flexibility. Changing fixed state means creating a different pipeline, so code that mutates material state after creation must route through pipeline recreation. This is the intended direction but it is a change in authoring mentality, not just in implementation.

### Capabilities are queried, never compiled around

Feature differences across platforms are reported in a capability set and branched on at runtime.

Conditional compilation for features would scatter platform knowledge through code that has no business holding it, and would make a missing feature invisible on the platforms that have it. Querying keeps one source compiling everywhere and makes gaps into ordinary data.

The cost is that callers must remember to check, and forgetting produces a runtime failure on one platform rather than a build failure everywhere. Asserts in development are what surface this.

### One interface, two nearly-identical backends, sharing a common layer

Desktop and mobile use variants of the same graphics API and differ in a small number of places: available formats, feature availability, and shader dialect.

They are separate classes rather than one class full of conditionals, with genuinely shared logic — the handle pools and timing ring — factored into a common layer. Keeping them adjacent rather than in separate source trees is what makes that sharing natural.

The cost is that a change to the shared layer must be considered against both, and every new graphics feature has to be thought about twice.

### Timing uses punctual markers, not bracket pairs

A marker names a point; the span runs from that marker to the next. Results are read by index over an ordered per-frame sequence, not looked up by name.

The design that was originally intended — paired begin and end calls with lookup by name — was rejected during implementation for a specific reason: name-based lookup requires every consumer to repeat the name literal, so a renamed span silently returns nothing instead of failing. Reading by index means a name is written down exactly once, at the marker, and consumers read it back.

It also preserved every existing marker position and name through a large refactor, keeping timings comparable across it.

The cost is that spans are a flat sequence rather than a tree. Nested attribution — a pass broken into sub-spans — is not expressible.

### Backbuffer configuration is a descriptor, not individual setters

Sample count, format, and vertical sync are set together in one call rather than through separate properties.

Backends that must recreate their presentation chain when any of these change need them as one atomic change. Individual setters would force either a recreation per property or a deferred-application mechanism.

### Pass brackets belong to the frontend, not to general callers

Resource creation and destruction, binding, and drawing are open to any caller. Entering and ending a render pass is not: general callers declare the pass they want and the rendering frontend brackets it.

This settles a question left open since the abstraction was built — whether to formalise the split between what `RenderManager` may call and what anything else may. Offscreen targets forced it, because they are the first feature with a caller outside the frontend that wants a pass of its own.

The frontend opens the scene pass before any application code runs, and passes do not nest. A caller bracketing its own pass from inside the update would therefore be opening a pass inside an open one. The options were to move the frame's pass structure into application code, or to keep the frontend the only thing that brackets passes. The second keeps the frame sequence in one place and leaves the ordering invariant checkable.

The split is by convention rather than by a separate restricted interface. Nothing in the type system stops a caller reaching for the brackets directly; the runtime assert on a nested pass is what catches it. A narrower interface for general callers remains available if this proves insufficient.

The cost is that a caller cannot express an arbitrary pass graph. What it can express is one offscreen pass feeding one post pass, which is what the current features need and is not the general case.

### Windowing is not a rendering concern

Window size, fullscreen state, and cursor locking were once part of this interface and are not. They belong to the platform layer.

They differ by operating system rather than by graphics API, and a backend has no business answering questions about a window. What remains here is the backbuffer size, which is a rendering concern and can legitimately differ from the window size.

## Limitations

- Offscreen render targets are single-sample. Multisampling exists on the backbuffer only, so offscreen output has aliased edges the direct path does not.
- Render target attachments are single-layer. Array layers and cubemap faces are rejected, which is what a cubemap reflection probe or a cascaded shadow map would need.
- The backbuffer is not itself addressable as a render target. It is reached by an invalid handle rather than by a handle from the pool.
- Redundancy elimination covers pipeline binds only. Vertex buffer, texture, and uniform buffer binds are not compared against current state.
- GPU spans are a flat ordered sequence per frame, with no nesting.
- Any code path bypassing this interface to call the graphics API directly must invalidate the cached bind state, or stale state becomes visible as incorrect rendering.
- The compute surface is implemented but unreachable. Shader compilation does not recognise a compute stage, so no compute shader can be authored, and the dispatch, storage-buffer, image-binding, and barrier methods have no callers.
- Resource lifetime is manual. Nothing is reference-counted or collected.
