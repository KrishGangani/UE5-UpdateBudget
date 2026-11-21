# 08 · Debugging And Profiling: UpdateBudget

**Status:** written in Phase 6 for every tool that exists. Phase 8 adds the profiling results: the Insights drill-downs (the cooling-down list cost of B2/B3, the sort cost at N = 4,000) and the budget sweep, with data. Every command below was checked against the 5.8.4 source; the cited lines are where the engine parses it. Most of these tools exist only in Development builds (stats, `ensure`, debug drawing, automation tests), which is one reason the headline benchmarks use packaged **Development** (09, Phase 4).

---

## 1. Where to look first

| Symptom | First check | Then |
|---|---|---|
| A client updates twice in one frame | `ub.Debug 1`: in a frame where every client is due, updates equal clients. `dumpticks enabled`: in budgeted mode no brain's tick function may be listed | The `NoDoubleUpdate` and `LiveSwitch` tests (05 T4) reproduce the known ways it can happen |
| A client stops updating | The registry count (`UpdateBudget/Clients` in the CSV, or the debug line) against the agents alive | `IsRegisteredWithBudget()` on the client; the lifetime tests cover streaming, re-registration and destruction during an update |
| Staleness above the bound | `MaxGapMs` against `ubmaxstaleness` + one frame, per frame | `Tools/analyse_ub.py` checks every frame and every agent's end-of-run age (05 M5) |
| UB slower than expected | `RunFrame` − `UpdateMs` in the CSV: the scheduler's own cost | Unreal Insights (§7) for what is inside it |
| The budget seems ignored | `MandatoryMs` > `BudgetMs`: the staleness bound forced the overrun (SC1); otherwise the predictions were too low | 02 §12 items 1, 2 and 9: the budget is soft, mandatory work beats it, and spare budget is used on purpose |
| A run's mode or settings are in doubt | The begin-play log lines (§2) and the capture's metadata (`ubmode`, `ubmaxstaleness`, `ubbaseline`) | — |

## 2. Logs

| Category | Where | Lines worth knowing |
|---|---|---|
| `LogUpdateBudget` | The subsystem (05 P6b) | `<Budgeted\|Native> mode (ub.Enabled n). Budget … ms (settings …, ub.BudgetMs …), n categories. Update timer resolution: … ns.` once per world; a **warning** for each inverted category; `Switched to … mode at … s, n clients.` at a live switch; a **warning** once per class and world for a Blueprint client that implements Event Tick (05 P6e) |
| `LogUBBench` | The benchmark director (05 G3) | `Spawned n agents (seed s) in … ms; start-position hash 0x…`, `BenchStart at …`, `Wave: …`, `BenchEnd after …`, `Run report written: …`. Errors for invalid run settings or a failed spawn |
| `LogUBBaseline` | The B2/B3 driver (05 G4) | `Baseline B2 active: bands … / … cm, intervals … / … / … s.`, and errors for an unknown value or a missing Significance Manager |
| `LogConfig` | The engine | `CVar [[ub.Enabled:1]] deferred - dummy variable created`: the `-ini:` value arrived before the plugin registered the cvar, and was taken over (`ConfigUtilities.cpp:424`, 09 S5-d) |
| `LogScript` | The Blueprint VM | `Runaway loop detected (over 1,000,000 iterations)`: too many Blueprint steps since the last reset (05 T6, 06 §38) |

- `log list` prints every category and its verbosity; `log <Category> <Verbosity>` changes one at run time (`LogSuppressionInterface.cpp:591`).
- Every tool script writes the log to a known file with `-abslog=` (05 A8); `run_bench.py` counts a run's log errors and fails the run on any (05 M3).

## 3. Console variables

| Command | Effect | Where it is built |
|---|---|---|
| `-ini:Engine:[ConsoleVariables]:ub.Enabled=1` on the command line (or `ub.Enabled=1` in `DefaultEngine.ini`'s `[ConsoleVariables]`) | Budgeted mode from the world's begin play (06 §26) | 05 P6b, P6c |
| `ub.Enabled 0` / `ub.Enabled 1` in the console while playing | Every running world switches at the end of the next frame that ticks actors, never while paused. The log says so; the capture gets a `ModeSwitch` event and `ubmode = Mixed` | 05 P6d |
| `ub.BudgetMs 1.5`; `ub.BudgetMs -1` returns to the settings value | Takes effect on the next frame | 05 P6f |
| `ub.Debug 1` / `2` | The debug view (§4) | 05 P6h |

## 4. The debug view

| Command | What it shows | Where it is built |
|---|---|---|
| `ub.Debug 1` | Per world, every frame, one on-screen line: mode, clients, updates (mandatory, deferred), spent of budget (orange and `OVER` when over), max gap, near clients and missed | 05 P6h |
| `ub.Debug 2` | Also a point above every agent: green when just updated, yellow half way, red as stale as the settings allow. Natively every agent stays green (B0) or pulses with its interval (B2/B3); budgeted, far agents redden between updates | 05 P6h |
| `Subsystem->OnFrameStats.AddLambda([](const FUpdateBudgetFrameReport& Frame) { … })` | The same numbers as the CSV, once per frame, for a HUD or a tool | 05 P6a, P6h |

Neither is ever on in a benchmark: the text and the points are built only when the cvar asks for them.

## 5. Stats and ticks

| Command | What it shows | Source |
|---|---|---|
| `stat UpdateBudget` | The cycle stats `RunFrame` (the whole budgeted path) and `Deadlines` (the significance loop) | `STATGROUP_UpdateBudget`, 05 P6b, 06 §32 |
| `stat Unit` | Frame, game-thread, render-thread and GPU times | `UnrealEngine.cpp:2632` |
| `dumpticks`, `dumpticks grouped`, `dumpticks enabled`, `dumpticks disabled` | Every registered tick function, by its `DiagnosticMessage`, with its state: `Enabled`, `Disabled` or `Cooling Down for … seconds` (`TickTaskManager.cpp:1703–1705`). In budgeted mode the brains' tick functions are disabled, and the scheduler's is listed as `UUpdateBudgetSubsystem::RunFrame` (05 P4). In B2/B3 the cooling-down entries are the H6 cost | `UnrealEngine.cpp:5731`, options at `:6153–6162` |

## 6. The CSV capture

**How a benchmark capture is made.** The game opens and closes the capture window itself: `CSV_EVENT(UBBench, "BenchStart")` and `"BenchEnd"`, with `-csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd` on the command line (05 G3, M3). By hand, `csvprofile start` and `csvprofile stop` (or `csvprofile frames=N`) do the same from the console (`CsvProfiler.cpp:1118`, options at `:1059–1077`).

**The columns.** Every capture has `UpdateBudget/Clients`, `UpdatesRun` and `MaxGapMs`. A budgeted capture adds the scheduler's numbers (`UpdateMs`, `BudgetMs`, `MandatoryRuns`, `MandatoryMs`, `Deferred`, `OverBudget`, `NearClients`, `NearMissed`), the timing columns `UpdateBudget/GameThread/RunFrame` and `.../Deadlines`, and the metadata `ubmode` and `ubmaxstaleness`. B2/B3 captures add `UBBaseline/GameThread/Register`, `.../Retune` and `UBBaseline/IntervalChanges`. Timing columns include the thread name, so list a capture's last header row before writing analysis code (07).

How to read them together:
- **`RunFrame` − `UpdateMs`** is the scheduler's own cost per frame: deadlines, sort and timer reads.
- **`MandatoryMs` > `BudgetMs`** on an `OverBudget` frame means the staleness bound forced the overrun. Otherwise the predictions were too low (SC1).
- **`MaxGapMs`** above `ubmaxstaleness` + one frame is a staleness violation (SC2).
- **`NearMissed`** above 0 is a near client not updated in its frame (SC3).

`Tools/csv_stats.py` computes the statistics (05 M2), `Tools/analyse_baselines.py <batch>` answers H1, H4 and H6 (05 M4), and `Tools/analyse_ub.py <batch>` checks SC1–SC3 into `Benchmarks/Summaries/<batch>_ub_check.md` (05 M5).

## 7. Unreal Insights

**Tracing a run.** `-trace=<channels>` starts a trace at launch (`TraceAuxiliary.cpp:1952`), sent to a running Insights with `-tracehost=<address>` or written to a file with `-tracefile=<path>` (`:1980–2000`). The benchmark runs pass `-traceautostart=0` instead, so an open Insights can't trace them and add overhead (`01b` §4, `TraceAuxiliary.cpp:267–270`): a traced run is a separate, labelled run, never a measured one.

**Seeing this project's scopes.** `SCOPE_CYCLE_COUNTER` scopes (`RunFrame`, `Deadlines`) become Insights timing events only while named events are on (`Stats.h:562`, `:586`; off by default, `CoreGlobals.cpp:453`). `stats.NamedEvents on` (`CoreMisc.cpp:548–551`) or `stat NamedEvents` (`UnrealEngine.cpp:2628`) turns them on. Named events cost time themselves, so a run with them on is for looking, not for numbers.

**What Phase 8 looks at** (results go here and in 09, with the trace files' names): the tick manager's cooling-down list in B2/B3 (H6), the scheduler's sort and deadline loop at N = 4,000 (02 §12 items 5 and 6), and where a frame's game-thread time goes in each configuration.

## 8. Tests as debugging tools

`Tools\run_tests.bat UpdateBudget.Lifetime` (or any name prefix) runs one suite headless and exits with its verdict (05 A8). Each lifetime test reproduces one way the engine can change a component under the scheduler: destroying, spawning or re-registering during an update, streaming removal, a live switch requested inside an update, and world teardown (05 T4). When a bug is suspected, the fastest check is often a new test in that file: a real world, a few clients, exact frame counts.

## 9. Blueprint clients

- A Blueprint client's **Event Tick** runs only natively; the subsystem warns once per class and world (§2). Move the logic into the **Budgeted Update** event.
- Editor builds compile Blueprints with debug sites, the instructions the Blueprint debugger's breakpoints use (`KismetCompilerMisc.cpp:2502–2517`, `KismetCompilerVMBackend.cpp:1969–1975`), so a client's Budgeted Update event can be stepped through in PIE. The S8 measurement includes their cost (09 S8).
- `Runaway loop detected` in a test that makes many Blueprint calls in one frame: reset the VM's step count per batch with `GInitRunaway()`, as the world tick does every frame (05 T6).
