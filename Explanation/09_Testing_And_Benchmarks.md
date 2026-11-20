# 09 · Testing And Benchmarks: UpdateBudget

**Rule:** every check is recorded with its date, its exact command and its result, **including failures**. The full functional and benchmark sections are filled in during Phases 7–8. Results from earlier phases are recorded as they happen.

---

## Phase 4 · Foundation checks (2026-11-20)

| # | Check | Command (from `P01_UpdateBudget\`) | Result |
|---|---|---|---|
| F-1 | Editor target builds | `Tools\build.bat` (`UpdateBudgetEditor Win64 Development`) | ✅ Succeeded in 57.6 s. 12 actions, **0 errors, 0 warnings**. Toolchain: MSVC 14.44.35229, Windows SDK 10.0.22621.0 |
| F-2 | Uncooked game loads the map and quits | `Tools\smoke_game.bat` (`UnrealEditor-Cmd -game -nullrhi … -ExecCmds="quit"`) | ✅ Exit 0. `UpdateBudget` and `UpdateBudgetCore` load before `LoadMap`; `Template_Default` loads in 0.152 s. **0 errors.** Warnings are engine-side `LogEditorDataStorageUI` only (editor binary in `-game`) |
| F-3a | Package a **Test** build | `Tools\package.bat Test` | ❌ **Failed by design of the engine distribution:** "Targets cannot be built in the Test configuration with this engine distribution." → headline benchmarks use packaged **Development** (`01` §7, `01b` §7) |
| F-3b | Package a **Development** build | `Tools\package.bat` | ✅ BUILD SUCCESSFUL in 142.6 s; 516 packages cooked; **0 errors, 0 warnings**. Output `Build\Windows\UpdateBudget.exe` (≈ 983 MB staged, git-ignored) |
| F-3c | Packaged build runs without a GPU | `Build\Windows\UpdateBudget.exe -nullrhi -nosound -unattended -traceautostart=0 -ExecCmds="quit"` | ✅ Exit 0; map loads; **0 errors, 0 warnings** |
| F-3d | Packaged build runs on the GPU | `… -windowed -ResX=960 -ResY=540 …` | ✅ Exit 0; D3D12, SM6; **chosen adapter 0 = NVIDIA GeForce RTX 5070 Laptop GPU** (driver 610.47), not the integrated Intel GPU; **0 errors, 0 warnings** |

**What F-3a taught us:** check what the engine distribution can build *before* writing the benchmark plan around it. The fix was a one-line config change plus an honest documentation update, made before any benchmark relied on the Test configuration.

---

## Step S1 · Workload checks (2026-11-20)

All runs use the uncooked editor binary in `-game -nullrhi -deterministic -FPS=60` mode. These are **sanity checks, not headline numbers.**

| # | Check | Result |
|---|---|---|
| S1-a | **Determinism:** `-UBAgents=1000 -UBSeed=7` twice; `-UBSeed=8` once | ✅ Seed 7 gave `0xCDBA5809` in both runs; seed 8 gave `0x214C8107` (different, as it should be). Unchanged after the simplify audit's switch to an incremental CRC, which proves it bit-identical to the old buffered one |
| S1-b | **Telemetry columns exist and are right** | ✅ `UpdateBudget/Clients` = N, `UpdateBudget/UpdatesRun` = N every frame, `UpdateBudget/MaxGapMs` = 16.667 (one fixed 1/60 s step) |
| S1-c | **First update counted** (code-review fix) | ✅ The first frame with updates shows `UpdatesRun` = 100 / 1,000 with `MaxGapMs` = 0 |
| S1-d | **Cost scales with N** | ✅ Median `FrameTime` 0.80 ms at N = 100 vs 3.07 ms at N = 1,000 (300 frames, first 30 dropped). Sanity only: uncooked editor binary, no RHI |
| S1-e | **No errors** | ✅ 0 errors and 0 warnings from our log categories in all S1 logs |
| S1-f | **Build** | ✅ 0 errors, 0 warnings after every change (MSVC 14.44.35229) |

**Findings that change later steps:**
1. **`GameThreadTime` is 0 under `-nullrhi`.** The engine fills it from per-thread frame stats that need a real RHI. Headline runs therefore use the real GPU at a fixed small resolution, verified in S2.
2. **The editor binary in `-game` writes CSVs to `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Profiling\CSV`**, not to the project. S2's harness must locate or redirect captures explicitly.
3. **The CSV profiler's own startup benchmark measured about 50 ns per `CSV_CUSTOM_STAT`** (and about 74 ns per timing stat). This is measured evidence for publishing once per frame.

**Audit trail for S1:**
- `code-review` (medium): 9 findings (4 important, 5 nits). All important ones fixed and re-tested (S1-c). Nits fixed or documented (multi-world CSV caveat in the header).
- `simplify` (4 parallel angles): 13 changes, all behaviour-preserving, verified by S1-a.
- Skipped on purpose:
  - caching the player camera per frame (keeps the workload realistic);
  - asynchronous spawning (capture starts after warm-up);
  - deduplicating the two one-line `DoesSupportWorldType` overrides (would need a new class).

## Step S2 · Harness checks and workload calibration (2026-11-20)

All runs use the **packaged Development build**, real GPU (RTX 5070 Laptop, adapter 0), 1280×720 windowed, fixed step `-deterministic -FPS=60`, on AC power with the power plan "Turbo".

| # | Check | Result |
|---|---|---|
| S2-a | **End to end** (N = 1,000, warm-up 3 s, capture 10 s) | ✅ `BenchStart` at exactly 3.000 s (frame 179); capture 10.000 s = 600 frames; run report written; CSV written; self-exit with code 0; **0 errors, 0 warnings** |
| S2-b | **Cross-build determinism** | ✅ Seed 1, N = 1,000 gives hash `0x7F189215` in both the editor and the packaged build |
| S2-c | **Time contract visible in the report** | ✅ `patrol_time` 12.966667 = `last_update` 13.000001 − first update (frame 2 = 0.0333 s) |
| S2-d | **`GameThreadTime` with a real RHI** | ✅ Populated (median 3.74 ms at N = 1,000, W = 64). Render thread 3.81 ms and GPU 3.43 ms were similar, which is why calibration matters (S2-f) |
| S2-e | **Wave split** (N = 500, wave at 5 s) | ✅ Split at frame 299; `UpdatesRun` median 500 before, 1,000 after; the wave's spawn frame cost 15.0–16.1 ms and is reported separately, never mixed into steady state |
| S2-f | **Calibration of the compute step** (B0, N = 1,000, 2 interleaved runs each, batch `2026-11-20_233941_calib`) | See the table below |
| S2-g | **PSO events** | 2–5 per run, on frames 1 and 14, which took 4.34 and 3.71 ms (below the 4.67 ms median). So: **no hitch** (asynchronous precaching). Counted in every manifest so a future change that turns them into hitches is visible |
| S2-h | **Tool self-tests** | ✅ `csv_stats.py --selftest` (percentiles 5.5 / 9.55 / 9.91 on 1…10; event parsing) |

**S2-f calibration** (median across 2 runs [min–max]):

| Compute iterations | GameThreadTime median (ms) | FrameTime median (ms) | Bound by |
|---|---|---|---|
| 0 | 2.460 [2.405–2.514] | 3.988 | render/GPU (FrameTime > GameThreadTime) |
| 64 | 3.873 [3.868–3.879] | 3.961 | about balanced |
| **128** | **5.211 [5.209–5.214]** | **5.229** | **game thread** |
| 256 | 7.688 [7.625–7.752] | 7.711 | game thread |

**Decision: 128 iterations**, now the brain's default.
- The slope is about **20 ns per iteration per agent**, linear.
- At 128 the frame is game-thread bound, so a game-thread improvement shows in frame time.
- One update costs about 5.2 µs.
- The native baseline should cross 16.67 ms between N = 2,000 and 4,000, inside the test range.
- Run-to-run spread was ≤ 0.13 ms on medians, which makes the protocol's "ranges must not overlap" rule practical.

**Audit trail for S2:** `code-review` (medium), 10 findings (6 important, 4 minor), all fixed and re-verified by S2-a, S2-e and S2-h. Two further tool bugs were found by the checks themselves (metadata keys; end-header rule) and fixed.


## Step S3 · Baseline drivers B2/B3 (2026-11-21)

Same build and environment as S2 (packaged Development, real GPU, 1280×720, fixed step, AC power, power plan "Turbo").

| # | Check | Result |
|---|---|---|
| S3-a | **Build** | ✅ Development game target: 0 errors, 0 warnings. Packaged Development: `BUILD SUCCESSFUL`, cook `0 error(s), 0 warning(s)` |
| S3-b | **Every configuration starts in the right mode** (smoke batch `2026-11-21_000138_s3smoke`: N = 1,000, 1 run + priming, warm-up 3 s, capture 8 s) | ✅ B1's log shows `tick.AllowBatchedTicks = "1"`. B2 and B3 logs show `Baseline B2 active.` / `Baseline B3 active.`, and the captures carry the metadata `ubbaseline = B2` / `B3`. 0 log errors in every run |
| S3-c | **B2 and B3 behave identically** (they share bands, view and setter) | ✅ Over the 480 captured frames: 2,382 band changes in both, 68,024 updates in both (median 136 per frame, against 1,000 in B0/B1), identical per-agent ages at the end of the run (median 66.7 ms, max 550 ms) and identical state changes (647; 651 in B0/B1) |
| S3-d | **The S3 clean-up changed no behaviour** (batch `2026-11-21_001816_s3smoke2`, after the simplify fixes, against S3-b) | ✅ Every count in S3-c is identical in both batches, for all four configurations. No `ensure` fired |
| S3-e | **The re-tune cost is measured** | ✅ `UBBaseline/GameThread/Retune` at N = 1,000: median 0.105 ms (B2), 0.139 ms (B3); p99 0.49 ms (B2), 0.51 ms (B3). `Register` is near zero after the first frame. One run each: a sanity check, **not** evidence |
| S3-f | **The baseline matrix, first capture** | Batch `2026-11-21_002119_baseline`: B0–B3 × N = 100 / 1,000 / 2,000 / 4,000, warm-up 10 s, capture 60 s. Started with 6 runs, then **stopped cleanly after round 4** when the protocol changed to 4 balanced runs (with 4 configs, rounds 1–4 put every config in every order position once). 64 measured runs + priming, 0 failures, 0 log errors. **B0/B1 at N ≥ 1,000 are not usable as evidence** (S3-g); B2/B3 behaviour and the H4/H6 results are (below) |
| S3-g | **Finding: game thread on an E-core** | B0/B1 runs sat at two stable levels about 1.6× apart (table below), and one run stepped between them mid-capture. This i7-14650HX is hybrid (8 P-cores + 8 E-cores), and Windows sometimes ran the game thread on an E-core. **Fix:** every run is pinned to the P-cores (mask `0xFFFF`), recorded per run (05 E5) |
| S3-h | **The fix, validated** | Batch `2026-11-21_012231_affinitycheck` (B0, B1 × N = 2,000 / 4,000 × 4, capture 20 s, pinned): the two levels are gone (table below) |
| S3-i | **The baseline matrix, re-captured pinned** | ✅ Batch `2026-11-21_015041_baseline_pinned`: B0–B3 × N = 1,000 / 2,000 / 4,000 × 4 interleaved runs + priming, warm-up 10 s, capture 60 s, every run pinned (`0xFFFF`, read back). 48 measured runs, **0 failures, 0 log errors**, 5 PSO events per run (no hitch, S2-g). **The two-level pattern is gone:** cut into 10 parts, no B0/B1 run varies by more than 1.19× between its parts (the first capture stepped by 1.6×). 22 of 24 stay within 1.09×; the two exceptions are round 1 at N = 1,000 (1.19× and 1.12×), noisy rather than stepped. Results in the table below; this batch is **the baseline UpdateBudget is compared against** (SC4) |

**Smoke-batch GameThreadTime medians (ms).** One run per configuration, so these only show that each mode works. They are **not results**: run-to-run spread is unknown at this sample size, and the two batches disagree on whether B2 or B3 is cheaper.

| Config | `s3smoke` | `s3smoke2` |
|---|---|---|
| B0 native, every frame | 4.949 | 5.161 |
| B1 batched ticks | 4.689 | 4.734 |
| B2 distance bands | 2.174 | 2.394 |
| B3 Significance Manager | 2.367 | 2.138 |

**Note on the 550 ms maximum age (S3-c):** it is above the Far interval (0.5 s). It is the age at the **end** of the run, so it includes the frames between an agent's last update and the report, and the delay before a band change takes effect. Whether the tick manager adds a further cooldown delay is part of the H3/H4 analysis on the full matrix, not assumed here.

**Audit trail for S3:**
- `code-review`: fixed unregistering per agent on `OnEndPlay` and in `OnWorldEndPlay`; separate `Register` and `Retune` timing; the `InitializeDependency` order; skipping null brains; exact band edges; squared distances in B2; corrected header comments.
- `simplify` (4 parallel reviews: reuse, simplification, efficiency, altitude). Applied:
  - one `GetSignificanceManager()` rule;
  - `Retune()` with early returns;
  - the lambdas defined once;
  - `GetRangePct` and `GetCameraCacheView`;
  - B3's old/new band check;
  - the cached benchmark subsystem pointer;
  - `RemoveSingleSwap` and `Reserve`;
  - B3's brains kept only in the manager;
  - one shared band test;
  - the append-only contract and its `ensure`.
- Skipped, with reasons:
  - Pre-building the delegate binding: `AddDynamic` is the idiomatic code, and it runs only on adoption frames, which are timed separately.
  - Caching the Significance Manager pointer: it is removed during world cleanup.
  - Iterating `GetAgents()` in B2 instead of `Brains`: it would add a pointer chase per agent per frame.
  - Changing the brain's per-agent camera lookup: it is part of the calibrated workload.
- Re-verified by S3-a and S3-d.


### S3 batch: what it shows

**S3-g evidence.** Each B0/B1 run cut into 10 equal parts, median `GameThreadTime` (ms) per part (`2026-11-21_002119_baseline`):

| Run | Parts 1 → 10 | Level |
|---|---|---|
| B0 N = 4,000 r1 | 16.8 17.1 18.5 **26.9** 26.3 27.7 26.4 26.9 26.4 26.8 | P-core, then **steps** to the slow level |
| B0 N = 4,000 r3 | 29.2 27.8 28.7 28.8 27.9 27.7 28.3 28.2 29.2 28.3 | slow (E-core) |
| B0 N = 4,000 r4 | 16.7 16.5 16.8 16.6 16.5 16.2 16.4 16.3 16.4 16.5 | fast (P-core) |
| B1 N = 2,000 r1 / r2 | 8.6 … 9.3 / 13.5 … 12.8 | fast / slow |

Flat within each part and a clean step between levels, about 1.6× apart, is the signature of a thread moving between core types. Thermal throttling would show as a gradual ramp. Rounds 1–2 ran before the other projects' agents started and already show both levels, so the agents' activity is not the cause.

**S3-h, pinned to the P-cores** (`GameThreadTime` median, ms; 4 runs each):

| | First capture (not pinned) | Pinned |
|---|---|---|
| B0 N = 2,000 | 9.6 – 16.2 | **9.09 – 9.69** |
| B0 N = 4,000 | 16.5 – 28.3 | **16.2 – 18.0** |
| B1 N = 2,000 | 8.8 – 14.0 | **8.14 – 9.06** |
| B1 N = 4,000 | 15.0 – 24.7 | **14.8 – 15.9** |

The remaining spread of 5–10% drifts slightly upwards over the batch (laptop thermals); interleaving spreads it evenly over the configs.

**S3-i, the pinned baseline matrix** (`2026-11-21_015041_baseline_pinned`, `GameThreadTime` in ms, median across 4 runs [min–max across runs]; summary and analysis in `Benchmarks/Summaries/`):

| Config | N | median | p95 | p99 |
|---|---|---|---|---|
| B0 native, every frame | 1,000 | 5.772 [5.727–6.480] | 6.779 [6.715–7.767] | 7.161 [6.993–8.565] |
| | 2,000 | 10.016 [9.813–10.298] | 11.032 [10.696–11.948] | 11.575 [11.223–12.943] |
| | 4,000 | 17.322 [17.151–17.670] | 18.735 [18.478–19.392] | 19.746 [19.276–20.720] |
| B1 batched ticks | 1,000 | 5.400 [5.345–5.726] | 6.445 [6.358–6.846] | 6.841 [6.664–7.372] |
| | 2,000 | 9.221 [9.169–9.717] | 10.301 [10.024–11.041] | 10.943 [10.617–11.705] |
| | 4,000 | 16.155 [15.476–16.349] | 17.586 [16.748–17.791] | 18.635 [17.661–18.849] |
| B2 distance bands | 1,000 | 2.288 [2.208–2.405] | 3.159 [3.047–3.414] | 3.846 [3.663–4.060] |
| | 2,000 | **2.641** [2.578–2.733] | **3.623** [3.551–3.729] | 5.660 [5.412–5.824] |
| | 4,000 | **3.312** [3.260–3.416] | **4.511** [4.323–4.612] | 12.445 [11.926–12.674] |
| B3 Significance Manager | 1,000 | 2.353 [2.169–2.523] | 3.229 [2.997–3.495] | 3.942 [3.739–4.230] |
| | 2,000 | 2.766 [2.758–2.780] | 3.691 [3.664–3.714] | 5.579 [5.508–5.808] |
| | 4,000 | 3.488 [3.369–3.538] | 4.556 [4.280–4.771] | 12.762 [12.491–13.177] |

What it shows:
- **The best baseline at N ≥ 2,000 is B2** by median and p95 (bold), the numbers SC4 asks UpdateBudget to beat. At N = 2,000 its median range sits below B3's without overlap; at N = 4,000 the two ranges overlap, so B2 and B3 are equally good there.
- **B0 crosses the 16.67 ms frame at N = 4,000** (median 17.3 ms), as the S2 calibration predicted.
- **The interval baselines' p99 is the H4 spike:** at N = 4,000 their p99 (12.4–12.8 ms) is almost 4× their median, against 1.14× for B0.
- **Run-to-run spread is small except in round 1.** B0 at N = 1,000 has one high run (r1, 6.48 ms against 5.73–5.80 for the other three). That run has no step, only noisier parts (5.9–7.0 ms). The drift check finds rounds 1–2 about 0.2–0.4 ms slower than rounds 3–4 for B0/B1 at N ≤ 2,000. The cause is not known; the interleaving spreads it over every config.

**H4 · periodic update spikes (B2/B3): confirmed.** The per-frame update count repeats with the band intervals. The counts are identical in every run (fixed step, same camera path), and identical between B2 and B3:

| N | UpdatesRun median | max | max / median | autocorrelation at 0.1 s (lag 6) | at 0.5 s (lag 30) |
|---|---|---|---|---|---|
| 100 | 41 | 63 | 1.54 | 0.99 | 0.95 |
| 1,000 | 130 | 691 | 5.32 | 0.87 | 0.97 |
| 2,000 | 172 | 1,363 | 7.92 | 0.30 | 0.98 |
| 4,000 | 209 | 3,107 | 14.87 | 0.05 | 0.99 |

Agents that got the same interval at the same moment stay in phase, so every 0.5 s a large group ticks together: at N = 4,000 that is 3,107 updates in one frame against a median of 209. This explains why B2/B3's p99 `GameThreadTime` at N = 4,000 (about 17–18 ms) is far above their median (3.6–3.9 ms). It is exactly the frame-time spike UpdateBudget's deadline spreading should remove (SC1, SC4). **This is a single-machine measurement** of one workload, and the spike size depends on how the agents enter their bands.

**H6 · re-tune cost (B2/B3)** (`UBBaseline/GameThread/Retune`, ms, median [min–max] over 4 runs):

| N | B2 median | B2 p99 | B3 median | B3 p99 | Band changes per frame |
|---|---|---|---|---|---|
| 100 | 0.009 | 0.019 | 0.024 | 0.065 | 0.08 |
| 1,000 | 0.079 | 0.312 | 0.110 | 0.363 | 0.90 |
| 2,000 | 0.165 | 0.531 | 0.237 | 0.619 | 1.55 |
| 4,000 | 0.407 | 1.020 | 0.432 | 1.121 | 2.29 |

The driver's own re-tuning grows roughly linearly with N, at about 0.1 µs per agent per frame. The Significance Manager path (B3) costs a little more at low N. The tick manager's cooling-down list cost is inside `GameThreadTime`, not in this column; Insights is used for it in Phase 8. (This table is from the first capture. The pinned re-capture gives Retune medians of 0.058 / 0.148 / 0.365 ms for B2 and 0.095 / 0.194 / 0.410 ms for B3 at N = 1,000 / 2,000 / 4,000, the same picture; the H4 update counts are identical in both batches, as a fixed step requires.)

**H1 · what tick batching saves (B0 − B1), from the pinned re-capture** (`GameThreadTime` median, per-run differences paired by run index, median [min–max]):

| N | B0 − B1 (ms per frame) | Per tick |
|---|---|---|
| 1,000 | 0.382 [0.362–0.754] | 0.38 µs |
| 2,000 | 0.647 [0.492–1.029] | 0.32 µs |
| 4,000 | 1.326 [1.004–1.675] | 0.33 µs |

**H1 is supported:** per-tick dispatch has a cost, and engine batching removes about 0.3–0.4 µs of it per tick, growing linearly with N. This is a lower bound on the dispatch overhead, because batching doesn't remove all of it. It agrees with Epic Games Japan's published figure of about 0.3 µs per tick (`01b` §5), which was measured in a DevelopmentEditor build; ours is a packaged Development build. It is only 6–8% of B0's frame: the real cost at this workload is the updates themselves, which is why the interval baselines (B2/B3) are 2.5× (N = 1,000) to 5× (N = 4,000) cheaper than B0. (The first, unpinned capture gave 0.454 ms at N = 1,000, carrying the E-core problem.)

---

## Step S4 · The scheduling policy (2026-11-21)

| # | Check | Result |
|---|---|---|
| S4-a | **Build** | ✅ Development editor target, 0 errors, 0 warnings (`UpdateBudgetScheduler.cpp` and the test file compiled; 4.2 s with the build accelerator) |
| S4-b | **Automation tests** (`Tools\run_tests.bat UpdateBudget.Scheduler`, headless, through the job queue) | ✅ 8 / 8 passed: NoClients, DeadlineOrder, BudgetCutOff, MandatoryBeatsBudget, ZeroBudget, CostPrediction, CategoryFallback, NoStarvation. `TEST COMPLETE. EXIT CODE: 0` |
| S4-c | **After the code-review and simplify fixes** (NaN guard in the constructor and in Run, real overload in NoStarvation, raw-pointer comparator, Lerp, MandatoryRuns set once; AndroidFileServer disabled) | ✅ Rebuild: 0 errors, 0 warnings. Tests: **8 / 8 passed**, `TEST COMPLETE. EXIT CODE: 0`. The config was not re-written with a `SecurityToken`. The log showed one tool warning (`-ReportOutputPath=` is deprecated); `run_tests.bat` now passes `-ReportExportPath=` (verified at `AutomationControllerManager.cpp:233`), and the next test run (S5) exercises it |

**Audit trail for S4:**
- `code-review` (medium) raised 9 findings: 3 important, 6 minor.
  - The important ones: the one-run default, H1 pairing by list position, and a game left running after Ctrl+C.
  - The minor ones: the per-run launch record, workloads pooled in the analysis, a NaN cost poisoning the moving average, a starvation test that never overloaded, missing docs, and 02's diagram not matching `Run`.
- All nine are fixed (05 E2/E3/E5/E6, 02 §6).
- One more bug was found by P03 (UE5-TestTrust): `start /wait` always returns 0. Also fixed (05 E5, 07).
- `simplify` (4 parallel reviews):
  - **Scheduler:**
    - one `SanitiseCostMs` for the constructor and `Run` (the constructor was a second way in for NaN);
    - `MandatoryRuns = NumMandatory`;
    - raw pointer reads in the sort comparator;
    - `FMath::Lerp`;
    - array `Init` in the NoStarvation test.
  - **`run_bench.py`:**
    - the batch pins itself once and the games inherit the mask;
    - one Windows-guarded `KERNEL32` setup;
    - a one-pass core-mask builder;
    - `math.ceil`;
    - the docstring no longer states the runs rule wrongly.
  - **`analyse_baselines.py`:**
    - reuses `csv_stats.column_values` and `summarize`, and the manifest's own statistics;
    - `table()`/`col()` helpers;
    - `LAGS` drives the H4 columns.

    Its output was verified **identical, byte for byte,** on the 64-run baseline batch.
  - **Skipped, with reasons:**
    - Keeping last frame's sort order as the starting point (measured about 2× faster sorting in a clang copy): the real subsystem recomputes every deadline each frame, so the gain must be measured in Phase 8 (11).
    - `Algo::LowerBoundBy` for the mandatory prefix: the scan costs 1–3 µs and is clear.
    - Sorting (deadline, index) pairs: only 3–6% faster.
    - A shared `cell()` in `csv_stats`: outside this change.
    - `launch()` returning a dict: no longer needed once the 5-tuple went away.

---

## Step S5 · Tick function, registry and budgeted path (2026-11-21)

| # | Check | Result |
|---|---|---|
| S5-a | **Build** | ✅ Development editor target, **0 errors, 0 warnings** (MSVC 14.44.35229; 18 actions, 11.8 s). Every changed source was older than the job start, so the build compiled the final code, after the code-review and simplify fixes. Afterwards only two code comments changed (corrected line citations: `ActorComponent.cpp:1747`, `Actor.cpp:4824`); S6's build compiles them |
| S5-b | **All automation tests** (`Tools\run_tests.bat UpdateBudget`, headless, through the job queue) | ✅ **14 / 14 passed**, `TEST COMPLETE. EXIT CODE: 0`, 0 errors and **0 warnings** in the log. Lifetime (6): `ModeAtBeginPlay`, `DestroyDuringUpdate`, `SpawnDuringUpdate`, `LevelUnloadOrder`, `NoDoubleUpdate`, `WorldTeardown`. Scheduler (8): unchanged from S4, all pass. The deprecated `-ReportOutputPath` warning of S4-c is gone (`-ReportExportPath` verified) |
| S5-c | **The timer** | The subsystem's begin-play line reads `Update timer resolution: 100.0 ns` on this machine: `QueryPerformanceCounter` runs at 10 MHz. One demo update (about 5.2 µs, S2-f) spans about 52 ticks, so a single measurement is quantised to about ±2%; the moving average smooths it |
| S5-d | **Budgeted path = native path** (uncooked editor `-game -nullrhi`, fixed step, N = 1,000, seed 1, warm-up 1 s, capture 5 s; run tags `s5eq_B0` and `s5eq_UB`, one queue job, 20.4 s) | ✅ **The two per-agent run reports are identical**: all 1,000 rows (final position, patrol time, last update time, age at report, state changes; 500 state changes in both). Same start hash `0x7F189215` as S2-b. Both exited normally with 0 errors; the 21 warnings in each log are all the engine's `LogEditorDataStorageUI` (editor binary in `-game`, as in F-2). **The `-ini:` route works as read in the source:** the budgeted log shows `CVar [[ub.Enabled:1]] deferred - dummy variable created` (`ConfigUtilities.cpp:424`), then `Budgeted mode (ub.Enabled 1)` once the plugin registered the variable. The native run shows `Native mode (ub.Enabled 0)` |

**What S5-d is for.** Until S6, every client's max staleness is 0, so the scheduler must update each agent once per frame at the same world times as its native tick. The player camera's cached view, which the brains sense from, is updated only after all tick groups (`LevelTick.cpp:1828–1847`), so every agent sees the same camera in both modes whatever the update order. Positions, patrol times, last update times and even state changes must therefore match exactly.

**Audit trail for S5:**
- `code-review` (medium): **no important bugs**; 8 smaller findings (1 pre-existing, 7 minor). Fixed:
  - **GC-nulled entries stayed in a native-mode registry** (pre-existing since S1): only the scheduler's loop ever removed them, so the CSV `Clients` count could be too high. Now removed after every garbage collection, in every mode (05 F4).
  - **A pending client that became garbage stayed linked**: now detached.
  - **`Register` only applied budgeted mode**: a client that was scheduler-driven elsewhere would never get its native tick back in a native world. Now the mode is compared and applied either way.
  - The `FRunClient` comment still described deadlines being changed during the loop: corrected.
  - 03 had not been updated: done.
- Documented rather than changed (02 §12, 05 F4):
  - A client re-registered **inside** another client's update misses one frame. Restoring its slot isn't worth the complexity for a case this rare.
  - The native tick controls (`Deactivate`, `SetComponentTickEnabled(false)`, `TickInterval`, the owner's actor tick, `bTickEvenWhenPaused`) don't steer the scheduler.
  - A frame with zero world delta leaves every client optional (nothing is overdue in such a frame).
- `simplify` (4 parallel reviews: reuse, simplification, efficiency, altitude). Applied:
  - `DetachClient` accepts null: four null checks and one of `Deinitialize`'s two loops are gone.
  - One `ApplyMode` helper for the two places that apply the mode.
  - The test flags are in one shared header.
  - Each test's in-update action is passed only to the client that performs it.
- Skipped, with reasons:
  - **One timer read per update** (chain the timestamps; about 30–50 ns per update saved): it would change what is measured (the policy's own bookkeeping would enter the cost). Phase 8 candidate (11).
  - **Skip the sort when the order is already sorted** (true for S5's equal deadlines): S6 replaces those deadlines; the seeded-sort candidate is already in 11.
  - **Write the `LastUpdateTime` mirror only in budgeted mode**: equal cost in every mode is a design rule (02 §9), and S7's live switch needs correct times.
  - **A null-only scan after GC**: it would leave garbage entries in place, and keep them alive, when garbage elimination is off. The `IsValid` pass runs once per collection.
  - **`TWeakObjectPtr` registry arrays** (would remove the GC hook): a design change to the GC-visible registry of 02 §8, for a rare edge case that the hook already handles.
  - **Joining and leaving in a `RegisterComponentTickFunctions` override** (would remove the guard and the `bStartWithTickEnabled` write): it depends on a super-call check in the engine and on bookkeeping that S7's live switch would have to bypass. The current mechanism uses only public controls and is covered by `NoDoubleUpdate`.
  - **A `SetComponentTickEnabled` override**: a second mechanism for the rule the guard already enforces, which direct `PrimaryComponentTick` changes would bypass anyway.

---

## Step S6 · Significance, settings and telemetry (2026-11-21)

| # | Check | Result |
|---|---|---|
| S6-a | **First build** | ❌ C2668 in `UpdateBudgetSubsystem.cpp`: `AddOnScreenDebugMessage` called with a `uint32` key is ambiguous between its `uint64` and `int32` overloads (07). Fixed with a `static_cast<uint64>` |
| S6-b | **Build** (inside the package job: `UpdateBudgetEditor` and `UpdateBudget`, Win64 Development) | ✅ `Result: Succeeded`, **0 errors, 0 warnings** in both targets (19 actions); cook `Success - 0 error(s), 0 warning(s)`; `BUILD SUCCESSFUL` |
| S6-c | **All automation tests** (`Tools\run_tests.bat UpdateBudget`, after the package, same source; the editor target was already up to date) | ✅ **19 / 19 passed**, `TEST COMPLETE. EXIT CODE: 0`, 0 errors and 0 warnings in the log: lifetime (6), scheduler (8, three now also assert `MandatoryMs`), significance (5: `Bands`, `OutOfView`, `MaxStaleness`, `FarNotBeyondNear`, `ViewCone`). The lifetime worlds have no player camera, so every client keeps significance 1 there and S5's once-per-frame expectations still hold |
| S6-d | **The first UB run** (batch `2026-11-21_034349_s6first`: UB, N = 2,000, 1 run + priming, warm-up 10 s, capture 60 s, budget 2.0 ms from the settings, pinned `0xFFFF`) | ✅ exit 0, **0 log errors, 0 log warnings**, 3,600 captured frames, 5 PSO events and 1 GC event. The log proves the mode: `Set CVar [[ub.Enabled:1]]`, then `Budgeted mode (ub.Enabled 1). Budget 2.00 ms (settings 2.00, ub.BudgetMs -1.00), 1 categories. Update timer resolution: 100.0 ns.` The capture carries `ubmode = Budgeted`, `ubmaxstaleness = 0.5` |
| S6-e | **SC1, budget adherence** (`Tools/analyse_ub.py`, `Benchmarks/Summaries/2026-11-21_034349_s6first_ub_check.md`) | ✅ **PASS on this run.** `UpdateMs` median 1.998 / p95 **2.000** / p99 2.012 / max 2.218 ms against a 2.2 ms threshold (1.10 × budget). 163 of 3,600 frames (4.5%) ended over budget, **none forced** (`MandatoryMs` never above the budget: median 0.079, max 0.871 ms). All 163 were soft: the last update came out slightly dearer than predicted |
| S6-f | **SC2, no starvation** | ✅ **PASS on this run.** The largest per-frame gap was **233.3 ms** against a bound of 516.7 ms (0.5 s + one frame); 0 frames over the bound. At the end of the run the oldest agent was 150 ms old, and none was never updated |
| S6-g | **SC3, relevance first** | ✅ 100% of frames had no near client missed (`NearMissed` 0 in every frame; a median of 11 near clients, at most 18). By construction under the default settings (02 §12 item 11): it confirms the mandatory path works |

**What the run shows** (one run: a check of the mechanism, **not** evidence for SC4; Phase 8 runs the interleaved matrix on one build):
- **The budget is spent, and held.** The policy runs the mandatory clients (median 17 per frame, 0.08 ms), then fills the budget in deadline order: a median of **520 updates per frame**, so each agent is updated about every 4 frames (mean gap about 66 ms), far fresher than its 0.5 s limit. The measured cost per update is about **3.8 µs** (2.0 ms / 520), against S2's whole-frame estimate of 5.2 µs per native agent, which also contained the native tick's own overhead.
- **The scheduler's own cost:** the `RunFrame` scope (median 2.292 ms) minus `UpdateMs` (1.998 ms) is **about 0.29 ms per frame** at N = 2,000. The significance loop (`Deadlines`) is 0.090 ms of it; the sort, the timer reads and the bookkeeping are the rest.
- **Game thread against the S3 baselines** (a different batch, so indicative only): UB's `GameThreadTime` was 3.571 / 4.207 / **4.899** ms (median / p95 / p99), against B2's pinned 2.641 / 3.623 / **5.660** ms at N = 2,000.
  - **The median and p95 are higher than B2's** at this 2 ms budget, which is expected: the design spends close to the whole budget every frame (02 §12 item 9), while B2 updates only about 172 agents per frame.
  - **The p99 is lower:** UB has no periodic spike (the update count never exceeds 625, against B2's 1,363).
  - At a 2 ms budget, SC4 ("median **and** p95 lower than the best baseline") is therefore unlikely to hold. The Phase 8 budget sweep (1 / 2 / 4 ms) will measure where it does, and the result will be reported whatever it is (01 §6.4).
- **The frame was render-thread bound in this run:** `RenderThreadTime` median 3.781 ms > `GameThreadTime` 3.571 ms; `FrameTime` 3.819 ms. B2 and B3 were already render-bound at N = 2,000 (their `FrameTime` of 4.8 ms was above their game-thread time). SC4 is defined on game-thread time, so it is unaffected, but frame-time comparisons at N = 2,000 now measure the renderer as much as the game thread.

**Audit trail for S6:**
- `code-review` (medium): **4 important findings, 4 minor; all fixed or documented.**
  - **The view cone was half the horizontal FOV**, so screen corners counted as out of view. It is now the cone that circumscribes the view rectangle (05 G1), with a corner test.
  - **A client at the camera counted as out of view**: a first-person pawn's own logic would have updated about 4 times a second. Now everything within 2 m of the camera is in view.
  - **`NearMissed` counted entries that could never run** (null, or destroyed during the loop). It is now counted from the outcome after the removal pass.
  - **`analyse_ub.py` stopped at the first bad run.** It now lists failed or unanalysable runs with their reason.
  - Minor:
    - The SC2 bound came from a CLI default; the capture now carries its own (`UBMaxStaleness`).
    - The forced/soft split compared two counts that measure different things; it now uses a measured `MandatoryMs`.
    - Inverted categories were accepted silently; they now get a warning.
    - SC3 checks the mechanism rather than significance freshness: documented (02 §12 item 11).
- `simplify` (4 parallel reviews). Applied:
  - one `ResolveBudgetMs()`;
  - one mode name;
  - SC3 counted in one pass from the outcome (it replaced three coupled sites);
  - `MandatoryMs` measured by the scheduler (three S4 tests assert it);
  - `analyse_ub.py` reuses `run_bench.is_failure` and the manifest's statistics;
  - two clean-ups in the significance code and its tests.
- Skipped, with reasons:
  - **B2/B3 reading the UB category settings** instead of their own constants (same values today): a change to the committed baseline driver. Follow-up for Phase 8, which re-runs every configuration on one build.
  - **Caching each owner's root component** (estimated 40–80 µs per frame at N = 4,000): a third registry array. Phase 8 decides with the `Deadlines` column (11).
  - **Raw pointers and a pre-clamped category index** in the deadline loop: about 1 ns per client.
  - **Testing distance before view**: 2–4 ns per client, but needs the category API split.
  - **Moving `series`/`table` helpers into `csv_stats`**: touches `analyse_baselines.py`, outside this step.
  - **Deleting `AlwaysInViewRadius`** (the altitude review: it never fires in the benchmark): the code review showed why a general plugin needs it.
- The first S6 build failed on an ambiguous overload (`AddOnScreenDebugMessage` with a `uint32` key, 07). It was fixed with an explicit cast before any later build.

---

## Step S7 · Live switching and the debug view (2026-11-21)

| # | Check | Result |
|---|---|---|
| S7-a | **Build** (`Tools\build.bat`, Development editor) | ✅ **0 errors, 0 warnings** (MSVC 14.44.35229; 14 actions, 10.1 s). Every changed source was older than the job start, so the build compiled the final code, after the code-review and simplify fixes |
| S7-b | **All automation tests** (`Tools\run_tests.bat UpdateBudget`, headless, through the job queue) | ✅ **24 / 24 passed**, `TEST COMPLETE. EXIT CODE: 0`, 0 errors and 0 warnings in the log: lifetime (8: the six of S5 with `NoDoubleUpdate` updated, plus `LiveSwitch` and `LiveSwitchKeepsTickState`), scheduler (11: the eight of S4 plus UE5-TestTrust's three), significance (5). Queue job `20261121_035419_46375d`, 25.5 s |
| S7-c | **Switching mid-run: no double update, no missed agent** (03 S7's check) | ✅ In a real game world (`Lifetime.LiveSwitch`): budgeted → native → budgeted over 9 frames, with the first switch requested from **inside** a client's update while the scheduler runs. Every client got exactly 9 updates (one per frame), none after it left play. A change undone within one frame switched nothing. `Lifetime.LiveSwitchKeepsTickState`: gameplay's tick choices, including those made while budgeted, are in force after the switch back |

**Mutation testing by UE5-TestTrust found 3 gaps in the scheduler tests, closed by 3 new tests.**
- **What happened.** UE5-TestTrust (P03 of this portfolio, a mutation-testing tool for UE automation tests) ran a campaign on this repository's `UpdateBudgetScheduler.cpp` at `b890bc6`: 39 mutants against the 8 S4 tests (campaign `2026-11-21_025734_mut-updatebudget`).
- **Before.** Its mutation score M1 was **92.1%** (35 of 38 killed, after its review; 89.7% before the review), with a median of 14.1 s per mutant.
- **What survived.** All four survivors were boundary shifts of a comparison:
  - `Deadline < NextFrameTime` → `<=`;
  - the cut-off `SpentMs + Predicted > BudgetMs` → `>=`;
  - `bOverBudget = SpentMs > BudgetMs` → `>=`;
  - the sort's tie-break `A < B` → `A <= B`.

  The last one was reviewed as equivalent: indices are unique, and the engine's introsort never compares an element with itself.
- **After.** UE5-TestTrust wrote one boundary test for each real gap (its commit `6b60e8b`). With them, its "after" campaign (`2026-11-21_031329_mut-updatebudget`) scored **M1 = 100.0%** (38 of 38), and the only survivor left is the equivalent tie-break.
- **Merged after review.** Each test checks a documented rule of the policy (05 E2, E3), not an implementation accident, so all three were merged here. Their inputs are binary fractions, so each boundary is hit exactly. The patch applied cleanly to this step's test file. The numbers above are UE5-TestTrust's; they come from its repository and were not re-measured here.

**Audit trail for S7:**
- `code-review` (medium): 2 important findings and 3 minor ones, all fixed or documented.
  - **A switch there and back un-paused what gameplay had paused.** The component now records gameplay's tick choice, see the altitude change below.
  - **A capture spanning a switch kept the mode it began with.** It is now marked `Mixed`, and the switch frame gets a CSV event.
  - A switch doesn't happen while paused: now stated in the cvar help.
  - The test cvar is process-wide, so running the lifetime tests switches an open PIE session too: documented here.
  - The test lacked the in-update case and a same-frame undo: both added.
- `simplify` (4 parallel reviews). Applied:
  - the frame report is now the accumulator, reset in one assignment (it fixes a missed reset of the budget value);
  - one `ApplyModeToClients` for begin play and switches;
  - the duplicate mode check is gone (the component's own early return);
  - one mode-name helper;
  - the engine's `FColor::MakeRedToGreenColorFromScalar` for the debug colours;
  - a test helper for the per-client mode check.
- **Altitude change:** the component overrides the virtual `SetComponentTickEnabled`, so gameplay's choices are recorded on every path (`Activate`, `Deactivate`, replication, Blueprint calls), even while budgeted. The `TickComponent` guard remains, for code that enables the tick function directly; `NoDoubleUpdate` now tests that path.
- Skipped, with reasons:
  - **A colour lookup table and direct line-batcher calls for `ub.Debug 2`** (an estimated 0.2–0.3 ms per frame at N = 4,000): debug-only.
  - **Applying a pending switch while paused**, from another hook: a paused world runs no updates, so nothing is lost by waiting.
- **Found while checking the S6 commit:** a scripted doc edit had turned `\r` in `Tools\run_tests.bat` into a carriage return, so git treated 09 as binary. Fixed in this commit; a scan for lone carriage returns is now part of the pre-commit audit (07).

---

## Phases 7–8 (to be written)

- Functional, edge-case, stability and scalability tests.
- The full benchmark protocol and the SC1–SC7 results.
