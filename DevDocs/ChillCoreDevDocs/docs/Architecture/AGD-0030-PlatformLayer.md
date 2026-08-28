# AGD-0030: Platform Layer

- **Scope:** The boundary between the engine and the operating system. Covers drawing-surface and graphics-context ownership, file input and output, and physical input capture, along with how a concrete platform is selected. Does not cover any specific platform's implementation details.

## Overview

- Three interfaces carry everything operating-system specific: a window, a file system, and physical input.
- Each has one implementation per platform. Engine and application code names only the interface.
- The concrete implementation is chosen in one place, at construction, by conditional compilation. That is the only permitted platform branch outside the platform layer itself.
- The interfaces are deliberately small. Anything larger belongs in a subsystem that sits in front of them.

## Concepts

- **Platform interface** — an abstract class defining what the engine needs from an operating system, with no reference to any specific one.
- **Platform backend** — a concrete implementation of one interface for one operating system.
- **Blob-shaped input and output** — files are read whole into memory rather than streamed.

## Architecture

`PlatformWindow` owns the drawing surface, the graphics context, and everything attached to them: creating and destroying the drawing surface, presenting a finished frame, pumping the operating system's event queue, reporting framebuffer size, fullscreen state, cursor locking, file-drop notification, and the current time. It also declares the drawing-surface and graphics-context lifecycle hooks, since it is the only thing positioned to know when either is lost.

`PlatformFileSystem` covers reading text and binary files, writing text, an atomic text write, copying, directory listing, existence checks, and last-write times. Nothing above it opens a file directly.

`PlatformInput` covers physical devices: keyboard, mouse, gamepad, and touch. It reports raw device state and nothing more. Deciding what that state means is the input system's job, one layer up.

Each interface exposes a static accessor. The accessor is where the platform branch happens — a single conditional selecting the concrete class — and it is the only such branch permitted outside the platform layer. Adding a platform means adding a backend and extending that one decision.

Time is on the window rather than in its own interface. Every platform that gives us a drawing surface also gives us a clock, and a separate interface for one function would not earn itself.

## Design decisions

### Three interfaces, deliberately narrow

The split is by kind of operating-system resource — a drawing surface, a file system, a set of input devices — because those are the three things every target platform provides differently and everything else can be built on.

Keeping each interface small is the load-bearing part. The temptation is to grow them until they answer every question a caller has; the discipline is that anything richer goes in front of the platform layer instead. The input system shapes raw device state into logical actions. The render manager owns rendering policy. Neither belongs behind a platform interface, because neither differs by platform.

The cost is indirection for cases that are not actually platform-specific, and a small amount of duplicated boilerplate across backends.

### The platform branch happens once, at the accessor

Every other approach — branching at call sites, or compiling different source trees per platform — was rejected in favour of one conditional inside each singleton accessor.

The benefit is that application code and the cross-cutting parts of the engine compile unchanged on every target. That claim was tested when a second platform was added, and it held: the cost landed inside the platform backends, which is where it belongs.

The cost is that platform selection is a build-time decision baked into the binary. There is no runtime platform switching, and none is wanted.

### File input and output is blob-shaped, with no streaming reads

A read returns the whole file. There is no incremental or seeking read anywhere in the interface.

This follows the platforms rather than leading them. Packaged application assets and browser-fetched resources are both naturally whole-file; a streaming abstraction would either be a lie on those platforms or would force the lowest common denominator onto all of them. Every current caller reads a complete file anyway.

The cost is real and worth stating: a genuinely large asset must fit in memory, and there is no way to process one incrementally. Adding streaming later means adding a second, separate path rather than widening this one.

### Atomic writes are a distinct operation

Writing text and writing text atomically are separate methods, rather than a flag on one.

An ordinary write mutates the target file in place, so an interruption can leave it truncated or half-written. For a file that is read back at startup, that turns a crash into permanent data loss. Making atomicity a separate method forces the caller to decide which guarantee they need instead of inheriting a default.

The guarantee is only as good as the backend. Where the platform provides a rename primitive, it is genuinely atomic; where it does not, the method currently falls back to an ordinary write and the guarantee is not met.

### The window owns the graphics context, and the lifecycle hooks with it

Drawing-surface and graphics-context lifecycle callbacks are declared on the window rather than on the renderer.

The window is what actually loses and regains these resources, and it is the only component that learns about it first-hand. Drawing-surface loss is separated from graphics-context loss because their costs differ by orders of magnitude — one stops presentation, the other invalidates every graphics handle the engine holds.

Both default to doing nothing, so a platform that never loses either implements neither.

## Limitations

- Directory listing and file copying are unimplemented on some backends, which blocks any feature needing to enumerate files there.
- The atomic write guarantee is not met on every backend; where it is unavailable it silently degrades to a plain write rather than failing.
- Touch input is captured into multiple pointer slots, but nothing consumes more than the first, so multi-touch gestures are unavailable despite the data being present.
- No streaming file access, by design. Large assets must fit in memory.
- Platform selection is build-time only.
