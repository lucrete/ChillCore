# 02 — Roadmap

Work that is in plan, in execution order. Summaries only — detail is in the planning doc named in each row. Work not yet in plan is in `03_TechBacklog.md`; the two are mutually exclusive (`01_DevProcess.md`).

Sequenced on dependency and cost, not product priority. Where priorities differ, this is the document to change.

| # | Work | Workstream | Plan doc | Notes |
| --- | --- | --- | --- | --- |
| 1 | Compute shaders | Rendering | `RenderingPlan.md` | Backend surface is built. Gated on shader compilation recognising a `#shader compute` stage — that one change unblocks the rest. Ends with a compute demo state. |
| 2 | Velocity editing | AudioTracker | `AudioTrackerPlan.md` | Primary editing affordance. The whole workstream is independent of rendering, platform, and build work. |
| 3 | Missing commands | AudioTracker | `AudioTrackerPlan.md` | Includes command coalescing — without it, drag-paint undo is unusable. |
| 4 | Project file handling | AudioTracker | `AudioTrackerPlan.md` | New / Open / Save As, in-engine project browser, auto-save on exit. |
| 5 | Android v1 ship-gate verification | Platform and build | `PlatformAndBuildPlan.md` | Cheapest item in the plan. Closes out a port everything downstream assumes is good; run it before more is built on the assumption. Now also covers the offscreen render-target path on device (AGD-0080), which is written but unverified there. |
| 6 | Persistence and lifecycle hooks | Persistence | `PersistencePlan.md` | Makes the Android context-loss cold-restart honest. Foundation for settings and save games — build once with both consumers in mind. |
| 7 | Automated testing | Build pipeline | `BuildPipelinePlan.md` | `RunTests.sh` beside `Build.sh` / `Run.sh`. First tests: engine starts, loads a scene, shuts down clean. |
| 8 | Packaged distribution | Build pipeline | `BuildPipelinePlan.md` | Archive of the self-contained output directory, labelled builds only. Cheapest once the output dir stands alone. |
| 9 | OpenXR support, PCVR | XR | `XrPlan.md` | Four milestones ending in an in-headset interaction demo. Index through SteamVR, Quest 2 over Link. Its first milestone is engine work with no XR dependency: per-camera projection, a render view loop, and a native graphics-binding accessor. |

## Sequencing notes

- **Rendering (1) is scheduled first by priority, not dependency.** It blocks nothing in the tracker, Android, or build work.
- **AudioTracker (2–4) is independent** and can run in parallel with any other row, including the rendering row.
- **Across workstreams, one hard dependency: 6 before any Android context-loss hardening.** The recovery policy is a deliberate cold restart on the premise that persistence restores the user's place.
- **Within a workstream the order is dependency and cost.** Across workstreams it is a preference and can be reordered freely.
- **XR (9) is last by sequencing, not by dependency.** It blocks nothing and nothing blocks it. Its first milestone touches the camera, the rendering frontend and the platform layer, so it does not want to run concurrently with other rendering work.
