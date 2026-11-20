# 03 · Implementation Roadmap: UpdateBudget

**Written:** end of Phase 3, 2026-11-20 · **Engine:** UE 5.8.4

**Rules for every step:**
- Each step is small enough to read in one sitting: a few hundred lines at most (Hard rule 6).
- A step is finished only when its code **compiles**, its check **passes**, and its `Explanation/` entries (`05`, `06`, and `07` where relevant) are **written in the same step** (Hard rule 13).
- After each step that adds C++: run `simplify`, then `code-review`.
- **The baselines are captured before the scheduler exists** (measurement rule, [Benchmarks/README.md](../Benchmarks/README.md)). Steps S1–S3 build only the workload and the native baselines. The scheduler starts at S4.

---

**Progress (2026-11-21):** Phase 4 ✅ (F1–F3). **S1 ✅**, **S2 ✅**, **S3 ✅**, **S4 ✅**, **S5 ✅**, **S6 ✅**, **S7 ✅** (each passed code review and fixes; S1 and S3–S7 also passed simplify). The first S3 baseline capture found the E-core problem (09 S3-g); the pinned re-capture (09 S3-i, batch `2026-11-21_015041_baseline_pinned`) is the baseline UpdateBudget is measured against, and it answered H1, H4 and H6. S5 connected the policy to real components: 14 automation tests pass, and the budgeted path reproduces B0's run exactly (09, S5). S6 added significance, settings and telemetry; the first UB run passed SC1–SC3 on one run, and showed that at a 2 ms budget UB's game-thread median sits above B2's (09, S6). S7 made the mode switch live, with exactly one update per client per frame through switches, and added the debug view; it also merged three boundary tests from UE5-TestTrust's mutation testing (09, S7). Next: **S8** (the Blueprint event, optional).

## Phase 4: Foundation (no gameplay code yet) ✅

| Step | What gets built | How it is checked |
|---|---|---|
| **F1** | `UpdateBudget\UpdateBudget.uproject` (EngineAssociation 5.8), `Source\UpdateBudget.Target.cs`, `UpdateBudgetEditor.Target.cs`, the game module (`UpdateBudget.Build.cs`, module `.h/.cpp`), `.gitignore` | `Build.bat UpdateBudgetEditor Win64 Development -Project=…` succeeds from the command line. Building from the command line rather than the editor's New Project wizard makes the setup reproducible and scriptable |
| **F2** | The plugin skeleton: `Plugins\UpdateBudgetCore\UpdateBudgetCore.uplugin`, `Build.cs`, module class, and the 5.8 `UE_API` export pattern | Builds; the editor lists the plugin as enabled |
| **F3** | `Config\DefaultEngine.ini` (`r.VSync=0` under `[SystemSettings]`, `bSmoothFrameRate=False`, default maps), an empty benchmark map, `DefaultGame.ini` | The editor opens the map (checked with the `run` skill); a packaged **Development** build launches and exits cleanly. *(Phase 4 finding: the launcher engine can't build Test, so headline benchmarks use packaged Development.)* |

## Phase 5: Implementation

| Step | What gets built | How it is checked | Explanation files touched |
|---|---|---|---|
| **S1. Workload, native path only ✅** | `UBudgetedUpdateComponent`, with its native-tick path (`TG_PrePhysics`), the time contract and update recording; `UUpdateBudgetSubsystem` (registry + per-frame CSV telemetry); `UUBDemoBrainComponent` (sense → decide → act, patrol from accumulated time, compute step); `AUBDemoAgent`; `UUBBenchmarkSubsystem` (seeded spawn; it replaces the planned spawner actor) | N = 100 / 1,000 agents spawn identically twice with the same seed (position hash matches). `stat unit` shows game-thread cost rising with N | 04, 05, 06, 07 |
| **S2. Benchmark harness ✅** | `UUBBenchmarkSubsystem` extended (it replaces the planned director actor): command-line values, an analytic camera path, the doubling wave, `CSV_EVENT(BenchStart)`, CSV metadata, final-position dump, and **compute-step calibration**; `Tools\csv_stats.py` (per-run median/p95/p99, median across runs + range, drops the CSV's last two rows); `Tools\run_bench.py` (packaged Development build, interleaved configs, priming run, environment record) | The script's output matches a hand calculation on a small test CSV. One fixed-step B0 run produces a CSV with the `UpdateBudget` and global columns | 05, 08, 09 |
| **S3. Baselines B0–B3, captured** | `UUBBaselineDriverSubsystem` (B2 distance → interval; B3 Significance Manager → interval, fetched in `OnWorldBeginPlay`), with its own timing scope for H6 | **The baseline capture.** B0, B1, B2, B3 × N = 100 / 1k / 2k / 4k × 4 interleaved runs (balanced rotation) + 1 priming run, fixed step, every run pinned to the P-cores, into `Benchmarks\Raw\`. Plus `tick.AddTestTickFunctions` for H1. H1, H4 and H6 are answered with data **before any scheduler code** | 05, 06, 09 |
| **S4. Pure policy + unit tests ✅** | `FBudgetClientState`, `FUpdateBudgetScheduler` (mandatory set, EDF, budget cut-off, EWMA) | Automation tests with fake clients and costs: the starvation bound holds; the budget is respected except for mandatory runs; deadline order holds; zero clients is fine; every client mandatory → all run and over-budget is flagged | 04, 05, 06, 09 |
| **S5. Subsystem, tick function, registry ✅** | `UUpdateBudgetSubsystem` (Game/PIE only), `FUpdateBudgetTickFunction` (registered in `OnWorldBeginPlay`, unregistered in `Deinitialize`), the parallel registry arrays, deferred add/remove, `IsValid` guard, the component's budgeted path. **Changed while building it:** `ub.Enabled` arrives here (read once at begin play), not in S6, because the budgeted path can't be built or tested without a way to select it; the client also follows `OnRegister`/`OnUnregister`; until S6, every max staleness is 0 (02 §13) | Tests in a real game world (the engine's `FTestWorldWrapper`): destroy an agent during its own update; spawn during the update loop; the streaming-unload order (components unregistered before `EndPlay`); no double update after re-registration or `Activate()`; world teardown right after a frame with deferred changes. "End PIE mid-frame" became that last test: the engine queues PIE end and travel between frames (02 §8). No crash, no stale entry. Plus: B0 and S5's budgeted path give identical final agent positions | 04, 05, 06, 07 |
| **S6. Significance, deadlines, settings, telemetry ✅** | Significance from distance and view → `MaxStaleness` → `Deadline`; `UUpdateBudgetSettings` and categories; cvars `ub.BudgetMs` and `ub.Debug` (`ub.Enabled` moved to S5); CSV stats; cycle stats; `UB` in `run_bench.py` | A first UB run at N = 2k: SC1/SC2 checked on a single run before the full matrix | 05, 06, 08 |
| **S7. Live switching + debug view ✅** | `ub.Enabled` change callback (switches every client between native and budgeted); debug draw tinted by update recency; `OnFrameStats` HUD | Switching modes mid-run causes no double updates and no missed agents (checked through the per-frame `MaxGapMs` and the end-of-run age report) | 05, 07 |
| **S8. (Optional) Blueprint event** | `ReceiveBudgetedUpdate` | Its cost per call is measured and documented, not assumed | 05, 06, 07 |

## Phases 6–10 (after the code exists)

| Phase | Work |
|---|---|
| 6 Explanation | Consolidate 04–07 into a single, consistent line-by-line pass. Check every API against 5.8.4 again |
| 7 Testing | Edge cases (0, 1 and 10k clients; budget 0; all agents mandatory; pause; time dilation; level streaming), stability (a 30-minute soak), scalability (N sweep) |
| 8 Profiling | The full matrix: 5 configs × 4 N × 5 interleaved runs, fixed step; a budget sweep of 1 / 2 / 4 ms; one uncapped confirmation run; Insights drill-downs. Optimisations happen only with data (`11_Optimization_Decisions.md`) |
| 9 Code review | The `code-review` and `simplify` skills, plus a written self-review: weaknesses, not praise |
| 10 Portfolio | Charts (`dataviz`), an architecture page (`artifact-design` / `artifact-diagramming`), the demo video, README results, interview Q&A |

**Rough matrix cost:** 100 runs × (about 15 s load + 10 s warm-up + 60 s capture) ≈ 2.5 hours, scripted and unattended.
