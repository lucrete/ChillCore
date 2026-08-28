# AudioTracker Plan

**Status:** Phases 0 to 5 have landed. What remains is the primary editing affordance, project file handling, several editor first-cuts, and the deferred polish phase.
**Current state:** AGD-0150 (AudioTracker) describes what exists. This plan covers only what does not.
**Index:** the AudioTracker section of `01_TechBacklog.md`.

---

## Velocity editing

The data model supports velocity. The editor does not expose it, so steps are effectively on or off.

### What velocity means here

Time is divided into discrete steps. Each step on a pattern track either fires its assigned sound or does not. Velocity is a per-step scalar controlling how loud that firing is, borrowed from the term for how hard a key was struck.

In this cut, **velocity controls gain only**. A step's output gain is the product of the master gain, the song track's gain, the pattern track's gain, and the step's velocity.

Velocity is also the on-off bit: there is no separate flag. Zero is off, anything above zero is on. This keeps the data model simple and makes "off" naturally distinct from "very quiet" without a special case.

### What it deliberately does not do

- **No sample selection by velocity.** Each pattern track has one assigned sound. Choosing among several by velocity range — the way a drum machine plays a different sample for soft and hard hits — builds on the same data but is out of scope.
- **No pitch change**, no filtering, no per-trigger envelope shaping. The sound plays as recorded.
- **No choke groups** or per-step one-shot-versus-loop selection.

This is the simplest form of velocity that is still musically useful, and it is the substrate the richer forms would build on.

### Why it matters

Without it, every hit is identical and the loop sounds machine-flat. Velocity is what makes accents possible — a bar of hi-hats with emphasised downbeats has a pulse where a constant-velocity bar has none — and what makes ghost notes possible, where a quiet hit sits beneath the full-volume ones and gives the rhythm texture without competing with it.

### What is missing

- A command for setting step velocity. The existing toggle command names it as pending.
- Visual encoding of velocity on the pattern grid. Cells currently render as a binary state; brightness or fill height would carry the value.
- Vertical drag on a cell to adjust it. This may need drag support adding to the interface input layer.
- Secondary affordances — scroll wheel, keyboard nudge — belong with the polish work below.

---

## Missing commands

Every project edit routes through a command so it is undoable. Several from the intended catalogue do not exist yet: setting a pattern track's gain, renaming a pattern, clearing a song cell, removing a song track, and setting a song track's gain.

Separately, no command implements coalescing. Drag-painting across cells and dragging a slider therefore push one history entry per event rather than one per gesture, which makes undo unusable for those interactions.

---

## Project file handling

Saving and loading work, but against a single fixed path.

- **New, Open, and Save As** do not exist.
- **An in-engine project browser**, scoped to the projects folder, showing tempo, song length, and last-edited time inline.
- **Auto-save on exit**, with a restore offer on the next launch.
- **Load errors on schema mismatch** surface in the interface rather than failing silently.

### File picking: in-engine for projects, native for samples

The right answer differs by what is being picked, and the split is worth recording because it looks inconsistent otherwise.

**Project files use an in-engine browser.** The tracker owns the projects folder, and users have no reason to keep project files elsewhere — arbitrary disk browsing mostly lets someone put work where the application cannot find it again. An in-engine list is also the only place project metadata can be shown inline, which a generic file dialog cannot do. It is one cross-platform code path rather than one native dialog per desktop platform, and it avoids the modal-dialog hazards that come with the platform's own picker.

**Sample import uses the platform's native dialog.** Samples come from anywhere on disk, which is exactly what a native picker is for, and users already know how it works. Drag-and-drop is implemented and works as a complementary affordance; the native dialog is not, so import is currently drag-and-drop only.

---

## Editor first-cuts to replace

- **Track source assignment is a cycle button.** The intended control is a dropdown, which is blocked on the interface's dropdown supporting dynamically populated options.
- **Neither grid shows a playhead.** The transport publishes its position for exactly this purpose and nothing reads it for display.
- **The non-Windows storage path is a placeholder**, pending a second desktop target.

---

## Polish

Deferred as a group. Each is a follow-up rather than part of the initial cut.

- Per-track gain sliders and solo. Muting works in both views; gain and solo do not.
- Variable step subdivision — eighths, sixteenths, thirty-seconds, triplets.
- Editing pattern length after creation.
- Waveform display for samples in the panel.
- Keyboard shortcuts, including play and stop, and direct track selection.
- Tempo and time-signature changes mid-song, with an automation lane.
- Offline render to an audio file through the existing engine.

---

## Verification debt

Two milestones were defined and never recorded as run.

- **Timing soak.** A ten-minute loop with zero sample-frame drift, no seam artefacts, and audio-thread cost under a low single-digit percentage.
- **Crash safety.** Killing the process between the temporary-file write and the rename, and confirming the previous project survives intact.
