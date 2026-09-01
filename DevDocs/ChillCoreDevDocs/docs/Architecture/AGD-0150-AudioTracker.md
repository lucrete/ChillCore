# AGD-0150: AudioTracker

- **Scope:** The pattern-based music tool — its engine, timing model, threading, sample library, synthesis, undo, and storage. Covers how sound is scheduled and mixed sample-accurately. Does not cover general playback.

## Overview

- A tracker composes music from patterns of triggers on a grid, arranged into a song.
- Timing is counted in sample frames, not seconds, so trigger placement is exact rather than approximately right.
- The audio thread only reads. Edits are published by swapping a pointer to a freshly built schedule.
- Every sound source, whether a decoded file or a generated one, reaches the audio thread as plain audio data. The mixing path does not know which it is.
- Editing is undoable through a command history. Files are written atomically.

## Concepts

- **Pattern** — a grid of steps across several tracks. A step is either triggered or not, with a velocity.
- **Song** — an arrangement of patterns over time.
- **Track source** — where a track's sound comes from: a sample from the library, or a synthesis preset with parameters.
- **Schedule** — the flattened, immutable list of what triggers at which sample frame, built from the project and handed to the audio thread.
- **Voice** — one sounding instance being mixed. Drawn from a fixed pool.
- **Sample frame** — one sample position per channel. The unit the audio device counts in, and therefore the only exact one.

## Architecture

`TrackerEngine` owns playback: the voice pool, the current schedule pointer, and the audio callback. It is created and owned by the application state that uses it, not by the engine's global subsystems, so nothing exists when the tracker is not running.

`TrackerClock` holds transport position as an atomic count of sample frames. Beats and bars are derived from that count with the tempo and time signature, rather than being stored.

`TrackerProject` is the edit-time model: patterns, tracks, the song arrangement, tempo. It lives entirely on the main thread.

`SampleLibrary` manages imported samples — their identity, verification, and on-disk records. `Synthesis` holds parameter-driven generators that produce audio from a preset and its parameters. Both feed the same voice pool.

`TrackerCommand` and its history provide undo. Every edit to the project model goes through a command.

`TrackerPaths` resolves the storage location once, creating directories on demand.

## Runtime flow

**On initialisation**, every usable library sample is decoded once into memory at the device's own rate and channel count. Synthesis presets are rendered to audio at edit time, not during playback.

**On any edit**, the main thread builds a fresh schedule from the project and atomically swaps it in. The previous schedule is retained rather than freed, because an audio callback may still be reading it.

**In the audio callback**, per output block: the voice pool is mixed unconditionally; then, only if playing and a schedule exists, the current loop position is computed and matching triggers start new voices.

Mixing runs unconditionally so that auditioning a sample works without starting the transport.

**Loop wrap** falls out of the position calculation rather than being a special case. Events are sorted by frame and matched against the wrapped position, so a trigger at the seam re-fires naturally with no boundary handling.

## Working with it

**Import a sample.** Drop it in. The original file is copied byte for byte, recorded in the library index with its size and content hash, and decoded into memory.

**Set what a track plays.** Assign either a library sample or a synthesis preset. The audio path is identical either way.

**Edit a pattern.** Toggle steps on the grid. Every edit goes through a command, so it is undoable.

**Save.** Projects and the library index are both written atomically, so an interrupted save leaves the previous file intact.

## Design decisions

### The tracker is owned by its application state, not by the engine

Everything tracker-related is created when the tracker state starts and destroyed when it ends. None of it exists as a global subsystem.

The tracker is one application's feature, not engine infrastructure, and making it global would mean every build carries its memory and initialisation cost regardless. Its only dependencies are subsystems already initialised by the engine, so the state can be entered directly without passing through anything else.

Synthesis sits inside the tracker for the same reason, rather than being a general engine facility. It has exactly one consumer. Lifting it out is the right move when a second consumer genuinely appears — extracting speculatively would mean designing an interface for demand that does not exist.

### The audio thread reads only; edits are published by pointer swap

The audio thread owns the voice pool and reads an atomic pointer to the current schedule. It never allocates, never locks, never touches growable containers, and never runs a decoder.

Any of those on the audio callback risks blocking, and blocking there is an audible glitch rather than a slow frame. Locking is the specific trap: a lock shared with the main thread means an edit can stall audio output.

The publish mechanism is to build a complete new schedule on the main thread and swap the pointer atomically. The audio thread sees either the old schedule or the new one, never a partially-updated one.

Retired schedules are kept alive for the session rather than freed. A callback may be mid-read when the swap happens, and there is no cheap way to know it has finished. Leaking a small amount is the correct trade against a use-after-free in the audio callback.

### Timing is counted in sample frames

Transport position is an integer count of sample frames. Musical positions are derived from it; seconds are never the unit of record.

Sample frames are what the audio device actually counts, so they are exact. Accumulating in seconds introduces floating-point drift that grows over a session, which is exactly the failure a tracker cannot tolerate — timing that is slightly wrong and gets worse.

The practical consequence is that trigger accuracy is bounded only by the sample rate, and that the rate must be discovered from the device at startup rather than assumed.

### A track's source is a tagged choice, resolved before the audio thread sees it

A track plays either a library sample or a synthesis preset with parameters. Both are turned into audio ahead of time, so the mixing path never branches on which it is.

This keeps the audio thread's contract simple — it mixes audio data, full stop — and means adding a new kind of source requires no audio-thread changes at all. Generating audio on the audio thread would violate its constraints outright.

Project files record the tagged choice, so the file format does not depend on the internals of either subsystem. Library contents stay out of project files; only identifiers cross, resolved on load.

### Samples are fully decoded at startup and never freed during a session

Every usable sample is decoded into memory up front. The cache only grows: imports add to it, and forgetting a sample removes its record but leaves the decoded audio until the process restarts.

Decoding on demand was rejected because first-play latency is audible and incompatible with sample-accurate scheduling, and because avoiding it would mean either a decoder on the audio thread or a worker thread and ring buffer — substantial machinery for a problem this domain does not have.

Append-only is what makes the audio thread's pointers safe. Because nothing is ever removed or moved, a voice's reference to audio data cannot dangle, and no lifetime synchronisation is needed at all. That property is worth more than the memory it costs.

This matches how trackers, drum machines, and hardware samplers have always worked. Streaming is a technique for long stems in editing suites, which is not what this is. The memory cost is shown in the interface rather than hidden, and the upgrade path — a length threshold selecting between resident and streamed — is understood but deliberately not built.

### Files are written atomically

Both the project file and the library index are written to a temporary file and then renamed into place.

An ordinary write mutates the target in place, so an interruption leaves it truncated. Both of these files are parsed at startup, and the verification logic assumes coherent content — a half-written file crashes the parser rather than degrading gracefully. Atomicity means a reader sees either the previous file or the new one.

Sample files are stored as byte-for-byte copies in their original format, so content hashes verify what is actually on disk rather than a re-encoding, and compressed formats stay compressed.

### Every edit goes through a command

Project edits are commands with an undo, held in a history.

Undo in an editing tool is not optional, and retrofitting it is far harder than routing edits through commands from the start. Keeping the model mutable only through commands means there is no path that silently bypasses history.

Library operations deliberately stay outside it. Importing and forgetting are confirmed operations against shared, on-disk state, not edits to the project being worked on, and folding them into a project's undo history would produce surprising results.

## Limitations

- All sample audio is resident in memory for the session, and forgetting a sample does not reclaim it until restart.
- Velocity is stored but not editable in the interface; steps are effectively on or off.
- No playhead is shown in either grid, though the transport position is available.
- Commands do not merge, so a dragged edit produces one history entry per event rather than one per gesture.
- Saving and loading use a single fixed project path. There is no new, open, or save-as.
- The tool is desktop-only — its storage paths and file operations depend on capabilities not implemented on all platforms.
