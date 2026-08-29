# 01 — Dev Process

How planning work is tracked. Four kinds of document, each with one job.

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

## Where things go

- New work found mid-task — backlog summary.
- Implementation detail — the planning doc for that workstream. Never the backlog.
- A decision nobody needs to make yet — backlog, under open design questions.
- A resolved design decision — the relevant AGD.
- Verification a plan defines — that planning doc's own "Done when" or verification section.
