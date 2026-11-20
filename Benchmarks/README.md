# P01 · Benchmarks

Every number in this project's write-up must trace back to a file in this folder. The method is in [../Explanation/01_Problem_Analysis.md §7](../Explanation/01_Problem_Analysis.md). The results, batch by batch, are in [../Explanation/09_Testing_And_Benchmarks.md](../Explanation/09_Testing_And_Benchmarks.md).

## Measurement rules

1. **Baselines first.** B0–B3 are captured before the scheduler exists, on the same workload, build and machine as every later run.
2. **Packaged Development build only** (never PIE, never the editor). The launcher engine can't build the Test configuration.
3. **Fixed step** (`-deterministic -FPS=60`), so every run simulates the same frames. The capture window is opened and closed by the game itself (`BenchStart` / `BenchEnd` CSV events), never by a timer.
4. **One discarded priming run per batch**, then **interleaved runs, with the config order rotated every round**.
   - **Runs per combination** = the smallest multiple of the number of configs that is at least 4, so every config sits in every order position equally often: 4 for B0–B3, 5 with UB.
   - Combinations that no success criterion needs are skipped.
5. **Every run is pinned to the performance cores** (logical CPUs 0–15, mask `0xFFFF`) and the applied mask is recorded per run.
   - This CPU is hybrid, and Windows sometimes put the game thread on an E-core, making a run about 1.6× slower (09, step S3).
   - The game executable is launched directly, so its exit code is real and a hung run is always killed.
   - While a batch runs, the display and system are kept awake for that process only. No power settings are changed.
6. **Statistics:** median, p95 and p99 per run (linear-interpolation percentiles, `Tools/csv_stats.py`), then the median across runs with the [min–max] across runs. FPS is never used as evidence.
7. **The environment is recorded with every batch** (below), automatically, by `Tools/run_bench.py`.

## Folders

| Folder | What goes in it | Rule |
|---|---|---|
| `Raw\` | CSV profiler captures (`.csv`), per-agent run reports (`_agents.csv`), game logs (`.log`), Unreal Insights traces (`.utrace`), one `_manifest.json` per batch | **Never edited.** Anything re-processed is written to `Summaries\`. Only the manifests are in git (exact command lines, environment, per-run statistics). The captures are too large for git (a full matrix is about 1 GB), so every batch cited in the docs is attached as `<batch>_raw.zip` to this repository's **Releases** page |
| `Summaries\` | Computed statistics (median, p95, p99, staleness), SC1–SC7 pass/fail tables, charts | Each summary names the batch it was computed from |

## File naming

`<batch>_<Config>[_W<work>]_N<agents>_r<run>.<ext>`, with `<batch>` = `YYYY-MM-DD_HHMMSS[_label]` (time-stamped since S2, so two batches can never collide; `run_bench.py` refuses to overwrite)

- `<Config>`: `B0` native tick · `B1` batched ticks · `B2` distance interval · `B3` Significance Manager · `UB` UpdateBudget (plus `UB-1ms` / `UB-4ms` for budget sweeps).
- Example: `2026-11-25_101500_B0_N2000_r1.csv` (+ `_agents.csv` per-agent report + `.log`), `2026-11-25_101500_manifest.json` in `Raw\`, and `2026-11-25_101500_summary.md` in `Summaries\`.
- `_r0_prime` files are the discarded priming run. They are kept for transparency and excluded from every summary.
- **How to run a batch:** `python Tools\run_bench.py --configs B0,B1,B2,B3 --agents 1000,2000,4000` (packaged Development build required: `Tools\package.bat`; on the development machine every batch runs as one job of the shared UE job queue, so nothing else can run beside it).

## Environment record (written into every manifest and summary)

```text
Engine:            UE 5.8.4 (CL 58902052), launcher build
Build:             Development, standalone packaged (never PIE)
Compiler:          MSVC 14.44.35229 (VS 2022 17.14), Windows SDK 10.0.22621.0
CPU / GPU / RAM:   i7-14650HX (8P + 8E) / RTX 5070 Laptop (driver 32.0.16.1047) / 32 GB
Storage:           project on D: (Kingston NVMe), engine on C: (WD NVMe)
OS:                Windows 11 build 26200
Power:             AC, power plan "Turbo"
Resolution:        1280x720 windowed, default scalability, no VSync, no sound
Command line:      exact, per run, in the manifest
Warm-up excluded:  10 s     Capture length: 60 s     Runs per combination: a multiple of the config count, at least 4 (interleaved) + 1 priming run per batch
CPU affinity:      performance cores only (0xFFFF), recorded per run
```

## Run log

| Batch | Configs | N | Runs | Purpose | Summary |
|---|---|---|---|---|---|
| `2026-11-20_233941_calib` | B0, work 0/64/128/256 | 1000 | 2 each | Calibrate the per-agent compute so N = 1000 is game-thread bound (chosen: 128) | [summary](Summaries/2026-11-20_233941_calib_summary.md) |
| `2026-11-20_235024_s2wave` | B0 | 500 + 500 wave | 1 + priming | S2 end-to-end check: the wave spawn, the wave split in the summary, self-exit | [summary](Summaries/2026-11-20_235024_s2wave_summary.md) |
| `2026-11-21_000138_s3smoke` | B0–B3 | 1000 | 1 + priming | S3 sanity: every baseline starts, is active and logs no errors (not evidence: one short run) | [summary](Summaries/2026-11-21_000138_s3smoke_summary.md) |
| `2026-11-21_001816_s3smoke2` | B0–B3 | 1000 | 1 + priming | The same check after the S3 clean-up; behaviour identical to the previous batch (09, step S3) | [summary](Summaries/2026-11-21_001816_s3smoke2_summary.md) |
| `2026-11-21_002119_baseline` | B0–B3 | 100, 1000, 2000, 4000 | 4 + priming (planned 6; stopped after round 4 when the protocol changed to balanced runs) | The first baseline matrix. **Not pinned:** its B0/B1 runs at N ≥ 1,000 carry the E-core problem (09, S3-g) and are not evidence; B2/B3 behaviour and the H4 counts are | [summary](Summaries/2026-11-21_002119_baseline_summary.md) · [analysis](Summaries/2026-11-21_002119_baseline_analysis.md) |
| `2026-11-21_012231_affinitycheck` | B0, B1 | 2000, 4000 | 4 + priming, capture 20 s | Validates P-core pinning: the two-level pattern is gone (09, S3-h) | [summary](Summaries/2026-11-21_012231_affinitycheck_summary.md) |
| `2026-11-21_015041_baseline_pinned` | B0–B3 | 1000, 2000, 4000 | 4 + priming | **The baseline matrix that UpdateBudget must beat** (SC4), every run pinned to the P-cores; H1, H4 and H6 (09, S3-i). N = 100 (SC5) is re-measured beside UB in Phase 8 | [summary](Summaries/2026-11-21_015041_baseline_pinned_summary.md) · [analysis](Summaries/2026-11-21_015041_baseline_pinned_analysis.md) |
