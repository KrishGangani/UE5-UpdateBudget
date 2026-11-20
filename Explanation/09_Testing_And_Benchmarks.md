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
| S3-f | **The baseline matrix** | Batch `2026-11-21_002119_baseline`: B0–B3 × N = 100 / 1,000 / 2,000 / 4,000 × 6 interleaved runs + priming, warm-up 10 s, capture 60 s. Results below once the batch has finished |

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

---

## Phases 7–8 (to be written)

- Functional, edge-case, stability and scalability tests.
- The full benchmark protocol and the SC1–SC7 results.
