# 02 — Roadmap

Work that is in plan, in execution order. Summaries only — detail is in the planning doc named in each row. Work not yet in plan is in `03_TechBacklog.md`; the two are mutually exclusive (`01_DevProcess.md`).

Sequenced on dependency and cost, not product priority. Where priorities differ, this is the document to change.

| # | Work | Workstream | Plan doc | Notes |
| --- | --- | --- | --- | --- |
| 1 | Frame-rate dependent camera control | Rendering | `RenderingPlan.md` | Free-camera look and movement are paced per frame, not per second. Hours of work, user-visible, and independent of the rest of the block. |
| 2 | Dirty-state cache beyond pipelines | Rendering | `RenderingPlan.md` | Cheapest call-count win. |
| 3 | Compute shaders | Rendering | `RenderingPlan.md` | Backend surface is built. Gated on shader compilation recognising a `#shader compute` stage — that one change unblocks the rest. Ends with a compute demo state. |
| 4 | Frustum culling | Rendering | `RenderingPlan.md` | Nothing tests whether an object is on screen. Needs bounding volumes, which no renderable, mesh or scene object has. Precedes the pre-pass: it halves what that doubles. |
| 5 | Depth pre-pass for opaque geometry | Rendering | `RenderingPlan.md` | Removes overdraw without giving up the pipeline sort — the two cannot coexist in a single pass. Largest rendering item, and the only one needing a graphics-abstraction change first: pipelines have no colour write mask. |
| 6 | Shadow maps | Rendering | `RenderingPlan.md` | The largest item in the plan, and the only one that changes the frame sequence: the scene pass opens before geometry is submitted, so a shadow pass has nowhere to run today. Reuses the pre-pass's depth-only machinery. |
| 7 | Velocity editing | AudioTracker | `AudioTrackerPlan.md` | Primary editing affordance. The whole workstream is independent of rendering, platform, and build work. |
| 8 | Missing commands | AudioTracker | `AudioTrackerPlan.md` | Includes command coalescing — without it, drag-paint undo is unusable. |
| 9 | Project file handling | AudioTracker | `AudioTrackerPlan.md` | New / Open / Save As, in-engine project browser, auto-save on exit. |
| 10 | Android v1 ship-gate verification | Platform and build | `PlatformAndBuildPlan.md` | Cheapest item in the plan. Closes out a port everything downstream assumes is good; run it before more is built on the assumption. Now also covers the offscreen render-target path on device (AGD-0080), which is written but unverified there. |
| 11 | Persistence and lifecycle hooks | Persistence | `PersistencePlan.md` | Makes the Android context-loss cold-restart honest. Foundation for settings and save games — build once with both consumers in mind. |
| 12 | Automated testing | Build pipeline | `BuildPipelinePlan.md` | `RunTests.sh` beside `Build.sh` / `Run.sh`. First tests: engine starts, loads a scene, shuts down clean. |
| 13 | Packaged distribution | Build pipeline | `BuildPipelinePlan.md` | Archive of the self-contained output directory, labelled builds only. Cheapest once the output dir stands alone. |

## Sequencing notes

- **Rendering (1–6) is a block, scheduled first by priority, not dependency.** No rendering item blocks the tracker, Android, or build work.
- **4 before 5 is a preference, not a dependency.** The pre-pass submits opaque geometry twice and culling reduces what is submitted, so the two compound and the pre-pass measures honestly only with culling already in place. Either can be built first.
- **5 before 6 is the one dependency inside the block.** Shadows render depth-only from the light's point of view, which is the machinery the pre-pass introduces — the colour write mask and the depth-only pipelines. Building shadows first means building that twice.
- **6 is the largest item in the plan and the most reasonable to reorder against.** Six rendering rows now sit ahead of the tracker work. If the tracker matters more than shadows do, this is the row to move rather than the block to reshuffle.
- **AudioTracker (7–9) is independent** and can run in parallel with any other row, including the rendering block.
- **Across workstreams, one hard dependency: 11 before any Android context-loss hardening.** The recovery policy is a deliberate cold restart on the premise that persistence restores the user's place.
- **Within a workstream the order is dependency and cost.** Across workstreams it is a preference and can be reordered freely.
