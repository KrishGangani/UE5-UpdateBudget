# P01 · UpdateBudget: a frame-time budget for gameplay logic

**Author:** Krish Gangani · [github.com/KrishGangani](https://github.com/KrishGangani)

**Domain:** Performance & profiling · game-thread CPU · **Difficulty:** ★★★☆☆ · **Engine:** UE 5.8.4

> **Status: in progress. There are no results yet.** Sections marked *(pending)* are filled in only from real measurements, in the phase named.

| Phase | 1 Problem | 2 Research | 3 Architecture | 4 Foundation | 5 Implementation | 6 Explanation | 7 Testing | 8 Profiling | 9 Review | 10 Portfolio |
|---|---|---|---|---|---|---|---|---|---|---|
| State | ✓ | ✓ | ✓ final | ✓ | in progress (S7 of 8) | ongoing | — | — | — | — |

---

## Problem statement

Thousands of ticking actors cost the same game-thread time whether or not they matter to the player. A static `TickInterval` doesn't react to load, so frame time grows with actor count. Details: [Explanation/01_Problem_Analysis.md](Explanation/01_Problem_Analysis.md).

## Engineering approach

Each agent gets a deadline from its significance: near and visible means "update now", far means "up to 0.5 s". Each frame a scheduler runs every agent that would otherwise miss its deadline, then the rest in earliest-deadline-first order while a measured millisecond budget lasts. Each update receives the accumulated delta time. The model is adapted from Epic's Animation Budget Allocator (skeletal meshes only) to general gameplay logic, with the staleness bound in seconds rather than frames. It is compared against native tick, engine tick batching, distance-based intervals and the Significance Manager. Details: [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md).

## Architecture diagram

Final (v1.0): [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md) §5–6. A rendered version is in [Media/architecture_v1.html](Media/architecture_v1.html)

## Before / after comparison

*(pending, Phase 8)*. Five configurations on one fixed workload: B0 native tick, B1 batched, B2 distance interval, B3 Significance Manager, UB UpdateBudget.

## Benchmark results

*(pending, Phase 8)*. Median, p95 and p99 game-thread ms at N = 100 / 1k / 2k / 4k agents, plus staleness distributions. Raw data in [Benchmarks/](Benchmarks/README.md). The pass/fail criteria were fixed in advance: [01_Problem_Analysis.md §6.4](Explanation/01_Problem_Analysis.md).

## Recorded demonstration

*(pending, Phase 10)*. B0 vs UB on the same camera path, with agents tinted by update recency and a live game-thread graph. Saved in `Media/`.

## Source code

The UE project is in [UpdateBudget/](UpdateBudget/) (UE 5.8.4, C++):
- the reusable system is the plugin `UpdateBudgetCore`;
- the demo and benchmark code is in the game module `UpdateBudget`.

## Documentation

| File | Contents |
|---|---|
| [00_Project_Overview](Explanation/00_Project_Overview.md) | Identity, demonstration, presentation, industry relevance |
| [01_Problem_Analysis](Explanation/01_Problem_Analysis.md) | Problem, existing work, root-cause hypotheses, success criteria, measurement plan |
| [01b_Research_Findings](Explanation/01b_Research_Findings.md) | Phase 2: verified engine behaviour (UE 5.8.4) and what it changed in the design |
| [02_System_Architecture](Explanation/02_System_Architecture.md) | Final v1.0: modules, classes, deadline + EDF scheduling, data flow, safety rules, rejected alternatives |
| [03_Implementation_Roadmap](Explanation/03_Implementation_Roadmap.md) | The small, ordered build steps; baselines captured before scheduler code |
| [04](Explanation/04_CPP_Fundamentals.md) · [05](Explanation/05_Line_By_Line_Code_Explanation.md) · [06](Explanation/06_Unreal_APIs_And_Macros.md) · [07](Explanation/07_Common_Mistakes.md) | C++ fundamentals, line-by-line code, APIs and macros, common mistakes. Written step by step with the code (current to S7) |
| [09](Explanation/09_Testing_And_Benchmarks.md) | Every check and benchmark batch so far, with its raw-data batch name |
| [08](Explanation/08_Debugging_And_Profiling.md) · [10](Explanation/10_Interview_Questions.md) · [11](Explanation/11_Optimization_Decisions.md) · [12](Explanation/12_Future_Scalability.md) | Debugging and profiling (the tools that exist since S6), interview questions, optimisation decisions (candidates so far), future scalability. Completed in Phases 8–10 |
| [Research/](Explanation/Research/) | The portfolio-wide research this project draws on: job market, technical challenges, existing solutions, local engine verification |

## License

MIT, see [LICENSE](LICENSE). Unreal Engine itself is not included and is covered by Epic Games' own license.
