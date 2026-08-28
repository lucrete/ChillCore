# AGD-0100: Frame Timing and Profiling

- **Scope:** Measuring where frame time goes, on both processor and graphics hardware, and getting that measurement out for analysis. Covers delta time, the frame-rate figure, phase timestamps, the profile history, and capture. Does not cover the developer UI that displays it.

## Overview

- One clock reading per frame produces the delta time every other system uses. Nothing else queries the clock for frame timing.
- Named timestamps at phase boundaries produce the processor-side breakdown. The frame loop's structure and the profiler's breakdown are the same thing.
- Graphics-side timing comes from the graphics abstraction as an ordered sequence of named spans, read back some frames later.
- A rolling history of recent frames feeds the on-screen graphs; a capture writes a span of frames out for offline analysis.

## Concepts

- **Delta time** — seconds elapsed since the previous frame, sampled once and reused by everything.
- **Timestamp** — a labelled instant recorded during the frame. The span between consecutive timestamps is one phase.
- **Standard phase** — one of a fixed set of timestamps identified by an enumerator rather than only by label, so its duration can be looked up directly.
- **Profile history** — a fixed-size ring of recent frames' phase durations.
- **Capture** — recording the history to a file over a requested duration.

## Architecture

`FrameTimer` is a singleton owning the clock, the delta time, the frame-rate figure, the per-frame timestamp array, and the profile history ring.

It reads the clock through the platform layer rather than any windowing library directly, so the dependency that time introduces is the platform's, not a graphics library's.

Timestamps are recorded into a pre-allocated fixed array. Recording one is a label store and a clock read, with no allocation. A subset of timestamps also carry an enumerator, which indexes a direct lookup so a known phase's duration can be fetched without searching by label.

Graphics-side timing is not owned here. `FrameTimer` reads the ordered span sequence out of the graphics abstraction each frame and folds it into the same history as the processor-side phases, so both appear together.

The history is a fixed-size ring holding total frame time, processor phases, graphics phases, and the presentation wait. It is written every frame and read by the developer UI and by capture.

## Runtime flow

**At frame start**, the clock is read once. Delta time is the difference from the previous reading, and that single value serves every system for the rest of the frame. The timestamp array is reset.

**During the frame**, each phase boundary records a timestamp. These are the same boundaries the frame loop is built from, so the breakdown reflects the loop's real structure rather than a parallel description of it.

**At frame end**, the previous frame's graphics spans — now available, having had time to complete — are read back by index, and the whole set is written into the history ring.

**The frame-rate figure** accumulates frames over a one-second window and averages. It updates once per second rather than every frame.

**Capture** records for a requested duration and writes the history out, taking column headings from the span names themselves rather than from a hard-coded list.

## Working with it

**Add a phase to the profiler.** Record a timestamp at the new boundary in the frame loop. It appears in the breakdown automatically; there is no separate registration.

**Measure something ad hoc.** Record timestamps around it. The label is all that is needed; only phases wanting direct lookup need an enumerator.

**Get numbers out for analysis.** Trigger a capture, or use the developer console command that formats a snapshot and places it on the clipboard.

## Design decisions

### The clock is read once per frame

Before this existed, components queried the clock independently and derived their own timing. Now one reading at frame start produces a delta time that everything shares.

Beyond removing redundant work, this makes the frame coherent: every system sees exactly the same elapsed time, so two components animating at the same rate stay in step. Independent clock reads within a frame drift apart by however long the work between them took.

### Timestamps go into a fixed pre-allocated array

The array is sized at compile time and never grows. Recording a timestamp cannot allocate.

Profiling instrumentation that allocates perturbs what it measures, and the failure mode of a growing container — a reallocation inside the frame being timed — lands precisely where it does most damage. A fixed array with a known ceiling is predictable.

The cost is a hard limit on timestamps per frame, and instrumentation that exceeds it is silently truncated rather than diagnosed.

### Known phases carry an enumerator as well as a label

Most timestamps are identified only by their label. A fixed set also carry an enumerator that indexes a direct lookup.

This is a deliberate two-tier arrangement. Ad hoc measurement should cost nothing to set up, so a label alone suffices. The standing frame phases are queried every frame by the profiler, so they get direct indexed access instead of a search.

The cost is that the enumerator set is a shared declaration, so adding a standing phase touches it. Ad hoc timestamps avoid this entirely, which is why the two tiers exist.

### The frame-rate figure averages over one second

A frame-rate figure derived from a single frame's delta fluctuates too much to read. An exponential average would smooth it but requires tuning and still moves every frame.

A one-second window produces a stable number that updates at a readable rate. The cost is that it reads as zero until the first window closes, and that it hides brief spikes — which is why the history graph exists alongside it and is the right tool for finding those.

### Graphics timing is read back late, not waited for

Graphics-side spans are read some frames after they were recorded, once the results are ready.

Waiting for them in the frame that issued them would stall the processor until the graphics hardware caught up, destroying the very thing being measured. Accepting a delay before results are available is the only honest option.

The cost is that graphics figures on screen lag the processor figures beside them by a few frames. For spotting trends and regressions this is immaterial; for correlating a specific one-off spike across both, it is a genuine trap.

### The presentation wait is measured, not hidden

The time spent waiting for display synchronisation is recorded as its own span rather than being folded into the last phase of work.

This matters because it is usually the largest single number in the frame and is not a cost of anything the engine did. Attributing it to whichever phase happened to precede it would make that phase look catastrophic and would mask real regressions elsewhere.

The measurement is an approximation. Where in the frame this stall lands is driver-dependent, and it can be absorbed by a later operation rather than by the presentation call itself, so the figure indicates the presence and rough scale of the wait rather than its precise boundary.

## Limitations

- The frame-rate figure reads zero until the first averaging window completes.
- Graphics timing results lag by a few frames, so processor and graphics figures in the same row are not from the same frame.
- The presentation wait measurement is an approximation whose accuracy depends on driver behaviour.
- Timestamps per frame, history length, and phase counts are all fixed at compile time. Exceeding any is silent truncation.
- Graphics spans are a flat sequence, so a phase cannot be broken into measured sub-spans.
- The developer console and on-screen display are desktop-only, so on other platforms the data is collected but only reachable through capture.
