# AGD-0140: Audio Subsystem

- **Scope:** Sound effect and music playback — loading, triggering, controlling, and mixing. Covers the library choice, format support, and the split between fire-and-forget and controlled sounds. Does not cover the tracker built on top of it, which is AGD-0150.

## Overview

- One audio library provides device output, mixing, decoding, and voice management. The engine writes none of that.
- Two playback shapes exist: fire-and-forget effects identified by an enumerator, and controlled sounds returning a handle.
- Effects are preloaded and triggered by identifier. Music is loaded on demand and streamed.
- Several audio formats decode natively; the project's chosen format is added by vendoring one more source file.

## Concepts

- **Sound effect** — a short, fire-and-forget sound. Triggered by identifier, plays to completion, needs no control.
- **Music** — a long clip, optionally looping, that the caller may need to stop or adjust while it plays.
- **Voice** — one instance of a sound currently playing. Several instances of the same effect can overlap.
- **Sound handle** — a controllable reference to a playing sound, returned only where control is needed.

## Architecture

`AudioManager` is a singleton owned by the engine, constructed with the other subsystems and given an update slot in the frame loop for cleanup.

Its interface is deliberately two-shaped. Effects are preloaded against an identifier and triggered by it; triggering returns nothing, because there is nothing useful to do with a fire-and-forget sound. Music is loaded on demand and returns a handle so it can be stopped or adjusted.

`Sound` wraps one controllable playing instance, tying its lifetime to the object.

Format decoding, device management, resampling, and mixing all belong to the vendored library. The engine's audio code is a thin shaping layer above it, which is the intent — none of that is engine-specific work.

The audio update runs late in the frame, after interface and developer overlay updates, so that a sound triggered by a button press this frame is not reaped in the same frame it started.

## Runtime flow

**At startup**, the audio engine initialises against the device's own sample rate and channel count rather than imposing values.

**Effects are preloaded** by identifier during application initialisation, since preloading is the whole point — the trigger path must not touch the disk.

**Triggering an effect** starts a voice and returns. Multiple triggers of the same effect overlap as separate voices; there is no cutting-off or voice stealing at this layer.

**Playing music** loads the file, optionally sets looping, starts it, and returns a handle. Long clips stream rather than being decoded into memory whole.

**Each frame**, the manager reaps finished sounds.

## Working with it

**Add a sound effect.** Preload it against an identifier during application startup, then trigger it by identifier. Effects shared across application states belong in application startup, not in a state, so that any state can be entered directly.

**Play a one-off sound not worth an identifier.** Trigger by path directly, optionally with a volume. This skips preloading and is appropriate for sounds triggered rarely.

**Play music.** Request it, hold the returned handle for as long as you may need to stop it, and stop it through the manager.

## Design decisions

### A single-header library, vendored like everything else

The audio library is a single header compiled into the engine, chosen over a proprietary middleware package, a lower-level output interface, and the platform's own audio interface.

The deciding factors were integration and licence. Compiling into the engine means the static runtime requirement is satisfied automatically, with no per-configuration build chores and no redistributable to copy. The licence imposes no attribution or distribution constraints.

The alternatives failed on specifics rather than capability. The proprietary option is closed-source and library-based, breaking the project's static-only convention and adding licence tracking for a feature set far beyond what is needed. The lower-level options — a general audio output library, or the platform's own — provide voices and mixing but leave decoding, streaming, looping, and voice management to be written by hand, which is a large amount of well-solved work to redo.

One alternative was genuinely close, with a comparable feature set and licence, and was rejected on development activity and on being multiple files rather than one. That is a thin margin, and it is the first thing to reconsider if the current library ever proves limiting.

### The chosen delivery format is added by vendoring one more decoder

The library decodes several formats natively but not the one the project ships assets in. That is added through the library's documented decoding-backend extension point, with a second vendored public-domain source file.

Using the supported extension point rather than an older include-based trick keeps the integration on a maintained path. It also keeps format knowledge out of the manager's interface entirely — callers load files uniformly and never learn which decoder handled them.

### Effects and music have different interfaces on purpose

Triggering an effect returns nothing. Playing music returns a handle.

The asymmetry reflects what callers actually need. A fire-and-forget sound has no meaningful control surface, so returning a handle would invite callers to store references they should not keep and create lifetime questions that need not exist. Music genuinely needs stopping and adjusting.

The cost is that a caller who later needs control over something declared as an effect has to change how it is played.

### Effects are preloaded and identified by enumerator

Effects are loaded ahead of time against identifiers rather than being loaded when triggered.

Triggering is on interaction paths where a disk read would be a visible stall, and the same effect is usually triggered many times. Identifying by enumerator rather than by path also means a rename does not silently become a missing sound at runtime.

A by-path trigger exists alongside for sounds not worth preloading. It is the right tool for something played rarely and the wrong one for anything on an interaction path.

### Device settings follow the device

The engine initialises with the device's default sample rate and channel configuration rather than requesting specific values.

Requesting a specific rate forces resampling somewhere — either in the library or in the driver — for no benefit, since nothing in the engine depends on a particular rate. Following the device avoids that entirely.

The consequence matters for anything sample-accurate built on top: the rate is discovered at runtime, not assumed.

### Audio updates after the interface

The manager's per-frame cleanup runs after interface and developer overlay updates.

This ordering is deliberate. Interface interactions trigger sounds, and reaping before those interactions were processed would consider a sound finished in the same frame it began.

## Limitations

- No voice limit or stealing policy. Triggering an effect many times in quick succession creates many overlapping voices.
- No positional or spatial audio, though the underlying library supports it.
- No effects, buses, or submixing exposed. The library's graph is available but nothing uses it.
- No audio input or capture.
- Music handles are caller-managed; nothing prevents holding one past its usefulness.
