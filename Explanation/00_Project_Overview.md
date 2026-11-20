# 00 · Project Overview: UpdateBudget

> *"This was the problem. This was my investigation. This was my solution. These were my technical decisions. This was how I measured the results."*

**Status (2026-11-20):** Phases 1–3 are done (problem, verified research in `01b`, final architecture v1.0 in `02`). No code exists yet, and no results exist yet.

---

## A. Identity

| | |
|---|---|
| **Name** | UpdateBudget: a frame-time budget for gameplay logic |
| **Technical category** | Performance and profiling: game-thread CPU, large-scale agent scalability |
| **Main objective** | Keep gameplay-logic time on the game thread close to a fixed millisecond budget with 1,000–4,000 active agents. Update the most significant agents first, and guarantee that no agent goes un-updated longer than a set bound |
| **Difficulty** | ★★★☆☆ Intermediate–Advanced |
| **Core UE technologies** | `FTickFunction` and tick groups, `UWorldSubsystem`, `UActorComponent`, `UDeveloperSettings`, console variables, CSV profiler, Unreal Insights, Significance Manager (baseline only) |
| **Relation to existing work** | It adapts the model of Epic's **Animation Budget Allocator** (a closed-loop ms budget with smoothed cost and accumulated delta, for skeletal meshes only) to general gameplay logic, with deadline-based (earliest-deadline-first) scheduling and a staleness bound in seconds. It is compared against `TickInterval`, engine tick batching and the Significance Manager. See `01_Problem_Analysis.md` §3 |

## B–D. Problem, solution, architecture

- Problem, root-cause hypotheses, success criteria and the measurement plan: **[01_Problem_Analysis.md](01_Problem_Analysis.md)**
- Solution and architecture (initial): **[02_System_Architecture.md](02_System_Architecture.md)**

## E. Technical demonstration

**What the viewer sees**
- A level with thousands of simple agents patrolling.
- Each agent is **tinted by how recently it was updated**: green this frame, yellow within 0.1 s, red close to the staleness limit.
- An on-screen graph of game-thread ms with the budget drawn as a line.
- A text overlay showing the active configuration: B0, B1, B2, B3 or UB.

**Before** (B0 native tick, then B1–B3)
- As the camera flies its fixed path and a spawn wave doubles the agent count, game-thread time climbs with the count.
- Every agent is green, even those far behind walls. The frame pays for all of them.

**After** (UB)
- The same path and the same wave.
- Gameplay-update time stays near the budget line.
- Agents near or in front of the camera stay green. Distant ones cycle yellow/red but never pass the staleness limit.
- When the wave arrives, the extra load shows up as **more staleness for far agents**, not as a frame-time spike.

**Recorded material:**
- a side-by-side video of B0 vs UB on the same path;
- before/after charts of median, p95 and p99 game-thread ms against agent count;
- staleness distributions;
- a short Insights capture walkthrough.

## G. Portfolio presentation (produced in Phase 10)

1. Problem statement, one paragraph (from `01` §1).
2. The investigation: root-cause hypotheses H1–H5 and what the baseline captures showed for each.
3. The architecture diagram (from `02` §2) and the per-frame flow.
4. The key decisions and the rejected alternatives: the tick-function choice, component vs interface, prediction vs check-after.
5. Before/after benchmarks against **all four** baselines, with the full environment and method.
6. What didn't work or didn't improve, stated plainly.
7. Video, source repository and the complete `Explanation/` folder.

## H. Industry relevance

**Skills demonstrated:**
- engine-level understanding of the tick system: tick functions, tick groups and where `FTickableGameObject` runs in the frame;
- profiling with more than one tool;
- statistical benchmarking on noisy hardware;
- data-oriented scheduling;
- object lifetime safety (GC-visible references, deferred removal);
- explaining trade-offs (soft budget, lost tick prerequisites, staleness vs cost).

**Studios and roles that ask for this:**
- EA Madrid's UE systems role: "define and manage per-system frame budgets";
- WB Games Montréal: "time-slicing or behavioral LOD";
- Ubisoft (UE AI): "CPU, AI budgets, behavioral LODs, agent counts";
- Riot Engine Performance: profiling with Insights;
- CDPR's open-world AI roles (performance at scale).

Sources: [research/01](Research/01_job_market.md) (EA6, WB2, U12, R1, C3, C4).

**Interview questions it prepares you for:**
- "Walk me through what happens when an Actor ticks. What does a tick cost besides your code?"
- "You have 3,000 AI agents and you're over budget on the game thread. What do you do first?"
- "Why is `TickInterval` not enough? What goes wrong with many actors on the same interval?"
- "How do you prove an optimisation worked? Why report p95/p99 and not average FPS?"
- "What's the difference between a frame-time budget and a tick-rate cap?"
- "How do you avoid starving low-priority work in a priority scheduler?"
- "Why a world subsystem instead of a manager actor? When would you pick a game-instance subsystem?"
- "Your update can destroy its own actor while you're iterating. How do you keep the loop safe?"
- "When would you choose Mass instead of this?"
