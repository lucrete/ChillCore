# 02 — Roadmap

Implementation order for the work inventoried in `01_TechBacklog.md`. The backlog says what is outstanding; this says in what order to do it, and why that order.

- **This is a recommendation, not a commitment.** It is sequenced on dependency and cost, not on product priority. Where priorities differ, this is the document to change.
- **Only real dependencies bind.** Everything else is a judgement about what makes the next item cheaper or safer, and can be reordered freely.
- **Stages are not sprints.** They group work that shares a rationale. Several stages contain independent items that can run in parallel or be split across people.

## What actually gates what

Four hard dependencies exist. Everything else is preference.

| This | Cannot start until | Because |
| --- | --- | --- |
| WebGL backend | CMake unification | The web toolchain integrates through CMake; there is no path through the hand-maintained desktop project. |
| Scripted build system | CMake unification | The presets are the script-facing contract. |
| Further graphics backends | CMake unification | Each new target needs a build description, and adding them one at a time to two systems compounds the problem unification solves. |
| Honest recovery from graphics-context loss | Persistence | The policy is a deliberate cold restart on the premise that persistence restores the user's place. Without it, the restart lands on the boot screen. |

Two soft dependencies are worth respecting:

- **Compute needs shader compilation to recognise a compute stage** before anything else about it can be built. That single change is the whole gate.
- **Material-to-pipelines should precede a third graphics backend.** The transitional shim methods cannot be implemented honestly by a backend for a modern graphics interface, so every backend added before they are removed pays for them.

## Stage 1 — Close the graphics abstraction

**Material to pipelines**, then the **acceptance measurements**.

Both are small and both de-risk everything after them. The migration removes the last transitional surface from the graphics interface, which is a debt every future backend would otherwise inherit. The measurements matter more than their size suggests: draw-call reduction was half the justification for the entire abstraction, and it is currently an assumption. If the numbers do not show the expected drop, something in the uniform tiering or the sort is not working, and that is far cheaper to discover now than after three more systems are built on top.

Do the measurements even if the migration slips. They are a day's work and they validate a large investment.

## Stage 2 — Make the tracker usable

**Velocity editing**, the **missing commands**, then **project file handling**.

This is the one workstream with a user waiting on it, and it is independent of everything else in this document — no graphics, platform, or build work blocks it or is blocked by it. It can run in parallel with any other stage.

Velocity first, because it is the primary editing affordance and its absence is what makes the tool feel unfinished. The missing commands include coalescing, without which drag-painting produces one undo entry per cell and undo is effectively unusable for the most common gesture. Project handling is last of the three because a single fixed save path is survivable in a way the other two are not.

## Stage 3 — Verify and harden Android

**The ship-gate verification**, then **persistence**.

The verification is the cheapest item in the entire backlog and it closes out a port that everything downstream assumes is good. Running it first means a failure surfaces before more is built on the assumption.

Persistence follows because it is what makes the context-loss policy honest. It is also the foundation the settings and save systems will need, so building it once with both consumers in mind costs the same as building it for either alone.

## Stage 4 — Unify the build

**CMake unification.**

This is the pivot of the roadmap. Nothing above it needs it; almost everything below it does. It is placed here rather than earlier for a specific reason: doing it before the Android port would have meant refactoring the build twice, once to reach parity for Windows alone and again to make it genuinely cross-platform. Now the cross-platform requirement is concrete, and verification is cheap because one target is known-good and the other reduces to a parity test.

Its secondary benefit is removing the divergence hazard where a new source file is added to one build and silently missed by the other.

## Stage 5 — Capabilities that unlock features

**Render targets**, **the dirty-state cache**, then **compute**.

Render targets first, because they block the largest class of features — shadows, reflections, any post-processing beyond the fixed resolve — and because the frontend already accepts a render target handle, so the work is confined to the backends.

The dirty-state cache is the cheapest remaining performance win and closes a real gap: the opaque list is already sorted so same-material draws are adjacent, but nothing below the pipeline level exploits it.

Compute last of the three. It is a larger feature than either, its gate is a single change to shader compilation, and nothing currently waits on it.

## Stage 6 — Further targets

**WebGL**, then **the scripted build**, then **further graphics backends**.

WebGL first because it is the strongest test of the abstraction after Android: it lacks compute, storage buffers, and indirect draw, so it forces the capability-gating paths to be genuinely exercised rather than merely present.

The scripted build follows because by then there are three targets, and building them by hand stops being reasonable.

Further graphics backends come last, and their order is open. Vulkan is the likely first, being cross-platform and applicable to Android as well.

## Deliberately unscheduled

- **Editor polish.** A list of follow-ups, none blocking anything. Pick from it when the tracker is otherwise in good shape.
- **Shader cross-compilation.** Deferred until a genuinely different shading language arrives. Building the pipeline before then is speculative work for targets that do not exist.
- **The Android file-system gaps.** Unimplemented deliberately, and only worth closing when an Android feature actually needs them.
- **The open design questions.** Resolve one when something forces it, not on a schedule. Resolving means writing the decision into the relevant Architecture doc.
