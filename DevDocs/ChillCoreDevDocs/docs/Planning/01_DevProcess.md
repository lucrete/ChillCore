# 01 — Dev Process

How planning work is tracked, and which document may reference which. Four kinds of document, each with one job.

## The documents

| Doc | Holds | Form |
| --- | --- | --- |
| `01_DevProcess.md` | This process. | — |
| `02_Roadmap.md` | Work that is in plan: committed and sequenced. | One table, execution order. |
| `03_TechBacklog.md` | Work not yet in plan: identified, not committed. | Summaries only. No implementation detail. |
| `RenderingPlan.md`, `AudioTrackerPlan.md`, `PlatformAndBuildPlan.md`, `BuildPipelinePlan.md`, `PersistencePlan.md` | Design and implementation detail for planned work. | Full detail: file references, step order, rationale. |

The per-workstream plans are collectively "the planning docs". Completed work leaves the planning docs for an Architectural Guide Doc under `docs/Architecture`.

## Lifecycle of a work item

1. **Identified.** A summary goes in `03_TechBacklog.md` under its workstream — one to three lines: what it is, why it is not scheduled.
2. **Ready to plan.** The item leaves the backlog. A row is added to `02_Roadmap.md` in sequence, and a section is written in the relevant planning doc. Detail is written when planning starts and grows as adjacent work surfaces more of it.
3. **Landed.** The planning-doc section becomes an AGD under `docs/Architecture`. The roadmap row and the planning-doc section are deleted.

## Invariant

`02_Roadmap.md` and `03_TechBacklog.md` are mutually exclusive. Every item is in exactly one.

- In the roadmap — it is planned. Detail lives in a planning doc. It is not in the backlog.
- In the backlog — it is not planned. It has no planning-doc section yet. It is not in the roadmap.

## References between documents

Which document may name which. The rules exist for one reason: nothing verifies a reference written in prose, so a pointer that goes stale sends a reader somewhere wrong with full confidence.

### An AGD does not reference another AGD

Architectural Guide Docs stand alone. An AGD must not cite another AGD, by number or by title.

Numbers get renumbered, and guides get merged and retired.

Where a boundary matters, state it — "does not cover the graphics abstraction beneath it" — rather than naming the guide on the other side. Where a decision in another guide matters, restate the part that matters in one sentence. Duplicating a sentence is cheaper than a dangling reference.

### A planning doc does not reference the roadmap or the backlog

The reference runs one way: the roadmap names the plan doc that carries a row's detail. A plan doc names neither the roadmap nor the backlog, and never cites a row number.

Row numbers are duplicated in every doc that cites one, so inserting a row silently invalidates every plan doc below it. Scheduling is also the roadmap's job alone — a plan that states its own position has two places to change when the order changes, and they disagree the moment one is missed.

State the substance instead: "the work that is in plan, in the order below", and "X is not in plan and is not covered here" rather than pointing at the doc where X sits.

### Everything else

- **Planning docs reference each other and AGDs freely.** They track work in motion, and the relationship between a plan and the guide it will become is the point.
- **The roadmap and the backlog reference each other and the plans.** Naming where an item goes next is their job.
- **`index.md` links to the guides.** Navigating to them is its job, and its markdown links fail loudly rather than silently.
- **Code comments cite nothing** — see `Guidelines/CodingGuidelines.md`.

## Where things go

- New work found mid-task — backlog summary.
- Implementation detail — the planning doc for that workstream. Never the backlog.
- A decision nobody needs to make yet — backlog, under open design questions.
- A resolved design decision — the relevant AGD.
- Verification a plan defines — that planning doc's own "Done when" or verification section.
