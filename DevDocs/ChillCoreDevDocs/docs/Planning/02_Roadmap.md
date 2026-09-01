# 02 — Roadmap

Work that is in plan, in execution order. Summaries only — detail is in the planning doc named in each row. Work not yet in plan is in `03_TechBacklog.md`; the two are mutually exclusive (`01_DevProcess.md`).

Sequenced on dependency and cost, not product priority. Where priorities differ, this is the document to change.

| # | Work | Workstream | Plan doc | Notes |
| --- | --- | --- | --- | --- |
| 1 | Post-processing follow-ups | Rendering | `RenderingPlan.md` | Baking the grade and tone map into a lookup table, then photometric light units. The lookup table needs no new render-target work — the 2D strip route is available now. |
| 2 | Frame-rate dependent camera control | Rendering | `RenderingPlan.md` | Free-camera look and movement are paced per frame, not per second. Hours of work, user-visible, and independent of the rest of the block. |
| 3 | Dirty-state cache beyond pipelines | Rendering | `RenderingPlan.md` | Cheapest call-count win. |
| 4 | Compute shaders | Rendering | `RenderingPlan.md` | Backend surface is built. Gated on shader compilation recognising a `#shader compute` stage — that one change unblocks the rest. Ends with a compute demo state. |
| 5 | Velocity editing | AudioTracker | `AudioTrackerPlan.md` | Primary editing affordance. The whole workstream is independent of rendering, platform, and build work. |
| 6 | Missing commands | AudioTracker | `AudioTrackerPlan.md` | Includes command coalescing — without it, drag-paint undo is unusable. |
| 7 | Project file handling | AudioTracker | `AudioTrackerPlan.md` | New / Open / Save As, in-engine project browser, auto-save on exit. |
| 8 | Android v1 ship-gate verification | Platform and build | `PlatformAndBuildPlan.md` | Cheapest item in the plan. Closes out a port everything downstream assumes is good; run it before more is built on the assumption. Now also covers the offscreen render-target path on device (AGD-0080), which is written but unverified there. |
| 9 | Persistence and lifecycle hooks | Persistence | `PersistencePlan.md` | Makes the Android context-loss cold-restart honest. Foundation for settings and save games — build once with both consumers in mind. |
| 10 | Automated testing | Build pipeline | `BuildPipelinePlan.md` | `RunTests.sh` beside `Build.sh` / `Run.sh`. First tests: engine starts, loads a scene, shuts down clean. |
| 11 | Packaged distribution | Build pipeline | `BuildPipelinePlan.md` | Archive of the self-contained output directory, labelled builds only. Cheapest once the output dir stands alone. |

## Sequencing notes

- **Rendering (1–4) is a block, scheduled first by priority, not dependency.** No rendering item blocks the tracker, Android, or build work, and no rendering item blocks another.
- **AudioTracker (5–7) is independent** and can run in parallel with any other row, including the rendering block.
- **Across workstreams, one hard dependency: 9 before any Android context-loss hardening.** The recovery policy is a deliberate cold restart on the premise that persistence restores the user's place.
- **Within a workstream the order is dependency and cost.** Across workstreams it is a preference and can be reordered freely.
