# 10 · Interview Questions: UpdateBudget

Ten questions this project prepares you for, each answered from this project's code and measurements.

1. **Why not just use `TickInterval`?** A fixed interval doesn't react to load or relevance: frame cost still grows with the agent count, and an agent that becomes important keeps waiting out its old interval unless you use `SetComponentTickIntervalAndCooldown` (06 §18). UpdateBudget gives a per-frame millisecond budget and a per-agent staleness limit instead.

2. **How does the scheduler decide who runs?** Each agent's significance (distance and view) gives a max staleness, so a deadline = last update + max staleness. Agents whose deadline falls before the next frame are mandatory and always run; the rest run earliest-deadline-first while the predicted cost still fits the budget, and the first one that doesn't fit ends the frame (05 P1).

3. **How do you know no agent starves?** By construction an agent becomes mandatory on the first frame where waiting would make it late. Tested: `NoStarvation` and `RandomFrameInvariants` in the policy, and 0 staleness violations in every measured run, N = 100 to 10,000 (09).

4. **Why a custom `FTickFunction` and not the subsystem's `Tick`?** A tickable world subsystem ticks after `TG_PostPhysics` and can't choose a group; the native baselines tick in `TG_PrePhysics`, so the scheduler must run there too for a fair comparison (05 P4).

5. **What can go wrong when components are destroyed during an update?** Removing from an array the policy reads through a raw pointer would read freed memory. Removals null the slot and adds wait in a pending list until the loop ends; GC-nulled entries are swept after each collection (05 P6e; tested by `DestroyDuringUpdate`, `SpawnDuringUpdate`).

6. **How did you make the comparison fair?** One `OnBudgetedUpdate` runs in every configuration; staleness is recorded at equal cost in every mode; the baselines read the same settings as UB; packaged Development, fixed 60 Hz step, interleaved runs, every run pinned to the performance cores after an E-core problem showed up in the data (09 S3).

7. **Did it beat the baselines?** It beat B0 and B1 about 4× at N = 4,000, but not B2/B3 at the default 2 ms budget, because it spends the whole budget on purpose. That is reported as SC4 not met (09 Phase 8): the budget is a target, and a smaller one would trade freshness for time.

8. **How do you switch modes at run time without a double update?** A cvar change only marks the switch; it is applied after the tick manager's `EndFrame`, from `OnWorldPostActorTick`, so every client updates once in the old mode this frame and once in the new mode from the next (05 P6d; `LiveSwitch` test; 11 switches under load in 09 P7-c).

9. **What does a Blueprint update cost?** About 1.1 µs per minimal event in an editor build, against 4 ns for the native path, measured on a Blueprint built in code; the editor number is an upper bound because editor-compiled Blueprints carry debug instructions (09 S8).

10. **What would you change next?** Measure the budget where UB matches B2/B3 (the planned sweep), stop counting skipped registry entries as free updates in the cost prediction, and move editor-only tests into their own module (09 Phase 8, 12).
