> **Raw Phase 2 research notes (D_existing_work), UE 5.8.4, 2026-11-20.** Written by a research pass and kept as an evidence trail. Every claim cites a file:line or URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md) (marked ✔). Treat anything here that is not restated there as unverified.**

# P01 UpdateBudget: Phase 2 research, part D (existing work)

Research date: 2026-11-20.

**Evidence labels**
- **opened**: I fetched the page and read its content.
- **snippet only**: the claim comes from a search-result summary. I did not read the page myself.
- **source read**: I read the code directly, either from a public GitHub repo or from the local UE install.

**Local engine check.** The machine's UE 5.8 install reports `5.8.4` (changelist 58902052) in `C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version`. (The project plan said 5.8.3 at the time; it has since been corrected to 5.8.4.) Where these notes say "5.8.4 source", I read the `.cpp` files that ship with that launcher install.

Epic docs pages were opened with `?application_version=5.8`, and each page title read "Unreal Engine 5.8 Documentation". The C++ API reference pages under dev.epicgames.com (FTickFunction, MassLOD structs) need JavaScript and came back empty. For those I used the engine source or search snippets, and each one is labelled.

---

## D1. Tick Optimization Toolkit (loonyware)

Sources:
- https://loonyware.com/assets/tick-optimization-toolkit/ (**opened**; the official page with docs and changelog)
- https://www.fab.com/listings/de8d1c17-cb95-4f41-9a15-6d3e57ea214e (**opened**; the Fab listing rendered enough text to read)
- https://forums.unrealengine.com/t/tick-optimization-toolkit/248272 (**opened**; author thread, first post Aug 31, 2021) and post /7 (**opened**; Jan 5, 2022, UE5 update)
- *(An unofficial redistribution site turned up in search. It is not linked here on purpose and was not used as evidence.)*

**What it does.** It reduces how often actors, components and timelines tick, or turns their tick off, based on distance and visibility. The official wording is "reducing their frequency or disabling them with distance and visibility."
- A *Focus* component sits on the player or viewer. A *Target* component sits on each optimized actor.
- Each target defines distance zones: a sphere or a Z-aligned box, optional mid zones, and a buffer size. Each zone has a tick *enabled* flag and a tick *interval* (in seconds) for the visible state and for the hidden state. The docs say an interval of 0.0 means "the tick will fire every frame".
- Visibility modes are None, Front, Rendered and Rendered On Screen.
- With multiple focuses, the zone comes from "the closest focus", and the target counts as visible "if any focus can see the actor".
- Other features:
  - Focus layers.
  - A Mimic component, so for example an AI controller follows its pawn.
  - Animation update-rate (URO) exposure.
  - Scalability through `tot.TickDistanceScale` and `tot.TickOptimizationLevel`.
  - Debug drawing through `tot.DebugDraw`.
- The feature list says it "Evenly distributes ticks between frames." The changelog for 1.4.0 says "Add tick interval balancing." Neither page explains how the balancing works.

**How tick rates are chosen.** From distance zone plus visibility state only. These are designer-set intervals per zone. Nothing on either page selects rates from a frame budget or a significance ranking.

**Millisecond budget.** None. The closest text is marketing: "gain precious milliseconds of game thread time". The plugin enforces intervals, not a time budget.

**Staleness or starvation guarantee.** None stated. By design, zones can turn ticking off completely ("disabling them"). The only first-tick guarantee is *Force Execute First Tick*, which fires the ticks "on the first frame after BeginPlay."

**Delta-time correction.** The plugin's docs don't mention it. The plugin sets standard tick intervals, and in 5.8.4 the engine itself passes interval ticks the time since their last tick (see D3, *Engine tick interval*). Plain interval ticks therefore get accumulated delta from the engine. I did not check whether the plugin's disable/enable path also preserves elapsed time.

**Benchmarks (published by the author).** The official loonyware page, the Fab listing and the author's forum post (Aug 31, 2021) all contain this. The mirror-site report is confirmed by the official page.
- "for 1024 AI Characters from 9 FPS (108 ms) to 55 FPS (18 ms)"
- "for 4096 Environment Actors from 20 FPS (48 ms) to 80-220 FPS (4 ms, GPU bound)"
- Conditions given: only "Results on my machine". There is no hardware, engine version or capture method, and the ms figures are whole-frame times. A downloadable Win64 example build is named `TickOptToolkitExample_Win64_Development.zip`, which suggests a Development build. I have not verified these numbers.

**Engine versions.**
- The example project says "4.25 and later" on loonyware and "4.27 and later" on Fab.
- Changelog 1.1.1 says "Update for UE 5.0 EA". Forum post /7 says it was updated "to work with Unreal Engine 5".
- Neither official page gives a supported-version list. The 1.5.0 changelog entry has no date or engine version. A mirror site claims "V1.5.0 (5.7) / V1.4.5 (5.4–5.6)" (**snippet only**, unofficial, do not cite).

---

## D2. Rare: "Aggregating Ticks to Manage Scale in Sea of Thieves" (Jon Holmes, Unreal Fest Europe 2019)

Sources:
- Video https://www.youtube.com/watch?v=CBP5bpwkO54 (**opened**, metadata only). I read the YouTube page's description and upload date (2019-05-20). The page has caption tracks, but they could not be fetched: the plain request returned empty, and a different way of fetching them was blocked by this session's permission rules. **I have not watched or transcribed the talk.**
- Event page https://www.unrealengine.com/en-US/events/unreal-fest-europe-2019/aggregating-ticks-to-manage-scale-in-sea-of-thieves (**snippet only**; the fetch returned HTTP 403)
- https://forums.unrealengine.com/t/batch-tick-support/2616595 (**opened**; Epic's Ben Zeigler compares the talk with engine tick batching, June 24, 2025)
- https://tomlooman.com/unreal-engine-cpp-timers/ (**opened**; recommends the talk for CPU-cache reasons)
- https://x.com/QuodSoler/status/1619083391873196050 and the matching LinkedIn post (**snippet only**; a third party calls it "Leveraging the instruction cache")
- No slides or written summary turned up in three searches (English and Japanese).

**What the official description says (opened, YouTube description).** Holmes "covers the techniques employed to efficiently manage the scale of Ticking Components and Actors within Sea of Thieves". It also goes through "real-world examples of how the team optimised these systems" and shows how it "plugs in neatly inside Unreal Engine 4".

**What "tick aggregation" means, from secondary sources only.**
- Many instances of the same type are updated by one manager, in one loop, instead of one engine tick function per object. The aim is lower per-tick overhead and better instruction and data cache use.
- The forum poster (not Epic) contrasted engine batching, which works "by tick group and prerequisite", with the talk's by-type approach.
- Ben Zeigler (named by the poster as the author of batched ticking) said the engine system was "designed to deal with similar issues" but "not require any type-specific setup". He also said manual aggregation "can have better cache coherency".

**Rare's numbers.** **None obtained.** I could not read the talk, and no secondary source quotes figures. Do not attribute any number to Rare until someone watches the talk and notes the timestamps.

**For the portfolio:** cite Rare's talk as prior art for *amortizing per-object tick overhead by type*. That addresses cost per update, not *which* updates run under a time budget. Add the precise mechanism only after watching the talk.

---

## D3. Epic official material (read at 5.8, plus 5.8.4 source)

### Actor Ticking (opened)
https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine?application_version=5.8
- Tick groups in order: TG_PrePhysics, TG_DuringPhysics, TG_PostPhysics, TG_PostUpdateWork. The engine also runs other work around them, such as timers, latent actions, camera updates and level streaming.
- Actors and components can tick every frame, at a set minimum time interval, or not at all. The page doesn't name the `TickInterval` property.
- Prerequisites are set with `AddTickPrerequisiteActor`, `AddTickPrerequisiteComponent`, and `AddPrerequisite` for custom `FTickFunction`s. The page prefers these over creating new groups, because actors in the same group can update in parallel.
- Runtime toggles: `SetActorTickEnabled`, `SetComponentTickEnabled`, `bCanEverTick`.
- There is no time-budget feature on this page.

### Engine tick interval and delta time (source read, UE 5.8.4)
`Engine/Source/Runtime/Engine/Private/TickTaskManager.cpp`, `FTickFunction::CalculateDeltaTime`. When the tick has an interval, it returns `CurrentWorldTime - LastIntervalTickSeconds`. In other words, **an interval tick already receives the time accumulated since its previous tick**, using world time, or unpaused time when `bTickEvenWhenPaused` is set. The API reference page (https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FTickFunction) was **opened but JS-only**.

Implication: "accumulated delta time" is **not new** for UpdateBudget. The engine does it for interval ticks, and ABA does it too (below).

### Significance Manager (opened)
https://dev.epicgames.com/documentation/en-us/unreal-engine/significance-manager-in-unreal-engine?application_version=5.8
- A framework that scores and sorts registered objects. `FSignificanceFunction` takes an Object and a Transform and returns a float. `FPostSignificanceFunction` has three modes: None, Concurrent or Sequential.
- Scoring runs in parallel, so it must be thread-safe. Sequential post-functions run in sorted order. `Update` must be called by the game, for example from `UGameViewportClient::Tick`.
- Quote: "The Significance Manager itself does not actually improve performance."
- It runs no updates and has no budget. The project has to supply the behaviour.
- Module name: `SignificanceManager`.

### Animation Budget Allocator (opened docs plus source read)
Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine?application_version=5.8
- Purpose: "constrain the computation time allotted for animation data running on specified skeletal meshes". It needs `USkeletalMeshComponentBudgeted`.
- `a.Budget.BudgetMs` (default 1.0): "The time in milliseconds that we allocate for Skeletal Mesh work".
- Work units: `a.Budget.InitialEstimatedWorkUnitTime` (0.08 ms) is used only for a component's first tick. After that the measured time is used, smoothed by `a.Budget.WorkUnitSmoothingSpeed` (5.0).
- A smoothed budget-pressure value decides when to reduce work. Related CVars: `BudgetFactorBeforeReduceWork` 1.5, `…AggressiveReducedWork` 2.0, `GBudgetPressureBeforeEmergencyReducedWork` 2.5, `BudgetPressureSmoothingSpeed` 3.0.
- Throttles: `ReducedWorkThrottle*`, `StateChangeThrottleInFrames`.
- Interpolation: `MaxInterpolatedComponents` 16, `InterpolationMaxRate` 6, `InterpolationTickMultiplier` 0.75, plus the falloff-aggression CVars.
- **`a.Budget.MaxTickRate` (default 10).** Docs: "The maximum tick rate we allow. If this is set then we can potentially go over budget". "Tick rate" here means frames between updates.
- `a.Budget.MinQuality`: quality is "NumComponentsTickingThisFrame / NumComponentsThatWeNeedToTick". A non-zero value can exceed the budget.
- `a.Budget.MaxTickedOffsreen` (default 4; that is Epic's spelling) is the number of off-screen components ticked, most significant first.

Source: `Engine/Plugins/Runtime/AnimationBudgetAllocator/.../AnimationBudgetAllocator.cpp`, UE 5.8.4.
- **Closed loop.** Each component's game-thread tick and completion time is summed and averaged. The average is smoothed into `AverageWorkUnitTimeMs`. Then `WorkUnitBudget = AvailableBudget / AverageWorkUnitTimeMs`, where `AvailableBudget = BudgetInMs - ExternalAnimationTimeSpent`.
- **Significance order.** Components are sorted by significance into three buckets: always ticked (rate 1), interpolated (rate 2 up to `InterpolationMaxRate`), and rate-limited. The rate-limited bucket's rate is capped by `MaxThrottleRate = min(…, MaxTickRate)`.
- **Staleness.** The cap is in **frames**, not seconds. It applies to the rendered, throttled bucket. Non-rendered components marked tick-even-if-not-rendered are limited to `MaxTickedOffsreenComponents`, and "Disable ticks for the rest" turns off the remainder. So the guarantee is a per-frame-count cap for the components in the budgeted set, not a universal per-object bound.
- **Delta.** `AccumulatedDeltaTime += InDeltaSeconds` each frame. On a tick, `SetExternalDeltaTime(AccumulatedDeltaTime * CustomTimeDilation)` is called and the accumulator resets. **ABA already corrects delta time.**
- A forum explanation of how the budget is spent (https://forums.unrealengine.com/t/animationbudgetallocator-doesnt-utilize-interpolation-on-registered-components-and-it-feels-like-framerate-drop-what-is-a-proper-way-to-address-it/2663219, **opened**). Euan_Carmichael answered (staff badge not visible on the page, Sep 2025): "tick as many meshes as possible within that budget", "then with the leftover budget, interpolate if possible". He also said interpolation is "just about as expensive on the game thread as the full animation update".

### Tick batching, 5.5 and later (opened, several sources)
- Tom Looman, UE 5.5 highlights (https://tomlooman.com/unreal-engine-5-5-performance-highlights/, **opened**). He quotes the release notes: "a new Tick Batching system for actors and components … tick.AllowBatchedTicks". The same notes add `ForEachNestedTick` "to better support manual tick batching".
- Tom Looman, UE 5.6 highlights (https://tomlooman.com/unreal-engine-5-6-performance-highlights/, **opened**):
  - "Improvements to the performance and thread safety of the tick.AllowBatchedTicks system".
  - `tick.IdleTaskWorkMS`.
  - StateTree "will now only tick when needed".
  - "Blueprints will now have their Tick function disabled if it's empty."
- 5.8.4 source: `tick.AllowBatchedTicks` defaults to `0`. Its help text reads "tick functions with bAllowTickBatching will be automatically combined into a single tick task."
- Epic Games Japan, Suzuki Takashi, "UnrealEngine5のCPU最適化テクニック" (UE5 CPU optimization techniques), Dec 9, 2024 (https://www.docswell.com/s/EpicGamesJapan/ZN1982-2024-12-09-104909, **opened**):
  - Batchable ticks are merged into one TaskGraph task, and prerequisites are respected.
  - Reported saving: about **0.6 ms per 2,000 ticks, roughly 0.3 µs each** (attribute to EGJ). Tick overhead was measured at 0.3–0.5 µs on a Ryzen 3975WX in a **DevelopmentEditor** build.
  - The feature is off by default.
- Ben Zeigler, Batch Tick Support thread (**opened**): batching in 5.5 suits simple actor and component ticks, and 5.6 fixed subtle issues. Epic uses **Mass** for specific types, where cache coherency is best. He points to the "Road to 60 fps" talk for the 5.6 work.
- **Batching cuts overhead per tick. It does not choose which ticks run and has no time budget.**
- Deadlock thread (https://forums.unrealengine.com/t/deadlock-in-fticktaskmanager-startframe-due-to-queuetickfunctionparallel-when-tick-allowconcurrenttickqueue-1/2689519, **snippet only**): an Epic reply recommends `tick.AllowBatchedTicks 1` over `tick.AllowConcurrentTickQueue`.

### Other Epic material
- "The Road to 60 fps in The Witcher 4 UE5 Tech Demo", Unreal Fest Orlando 2025. Ari Arnbjörnsson's post (https://x.com/flassari/status/1984257443178901558, **snippet only**) says "300+ moving characters … 60fps on a base PS5". I could not find what it says about tick or game-thread work.
- "Myth-busting 'Best Practices' in Unreal Engine" (https://dev.epicgames.com/community/learning/tutorials/l3E0/myth-busting-best-practices-in-unreal-engine, **opened but JS-only**, so no content).
- Mass LOD: https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine?application_version=5.8 (**opened**). Covered in D5.

---

## D4. EnhancedTick and Optimization Tools

### EnhancedTick (Thyke)
Sources:
- https://github.com/Thyke/EnhancedTick (**opened**). MIT licence, 35 stars. GitHub API: created 2024-07-24, last push 2025-05-03.
- Source read: `EnhancedTickSystem.h/.cpp` and `.uplugin`, via raw.githubusercontent.

README features (quoted):
- "Cache-optimized, batched tick processing"
- "Type-based grouping of similar components"
- "Spatial awareness for location-based optimization"
- "Customizable tick priorities and conditional ticking"
- Multithreading of safe ticks and profiling stats

Priority flags: `HighPrio` ("processed first") and `LowPrio` ("can be skipped in heavy frames").

**How LowPrio ticks are dropped (source).**
- `UEnhancedTickSystem` is a `UWorldSubsystem` plus `FTickableGameObject`. It walks the tick groups in order inside one tickable `Tick`.
- In `TickGroupBatches`, a LowPrio batch runs only when `(FrameCounter % 3) == 0`, so **a fixed one frame in three**. `FrameCounter` wraps at 1000.
- `FComponentTypeBatch::TickBatch` has a second gate: `if (LowPrio && FApp::GetCurrentTime() < 30.0f) return;`. The comment says "under low FPS conditions", but `FApp::GetCurrentTime()` returns the app's current time, not FPS. As written, the gate does not measure frame rate. This is my reading of the code and is worth flagging if cited.
- Batches are auto-flagged LowPrio when the average cost per entity is under 100 ns and the batch isn't HighPrio. This check runs every 300 frames.
- **No ms budget.** Timing is measured only for stats.
- **No staleness bound** beyond the fixed 1-in-3 cadence.
- **No delta correction.** Skipped LowPrio batches later receive the current frame's `DeltaTime`, and the code has no accumulator.
- No benchmarks. No engine version stated (the `.uplugin` has none).

### Optimization Tools plugin (Creative Elicya)
Sources:
- https://www.fab.com/listings/2647dad4-4cf4-4dfa-bd85-cfde0a21bbeb (**opened**). Shows v1.5 and price 0.0 (free) in the page data. A search snippet said "starts at $28.99", possibly an older or other listing, so treat it as unverified.
- Forum thread https://forums.unrealengine.com/t/creative-elicya-optimization-tools-plugin/2683909 (**opened**; Dec 10 and Dec 23, 2025)
- Documentation, a public Google Doc (https://docs.google.com/document/d/1Jp9vPlcd9QunrkrfBJaK0ZbCy4_A30wa3bH818mhXls, **opened** via text export)

Features:
- **Aggregator Ticking:** "Combine similar Actor updates into shared ticks". Objects register into named collections, and an `AggregatorTick` event iterates them "instead of each Object's individual Tick". v1.5 reworked it to be "simpler and aggregator-only".
- **Pool Ticking:** "Distribute Actor ticks across frames for a group of Actor". The docs say "only one (or a small batch) is updated per frame, cycling through the pool sequentially". That is round-robin.
- **Significance:**
  - Distance bands (`DistanceThresholds`) per player viewpoint.
  - Optional view-based downgrade, which the docs say actors "may be downgraded".
  - `UseLowerSignificanceWhenHidden`.
  - Each level drives tick intervals, collision, CharacterMovement settings, animation frame skip, visibility and more.
  - `SignificanceActivationDelay` staggers registration.

What it does not do:
- **No ms budget** anywhere.
- **No staleness guarantee.** The docs say Pool Ticking "requires careful tuning to maintain timely updates".
- Delta handling is not documented.
- No benchmarks.

Example project: UE 5.7, plus a UE 5.4 "old version". No supported-version range is given.

---

## D5. Other existing UE solutions and articles

1. **Epic Mass LOD / Simulation LOD variable tick** (overview page at 5.8, **opened**; API details **snippet only** because the API pages are JS-only)
   - Four LODs: High, Medium, Low and Off. Each level gets distances and "a maximum count of Entities per level". Simulation LOD gives "a variable frequency update".
   - API snippets: `FMassSimulationVariableTickParameters::TickRates` (seconds per LOD) and `bSpreadFirstSimulationUpdate`. `FMassSimulationVariableTickFragment` holds `DeltaTime` and `LastTickedTime`, described as "accumulated delta time to use on the next tick".
   - Budget type: a **count** budget per LOD plus a time interval per LOD. There is **no ms budget**. The LOD's tick rate implicitly bounds staleness. It applies only to Mass entities, not Actors.
   - No Epic benchmarks found.

2. **Tom Looman, Project Orion `URogueDeferredTaskSystem`**
   - Source: https://github.com/tomlooman/ActionRoguelike (**source read**; `.uproject` EngineAssociation 5.8). Page: https://tomlooman.com/unreal-engine-sample-game-action-roguelike/ (**snippet only**).
   - It is a `UTickableWorldSubsystem` with a FIFO `TSpscQueue` of *one-shot* lambdas or delegates.
   - Frame start time is recorded on `FCoreDelegates::OnBeginFrame`. The queue drains until frame time reaches `1/120 s`, but always spends at least `MinBudgetSeconds = 0.001` (1 ms). It is behind `#define USE_DEFERRED_TASKS`.
   - **This is a real ms budget**, open-loop on wall-clock frame time.
   - **No significance.** It is FIFO.
   - **No per-object staleness bound.** The 1 ms minimum guarantees progress, but not a maximum delay.
   - No delta handling. These are one-shot tasks, not recurring updates. No benchmarks.
   - The same repo's `URogueSignificanceSettings` uses **count-based** `BucketSizes` per tag on top of `USignificanceManager`.

3. **Coconut Lizard, "Animation Budget Allocator"** (Christopher Robertson, Apr 28, 2023; https://www.coconutlizard.co.uk/blog/animation-budget-allocator/, **opened**)
   - Third-party ABA measurements in a cooked **Development** build, empty map, humanoid AI, default 1.0 ms budget. Hardware is not stated, and the article refers to the UE 5.1 era.
   - AnimGameThreadTime results (attribute to Coconut Lizard):

     | AI count | ABA off | ABA on |
     |---|---|---|
     | 64 | 14.2 ms | 4.0 ms |
     | 128 | 29.3 ms | 7.6 ms |
     | 256 | 60.8 ms | 15.1 ms |

   - Caveat: past about 128–255 components, quality degrades "to the point where it's immersion breaking."
   - This also shows that ABA overruns its 1 ms budget under heavy load (15.1 ms at 256 AI), which matches its documented cap trade-off (MaxTickRate, MinQuality).

4. **Eric Friedman, "Unreal Engine – Data Oriented Design and the Cost of Tick"** (Oct 14, 2024; https://www.jooballin.com/p/unreal-engine-data-oriented-design, **opened**)
   - 10,000 actors. Timed section: individual Tick about **8.2 ms**, a manager calling the actors about **4.7 ms**, a manager working on its own data about **3.6 ms**.
   - Engine version, hardware and build config are not stated.
   - This is aggregation for overhead. No budget, no staleness.

5. **Count- or distance-based significance plugins.** None of them has an ms budget, a staleness bound or delta handling, as far as their pages say.
   - *Significance Manager: Bucket System* (Sandwich Cake Studios; https://www.fab.com/listings/e4239b0c-bacb-4307-bf61-08c68973d5b6, **opened**). "the nearest actors go into the first bucket", with fixed bucket sizes. It claims "200 AI´s at 60 fps" with no stated conditions.
   - *JSig* (https://codeberg.org/jerobarraco/JSig, **opened**; last commit 2026-06-17). Distance or occlusion significance adjusts tick intervals, visibility and activation.
   - *WardenSystem* (https://github.com/markveligod/WardenSystem, **opened**). Replaces Significance Manager with clustering, for UE 5.0–5.4.
   - *World Director Actor* (BitProtectStudio; https://www.fab.com/listings/cb6da23f-5407-4858-a024-9b9b63e72aaf, **opened**). Off-camera actors "disable client rendering/animation/tick". No numbers.
   - *Tickify* (https://www.fab.com/listings/7f5837cd-8978-451b-a037-75fcc1cf7b86, **opened**). Blueprint subscribe-to-tick nodes, "a complexity of O(n)".

6. **General time-slicing practice**
   - Allen Chou, "Game Programming: Time Slicing" (http://allenchou.net/2021/05/time-slicing/, **snippet only**). Engine-agnostic: split a batch across N frames.
   - Tom Looman's timers guide (**opened**). Names time slicing as an option. Notes 5.4's `bMaxOncePerFrame` to avoid timer catch-up bursts, and says the work "could be done 1 time per frame with a higher DeltaTime to compensate."
   - The "Tick Happens" community tutorial, a centralised World Subsystem tick manager (forum intro https://forums.unrealengine.com/t/community-tutorial-tick-happens/2679765, **opened**; the full tutorial was not read).

---

## Summary table

| Solution | What it controls | ms budget? | Staleness bound? | Delta correction? | Benchmarks? | Evidence |
|---|---|---|---|---|---|---|
| Epic Animation Budget Allocator | Skeletal-mesh anim ticks only | **Yes, closed-loop**: measured, smoothed per-component time, then a work-unit budget | **Partial**: throttle rate capped at `MaxTickRate` (10 frames); docs say it can exceed budget. Off-screen extras are disabled | **Yes**: accumulated delta via `SetExternalDeltaTime` | Third party (Coconut Lizard): 256 AI 60.8 to 15.1 ms, cooked Dev | Docs opened; 5.8.4 source read |
| Epic Significance Manager | Scoring and sorting only | No | No | n/a | None | Docs opened |
| Engine `TickInterval` | Per tick-function interval | No | Interval only | **Yes** (`CalculateDeltaTime`) | n/a | 5.8.4 source read |
| Epic tick batching (5.5+) | TaskGraph overhead per tick | No | n/a (skips nothing) | n/a | EGJ: about 0.6 ms per 2,000 ticks, DevelopmentEditor | Opened |
| Epic Mass Simulation LOD | Mass entities, variable tick per LOD | No (count per LOD) | Implicit, per-LOD interval in seconds | Yes (snippet) | None found | Overview opened; API snippet |
| Tick Optimization Toolkit | Actor, component and timeline tick interval or on/off by distance zone and visibility | No | No (zones can disable tick) | Undocumented; relies on engine intervals | Author: 1024 AI 108 to 18 ms; 4096 actors 48 to 4 ms, GPU bound; "my machine" | Opened (official) |
| Rare SoT tick aggregation | Type-batched tick managers (from secondary sources) | Unknown | Unknown | Unknown | Not obtained | Description opened; talk not watched |
| EnhancedTick | Type-batched ticks; LowPrio runs 1 frame in 3 | No | Fixed 1-in-3 cadence only | No | None | README and source read |
| Optimization Tools (Creative Elicya) | Significance bands to intervals and more; round-robin pool; aggregator | No | No | Undocumented | None | Fab, forum and docs opened |
| Looman `RogueDeferredTaskSystem` | FIFO one-shot tasks | **Yes**, open-loop: 1/120 s frame target, at least 1 ms per frame | No (progress only) | No | None | Source read |
| Bucket System, JSig, WardenSystem, World Director, Tickify | Count or distance buckets, intervals, activation | No | No | Undocumented | Bucket System: "200 AI at 60 fps", no conditions | Opened |
| Friedman DoD article | Manual aggregation | No | n/a | n/a | 10k actors 8.2 to 4.7 to 3.6 ms (conditions not stated) | Opened |

---

## Remaining gap (stated conservatively)

Epic's Animation Budget Allocator already has the core ideas:
- A closed-loop millisecond budget built from measured per-component cost.
- Updates ordered by significance.
- A cap on how many frames an update can be skipped (`MaxTickRate`).
- Accumulated delta time.

The engine's own interval ticks also pass accumulated delta. So UpdateBudget must not present any of these ideas as new. It should present itself as a **generalisation of the ABA model.** The pieces I could not find combined in any public UE solution I reviewed:
- It applies to arbitrary gameplay update callbacks on Actors and components, not only skeletal-mesh animation.
- One scheduler runs a closed-loop ms budget, significance priority, and an explicit per-object maximum staleness.
- The staleness limit is stated in time and holds for every registered object.
- It publishes measurements of budget adherence and staleness under a fixed workload.

Every other tool I looked at does only part of this:
- **Intervals or distance/visibility rules only:** Tick Optimization Toolkit, Optimization Tools, JSig, Bucket System.
- **Overhead reduction only:** tick batching, EnhancedTick, Rare's aggregation as described secondhand, Friedman.
- **Counts per LOD:** Mass LOD.
- **An ms budget without priority or staleness:** Looman's deferred queue.

**Limits of this search:**
- I did not watch Rare's talk.
- Studio-internal schedulers, for example in Fortnite or Sea of Thieves, are not public.
- Fab listings change over time.
- A web search cannot prove absence.

The safe portfolio wording is therefore: *"builds on Epic's Animation Budget Allocator and Significance Manager. Among the public UE tools reviewed (list them), none applied a measured ms budget with significance priority and a per-object staleness bound to general gameplay updates."*
