# 11 · Optimization Decisions: UpdateBudget

**Status:** filled in **Phase 8**, with data. Candidates noticed while implementing are recorded below, with their evidence, and are applied only after Phase 8 measures them.

**Will contain:** Every optimisation considered: the measurement that motivated it, the change, the before/after numbers, and what was rejected.

## Candidates recorded during implementation (not applied; Phase 8 decides with data)

| Candidate | Evidence so far | Why it waits |
|---|---|---|
| **Seed the per-frame sort with last frame's order.** Rebuild `0..N−1` only when N changes | On a clang -O2 copy of UE 5.8's introsort, with 3 staleness tiers and deadlines on frame boundaries: about **2× faster** sorting at N = 1,000 and 4,000 (N = 4,000: 140–195 µs vs 275–380 µs per frame). Exactly the same order, because (deadline, index) is a strict total order | The benchmark kept deadlines stable between frames. The real subsystem recomputes every deadline from significance each frame (02 §6 step 2), so the gain will be smaller. Measure it in the MSVC Development build with a timing scope around `Order.Sort` |
| A binary search (`Algo::LowerBoundBy`) for the mandatory prefix | The linear scan costs about 1–3 µs per frame at N = 4,000 | Too small to matter |
| Sort (deadline, index) pairs instead of indices | 3–6% faster than the index sort with raw reads; adds a 64 KB buffer | Revisit only after the seeded sort |
| A partial sort or heap (only the clients that can fit) | Not measured yet | 02 §12 item 6: v1 uses a full sort, measured at N = 4,000 first |
| **One timer read per update** instead of two (S5 simplify): the reading that ends one client starts the next | `RunClient` reads `QueryPerformanceCounter` twice per update; the efficiency review estimated 10–25 ns per read, about 0.1–0.2 ms per frame at N = 4,000 (an estimate, not a measurement). The timer's resolution is 100 ns (09, S5-c) | The policy's own bookkeeping would then be counted as client cost. Measure the two-read overhead with an Insights scope first |
| Check `Algo::IsSorted` before sorting (S5 simplify) | With S5's equal deadlines the order is already sorted every frame | S6's significance-derived deadlines change every frame; decide together with the seeded sort above |
