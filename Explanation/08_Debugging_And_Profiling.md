# 08 · Debugging And Profiling: UpdateBudget

**Status:** the tools below exist since S6 and are listed as they are built. The full profiling guide (Unreal Insights drill-downs, the cooling-down list cost of B2/B3, the sort cost at N = 4,000) is written in **Phases 7–8**, with data.

---

## What you can switch and see (as of S6)

| Tool | Command | What it shows | Where it is built |
|---|---|---|---|
| Mode | `-ini:Engine:[ConsoleVariables]:ub.Enabled=1` on the command line (or `ub.Enabled=1` in `DefaultEngine.ini`'s `[ConsoleVariables]`) | Budgeted mode for every world that begins play afterwards. Read once per world; S7 adds live switching | 05 F4, 06 §26 |
| Budget | `ub.BudgetMs 1.5` in the console; `ub.BudgetMs -1` returns to the settings value | Takes effect on the next frame | 05 G4 |
| On-screen line | `ub.Debug 1` | Per world, every frame: mode, clients, updates (mandatory, deferred), spent of budget (orange and `OVER` when over), max gap | 05 G4 |
| Cycle stats | `stat UpdateBudget` (Development builds) | `RunFrame` (the whole budgeted path) and `Deadlines` (the significance loop) | 05 G4, 06 §32 |
| The run's configuration | The log line `LogUpdateBudget: Display: <Budgeted\|Native> mode (ub.Enabled n). Budget … ms (settings …, ub.BudgetMs …), n categories. Update timer resolution: … ns.` | Proves which mode, budget and category count a run used. A warning follows for any inverted category | 05 G4 |
| Ini route check | `LogConfig: CVar [[ub.Enabled:1]] deferred - dummy variable created` | In the editor binary, the `-ini:` value arrived before the plugin registered the variable and was taken over (09, S5-d) | 06 §26 |

## The CSV capture

Every capture has `UpdateBudget/Clients`, `UpdatesRun` and `MaxGapMs`. A budgeted capture adds the scheduler's numbers: `UpdateMs`, `BudgetMs`, `MandatoryRuns`, `MandatoryMs`, `Deferred`, `OverBudget`, `NearClients` and `NearMissed`. It also adds the timing columns `UpdateBudget/GameThread/RunFrame` and `.../Deadlines`, and the metadata `ubmode` and `ubmaxstaleness` (02 §4.1).

How to read them together:
- **`RunFrame` − `UpdateMs`** is the scheduler's own cost per frame: deadlines, sort and timer reads.
- **`MandatoryMs` > `BudgetMs`** on an `OverBudget` frame means the staleness bound forced the overrun. Otherwise the predictions were too low (SC1).
- **`MaxGapMs`** above `ubmaxstaleness` + one frame is a staleness violation (SC2). `Tools/analyse_ub.py` checks every frame and every agent's end-of-run age.

`Tools/analyse_ub.py <batch>` writes these checks for a batch's UB runs into `Benchmarks/Summaries/<batch>_ub_check.md` (05 G5).
