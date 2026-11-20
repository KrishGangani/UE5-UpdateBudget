# 01 · Problem Analysis: UpdateBudget

**Phase:** 1 (Problem Analysis), drafted at Step 0 on 2026-11-19; updated after Phase 2 on 2026-11-20 (evidence in [01b_Research_Findings.md](01b_Research_Findings.md))
**Engine:** UE **5.8.4** (chosen 2026-11-20). The engine facts below were first read from the 5.8.3 source and re-checked on 5.8.4 ([research/04](Research/04_local_engine_verification.md) §0).
**Status of every claim:** marked **[verified]** (read in engine source or a cited source), **[hypothesis]** (to be confirmed by measurement), or **[target]** (a pass/fail threshold, not a result).

---

## 1. The problem in one paragraph

A game world often holds hundreds to thousands of actors that run gameplay logic every frame: AI "brains", turrets, interactive props, ambient creatures, traps. In Unreal, each one usually overrides `Tick`. The engine then runs every one of them every frame (or at a fixed `TickInterval`), **whether or not it matters to the player right now**. A guard 200 m behind a wall costs the same as one aiming at you.

So game-thread time grows with *how many* actors exist, not with *how many matter*. When designers add more actors, or a wave spawns, the frame goes over budget and the game drops below its target frame rate.

The usual manual fix, hand-tuned tick intervals per distance, is static. It doesn't react when the load changes, and it gives no guarantee about how stale a far actor's logic can get.

## 2. Where this happens in real games

| Evidence | What it shows | Source |
|---|---|---|
| Hitman: Absolution crowds | ~1,200 agents with a hard per-system ms budget (5 ms of main-CPU time on PS3), split between animation, AI and framework | [research/02](Research/02_technical_challenges.md) [T2-A] (raw text checked) |
| Assassin's Creed Unity | Shows **10,000** crowd NPCs while keeping only **40** "real" AIs. Detail is spent where it matters | [research/02](Research/02_technical_challenges.md) [T2-B] |
| Fortnite / Epic | Epic built the **Animation Budget Allocator**, a closed-loop ms budget, but only for skeletal mesh animation | [research/03](Research/03_existing_solutions_gap_analysis.md) §2; [research/04](Research/04_local_engine_verification.md) §4 |
| Sea of Thieves (Rare) | Gave a whole talk to aggregating ticks to manage scale | [research/03](Research/03_existing_solutions_gap_analysis.md) §2.3 (snippet only) |
| Job postings, 2026 | EA Madrid: "define and manage per-system frame budgets". WB Montréal: "time-slicing or behavioral LOD". Ubisoft: "CPU, AI budgets, behavioral LODs, agent counts" | [research/01](Research/01_job_market.md) §2 (EA6, WB2, U12) |

**What these share:** fidelity is spent by relevance, inside a fixed time budget. Engines and studios build this for animation and crowds. General gameplay logic is usually left to hand-tuned ticking.

## 3. What already exists, and why it doesn't fully solve the problem

| Existing tool | What it does | What it doesn't do | Evidence |
|---|---|---|---|
| `FTickFunction::TickInterval` | Run a tick at most every N seconds | It is static, with no feedback from frame cost and no relevance | [verified] `EngineBaseTypes.h` |
| Tick batching (`tick.AllowBatchedTicks`) | Combines compatible tick functions into one task, cutting dispatch overhead | It is **off by default** (`0`) in 5.6.1, 5.7.4, 5.8.3 and 5.8.4. It cuts overhead per tick but still runs every tick, so it is not a budget | [verified] `TickTaskManager.cpp`; [research/04](Research/04_local_engine_verification.md) §3 |
| Significance Manager (`USignificanceManager`) | Scores objects with your function and calls your callback | It **doesn't measure or cap time**. You must call `Update` and write the policy yourself | [verified] `SignificanceManager.h`; [research/04](Research/04_local_engine_verification.md) §4 |
| `OrderedBudget.h` (Significance Manager plugin) | A count-based LOD budget | Counts objects, not milliseconds | [verified] `01b` §3 |
| Animation Budget Allocator | A real closed-loop ms budget from smoothed, measured cost; significance ordering; skips capped at `MaxTickRate` (10 **frames**); accumulated delta via `SetExternalDeltaTime` | Covers **skeletal mesh components only** (`USkeletalMeshComponentBudgeted`). Its skip cap counts frames, not seconds | [verified] `IAnimationBudgetAllocator.h`, `AnimationBudgetAllocatorParameters.h:34`, `AnimationBudgetAllocator.cpp:185, 477`; `01b` §5 |
| URO (`FAnimUpdateRateParameters`) | Skips animation updates by LOD or visibility | Animation only | [research/03](Research/03_existing_solutions_gap_analysis.md) §2.1 |
| Mass time-slicing (`FExecutionLimiter`, 5.7+) | Limits a query to N entities per frame | Counts entities, not ms. Only for code rewritten as Mass processors | [research/03](Research/03_existing_solutions_gap_analysis.md) §2.1 |
| Fab / GitHub: Tick Optimization Toolkit, Optimization Tools, EnhancedTick; Tom Looman's `URogueDeferredTaskSystem` | Distance/visibility zones that set intervals or switch tick off; significance bands; round-robin pools; type batching. Looman's system is an open-loop ms cap for one-shot tasks | None applies a measured ms budget with a staleness bound to general gameplay updates. Benchmark claims have no stated conditions (e.g. Tick Optimization Toolkit: "1024 AI … 108 ms → 18 ms") | `01b` §5 |

**The gap (medium strength), after Phase 2:**
- The Animation Budget Allocator already combines a measured budget, significance order, a frame-based skip cap and accumulated delta, but only for skeletal meshes.
- Among the public tools reviewed, none applies a **measured game-thread ms budget**, significance priority and a **per-object staleness bound in seconds** to **general gameplay logic**.
- None publishes controlled measurements of both.

We present UpdateBudget as **adapting the Animation Budget Allocator's model to gameplay logic**, not as a new idea. Seconds matter because a 10-frame cap is 333 ms at 30 fps but 69 ms at 144 fps.

## 4. Root-cause hypotheses

An engineer doesn't guess the fix. Each hypothesis below gets a measurement that can confirm or reject it, and the baseline runs come before any solution code.

| # | Hypothesis | Why we suspect it | How we test it |
|---|---|---|---|
| H1 | **Each tick has dispatch overhead**, separate from the logic inside it | Epic added tick batching (5.5) to cut the task overhead per tick. **Phase 2:** without batching, each enabled tick function gets its own task and completion event every frame | Measure game-thread ms vs N with **empty** ticks, batching off and on. Calibrate with the engine's `tick.AddTestTickFunctions` (10,000 no-op ticks). The slope is pure overhead |
| H2 | **Cost scales with actor count, not relevance** | Native ticking has no notion of relevance | Game-thread ms vs N while the number of *relevant* actors (near or visible) is held constant. If cost rises with N, H2 holds |
| H3 | **Static intervals don't adapt to load** | A `TickInterval` picked for N = 500 is wrong for N = 4,000 | Run the distance-interval baseline (B2) at several N and check whether game-thread p95 stays flat |
| H4 | **Interval ticks clump into periodic spikes** when many actors with the same interval register in the same frame | **Phase 2 (code):** interval ticks fire as soon as they are enabled, re-arm with overrun credit, and nothing staggers them, so same-interval ticks stay in step | Capture frame-time series with B2 and look for periodicity (a sawtooth). **Supported by code, not yet measured** |
| H5 | **Scattered memory access** (actor → component → data spread across the heap) adds cache misses | The data-oriented-design literature, e.g. Hitman's 36-byte agent core [T2-A] | Measure, don't assume. A dense scheduler array only helps the *scheduling* part. The logic inside each update still touches its own actor. We report this limit honestly |
| H6 | **Re-tuning many intervals is itself costly** (the B2/B3 approach) | **Phase 2 (code):** disabling or re-timing a cooling-down tick walks the whole cooling-down list | Time the baseline drivers' interval updates with a CSV/Insights scope at each N |

**Not a cause we accept without checking:** "the GPU is the bottleneck". Every capture records `stat unit` / CSV GPU time to confirm the scene is **game-thread bound**. If it isn't, the workload is changed until it is, because otherwise a game-thread fix shows nothing.

## 5. The cost of leaving it unsolved

The frame budget is fixed. At 60 fps every frame must finish in **16.67 ms**, and the game thread shares that with everything else (physics, animation, UI, scripting).

*Arithmetic illustration only, not a measurement:* if one agent's update costs *c* microseconds, N agents cost N × c. At a hypothetical *c* = 5 µs, 2,000 agents use 10 ms, 60% of the frame, before anything else runs.

When this is ignored, teams typically:
- cap actor counts and cut design ideas;
- hand-tune intervals per level, which breaks every time content changes;
- or ship with frame drops.

On weaker platforms the same content simply doesn't fit.

## 6. What "solved" means: success criteria, defined now

### 6.1 The workload (identical in every run)

- **Agents:** N ∈ {100, 1,000, 2,000, 4,000}, placed by a **fixed random seed** on a grid with jitter.
- **Per-agent logic** is a realistic "sense → decide → act" loop:
  - a distance and view-cone check against the player;
  - a small state machine (patrol / alert / return);
  - movement along a patrol loop driven by **accumulated time**, so its correct position is known exactly;
  - a **calibrated compute step** that stands in for heavier logic, with a configurable cost. **Calibrated in S2 to 128 iterations** (about 20 ns per iteration per agent; about 5.2 µs per whole update; frame game-thread bound; see `09` S2).
- **Player/camera:** follows a fixed spline path at a fixed speed, so the set of relevant agents changes over time in exactly the same way every run.
- **Stress event:** at a fixed time, N doubles (a spawn wave). This tests reaction to load.

### 6.2 Compared configurations (all in the same build, switched by console variable)

| ID | Configuration | Why it is included |
|---|---|---|
| B0 | Native component `Tick` every frame, in `TG_PrePhysics` (set explicitly: components default to `TG_DuringPhysics`) | What most projects do. Same tick group as UB, so the comparison is fair |
| B1 | B0 + `tick.AllowBatchedTicks 1` | Epic's own overhead fix. Leaving it out would be unfair |
| B2 | Distance-based `TickInterval` (near: every frame; mid: 0.1 s; far: 0.5 s) | The usual manual fix |
| B3 | Significance Manager scores → per-actor `TickInterval` | Epic's tool used as intended |
| **UB** | **UpdateBudget** (budget 2.0 ms, max staleness 0.5 s; defaults explained in `02` §6) | Our system |

### 6.3 Metrics

| Metric | Unit | Tool |
|---|---|---|
| Frame time: median, p95, p99 | ms | CSV profiler (`-csvCaptureFrames=`), Python stats script |
| Game-thread time: median, p95, p99 | ms | CSV profiler; spot checks in Unreal Insights |
| Gameplay-update time per frame (all agents) | ms | Our CSV stat (`CSV_CUSTOM_STAT`) and Insights scope |
| Updates run per frame; mandatory (staleness-forced) updates per frame | count | Our CSV stats |
| Staleness, the time since an agent's last update: p99 and max, split into near/visible and far agents | ms | Our CSV stats |
| Behaviour error vs an every-frame reference run: patrol position error and state-transition delay | cm, s | Fixed-time-step runs compared offline |
| Registration cost (spawn-time overhead added by our component) | µs per agent | Insights scope |

### 6.4 Pass/fail criteria [target]

| # | Criterion | Threshold |
|---|---|---|
| SC1 | **Budget adherence** | At N = 1,000/2,000/4,000, p95 gameplay-update time ≤ 1.10 × budget. Frames where mandatory updates force an overrun are counted and reported separately |
| SC2 | **No starvation** | Max staleness ≤ max-staleness setting + 1 frame for **every** agent in **every** run. **0 violations** allowed |
| SC3 | **Relevance first** | Near/visible agents are updated every frame in ≥ 99% of frames at N ≤ 2,000 |
| SC4 | **Real improvement** | Game-thread median **and** p95 lower than the **best** of B0–B3 at N ≥ 2,000, and outside run-to-run spread (§7). If not met, we report it and analyse why. No reframing |
| SC5 | **Low overhead when not needed** | At N = 100 (budget never binds): game-thread median within +0.10 ms of B0 |
| SC6 | **Behaviour stays correct** | Patrol position error after 60 s ≤ a tolerance fixed in Phase 3 (from the patrol speed). State transitions delayed by no more than the max-staleness setting |
| SC7 | **Editor and spawn cost** | Registration adds ≤ a per-agent cost fixed in Phase 3. No measurable change to PIE start time |

## 7. Measurement plan (summary; the full protocol goes in `09_Testing_And_Benchmarks.md`)

- **Build:** packaged **Development** build, **standalone, never PIE**, for headline numbers. The launcher engine can't build Test ("Targets cannot be built in the Test configuration with this engine distribution", Phase 4 build log), and Shipping drops the CSV profiler. Development's small extra overhead (the stats system) is identical in every configuration, so comparisons stay fair. Editor numbers, if any, are labelled "editor".
- **Determinism:**
  - Fixed random seed and fixed camera path.
  - **Headline A/B runs and correctness runs use a fixed step: `-deterministic -FPS=60 -novsync`.** The engine then gives every frame exactly 1/60 s and never waits, while CSV `FrameTime`/`GameThreadTime` still record the real cost [verified, `01b` §4]. Every configuration simulates identical frames.
  - One **uncapped real-time** confirmation run (`-novsync`, `t.MaxFPS 0`, no fixed step) shows the result also holds under real deltas.
  - CSV capture starts on an event fired after warm-up: `-csvStartOnEvent=BenchStart -csvCaptureOnEventFrameCount=N -ExitAfterCsvProfiling`. `-traceautostart=0` stops an open Insights from tracing the run. `GameUserSettings.ini` is reset before runs.
- **Environment recorded with every result:**
  - CPU i7-14650HX, GPU RTX 5070 Laptop (driver 32.0.16.1047), 32 GB RAM, Windows 11 build 26200;
  - storage: project on D: (Kingston NVMe SSD), engine on C: (WD NVMe SSD). This matters for any load-time number;
  - UE version, build configuration, resolution, scalability preset, Windows power mode, AC power.
- **Laptop noise control:**
  - AC power and the same power mode every time;
  - 10 s warm-up excluded, then a 60 s capture;
  - **runs per combination = the smallest multiple of the number of compared configurations that is at least 4** (4 for B0–B3, 5 once UB joins), **interleaved** with the order rotated every round, so every configuration sits in every order position equally often; after **one discarded priming run** that warms the caches. *(Changed in S3 from "6 runs": with 4 configurations, 6 runs over-represents the first two order positions, and 4 runs is the balanced design. Reason and data: 09, step S3.)*
  - **every run pinned to the performance cores** (this CPU is hybrid: 8 P-cores + 8 E-cores). *(Added in S3, after the baseline batch showed Windows placing the game thread on an E-core: 09, step S3.)*
  - confirm the RTX GPU is the active adapter.
- **Statistics:** no shipped tool prints p95/p99, so a small script (`Tools\csv_stats.py`) computes them. It drops the CSV's last two rows (a repeated header and metadata).
  - Percentiles are computed per run; we report the **median across runs** plus the **min–max range**.
  - We only claim an improvement when the ranges don't overlap. This rule is conservative on purpose.
- **Raw data:** untouched captures go into `Benchmarks\Raw\`, computed results into `Benchmarks\Summaries\`, each with the exact command line used. Naming rules are in `Benchmarks\README.md`.

## 8. Limits we accept up front

- **The budget is soft.** One update that is itself slow can overrun it. We measure how often.
- **Mandatory updates can exceed the budget.** That is deliberate: the staleness bound beats the budget, because a starving actor is a correctness bug. SC1 reports these frames separately.
- **Not all logic tolerates variable delta time.** Physics forces and frame-exact timers may not. The system targets logic that integrates time correctly, and `07_Common_Mistakes.md` will document what doesn't fit.
- **Tick prerequisites are lost** for actors driven by the scheduler, because they run inside one scheduler tick. This must be stated, and it rules out some actors.
- **Single-player and listen-server view only in v1.** On a dedicated server, "relevance" must use all players' views (see `12_Future_Scalability.md`).
- **Small N may show no gain.** That's a valid result.
