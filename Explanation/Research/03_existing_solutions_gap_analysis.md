# 03 · Existing Solutions and Gap Analysis (UE 5.6 / 5.7 / 5.8)

**Date:** 2026-11-19
**Engine update (2026-11-20):** `UE_5.8` on this machine is now **5.8.4** (CL 58902052). Statements tagged 5.8.3 below were true when written. The facts the roadmap relies on were re-checked on 5.8.4 and are unchanged (`04_local_engine_verification.md` §0).
**Purpose:** For each of the 11 candidate project ideas, record what Unreal Engine already ships, what the community and Fab already offer, how mature each feature is in UE 5.6, 5.7 and 5.8, the known limitations, and the gap a portfolio project could still address. The portfolio must never claim "nobody has built this". This file is the evidence that lets every project name the existing work and state exactly how it differs.

---

## 0. Method

### 0.1 Evidence tiers (every claim is tagged)

| Tag | Meaning |
|---|---|
| **[SRC]** | Read directly from the engines installed on this machine (`C:\Program Files\Epic Games\UE_5.x`): `.uplugin` descriptors, headers, `.cpp` files, `BaseEngine.ini`. This is the strongest evidence for *this* machine. Engines: **5.6.1** (CL 44394996), **5.7.4** (CL 51494982), **5.8.3** (CL 58210709). Also see [04_local_engine_verification.md](04_local_engine_verification.md). |
| **[DOC]** | An Epic page (docs, release notes, blog) that I opened and read. |
| **[FORUM]** | An Epic Developer Community forum thread that I opened and read. |
| **[WEB]** | A third-party page I opened and read (blog, GitHub, Fab listing). |
| **(snippet only)** | Seen only as a search-result summary. I did not open the page. Treat it as weaker evidence. |
| **(unverified)** | A name or claim I could not confirm in docs or source. |

### 0.2 What "status" means here

- **Plugin flag** = the `IsBetaVersion` / `IsExperimentalVersion` fields in the plugin's `.uplugin` file. These drive the Beta/Experimental badge in the editor's Plugins window. I read them from all three installed engines [SRC]. The full list is in [Appendix A](#appendix-a-plugin-maturity-flags-read-from-the-installed-engines).
- **"No flag"** means neither field is `true`. That is **not** the same as Epic's formal "Production-Ready" label.
- **Epic label** = what Epic's release notes or docs say. **It can disagree with the plugin flag.** Iris in 5.8 is the main example (§4).

### 0.3 Known gaps in my own method

- Epic's documentation site now defaults to **"Unreal Engine 5.9 Documentation"**. That header appeared on the Iris and Mass Gameplay pages I opened. A docs page without `?application_version=` therefore describes 5.9 (preview), not 5.8.
- The UE 5.8 release-notes page is about 358k characters. My reads were cut off inside the "AI State Tree" section, so the detailed 5.8 line items for some later sections (Gameplay, Mass, Networking, World Building) were not visible. For those I used the 5.8 highlights section [DOC], Tom Looman's 5.8 article [WEB] and the hotfix notes in the 5.8 forum thread [FORUM].
- Some Epic pages (World Building Guide, productboard roadmap cards) did not render their body text when opened. Facts from them are marked (snippet only).

---

## 1. Release dates and what changed in 5.7 and 5.8

### 1.1 Dates

| Version | Release date | Evidence |
|---|---|---|
| UE 5.7 | **12 November 2025** | Epic forum announcement "Unreal Engine 5.7 Released", posted by Tina_Wisdom on Nov 12, 2025 [FORUM]. Hotfixes 5.7.1–5.7.4 are listed there without dates. This machine has **5.7.4** [SRC]. |
| UE 5.8 | **17 June 2026** (at State of Unreal) | Epic forum announcement "Unreal Engine 5.8 Released", posted by Flak on June 17, 2026 [FORUM]. GameFromScratch's article has the same date [WEB]. The thread lists hotfixes 5.8.1, 5.8.2, 5.8.3 **and 5.8.4**, all without dates. This machine has **5.8.3**, so one hotfix is available [SRC][FORUM]. |
| UE 5.9 | Preview "MegaJam Preview" on **9 Nov 2026**; full release not dated | (snippet only): search summaries of biunivoca.com, digg.com, arcadianpixels.com. |

**Context, unverified and secondhand:** GameFromScratch (Nov 4, 2026) reports, from an attendee's account of the Unreal Fest Tokyo keynote, that UE6 will remove Actors, Blueprints and the C++ gameplay framework in favour of Verse and Scene Graph, with Early Access targeted for end of 2027 [WEB]. The article itself says it has no official Epic document behind this. **Do not treat it as fact.** It matters to the portfolio in one way only: Actor/Component-based systems remain the UE5 standard, and the 5.x line is still supported, but the risk should be noted in the roadmap.

### 1.2 UE 5.7: changes in the areas this file covers

From the UE 5.7 release notes [DOC] unless marked otherwise:

- **Networking / Iris:** Iris moved **out of Experimental to Beta**. Seamless travel is supported, and replication lifecycle functions were renamed. Iris polling now checks only objects and properties marked dirty, and it can run in parallel. `SetConnectionFilter` was removed from `UReplicationSystem`. Iris works with `NMT_JoinNoPawn`. Epic staff (Jan 13, 2026): "Iris is still disabled by default in 5.7 (see net.Iris.UseIrisReplication)" [FORUM]. The `UReplicationBridge` base class was removed to cut virtual-call overhead [WEB: Looman 5.7].
- **Mass:** **Processor time-slicing** with `FMassEntityQuery::FExecutionLimiter`, which limits a query to a set **entity count** (not milliseconds). Also new: `MassQueryExecutor`; `FMassFragmentRequirements` and `FMassEntityQuery` are no longer UStructs; Mass Debugger entity search; a `MassSceneComponentVelocityTranslator`; "Mass puppet actors now get destroyed immediately when the puppet is destroyed." [SRC] `FExecutionLimiter` is in `MassEntityTypes.h` in 5.8. It is absent from the 5.6 MassEntity public headers.
- **Mover:** `SmoothWalkingMode` and `SimpleSpringWalkingMode`; **"simple montage replication support for the character-focused MoverComponent"**; Mover-Mass translators with a Mover Integrations plugin; user-defined struct support for Mover data collections.
- **StateTree:** Rewind Debugger track, Live Property Binding, `ExecutionRuntimeData`, copy/paste of nodes, and a Blueprint helper to run a StateTree on an actor. Static-binding data was moved out of instance data, which lowers per-node memory [WEB: Looman 5.7].
- **Tick / game thread:** "`RegisterTickGroupWorkHandle` to safely schedule batched gameplay thread work". [SRC] That function is in `TaskSyncManager.h` in 5.6, 5.7 and 5.8. Also: a new `FTickableGameObject` constructor option that starts with tick disabled, and a new cvar `tick.AllowBatchedTicksUnordered` (default 0) [SRC].
- **Animation:** Motion Matching integration with Choosers is labelled "(Experimental)". There is a new Chooser column `FPoseSearchColumn`, PoseSearch searches can return multiple results, Motion Warping switch-off conditions were marked experimental, there is a new `AddAnimationNotifyEvent()` overload, and "Animation Budget Allocator: Enabled component tick when unregistering".
- **World Partition / HLOD:** a Custom HLOD actor class (`WorldPartitionCustomHLODActor` per Looman [WEB]), reworked HLOD layer UX, runtime cell-transformer insertion functions, and a grid streaming dependency mode. FastGeo PSO precaching moved off the game thread.
- **PCG:** **the framework is "Production Ready"**. Also: GPU nodes (Scene Capture, Save Texture to Asset), PCG FastGeo Interop to cut game-thread cost for dense scatter, graphs runnable without a world, LLM memory tags, and separate editor and game cache-budget cvars.
- **Developer iteration:** Incremental Cooking (Beta). ThinLTO is on by default for Clang, and Horde has an experimental Build Health dashboard [WEB: Looman 5.7].
- **Memory / GC:** `gc.PerformGCWhileAsyncLoading` can run during async loading when low memory is detected [WEB: Looman 5.7].

### 1.3 UE 5.8: changes in the areas this file covers

From the UE 5.8 release notes highlights and the visible detail sections [DOC] unless marked otherwise:

- **Iris:** "Iris is now production-ready for licensees in UE 5.8." Also new: protocol-mismatch handling, object lifecycle warnings, Scene Graph integration and RemoteObjects RPC routing. **But on disk:** `Iris.uplugin` in 5.8.3 still has `IsBetaVersion: true`, and `net.Iris.UseIrisReplication` still defaults to **0** in `IrisConfig.cpp` [SRC]. The Iris docs page still says "Experimental" [DOC]. **Iris is still opt-in in 5.8.**
- **Mass ("Faster, Modular, and Built for Scale"):** Mass Signals moved into the core engine. Entities can be created off the game thread through archetype-based, lock-free scheduling. A sparse/virtual fragment system avoids costly archetype moves. There is a new MassCore module. [SRC] 5.8 adds `Engine/Source/Runtime/Mass/{MassCore, MassSignals, MassEngine, MassDeveloper}`, and `MassEntity.uplugin` no longer exists. Looman adds that processor execution and dependency resolution were overhauled, and Mass Trace has new events [WEB].
- **Mover:** fixes to the core movement modes, better root motion and montage handling, and ChaosMover trajectory prediction for motion matching. Epic describes work toward moving Mover "out of Experimental", but `Mover.uplugin` is still Experimental in 5.8.3 [SRC].
- **StateTree:** trees can define their own starting state, and a start state can be overridden by gameplay tag. There is a Compiler Manager, queued asynchronous compilation, and out-of-date tracking.
- **World Partition:** streaming debugging tools in Unreal Insights and the editor ("per-cell analysis and sessions playback"). Looman names the new tab the "Spatial Profiler" and the trace channel `WorldStreaming` [WEB]. [SRC] The new `WorldStreamingInsights` plugin is flagged Experimental. Also new: an HLOD perceptual-difference rebuild heuristic, and Build HLOD for a selected region. Looman measured editor HLOD visibility updates about 136x faster in his test scene [WEB]. A GC trigger based on pending actors to unload exists but is off by default [WEB].
- **Level Streaming Persistence (Experimental):** "Added opt-in support for persisting actor states in a map's persistent level." [SRC] 5.8 also adds destroyed-actor persistence, respawning of runtime-spawned actors, `EjectPlacedActor`, `RecreateActorInLevel` and `RecreateActorInPersistentLevel`. **None of these exist in the 5.6 or 5.7 headers.** See §5.
- **Streaming / assets:** Streamable Manager gets just-in-time "trickle" async loading and `FStreamableHandle::SetPriority`. The asset registry cache memory-maps its tag storage. Commandlets print peak memory. Looman reports that the Asset Size Map supports editor and cooked packages, that `LightWeightInstances` was deprecated, and that a new "UObject Count" trace counter exists [WEB].
- **Testing / perf:** Horde Performance Reporting (Experimental), a trends view built on the **Automated Performance Tests framework (APT)**. Looman notes it needs a user-supplied database [WEB]. The detail notes also contain `AutomatedPerfTestNode` and CQTest coverage fixes.
- **Navigation / AI:** static meshes can define their own walkable areas and per-mesh area costs. Navmesh memory was reduced (link pool sizing, packed vertices). EQS gets `bStripFromClientBuilds`, and there are Mass StateTree navigation tasks.
- **Animation:** custom widgets for AnimNotifies, an option not to merge notify-state lifetimes, a fix to montage forced-position handling during advance, `UAnimNotifyState_PoseSearchSamplingRange`, and an experimental `UPoseSearchInteractionLibrary::MotionMatchMulti`. Looman notes many new `a.budget.*` cvars that the release notes do not mention [WEB].
- **Tick:** an `FBatchTickFunctionTask` TickGroup fix, and tick unregistration in `ULevel::ClearLevelComponents`.
- **PCG:** non-destructive manual editing, GPU runtime scatter "within the same performance range as the landscape GPU grass system", and actor/component-less runtime generation. Looman reports up to 30% lower scheduler game-thread cost and the new `pcg.RuntimeGeneration.TimeBetweenRuntimeGenSchedulerTicks` [WEB]. [SRC] `PCG.uplugin` is **EnabledByDefault** in 5.8.
- **Editor / validation:** "The Data Validation Commandlet has been refactored to make custom behaviors via a child class". Fixes for data-validation fixers. Game Features cvar renames and support for `AdditionalPluginDirectories`.

---

## 2. Candidate 1: Frame-budgeted gameplay update scheduler

**Idea:** a game-thread time budget for many Actors/Components, with significance-based update rates and time-slicing.

### 2.1 What the engine ships

| Feature | Names (verified) | What it does / does not do | Evidence |
|---|---|---|---|
| Per-tick-function controls | `FTickFunction::TickInterval`, `TickGroup` / `EndTickGroup`, `AddPrerequisite`, `bHighPriority`, `bRunOnAnyThread`, `bAllowTickBatching` | Fixed intervals and ordering. No budget and no feedback loop. | [SRC] `EngineBaseTypes.h` (see 04 file); Actor Ticking docs `dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine` (snippet only); FTickFunction API (snippet only) |
| Automatic tick batching | cvar `tick.AllowBatchedTicks` (added 5.5) | Groups tick functions that share prerequisites, tick groups and non-high-priority into one task, which cuts TaskGraph overhead. **Does not group by class.** **Default 0 in 5.6.1, 5.7.4 and 5.8.3** [SRC]. `tick.AllowBatchedTicksUnordered` was added in 5.7 (default 0) [SRC]. | Ben Zeigler (Epic), Jun 24 2025: batching in 5.5 "should be good to use for simple actor/component ticks", "a few subtle issues that were fixed in 5.6", and manual aggregation "can have better cache coherency" [FORUM]. Tom Looman tweet: it doesn't sort by class (snippet only). |
| Other tick-manager cvars | `tick.AllowConcurrentTickQueue` (0), `tick.AllowAsyncTickDispatch` (0), `tick.IdleTaskWorkMS` (0; 5.6+) | Dispatch mechanics, not budgeting. | [SRC] `TickTaskManager.cpp` 5.6–5.8 |
| Task Sync Manager | `FTaskSyncManager`, `RegisterTickGroupWorkHandle` | Named sync points for batched game-thread or worker work. The 5.8 header still carries a comment that it is considered experimental. | [SRC] `TaskSyncManager.h`; 5.7 release notes [DOC] |
| Significance Manager plugin | `USignificanceManager` (`RegisterObject`, `UnregisterObject`, `Update(TArrayView<const FTransform>)`), `FOrderedBudget` | Computes a significance value per object and calls your post-significance callback. **The game must call `Update` itself.** It does not measure or cap time. | Docs `dev.epicgames.com/documentation/en-us/unreal-engine/significance-manager-in-unreal-engine` (snippet only). [SRC] plugin present, no flag, 5.6–5.8. |
| Animation Budget Allocator plugin | `IAnimationBudgetAllocator`, `USkeletalMeshComponentBudgeted`, cvars `a.Budget.Enabled`, `a.Budget.BudgetMs`, `a.Budget.BudgetPressure*`, `a.Budget.InitialEstimatedWorkUnitTime` ... | **A real millisecond-budget, closed-loop throttle, but only for skeletal mesh components.** It is the closest existing analogue to this idea and must be cited. | Docs `dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine` (snippet only). [SRC] cvar list from 5.8 plugin source; plugin no flag 5.6–5.8. |
| URO | `FAnimUpdateRateParameters` (`UpdateRate`, `EvaluationRate`, `AdditionalTime`, `TickedPoseOffestTime`) | Skips animation updates by LOD/visibility. Skeletal meshes only. | [SRC] `EngineTypes.h`, `SkinnedMeshComponent.cpp` |
| Mass time-slicing | `UE::Mass::FExecutionLimiter` (5.7+) | Limits a query by entity count per frame, not by ms. Only for code written as Mass processors. | 5.7 notes [DOC]; [SRC] |
| Other engine budgets (proof the pattern exists) | `pcg.FrameTime` (default 5.0 ms [SRC]), World Partition streaming time limits (§5) | Subsystem-specific budgets. | [SRC] |

### 2.2 Status per version

| Item | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| `tick.AllowBatchedTicks` | exists, default 0 | default 0 | default 0 |
| `tick.AllowBatchedTicksUnordered` | absent | default 0 | default 0 |
| SignificanceManager plugin | no flag | no flag | no flag |
| AnimationBudgetAllocator plugin | no flag | no flag | no flag (many `a.Budget.*` cvars) |
| Mass `FExecutionLimiter` | absent | present | present |

### 2.3 Community / Fab

- **Tick Optimization Toolkit** (loonyware, Fab `fab.com/listings/de8d1c17-cb95-4f41-9a15-6d3e57ea214e`; docs `loonyware.com/assets/tick-optimization-toolkit/`) (snippet only). It scales tick frequency by distance and visibility, balances tick intervals across frames, and claims to handle thousands of actors, with multiplayer support. The publisher claims "1024 AI 108 ms → 18 ms", but I saw that only on mirror sites (snippet only, unverified).
- **Optimization Tools plugin** (free, Fab `fab.com/listings/2647dad4-4cf4-4dfa-bd85-cfde0a21bbeb`) [WEB]: significance system, aggregator ticking, pool ticking spread across frames, async traces. The listing does **not** mention a millisecond budget.
- **EnhancedTick** (GitHub `github.com/Thyke/EnhancedTick`, MIT) [WEB]: type-based batching, priorities, and LowPrio ticks that "may be dropped" on heavy frames. No ms budget is described, and no benchmarks are published.
- **Mono Ticking Plugin** (Marketplace) and **TickCheck: Tick Usage Scanner** (Fab, editor-only analyser) (snippet only).
- **Rare, "Aggregating Ticks to Manage Scale in Sea of Thieves"** (Unreal Fest Europe 2019, Jon Holmes) (snippet only). This is the reference talk for tick aggregation.

### 2.4 Known limitations

- Engine batching cuts dispatch overhead. It is not a budget and does not sort by type [FORUM][snippet].
- The Significance Manager has no time model, so each project writes its own policy [snippet].
- The ABA covers skeletal meshes only. Users report missing interpolation unless `SetComponentSignificance` flags and `MaxInterpolatedComponents` are set (UE 5.5 thread, snippet only). Coconut Lizard advises against toggling `a.Budget.Enabled` at runtime (snippet only).
- FTickableGameObject has no TickInterval or tick groups [WEB: voithos.io].

### 2.5 Remaining gap

The engine has fixed intervals, significance scoring and one closed-loop ms budget (ABA, skeletal meshes only). No *general* scheduler enforces a measured game-thread ms budget across heterogeneous gameplay Actors/Components with significance-weighted fairness. Fab tools throttle by distance and visibility and aggregate ticks, but I found none that advertises closed-loop ms-budget control with bounded staleness or starvation guarantees. A project here must present itself as **"generalising the ABA's budget model to arbitrary gameplay work"**. It must benchmark against `tick.AllowBatchedTicks=1`, plain `TickInterval` and a distance-throttle baseline, and report worst-case update latency, not just mean ms. **Gap strength: medium.**

---

## 3. Candidate 2: Large agent counts (Mass) and representation-switch consistency

### 3.1 What the engine ships

- **MassEntity core:** an engine runtime module since 5.5 (snippet only: x157 docs). [SRC] In 5.6 and 5.7 a `MassEntity.uplugin` shell (Beta) still exists. In 5.8 it is gone, and the code lives in `Engine/Source/Runtime/MassEntity` plus the new `Runtime/Mass/{MassCore, MassSignals, MassEngine, MassDeveloper}`.
- **MassGameplay plugin** (representation, LOD, spawning, replication, StateTree, SmartObjects modules). [SRC] modules in 5.8: `MassActors, MassCommon, MassLOD, MassMovement, MassReplication, MassRepresentation, MassSimulation, MassSmartObjects, MassSpawner, MassEQS, MassCharacterTrajectory` and others.
- **Representation LOD:** `UMassRepresentationSubsystem`, `EMassRepresentationType { HighResSpawnedActor, LowResSpawnedActor, SkinnedMeshInstance, StaticMeshInstance, None }`, `FMassRepresentationParameters` (`LODRepresentation`, `bKeepActorExtraFrame`, `bKeepLowResActors`), `UMassVisualizationTrait`. Docs: Overview of Mass Gameplay [DOC] `dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine`. EMassRepresentationType API (snippet only).
- **Actor ↔ entity synchronisation** [SRC, 5.8]: `UMassTranslator` with `EMassTranslationDirection::{ActorToMass, MassToActor, BothWays}`. Shipped translators: `UMassCapsuleTransformToMassTranslator`, `UMassCharacterMovementToActorTranslator` / `...ToMassTranslator`, `UMassCharacterOrientationToActorTranslator` / `...ToMassTranslator`, `UMassSceneComponentLocationToActorTranslator` / `...ToMassTranslator`, `UMassSceneComponentVelocityToMassTranslator`, `UMassTransformToActorCapsuleTranslator`. Sync traits: `UMassAgentSyncTrait`, `UMassAgentMovementSyncTrait`, `UMassAgentOrientationSyncTrait`, `UMassAgentCapsuleCollisionSyncTrait`, `UMassAgentFeetLocationSyncTrait`. **All of them sync transform, movement, orientation or collision. None syncs arbitrary gameplay state.**
- **Instanced Actors plugin** (`AInstancedActorsManager`, `UInstancedActorsSubsystem`, `UInstancedActorsComponent`): Mass-backed hydration and dehydration between ISM and Actor. The API docs say managers "provide replication and persistence for their managed instances" (snippet only). It was introduced in 5.4 as experimental, and PCG can spawn Instanced Actors since 5.6 (snippet only: World Building Guide).
- **MassInsights plugin** (5.6+): fragment views, processor dependencies, processor timing [FORUM; SRC Beta, enabled by default].
- **Epic samples:** CitySample (MassCrowd/MassTraffic), and Lego Fortnite uses InstancedActors and MassGameplay [FORUM].

### 3.2 Status per version [SRC plugin flags]

| Plugin | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| MassEntity (plugin shell) | Beta | Beta | removed (engine modules only) |
| MassGameplay | Experimental | Experimental | Experimental |
| MassAI / MassCrowd / ZoneGraph | Experimental | Experimental | Experimental |
| InstancedActors | Experimental | Experimental | Experimental |
| MassInsights | Beta (on by default) | Beta | Beta |
| MetaHumanCrowd | absent | absent | Experimental (new in 5.8) |

The Mass Gameplay docs page still says "Mass Replication is considered experimental for Unreal Engine 5.1" and "Instanced Static Mesh animation is considered experimental" [DOC].

### 3.3 Community / Fab

- MassSample (Megafunk) `github.com/Megafunk/MassSample` and MassCommunitySample (getnamo) `github.com/getnamo/MassCommunitySample` (snippet only).
- x157 Mass notes `x157.github.io/UE5/Mass/`, vrealmatic visualization notes, and hititou MassGameplay source analysis `github.com/hititou/MassGameplay_Analysis` (snippet only).
- Fab crowd plugins were **not surveyed** in this pass.

### 3.4 Known limitations (with sources)

- Epic (James Keeling, Apr 25 2025): "Our approach has been to keep the entity while removing the Actor representation when switching LOD". There are "no best practices for Mass replication" yet, and no Blueprint support is planned: "it would negate all of the performance gains of Mass". BehaviorTree and GAS will likely not be ported, and ZoneGraph is in maintenance mode [FORUM].
- Mass replication: Epic is investigating Iris-based Mass replication. "It will not be in 5.7" (Nov 6 2025). On Jun 1 2026: "Replication improvements are in development but unlikely to land in time for 5.8" and are not production-ready [FORUM]. A 5.8 Preview crash was tied to the processing-queue cvar [FORUM]. In 5.7 the `mass.UseProcessingQueue` default reportedly broke MassReplicator queries (snippet only).
- The 5.8.2 hotfix notes mention "safer puppet handle assignment in MassActors" [FORUM], a sign that actor/entity binding is still being hardened.

### 3.5 Remaining gap

Mass ships representation switching and translators for transform, movement, orientation and collision. Instanced Actors handles hydration for mostly static props. **No shipped, generic mechanism guarantees that non-spatial gameplay state survives Actor → ISM → None → Actor transitions without loss or duplication**: health or attributes, AI memory, montage or animation phase, cooldowns, interaction reservations. Nothing measures that correctness either. Epic's stated approach is to keep the entity and drop the Actor, so each project hand-writes the hand-off. A project could define an explicit state-ownership contract per representation, plus an automated invariant checker and benchmarks: transition cost in µs, divergence errors per 1,000 transitions, and frame-time spikes during mass LOD flips. **Gap strength: strong.** Risks: everything is Experimental, the API churns between versions (5.7 and 5.8 both restructured Mass), multiplayer replication of Mass is not ready, and the Mass/Actor future under UE6 is unclear (unverified).

---

## 4. Candidate 3: Network bandwidth and server CPU at scale

### 4.1 What the engine ships

| System | Names | Notes | Evidence |
|---|---|---|---|
| Generic (legacy) replication | `UNetDriver`, `FObjectReplicator`, `NetUpdateFrequency`, relevancy/priority | Default system in 5.6–5.8. | Networking Overview `dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine` (snippet only) |
| Push model | `net.IsPushModelEnabled`, `FDoRepLifetimeParams::bIsPushBased`, `MARK_PROPERTY_DIRTY`, `MARK_PROPERTY_DIRTY_FROM_NAME`, `net.PushModelSkipUndirtiedReplication`, `net.PushModelValidateProperties` | Game code marks dirty instead of the server diffing properties. | [SRC] `PushModel.h`, `UnrealNetwork.h` (5.8). Console commands page (snippet only). |
| Replication Graph plugin | `UReplicationGraph` | **Not deprecated.** Epic (Jan 13 2026): "no plans to deprecate the legacy replication system or the Replication Graph" [FORUM]. Cannot be combined with Iris: "the network driver can only use one or the other" (snippet only: Migrate to Iris docs). | [SRC] flag Beta in 5.6–5.8 |
| Iris | `UReplicationSystem`, `UObjectReplicationBridge`, replication fragments, `FNetSerializer` (quantised state copies) | Docs: Iris "keeps a full copy of all replicated state data in a quantized form" and works best when game code notifies it (snippet only: Components of Iris). It needs `RegisterReplicationFragments` and the registered subobject list (snippet only). | Iris docs `dev.epicgames.com/documentation/en-us/unreal-engine/iris-replication-system-in-unreal-engine` [DOC, shows "Experimental"] |
| Iris filtering | `UNetObjectFilter`, `UNetObjectGridFilter`, `UNetObjectGridWorldLocFilter`, `UNetObjectConnectionFilter`, `UAlwaysRelevantNetObjectFilter`, `UFilterOutNetObjectFilter` | The official replacement for RepGraph spatialisation. | [SRC] 5.8 `Runtime/Net/Iris` |
| Iris prioritisation | `UNetObjectPrioritizer`, `ULocationBasedNetObjectPrioritizer`, `USphereNetObjectPrioritizer`, `USphereWithOwnerBoostNetObjectPrioritizer`, `UFieldOfViewNetObjectPrioritizer` | Built-in priority models. | [SRC] |
| Networking Insights | Packet Overview / Packet Content / Net Stats | Bit-level packet inspection. | `dev.epicgames.com/documentation/en-us/unreal-engine/networking-insights-in-unreal-engine` (snippet only) |
| Network Prediction plugin | NPP | No plans to integrate with CMC (Epic, snippet only). | [SRC] flag Beta 5.6–5.8 |
| Mover / ChaosMover | `UMoverComponent` | Rollback networking via NPP or Chaos networked physics. "Currently in Experimental" (GASP 5.7 blog, snippet only). CMC stays supported (Mover docs, snippet only). | [SRC] Experimental 5.6–5.8 |

### 4.2 Iris status per version (all sources)

| | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| `Iris.uplugin` flag [SRC] | Experimental | **Beta** | **Beta** (unchanged) |
| Source location [SRC] | `Runtime/Experimental/Iris` | `Runtime/Net/Iris` | `Runtime/Net/Iris` |
| `net.Iris.UseIrisReplication` default [SRC] | not checked | **0** | **0** |
| Epic label | Experimental | Beta: "moved out of Experimental" [DOC 5.7 notes]; "compiled in by default", "disabled by default" [FORUM] | "production-ready for licensees" [DOC 5.8 notes] |
| Docs page header | — | — | Shows "5.9 Documentation" and "Experimental" [DOC] |

### 4.3 Community

- **BorMor, "Iris Replication System: 100 Players in One Place"** (Oct 25 2025, UE 5.7 Preview 1) [WEB]. 100 players on a 2-vCPU Linux server: default replication 83.9 ms/tick vs stock Iris 60.1 ms. Patching a per-frame 524 KB allocation in `FReplicationWriter` brought it to 45.5 ms. **No bandwidth comparison and no custom filtering or prioritisation were tested.** Iris was enabled with `net.Iris.UseIrisReplication=1`, `net.Iris.PushModelMode=1` and `SetupIrisSupport(Target)`. The author calls RepGraph deprecated, which contradicts Epic [FORUM].
- StraySpark Iris articles (snippet only; they disagree on the 5.7 label), vorixo "Iris – Replication filters" `vorixo.github.io/devtricks/iris-replication-filter/` (snippet only), Kieran Newland push-model article (snippet only).

### 4.4 Remaining gap

Relevancy, filtering and prioritisation frameworks already exist: Iris filters and prioritizers, RepGraph, push model. **Building "a new relevancy system" would not be a real gap.** What is missing is a **controlled, reproducible comparison on one gameplay workload: legacy vs legacy+push vs RepGraph vs Iris (default) vs Iris with a game-specific prioritizer**. It should report bandwidth per client, server ms per net tick and correctness under emulated loss, which Iris 5.7 now supports via `netEmulation.PktBufferBloatInMS` [WEB: Looman 5.7]. The only public benchmark I found measures frame time, not bandwidth. A gameplay-aware `UNetObjectPrioritizer` would make the project distinct. Mass + Iris replication is unavailable in 5.8. **Gap strength: medium.**

---

## 5. Candidate 4: World Partition streaming and streamed-actor state

### 5.1 What the engine ships

- **Streaming sources:** player controllers, and `UWorldPartitionStreamingSourceComponent` (implements `IWorldPartitionStreamingSourceProvider`) [SRC 5.8 header]. Docs `dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine` (snippet only).
- **Streaming time limits:** `s.LevelStreamingActorsUpdateTimeLimit` and `s.LevelStreamingComponentsRegistrationGranularity` [FORUM]. Since 5.6: multiple AddToWorld/RemoveFromWorld per frame and a unified budget for async loading plus level streaming (snippet only: World Building Guide).
- **Fast Geo Streaming plugin** (5.6+): lightweight registration of static geometry through runtime cell transformers, with its own time budget (snippet only: World Building Guide). [SRC] Experimental 5.6–5.8.
- **HLOD:** layer types Instancing / Merged Mesh / Simplified Mesh (snippet only: HLOD docs). 5.7 adds a Custom HLOD actor. 5.8 adds a perceptual-difference rebuild and region builds [DOC].
- **Level Instances / Packed Level Actors**. HLOD for sub-World-Partition is experimental (snippet only).
- **World Partition Insights (5.8):** `WorldStreamingInsights` plugin (Experimental [SRC]), trace channels `WorldStreaming`, `WorldStreamingPriority`, `WorldStreamingDependencies` (snippet only: Insights reference). The Spatial Profiler tab and `wp.Editor.ExportMinimapForInsights` come from [WEB: Looman 5.8].
- **Level Streaming Persistence plugin** (`ULevelStreamingPersistenceManager : UWorldSubsystem`, `ULevelStreamingPersistenceSettings`) [SRC]:
  - **5.6 / 5.7:** persists **config-listed properties** (`+Properties=(Path="/Script/Engine.TextRenderComponent:Text")` style) of objects in streamed levels. It offers `SerializeTo` / `InitializeFrom` byte arrays for save games, and `Get/SetPropertyValue`.
  - **5.8 adds** [SRC header diff]: `bIncludePersistentLevel`, `bPersistAllActorDestruction` / `PersistedActorDestructionAllowList` (destroyed map actors stay destroyed), `RuntimeRespawnedActorClasses` (runtime-spawned actors are recorded and respawned), `bCanRemoveActorsBeforeBeginPlay`, `EjectPlacedActor`, `RecreateActorInLevel`, `RecreateActorInPersistentLevel`, and per-property object/outer class filters.
  - **Limits read from source:** it is created only for non-client game worlds (`NM_Client` excluded), so it is server/standalone only. Persisted properties must be listed in config. Restoring references and hiding visual pop on recreate are "left up to the callee" (header comment). It is flagged **Experimental** in all three versions.
- **Instanced Actors** persistence for instanced props (§3).

### 5.2 Status per version [SRC]

| Plugin | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| LevelStreamingPersistence | Exp (properties only) | Exp (properties only) | Exp (+ destruction, runtime respawn, persistent level) |
| FastGeoStreaming | Exp | Exp | Exp |
| WorldStreamingInsights | absent | absent | Exp |
| WorldPartitionHLODUtilities | no flag | no flag | no flag |

### 5.3 Community / Fab

- **SPUD (Steve's Persistent Unreal Data)**, MIT, `github.com/sinbad/SPUD` (snippet only). It officially supports World Partition (`bSupportWorldPartition=true`) and persists streamed-level state automatically. Runtime-spawned actors are handled by `USpudRuntimeStoredActorComponent`. Open issue #117: WP changes between 5.4 and 5.5 broke saves. It is still marked beta.
- A Nance Dev Diary (Patreon/YouTube) used Level Streaming Persistence so WP actors remember their state (snippet only).
- The usual DIY pattern is a GUID-keyed persistence manager [snippet: Epic forum threads 535746, 2711156, 2020977]. FlickerVoid issue #25 shows the "records dropped for unloaded cells" bug in hand-made save systems (snippet only).
- Easy Multi Save (Fab) was **not checked**.

### 5.4 Known limitations

- `UWorld::AddToWorld` breaks out when its time limit is exceeded, but `AActor::IncrementalRegisterComponents` ignores time limits. Component registration granularity is a count, not a time [FORUM, UE 5.5, Apr 2025]. Epic's answer pointed to a PDF of 5.6 cvar changes that is not included in the thread [FORUM].
- Spatially loaded actors lose runtime changes on unload unless you persist them yourself (snippet only: forum threads).
- WP navmesh support for data layers is "quite minimal" (Epic, snippet only).

### 5.5 Remaining gap

**Persistence of streamed-actor state is largely covered.** UE 5.8 Level Streaming Persistence handles properties, destruction and runtime-spawned actors (Experimental, server-side, config-driven), and SPUD supports WP. A "WP save system" project would be a weak portfolio piece unless it targets something these miss: schema versioning and migration of saved state, multiplayer client prediction of restored state, or measured restore cost per cell. **Streaming hitch control is the stronger angle.** A per-actor-class registration cost model that schedules `AddToWorld` work by measured cost instead of component count would answer the overrun described in the forum, and could be validated with 5.8's World Partition Insights. Epic is actively improving this (5.6 unified budgets, FastGeo), so the project must benchmark against those. **Gap strength: weak (persistence) / medium (streaming-hitch budgeting).**

---

## 6. Candidate 5: Animation–gameplay synchronisation

### 6.1 What the engine ships

- **Notifies:** `UAnimNotify`, `UAnimNotifyState`, and `FAnimNotifyEvent` fields `MontageTickType` (`EMontageNotifyTickType`: Queued / BranchingPoint), `NotifyTriggerChance`, `NotifyFilterType` / `NotifyFilterLOD`, `bTriggerOnDedicatedServer` [SRC 5.8 `Public/Animation/AnimTypes.h`]. Docs: "Queued Notifies is asynchronous, while Branching Points are synchronous"; Branching Points are more precise but more expensive (snippet only: 4.27 notifies docs). Current docs `dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine` (snippet only).
- **URO internals** [SRC 5.8]: with URO, `TickPose` uses `DeltaTime + AnimUpdateRateParams->GetTimeAdjustment()`. `AdditionalTime` holds "Total time of the last series of skipped updates". **Skipped time is applied at the next update, so notifies inside the skipped window fire late and bunched in one update, not lost.** In `LookAheadMode`, `AdditionalTime = LookAheadAmount`, so the animation advances ahead of real time. **Inference from the source: notifies can then fire early.** `OnUROSkipTickAnimation()` is called on skipped frames. The ABA path uses `bExternalTickRateControlled` / `ExternalDeltaTime`.
- **Server notify handling:** cvar `a.EnableQueuedAnimEventsOnServer` (default 1 in 5.6 and 5.8), "Enabling this is recommended in projects using Listen Servers" [SRC]. The server-move path dispatches and clears queued anim events; the code was restructured in 5.8 [SRC].
- **Montage replication:** `ACharacter::RepRootMotion` (`FRepRootMotionMontage`) for root-motion montages [SRC]; GAS `FGameplayAbilityRepAnimMontage` via `UAbilitySystemComponent::PlayMontage` (snippet only); Mover "simple montage replication" (5.7 [DOC]).
- **Locomotion / warping:** Pose Search (Motion Matching), Chooser, Motion Warping, Animation Warping, UAF (formerly AnimNext). Game Animation Sample (GASP), updated for 5.7 with a Mover character (snippet only).

### 6.2 Status per version [SRC flags]

| Plugin | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| PoseSearch | no flag | no flag | no flag |
| Chooser | no flag | no flag | no flag |
| Motion Matching ↔ Chooser integration | — | "(Experimental)" [DOC] | — (5.8: column fixes [DOC]) |
| MotionWarping | Beta | Beta | Beta |
| AnimationWarping | no flag | no flag | no flag |
| AnimNext / UAF | AnimNext Exp | UAF Exp | UAF Exp |
| Mover | Exp | Exp | Exp |

### 6.3 Known limitations (with sources)

- "URO and AnimNotifies" (UE 4.27, Jul 2022): with URO, notify timing drifts (footsteps), and BranchingPoint vs Queued "doesn't make a difference". There are no replies [FORUM]. The source reading above explains the mechanism.
- **UE 5.6.1** (the user's version): NotifyBegin/NotifyEnd fire repeatedly on a dedicated server during a montage. The poster traced it to `ServerMovePacked → MoveAutonomous → ClearQueuedAnimEvents`, says it is not present in 5.5.4, and gives tracker id **UE-308913**. The workaround is client-authoritative notify states plus a server RPC [FORUM, Aug–Oct 2025]. Whether it is fixed in 5.7/5.8 is **unverified**; the code path changed in 5.8 [SRC].
- Notifies don't fire when the mesh doesn't tick (`VisibilityBasedAnimTickOption`), or on servers without `bTriggerOnDedicatedServer` (snippet only: bugnet.io). High PlayRate can step past notify windows (snippet only). Queued notify states can fire twice (snippet only, forum 142233). Notify-state ticks depend on frame rate (snippet only, forum 740638).
- Mover montage replication desync on sim proxies when start position ≠ 0 was fixed in 5.7.1 (UE-350214) (snippet only: 5.7.1 hotfix summary).

### 6.4 Community

- No plugin found that schedules gameplay events from montage timing independently of the animation update rate. Search results were only forum workarounds such as socket colliders, branching points and always-tick meshes (snippet only).
- vorixo "GAS – Advanced Network Optimizations" (replication proxy for montage info) (snippet only).

### 6.5 Remaining gap

Gameplay-critical events (hit windows, footstep-driven gameplay, ability commit points) are tied to animation evaluation. In UE 5.6–5.8 they fire late or bunched under URO and the ABA, can fire early in look-ahead mode, are filtered by LOD, depend on mesh visibility and tick settings, and have open server-side bugs on 5.6.1. Epic ships the primitives but no **gameplay-clock event track** that fires authored montage events at the correct gameplay time regardless of animation update rate, net role or visibility. A project can measure this objectively: **timing error in ms per event vs authored time, missed or duplicate event counts**, across URO rates, ABA budgets, frame rates and server/client roles, before and after. **Gap strength: strong.** It must cite branching points, `a.EnableQueuedAnimEventsOnServer`, GAS and Mover montage replication as the existing partial answers.

---

## 7. Candidate 6: Automated testing (suite effectiveness, perf regression gating)

### 7.1 What the engine ships

- **Automation Test Framework:** `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `IMPLEMENT_COMPLEX_AUTOMATION_TEST`, `DEFINE_LATENT_AUTOMATION_COMMAND`; **Automation Spec** `BEGIN_DEFINE_SPEC` / `END_DEFINE_SPEC` [SRC 5.8 `Core/Public/Misc/AutomationTest.h`]. Docs `dev.epicgames.com/documentation/unreal-engine/automation-test-framework-in-unreal-engine` (snippet only).
- **Functional tests:** `AFunctionalTest` (Developer/FunctionalTesting module [SRC]); FunctionalTestingEditor plugin (no flag).
- **CQTest** ("Code Quality Test", from Rare's Sea of Thieves framework): `TEST_CLASS`, `TEST_METHOD`, `BEFORE_EACH`, `AFTER_EACH`, `FActorTestSpawner` (snippet only: CQTest docs `dev.epicgames.com/documentation/en-us/unreal-engine/cqtest-test-framework-for-unreal-engine`). Epic recommends CQTest for functional tests and Low-Level Tests for unit tests (snippet only, forum Nov 2025). [SRC] no flag 5.6–5.8.
- **Low-Level Tests:** `LowLevelTestsRunner` [SRC]; 5.8 fixes for its game-thread tag [DOC].
- **Gauntlet:** a UAT-side session framework plus `UGauntletTestController` (docs `dev.epicgames.com/documentation/en-us/unreal-engine/gauntlet-automation-framework-in-unreal-engine`, snippet only). [SRC] Gauntlet plugin Beta 5.6–5.8.
- **Automated Performance Testing (APT) plugin** `Engine/Plugins/Performance/AutomatedPerfTesting` [SRC]. Test types in 5.6: StaticCamera, Sequence, Replay, Material, Soak. In 5.8: StaticCamera, Sequence, Replay, Material, ProfileGo. Settings classes `UAutomatedPerfTestProjectSettings`, `UAutomatedSequencePerfTestProjectSettings`, `UAutomatedStaticCameraPerfTestProjectSettings` (snippet only: API). Epic's Matt Oztalay (Nov 2025): tests "switch between static cameras/do a flythrough/run a replay", "UI flow is still an open question", no easy backport to 5.5, and more framework work planned for 5.8 [FORUM]. Introduced at Unreal Fest Bali 2025 as "very experimental" glue over BuildGraph, UAT, Gauntlet, CSV Profiler, PerfReportTool and Insights (snippet only).
- **CSV Profiler + PerfReportTool** (e.g. `-reportType ClientPerf -summaryTable KeyStats`, snippet only).
- **Horde Performance Reporting (5.8, Experimental):** a trends view over APT data [DOC]. It needs a user-supplied database [WEB: Looman 5.8].

### 7.2 Status per version [SRC]

| Plugin | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| CQTest | no flag | no flag | no flag |
| FunctionalTestingEditor / RuntimeTests | no flag | no flag | no flag |
| Gauntlet | Beta | Beta | Beta |
| AutomationUtils | Beta (on) | Beta (on) | Beta (on) |
| AutomatedPerfTesting | Experimental | Experimental | Experimental |
| Horde perf reporting | — | — | Experimental [DOC] |

### 7.3 Community

- Code coverage: no built-in coverage. "Unreal Engine ... offers no built-in coverage functionality" (snippet only: arXiv 2601.16881). Community setups use OpenCppCoverage (UE4-TDD-CI_Testing) or patch UBT for gcov (pgaleone.eu 2022). Hazelight's Angelscript fork has coverage (all snippet only).
- **Mutation testing: nothing found for Unreal.** Only general C++/Clang research appeared (snippet only: arXiv 2210.17215).
- Daedalic `ue4-test-automation` (Gauntlet, UE 4.23–4.27) (snippet only). Rare "Automated Testing at Scale in Sea of Thieves" (UFE 2019) (snippet only). Andrew Fray, "The Topography of Unreal Test Automation in 2025" (snippet only; notes CQTest lacks parametric tests).

### 7.4 Remaining gap

Test frameworks are plentiful (Automation, Spec, CQTest, LLT, Gauntlet). Epic now ships an experimental perf-test harness (APT) plus Horde trends. Two gaps remain:

1. **Measuring suite effectiveness.** No coverage tool ships, and I found no mutation testing for UE gameplay C++. A project that injects controlled faults (mutants) into a gameplay module and reports a mutation score for its CQTest/Spec suite would be distinctive. Expect it to be hard with MSVC and UHT-generated code.
2. **Gameplay (game-thread) perf regression gating that runs locally without Horde.** It would use statistical thresholds (median and p95 vs baseline, with noise bounds) on a deterministic gameplay workload. APT is camera/replay/tech-art oriented and experimental.

**Gap strength: strong (effectiveness via mutation testing) / medium (perf gating).**

---

## 8. Candidate 7: Memory and asset loading (transitive hard-reference budgets)

### 8.1 What the engine ships

- `UAssetManager`, `FPrimaryAssetId`, Asset Bundles (`AssetBundles` meta on soft references), `FStreamableManager` / `FStreamableHandle`. 5.8 adds JIT trickle loading and `SetPriority` [DOC]. Docs `dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine` (snippet only).
- **Size Map / Reference Viewer / Asset Audit** (AssetManagerEditor plugin, on by default, no flag [SRC]). Size Map shows disk or memory size of an asset plus its dependencies (snippet only: Reference Viewer docs). 5.8 adds editor and cooked packages to the Size Map [WEB: Looman 5.8].
- **Memory Insights + LLM:** `-trace=default,memory,metadata,assetmetadata -llm`; LLM is unavailable in Shipping (snippet only: Memory Insights docs `dev.epicgames.com/documentation/en-us/unreal-engine/memory-insights-in-unreal-engine`). **Per-tag memory budgets exist in Insights since 5.6.** [SRC] `TraceInsights/.../MemoryProfiler/ViewModels/MemTagBudget.{h,cpp}` and `MemTagBudgetGrouping` are present in 5.6 and 5.8. The roadmap card "Insights Asset Memory Profiling (Experimental)" describes per-platform budgets per asset type and over-budget marking (snippet only).
- `memreport` still exists, but Epic staff advise against it as a primary tool; Memory Insights is the main tool (snippet only).
- **Asset Referencing Restrictions plugin** (`UAssetReferencingPolicySettings`, `UAssetReferencingPolicySubsystem`, `EditorValidator_PluginAssetReferences`): rules for which folders or plugins may reference which [SRC; no flag].
- 5.8 adds the "UObject Count" trace counter [WEB: Looman 5.8].

### 8.2 Community / Fab

- **YAGER DependencyAnalyser** `github.com/alessianigretti/DependencyAnalyser` (fork of YAGER-Development) [WEB]. It measures dependency count and size per asset and has an **Automation test suite with warning/error size limits per asset type**, runnable headless with `-unattended -buildmachine`. Tested on UE 5.1.1 and 5.3.2, 167 stars. **This already does "budget enforcement of dependency size".**
- **CommonValidators** `github.com/nurbed/CommonValidators` [WEB]. Its Heavy Reference validator has a 2 MB default, but the README says it is "not working reliable as of now". The repo also has Blocking Load, Empty Tick and Pure Node validators.
- HardReferenceFinder `github.com/heyomidk/HardReferenceFinder` (UE ≤5.3), Unused Asset Detective (Fab, 5.6–5.8) and Fab Package Builder (transitive dependency scan) (all snippet only).

### 8.3 Remaining gap

Editor-side transitive-size budgets already exist (YAGER DependencyAnalyser, CommonValidators, Size Map). Runtime per-tag budgets exist in Memory Insights (5.6+). What remains is narrower. Nothing I found **correlates the editor-estimated dependency cost with the measured runtime memory of the same Primary Asset/bundle in a cooked Test build**. Nothing **gates a change on the delta** either: "this commit added +38 MB to the `Hero` bundle's resident cost". Unless the project targets that correlation or delta, it would duplicate existing tools. **Gap strength: weak.**

---

## 9. Candidate 8: Editor tools and content validation

### 9.1 What the engine ships

- **Data Validation plugin** (enabled by default, no flag [SRC]). `UEditorValidatorBase` (`CanValidateAsset`/`ValidateLoadedAsset`, `GetAssetsToValidateFromChangelist`, `PostAssetValidation`, Blueprint `K2_CanValidate`/`K2_ValidateLoadedAsset`), `UEditorValidatorSubsystem` (`ValidateChangelistPreSubmit`), `UDataValidationCommandlet`, `UDataValidationChangelist`, `UDirtyFilesChangelistValidator`, `DataValidationFixers.h` (auto-fix support). Built-in validators: `UEditorValidator_ActionUtility`, `_Localization`, `_Material` [SRC header list 5.8]. API docs `dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/DataValidation` (snippet only). 5.8: commandlet refactored for child-class customisation [DOC].
- `UObject::IsDataValid(FDataValidationContext&)` per class (snippet only).
- Asset Referencing Restrictions (§8), Plugin Audit and Plugin Reference Viewer (Beta [SRC]).
- Editor Utility Widgets/Blueprints, EditorScriptingUtilities (Beta [SRC]), Python, commandlets (e.g. `ResavePackagesCommandlet`, a `BatchProcessCommandlet` usage line in the 5.8 notes [DOC]). Asset Registry, with a memory-mapped cache in 5.8 [DOC].

### 9.2 Community / Fab

- **Linter v2** (Gamemakin/Allar) `fab.com/listings/10850cb2-be1d-43d2-971d-922c73d218f3`: rule sets as Data Assets, style-guide and marketplace rules, commandlet (snippet only; listing about 6.7 years old).
- CommonValidators (above) [WEB]; Asset Naming Assistant, Smart Naming Cleanup and Asset Naming Convention Keeper (Fab, snippet only). "ta Asset Doctor" (CI-capable, 5.7–5.8) was seen **only on a piracy mirror**, so it is unverified.

### 9.3 Remaining gap

The validation *framework* is mature and built in: changelist validation, commandlet, fixers, reference-domain rules. Naming and linting tools are common. A project that "adds a validation framework" has no gap. The remaining room is **semantic, cross-asset gameplay-data validation**, for example: every GameplayTag referenced by abilities/StateTrees exists, DataTable row handles resolve, ability cost/cooldown curves are monotonic, and Smart Object slots match animations. It would be **measured** by seeded-defect detection rate and incremental changelist-validation time on a large asset set. **Gap strength: weak (framework) / medium (semantic rules with measured outcomes).**

---

## 10. Candidate 9: AI at scale

### 10.1 What the engine ships

- **AI Perception:** `UAIPerceptionSystem`, `UAIPerceptionComponent`, `UAISense_Sight`. [SRC 5.8] Sight already has time-slicing: `MaxTracesPerTick` (default 6), `MaxAsyncTracesPerTick` (10), `MaxTimeSlicePerTick` (0.005 s), `MinQueriesPerTimeSliceCheck`, `bUseAsynchronousTraceForDefaultSightQueries`. These are config properties (snippet only: forum tuning reports).
- **EQS** (EnvironmentQueryEditor on by default, no flag [SRC]); 5.8 adds `bStripFromClientBuilds` [DOC].
- **StateTree / GameplayStateTree** (no flag 5.6–5.8 [SRC]); 5.7 and 5.8 improvements listed in §1. Docs `dev.epicgames.com/documentation/unreal-engine/state-tree-in-unreal-engine` (snippet only).
- **Smart Objects** (no flag [SRC]); docs `dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine---overview` (snippet only).
- **MassAI, MassCrowd, ZoneGraph** (Experimental [SRC]); HTNPlanner (Beta [SRC]); LearningAgents (Experimental [SRC]).
- **No built-in influence or tactical map system.** No such plugin in the 5.8 plugin list [SRC: `Engine/Plugins/AI` has AISupport, EnvironmentQueryEditor, HTNPlanner, MassAI, MassCrowd, MLAdapter].

### 10.2 Community / Fab

- **TCAT, Tactical Crowd AI Toolkit** (GPU influence maps, Krafton Game Tech Lab): GitHub `github.com/Over2K-GTL/TacticalCrowdAIToolkit` and Fab `fab.com/listings/d81cbf03-5e90-4920-ad24-1921e314399a`. Behavior Tree, EQS and Insights integration (snippet only).
- **Wise Feline Influence Maps** (NoOpArmy, Fab) `nooparmygames.com/wise-feline-influence-maps-for-unreal/` (snippet only).
- **Wise Feline Utility AI** (Fab) (snippet only).

### 10.3 Remaining gap

Influence maps (including GPU ones), utility AI, StateTree and Smart Objects all exist, and sight perception already time-slices. A tactical-map project would be weak. The remaining angle is **perception/decision LOD for hundreds of Actor-based agents**: sharing sensing results across squads and adapting `MaxTracesPerTick`-style budgets to load. It would be measured as **detection latency (ms from line-of-sight to perception event) vs CPU ms**. The "budget" half overlaps Candidate 1, so the two would duplicate each other. **Gap strength: weak–medium.**

---

## 11. Candidate 10: Procedural / runtime generation (PCG)

### 11.1 What the engine ships

- **PCG framework:** Beta (5.6) → "Production Ready" (5.7 [DOC]) → enabled by default (5.8 [SRC]). Docs `dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine` (snippet only).
- **Runtime generation:** `FPCGRuntimeGenScheduler` (snippet only: API). Cvars [SRC 5.8]: `pcg.FrameTime` (5.0 ms, "Allocated time in ms per frame"), `pcg.RuntimeGeneration.NumGeneratingComponents` (16), `pcg.RuntimeGeneration.Server.Mode` (2 = "all generation sources"), `pcg.RuntimeGeneration.TimeBetweenRuntimeGenSchedulerTicks`, `...EnablePooling`, `...ISM.ComponentlessPrimitives`, `...EnableWorldStreamingQueries`. Docs: generation modes and runtime-generation debugging (snippet only).
- **GPU processing:** GPU nodes, Custom HLSL node (snippet only: GPU docs). 5.7 adds more GPU nodes, and 5.8 says GPU runtime scatter is on par with GPU landscape grass [DOC]. **I could not confirm a separate maturity label for GPU processing.**
- **Determinism:** seed-based. Epic forum answer (2026): "minor differences based on hardware/platform" (snippet only).
- **Navigation interplay** [SRC 5.8]: the PCG plugin source has **no** references to `UNavigationSystem` / navigation build locks. `PCGProceduralISMComponent.cpp` sets `bNavigationRelevant = false` and `bCanEverAffectNavigation = false`, so componentless or procedural instances do not affect the navmesh. The engine offers `UNavigationSystemV1::AddNavigationBuildLock` / `RemoveNavigationBuildLock(..., ELockRemovalRebuildAction)` [SRC 5.8 `NavigationSystem.h`].

### 11.2 Status per version [SRC]

| Plugin | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| PCG | Beta | no flag ("Production Ready") | no flag, **EnabledByDefault** |
| PCGFastGeoInterop | absent | Experimental | Experimental |
| PCGGeometryScriptInterop | Beta | Beta | Beta |

### 11.3 Known limitations

- The runtime budget does not seem to include scheduler time. One team saw about 1 ms spikes against a 500 µs budget on PS5. Epic suggested capping concurrent generating components (snippet only: forum "Runtime Cost in PCG Hierarchical Generation").
- Runtime navmesh for PCG content needs `Dynamic` generation, which is expensive. The forum workaround is to lock nav builds during PCG and rebuild dirty areas afterwards, and users say the lock API is undocumented (snippet only: forum thread 2693069). *(Corrected 2026-11-21 during P08 Phase 2: an earlier version attributed a "20 frames back" quote to this workaround; that quote comes from forum thread 2704680, which is not about PCG.)* Older reports put Dynamic navmesh at 20–40 ms (snippet only).

### 11.4 Community

- "Runtime Navigation Area Modifiers" (Gumroad, Wouter Weynants): PCG-integrated nav *area* modifiers, not rebuild orchestration (snippet only).
- GPU-PCG (KisaragiTomi), UE 5.7 source build (snippet only).

### 11.5 Remaining gap

PCG itself is production-grade and already time-sliced, with pooling and GPU paths. The gap is at the **boundary between runtime PCG and gameplay systems, especially navigation and AI readiness**. PCG does not coordinate navmesh rebuilds. Procedural ISM instances are invisible to navigation. The known fix (build locks + dirty-area rebuild) is a forum workaround with no measured or packaged form. A project could orchestrate runtime PCG → nav-ready → AI-active with a frame budget. It would measure time-to-navigable per cell, worst-frame ms during generation, and AI path failures. It could also verify determinism by hashing the generated output across runs and machines. **Gap strength: medium–strong.**

---

## 12. Candidate 11: Modular gameplay architecture and iteration time

### 12.1 What the engine ships

- **Game Features plugin** (Beta 5.6–5.8 [SRC]): `UGameFeaturesSubsystem` (`LoadAndActivateGameFeaturePlugin`, `DeactivateGameFeaturePlugin`, `UnloadGameFeaturePlugin(bKeepRegistered)`), actions `GameFeatureAction_AddComponents`, `_AddCheats`, `_AddActorFactory`, `_AddWorldPartitionContent`, `_DataRegistry` and others [SRC 5.8 headers]. Leak checks: `GameFeaturePlugin.VerifyUnload`, `...LeakedAssetTrace.*`, some now moved to `PluginManager.LeakedAssetTrace.*` (snippet only: cvar reference). API `dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameFeatures` (snippet only).
- **Modular Gameplay plugin** (Beta [SRC]): `UGameFrameworkComponentManager`, `IGameFrameworkInitStateInterface` [SRC]. The Lyra sample uses these patterns (not opened this pass).
- **Content boundaries:** Asset Referencing Restrictions (no flag), Plugin Audit and Plugin Reference Viewer (Beta) [SRC].
- **UBT:** circular-dependency detection, `CircularlyReferencedDependentModules`, `bValidateCircularDependencies` (snippet only: Module Properties docs). UBT modes [SRC 5.8 `Programs/UnrealBuildTool/Modes`]: **`AnalyzeMode`** ("Outputs ... a module dependency graph (in .gefx format and list of module references)"), `IWYUMode`, `ProfileUnitySizesMode`, `AggregateParsedTimingInfo`, `JsonExportMode`, `QueryMode`.
- **UBA:** free since 5.4 (Beta then), with local-only executor settings `bAllowUBAExecutor` / `bAllowUBALocalExecutor` (snippet only: Build Configuration docs, Epic tutorial). UBA Cache docs were promised for 5.8 (snippet only). **Live Coding** docs (snippet only). Incremental Cooking is Beta in 5.7 and 5.8 [DOC].

### 12.2 Status per version [SRC]

GameFeatures Beta / Beta / Beta. ModularGameplay Beta / Beta / Beta. AssetReferenceRestrictions no flag ×3. PluginAudit Beta ×3. PluginReferenceViewer Beta ×3.

### 12.3 Community

- CircularDependenciesDetector (bstt, Blueprint assets, UE4) (snippet only). The Epic KB "Workaround for Circular Dependencies" (snippet only). No third-party **C++ module layering-rule checker** found (snippet only).

### 12.4 Remaining gap

Feature modularity (Game Features, component injection, init states) and content-reference rules (Asset Referencing Restrictions) are built in. UBT can export the module graph and detects cycles. What I did not find is a **C++ module/plugin layering-rule checker for CI**, for example "features may depend on core, never on each other; no Editor module in runtime dependency closure", built on UBT's Analyze/JsonExport output. Also missing is **measured iteration cost** tied to architecture: incremental build seconds after touching a core header vs a feature header, Live Coding success rate, and GFP activate/deactivate leak counts. **Gap strength: medium.** Risks: GameFeatures and ModularGameplay are Beta, and the UE6 direction for the gameplay framework is unverified (§1.1).

---

## 13. Summary table

| # | Idea | Existing solutions (main) | Status (5.6 → 5.7 → 5.8) | Remaining gap | Gap strength |
|---|---|---|---|---|---|
| 1 | Frame-budgeted gameplay scheduler | `TickInterval`, tick batching (`tick.AllowBatchedTicks`, off by default), Significance Manager, **Animation Budget Allocator (ms budget, skeletal meshes only)**, URO, Mass `FExecutionLimiter`; Fab: Tick Optimization Toolkit, Optimization Tools, EnhancedTick; Rare aggregation talk | SigMgr/ABA no flag ×3; batching default 0 ×3; `FExecutionLimiter` 5.7+ | General closed-loop ms budget across heterogeneous gameplay work with fairness and staleness guarantees; generalise the ABA model and benchmark against batching and distance throttling | **Medium** |
| 2 | Mass agents + representation-switch consistency | MassGameplay representation LOD, translators/sync traits (transform, movement, orientation, capsule only), Instanced Actors, MassInsights, CitySample | MassGameplay/AI/Crowd/InstancedActors Exp ×3; MassEntity plugin shell removed in 5.8; Mass replication experimental, no Iris-for-Mass in 5.8 | Generic state hand-off contract and invariant checking for non-spatial gameplay state across Actor↔ISM↔None, with measured divergence and transition cost | **Strong** (high API-churn risk) |
| 3 | Network bandwidth / server CPU | Push model, RepGraph (Beta, not deprecated), **Iris with filters and prioritizers**, Networking Insights, NPP (Beta), Mover (Exp); BorMor 100-player Iris benchmark | Iris Exp → Beta → Beta flag (Epic says "production-ready for licensees" in 5.8); `UseIrisReplication` default 0 in 5.7/5.8 | Controlled multi-configuration comparison (legacy/push/RepGraph/Iris/Iris+custom prioritizer) on one workload, with bandwidth and server ms; gameplay-aware prioritizer | **Medium** |
| 4 | World Partition streaming + state | **Level Streaming Persistence (5.8: properties + destruction + runtime respawn)**, **SPUD (WP support)**, Instanced Actors, FastGeo, 5.8 WP Insights, streaming time limits | LSP Exp ×3 (big feature jump in 5.8); FastGeo Exp ×3; WP Insights Exp (5.8) | Persistence mostly solved. Remaining: cost-based (not count-based) AddToWorld/registration budgeting, validated with WP Insights | **Weak** (persistence) / **Medium** (hitch budgeting) |
| 5 | Animation–gameplay sync | Branching Point vs Queued, `bTriggerOnDedicatedServer`, `a.EnableQueuedAnimEventsOnServer`, `RepRootMotion`, GAS montage rep, Mover montage rep (5.7), Motion Warping, Pose Search, Chooser | PoseSearch/Chooser no flag; MotionWarping Beta; Mover/UAF Exp; 5.6.1 server notify bug UE-308913 | Gameplay-clock event track independent of URO/ABA/visibility/net role; measured timing error and missed/duplicate events | **Strong** |
| 6 | Automated testing | Automation/Spec, AFunctionalTest, CQTest, LLT, Gauntlet (Beta), **APT plugin (Exp)**, CSV+PerfReportTool, **Horde perf trends (5.8 Exp)** | CQTest no flag ×3; APT Exp ×3 | Mutation-based measurement of suite effectiveness (nothing found for UE); local, statistical game-thread perf gating without Horde | **Strong** (effectiveness) / **Medium** (perf gating) |
| 7 | Memory / asset loading budgets | Asset Manager, bundles, StreamableManager (5.8 JIT trickle), Size Map, Asset Audit, **Memory Insights tag budgets (5.6+)**, LLM; **YAGER DependencyAnalyser (size budgets as automation tests)**, CommonValidators | AssetManagerEditor no flag ×3 | Only the correlation of editor estimates with measured cooked runtime memory per bundle, plus per-commit delta gating | **Weak** |
| 8 | Editor tools / content validation | Data Validation (validators, changelist, commandlet, fixers), Asset Referencing Restrictions, Linter v2, CommonValidators, naming plugins | DataValidation no flag, on by default ×3 | Semantic cross-asset gameplay-data rules with measured detection rate and validation time | **Weak** (framework) / **Medium** (semantic rules) |
| 9 | AI at scale | AI Perception (sight already time-sliced), EQS, StateTree, Smart Objects, MassAI; **TCAT GPU influence maps**, Wise Feline Influence Maps | StateTree/SmartObjects no flag ×3; MassAI Exp ×3 | Perception/decision LOD with measured detection latency vs CPU; overlaps Candidate 1 | **Weak–Medium** |
| 10 | PCG runtime generation | PCG (Production Ready 5.7), runtime scheduler, `pcg.FrameTime`, GPU nodes, FastGeo interop, seeds; nav build locks exist but PCG does not use them | PCG Beta → no flag → no flag + on by default | Runtime PCG → nav-ready → AI-active orchestration with budget and measurements; determinism verification harness | **Medium–Strong** |
| 11 | Modular gameplay architecture / iteration | Game Features, Modular Gameplay, Lyra patterns, Asset Referencing Restrictions, Plugin Audit, UBT Analyze/IWYU/cycle detection, UBA, Live Coding | GameFeatures/ModularGameplay Beta ×3 | C++ module layering-rule checker in CI plus measured iteration cost tied to architecture | **Medium** |

### 13.1 Surprises worth acting on

1. **Candidate 4 is mostly already solved for persistence.** UE 5.8's Level Streaming Persistence (Experimental) persists properties, map-actor destruction and runtime-spawned actors, with a save-game byte blob. SPUD (MIT) supports World Partition. Neither is in the 5.6/5.7 headers for LSP's lifecycle features. Re-scope this idea toward streaming-cost budgeting or drop it.
2. **Iris "production-ready" in 5.8 is a release-notes label only.** On disk, the plugin is still Beta, the runtime switch still defaults to off, and the docs page still says Experimental. Any Iris project must state "opt-in Iris on 5.8.3".
3. **Memory budget enforcement already exists twice.** YAGER's DependencyAnalyser runs dependency-size budgets as automation tests, and Memory Insights has had per-tag budget views since 5.6. Candidate 7 is weak.
4. **The Animation Budget Allocator is already a closed-loop ms-budget scheduler** (skeletal meshes only, `a.Budget.BudgetMs`). Candidate 1 must present itself as generalising it, not inventing the concept.
5. **Epic now ships an experimental perf-test harness** (APT, 5.6+) and Horde perf trends (5.8). Candidate 6's perf-gating half is only a medium gap. The mutation-testing half has no prior UE work that I found.
6. **Influence maps already exist as Fab/GitHub plugins, including a GPU one (TCAT).** A tactical-map project would be weak.
7. **The user's own engine version (5.6.1) has an open server-side notify bug (UE-308913).** That is concrete, reproducible evidence for Candidate 5.
8. **PCG has no navigation coordination in source** (5.8), and procedural ISM instances never affect navigation. Candidate 10's gap is backed by source.
9. **Docs now default to 5.9** (preview released Nov 9 2026). Always cite with `?application_version=5.x`.

---

## 14. Limitations of this analysis

- **Plugin flags are not Epic's maturity labels.** "No flag" does not equal "Production-Ready", and Iris shows the two can disagree. I reported both where I had both.
- **5.8 release notes were partly unreadable** (truncated fetch). Some 5.8 details for Mass, Networking and World Building come from secondary sources (Tom Looman, forum hotfix notes).
- **Fab coverage is incomplete.** Fab search is not exhaustive, several listings were seen only as search snippets, and some details appeared only on piracy mirror sites. Those are marked unverified and must not be cited in the portfolio. I did not survey Fab for Mass crowd plugins, save systems such as Easy Multi Save, or networking plugins.
- **No studio-internal knowledge.** Studios may have solved these gaps internally. "Gap" here means **not shipped in UE and not found publicly**, never "nobody has done it".
- **Bug status is unverified:** whether UE-308913 is fixed in 5.7/5.8; whether `tick.AllowBatchedTicks` is enabled by any platform config (I checked only `BaseEngine.ini` and `ConsoleVariables.ini`).
- **The source reading is static.** Statements such as "notifies fire late under URO" and "may fire early in LookAheadMode" come from code reading and must be **measured** in Phase 1/2 of the chosen project before any portfolio claim.
- **UE6 statements are secondhand** and are not used in any gap rating.

---

## 15. Sources

### 15.1 Local engine evidence [SRC] (UE 5.6.1 / 5.7.4 / 5.8.3, `C:\Program Files\Epic Games\UE_5.x\Engine\`)

- `Build/Build.version` (versions, changelists)
- `Plugins/**.uplugin`: maturity flags (Appendix A)
- `Source/Runtime/Engine/Private/TickTaskManager.cpp`: `tick.*` cvars and defaults
- `Source/Runtime/Engine/Public/TaskSyncManager.h`: `RegisterTickGroupWorkHandle`
- `Source/Runtime/Net/Iris/Private/Iris/IrisConfig.cpp` (5.7, 5.8): `net.Iris.UseIrisReplication` default
- `Source/Runtime/Net/Iris/**`: filter and prioritizer classes (5.8)
- `Source/Runtime/Net/Core/Public/Net/Core/PushModel/PushModel.h`, `Source/Runtime/Engine/Public/Net/UnrealNetwork.h`
- `Plugins/Runtime/LevelStreamingPersistence/Source/LevelStreamingPersistence/{Public,Private}/*` (5.6 vs 5.8 diff)
- `Plugins/Runtime/MassGameplay/Source/**`: translators, sync traits
- `Source/Runtime/MassEntity/Public/MassEntityTypes.h`, `MassEntityQuery.h`: `FExecutionLimiter`
- `Source/Runtime/Mass/*` (5.8 module list)
- `Source/Runtime/Engine/Classes/Engine/EngineTypes.h`, `Private/Components/SkinnedMeshComponent.cpp`, `Private/Components/SkeletalMeshComponent.cpp`: URO time adjustment
- `Source/Runtime/Engine/Public/Animation/AnimTypes.h`: notify event fields
- `Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`: `a.EnableQueuedAnimEventsOnServer`
- `Source/Runtime/Engine/Classes/GameFramework/Character.h`: `FRepRootMotionMontage`
- `Source/Runtime/AIModule/Classes/Perception/AISense_Sight.h`, `Private/Perception/AISense_Sight.cpp`
- `Plugins/AnimationBudgetAllocator` source: `a.Budget.*` cvars
- `Plugins/PCG/Source/**`: `pcg.*` cvars, navigation flags; `Source/Runtime/NavigationSystem/Public/NavigationSystem.h`
- `Plugins/Performance/AutomatedPerfTesting/Source/*/Public` (5.6, 5.8)
- `Plugins/Editor/DataValidation/Source/*/Public`, `Plugins/Editor/AssetReferenceRestrictions/Source/**`
- `Source/Developer/TraceInsights/Private/Insights/MemoryProfiler/ViewModels/MemTagBudget*` (5.6, 5.8)
- `Source/Runtime/Core/Public/Misc/AutomationTest.h`; `Source/Developer/{FunctionalTesting, LowLevelTestsRunner}`
- `Source/Programs/UnrealBuildTool/Modes/*` (5.8)
- `Plugins/Runtime/GameFeatures/Source/GameFeatures/Public/*`, `Plugins/Runtime/ModularGameplay/Source/**`
- `Source/Runtime/Engine/Classes/Components/WorldPartitionStreamingSourceComponent.h`

### 15.2 Epic official, opened [DOC]

- UE 5.8 Release Notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8
- UE 5.7 Release Notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes?application_version=5.7
- Iris Replication System: https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-replication-system-in-unreal-engine
- Overview of Mass Gameplay: https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine

### 15.3 Epic forums, opened [FORUM]

- UE 5.8 Released (Jun 17 2026, hotfix notes): https://forums.unrealengine.com/t/unreal-engine-5-8-released/2729274
- UE 5.7 Released (Nov 12 2025): https://forums.unrealengine.com/t/unreal-engine-5-7-released/2673913
- Batch Tick Support (Ben Zeigler, Jun 2025): https://forums.unrealengine.com/t/batch-tick-support/2616595
- Mass(Entity) RoadMap/Vision (James Keeling, Apr 2025): https://forums.unrealengine.com/t/mass-entity-roadmap-vision-and-more-questions/2527030
- MassAI Replication State in 5.7 and future roadmap: https://forums.unrealengine.com/t/massai-replication-state-in-5-7-and-future-roadmap/2684012
- 5.7 Iris Strategy (Alex Koumandarakis, Jan 13 2026): https://forums.unrealengine.com/t/5-7-iris-strategy/2699685
- World Partition streaming performance: https://forums.unrealengine.com/t/world-partition-streaming-performance/2577446
- Automated Performance Testing Plugin (Matt Oztalay, Nov 2025): https://forums.unrealengine.com/t/automated-performance-testing-plugin/2690779
- UE 5.6.1 NotifyBegin/NotifyEnd on server (UE-308913): https://forums.unrealengine.com/t/ue-5-6-1-why-is-notifybegin-notifyend-constantly-being-called-in-my-anim-notify-state-on-the-server/2651617
- URO and AnimNotifies (UE 4.27): https://forums.unrealengine.com/t/uro-and-animnotifies/608584

### 15.4 Third-party, opened [WEB]

- Tom Looman, UE 5.7 Performance Highlights: https://tomlooman.com/unreal-engine-5-7-performance-highlights/
- Tom Looman, UE 5.8 Performance Highlights: https://tomlooman.com/unreal-engine-5-8-performance-highlights/
- GameFromScratch, Unreal Engine 5.8 Released: https://gamefromscratch.com/unreal-engine-5-8-released/
- GameFromScratch, The Future of Unreal Engine 6: https://gamefromscratch.com/the-future-of-unreal-engine-6/
- AcademyClass, UE 5.8 features explained: https://academyclass.com/blog/unreal-engine-5-8-new-features-explained/
- BorMor, Iris: 100 Players in One Place: https://bormor.dev/posts/iris-one-hundred-players/
- Unreal Directive, Mover plugin page: https://unrealdirective.com/resources/engine-plugins/mover/
- voithos.io, Fancier Ticking in Unreal: https://voithos.io/articles/fancier-ticking-in-unreal/
- Fab, Optimization Tools plugin: https://www.fab.com/listings/2647dad4-4cf4-4dfa-bd85-cfde0a21bbeb
- GitHub, Thyke/EnhancedTick: https://github.com/Thyke/EnhancedTick
- GitHub, nurbed/CommonValidators: https://github.com/nurbed/CommonValidators
- GitHub, alessianigretti/DependencyAnalyser (YAGER): https://github.com/alessianigretti/DependencyAnalyser

### 15.5 Seen in search results only (snippet only)

**Epic docs / pages**
- Actor Ticking: https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine?lang=en-US
- FTickFunction API: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FTickFunction?lang=en-US
- Significance Manager: https://dev.epicgames.com/documentation/en-us/unreal-engine/significance-manager-in-unreal-engine · API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/SignificanceManager/USignificanceManager
- Animation Budget Allocator: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine · API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/AnimationBudgetAllocator/USkeletalMeshComponentBudgeted
- Mass Entity: https://dev.epicgames.com/documentation/en-us/unreal-engine/mass-entity-in-unreal-engine · EMassRepresentationType: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MassRepresentation/EMassRepresentationType?lang=en-US · FMassRepresentationParameters: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/MassRepresentation/FMassRepresentationParameters
- AInstancedActorsManager: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/InstancedActors/AInstancedActorsManager?lang=en-US
- World Building Guide (KB): https://dev.epicgames.com/community/learning/knowledge-base/r6wl/unreal-engine-world-building-guide
- Introduction to Iris: https://dev.epicgames.com/documentation/unreal-engine/introduction-to-iris-in-unreal-engine?lang=en-US · Components of Iris: https://dev.epicgames.com/documentation/en-us/unreal-engine/components-of-iris-in-unreal-engine · Migrate to Iris: https://dev.epicgames.com/documentation/unreal-engine/migrate-to-iris-in-unreal-engine
- Networking Overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine · Replicating UObjects: https://dev.epicgames.com/documentation/en-us/unreal-engine/replicating-uobjects-in-unreal-engine · Networking Insights: https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-insights-in-unreal-engine · Console Commands for Network Debugging: https://dev.epicgames.com/documentation/unreal-engine/console-commands-for-network-debugging-in-unreal-engine?lang=en-US
- Mover: https://dev.epicgames.com/documentation/en-us/unreal-engine/mover-in-unreal-engine · GASP 5.7 blog: https://www.unrealengine.com/tech-blog/explore-the-updates-to-the-game-animation-sample-project-in-ue-5-7
- World Partition: https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine · WP HLOD: https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine
- ULevelStreamingPersistenceManager API: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/LevelStreamingPersistence/ULevelStreamingPersistenceManage-?lang=en-US · Level Streaming Persistence roadmap card: https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/1098-level-streaming-persistence-experimental-
- WorldStreamingInsights API: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/WorldStreamingInsights?lang=en-US · Unreal Insights Reference: https://dev.epicgames.com/documentation/unreal-engine/unreal-insights-reference-in-unreal-engine-5?lang=en-US
- Animation Notifications (4.27): https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifications-notifies?application_version=4.27 · Animation Notifies: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine
- CQTest: https://dev.epicgames.com/documentation/en-us/unreal-engine/cqtest-test-framework-for-unreal-engine · Automation Test Framework: https://dev.epicgames.com/documentation/unreal-engine/automation-test-framework-in-unreal-engine?lang=en-US · Gauntlet: https://dev.epicgames.com/documentation/en-us/unreal-engine/gauntlet-automation-framework-in-unreal-engine · Gauntlet Controller: https://dev.epicgames.com/documentation/unreal-engine/gauntlet-controller-in-unreal-engine?lang=en-US · APT settings API: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/AutomatedPerfTesting/UAutomatedPerfTestProjectSetting-?lang=en-US
- Memory Insights: https://dev.epicgames.com/documentation/en-us/unreal-engine/memory-insights-in-unreal-engine · Asset Management: https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine · Reference Viewer: https://dev.epicgames.com/documentation/en-us/unreal-engine/reference-viewer-in-unreal-engine · Insights Asset Memory Profiling roadmap card: https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/2076-insights-asset-memory-profiling-experimental-
- DataValidation API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/DataValidation · UEditorValidatorBase: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/DataValidation/UEditorValidatorBase?lang=en-US · UEditorValidatorSubsystem: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/DataValidation/UEditorValidatorSubsystem?lang=en-US
- StateTree: https://dev.epicgames.com/documentation/unreal-engine/state-tree-in-unreal-engine · Smart Objects overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine---overview · UAISense_Sight (5.0 API): https://docs.unrealengine.com/5.0/en-US/API/Runtime/AIModule/Perception/UAISense_Sight/
- PCG framework: https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine · PCG GPU: https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine · PCG runtime-gen debugging: https://dev.epicgames.com/documentation/unreal-engine/pcg-runtime-generation-debugging-in-unreal-engine?lang=en-US · PCG generation modes: https://dev.epicgames.com/documentation/unreal-engine/using-pcg-generation-modes-in-unreal-engine?lang=en-US · FPCGRuntimeGenScheduler: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PCG/FPCGRuntimeGenScheduler?lang=en-US
- GameFeatures API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameFeatures · UnloadGameFeaturePlugin: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/GameFeatures/UGameFeaturesSubsystem/UnloadGameFeaturePlugin?lang=en-US · Console Variables Reference: https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-console-variables-reference · Module Properties: https://dev.epicgames.com/documentation/en-us/unreal-engine/module-properties-in-unreal-engine · Build Configuration: https://dev.epicgames.com/documentation/unreal-engine/build-configuration-for-unreal-engine · Horde UBA tutorial: https://dev.epicgames.com/documentation/en-us/unreal-engine/horde-unreal-build-accelerator-and-remote-compilation-tutorial-for-unreal-engine · Live Coding: https://dev.epicgames.com/documentation/unreal-engine/using-live-coding-to-recompile-unreal-engine-applications-at-runtime · Circular dependencies KB: https://dev.epicgames.com/community/learning/knowledge-base/kj4B/unreal-engine-workaround-for-circular-dependencies
- Epic public roadmap (5.8 tab): https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/tabs/94-forward-looking
- Rare, Aggregating Ticks to Manage Scale in Sea of Thieves: https://www.unrealengine.com/en-US/events/unreal-fest-europe-2019/aggregating-ticks-to-manage-scale-in-sea-of-thieves · video: https://www.youtube.com/watch?v=CBP5bpwkO54

**Epic forums (snippet only)**
- APT guidance: https://forums.unrealengine.com/t/automated-performance-testing-plugin-guidance/2666136 · APT talk thread: https://forums.unrealengine.com/t/talks-and-demos-a-tech-artists-guide-to-automated-performance-testing-unreal-fest-bali-2025/2575028 · APT + Horde: https://forums.unrealengine.com/t/automated-performance-testing-framework-horde/2709068
- ABA interpolation (5.5): https://forums.unrealengine.com/t/animationbudgetallocator-doesnt-utilize-interpolation-on-registered-components-and-it-feels-like-framerate-drop-what-is-a-proper-way-to-address-it/2663219
- Queued AnimNotifies skipped on server: https://forums.unrealengine.com/t/queued-animnotifies-skipped-on-server/420300 · Queued notify state twice: https://forums.unrealengine.com/t/queued-anim-notify-state-triggers-twice-bug-or-am-i-misunderstanding-it/142233 · Notify frame-rate dependence: https://forums.unrealengine.com/t/anim-notify-frame-rate-independent-tick/740638
- Mover 2.0 and GAS (5.4 preview): https://forums.unrealengine.com/t/ue-5-4-preview-1-mover-2-0-and-gas/1780825 · NPP + CMC status: https://forums.unrealengine.com/t/status-of-networkprediction-plugin-working-in-tandem-with-charactermovementcomponent-troubles-synchronizing-walk-speed-stamina-across-client-server/2640104 · 5.7.1 hotfix: https://forums.unrealengine.com/t/5-7-1-hotfix-released/2681235
- WP persistence threads: https://forums.unrealengine.com/t/persistent-actors-with-world-partition/535746 · https://forums.unrealengine.com/t/how-to-sustain-actor-state-in-world-partition-on-multiplayer-game/2711156 · https://forums.unrealengine.com/t/world-partition-how-to-permanently-destroy-an-actor-at-runtime/2020977 · WP navmesh + data layers: https://forums.unrealengine.com/t/world-parition-nav-mesh-with-dynamic-build-and-data-layers/2686688
- PCG runtime cost: https://forums.unrealengine.com/t/runtime-cost-in-pcg-hierarchical-generation/2708776 · PCG + navmesh: https://forums.unrealengine.com/t/dynamicmodifieronly-and-runtime-navmesh-building-pcg/2693069 · Nav build lock: https://forums.unrealengine.com/t/navmesh-runtime-regeneration-addnavigationbuildlock/2704680 · PCG cross-partition determinism: https://forums.unrealengine.com/t/cross-partition-coherent-generation-in-pcg-with-hierarchical-runtime-generation/2728364
- AI perception scaling: https://forums.unrealengine.com/t/ai-perception-detecting-the-players-slows-down-significantly-with-the-number-of-ais-in-the-level/88176 · CQTest vs Spec: https://forums.unrealengine.com/t/cqtest-or-spec-test/2687739 · UBA + Horde 5.6: https://forums.unrealengine.com/t/uba-no-longer-working-with-horde-since-5-6-preview/2593010 · Push model: https://forums.unrealengine.com/t/push-model-networking/510684

**Community / Fab / research (snippet only)**
- Tick Optimization Toolkit: https://www.fab.com/listings/de8d1c17-cb95-4f41-9a15-6d3e57ea214e · https://loonyware.com/assets/tick-optimization-toolkit/ · Mono Ticking: https://www.unrealengine.com/marketplace/en-US/product/mono-ticking-plugin · TickCheck: https://www.fab.com/listings/afdc37bd-12a8-4ad9-bfb4-0f1f12ee0ced · Tom Looman tick-batching tweet: https://x.com/t_looman/status/1877018904435859602
- Coconut Lizard ABA: https://www.coconutlizard.co.uk/blog/animation-budget-allocator/
- MassSample: https://github.com/Megafunk/MassSample · MassCommunitySample: https://github.com/getnamo/MassCommunitySample · x157 Mass: https://x157.github.io/UE5/Mass/ · MassGameplay analysis: https://github.com/hititou/MassGameplay_Analysis · vrealmatic Mass visualization: https://vrealmatic.com/unreal-engine/mass/visualization
- StraySpark Iris opt-in: https://www.strayspark.studio/blog/iris-replication-unreal-engine-opt-in-2026 · vorixo Iris filters: https://vorixo.github.io/devtricks/iris-replication-filter/ · Kieran Newland push model: https://www.kierannewland.co.uk/push-model-networking-unreal-engine/ · vorixo GAS replication proxy: https://vorixo.github.io/devtricks/gas-replication-proxy/
- SPUD: https://github.com/sinbad/SPUD · SPUD WP PR: https://github.com/sinbad/SPUD/pull/45 · SPUD FAQ: https://github.com/sinbad/SPUD/blob/master/doc/faq.md · FlickerVoid issue #25: https://github.com/tec-sx/FlickerVoid/issues/25 · Nance Dev Diary: https://www.patreon.com/NanceDevDiaries/posts/how-i-made-my-125344091
- bugnet.io notify guides: https://bugnet.io/blog/fix-unreal-anim-notify-not-firing-at-playback-rate · quodsoler notify reference: https://github.com/quodsoler/unreal-engine-skills/blob/main/skills/ue-animation-system/references/anim-notify-reference.md
- Andrew Fray, Topography of Unreal Test Automation 2025: https://andrewfray.wordpress.com/2025/04/09/the-topography-of-unreal-test-automation-in-2025/ · Unreal Directive CQTest: https://unrealdirective.com/resources/engine-plugins/cqtest/ · zomgmoz CQTest: https://zomgmoz.tv/unreal/Testing/CQTest · arXiv coverage in C++ engines: https://arxiv.org/html/2601.16881v1 · arXiv Clang mutation testing: https://arxiv.org/pdf/2210.17215 · pgaleone coverage: https://pgaleone.eu/2022/02/23/code-coverage-with-unreal-engine/ · UE4-TDD-CI_Testing: https://github.com/Floating-Island/UE4-TDD-CI_Testing · Daedalic ue4-test-automation: https://github.com/DaedalicEntertainment/ue4-test-automation · Hazelight Angelscript tests: https://angelscript.hazelight.se/scripting/script-tests/ · Rare automated testing talk: https://www.youtube.com/watch?v=KmaGxprTUfI · APT talk video: https://www.youtube.com/watch?v=aqNL8tdfIHI
- HardReferenceFinder: https://github.com/heyomidk/HardReferenceFinder · Unused Asset Detective: https://www.fab.com/listings/dccf923b-a2a5-4e8e-8fc5-b396baad02b0 · Fab Package Builder: https://www.fab.com/listings/3e90f27b-6da9-4be8-b10c-b0a09e8ca02f
- Linter v2: https://www.fab.com/listings/10850cb2-be1d-43d2-971d-922c73d218f3 · Asset Naming Assistant: https://www.fab.com/listings/4ac15624-5cd9-4574-bccc-cc7db2e7e550 · Smart Naming Cleanup: https://www.fab.com/listings/ebc32a83-399b-454e-ad8b-26cce8b1db5c · Unreal Directive Data Validation: https://unrealdirective.com/articles/data-validation-what-you-need-to-know/
- TCAT: https://github.com/Over2K-GTL/TacticalCrowdAIToolkit · https://www.fab.com/listings/d81cbf03-5e90-4920-ad24-1921e314399a · Wise Feline Influence Maps: https://www.nooparmygames.com/wise-feline-influence-maps-for-unreal/ · Wise Feline Utility AI: https://www.fab.com/listings/94f4a41c-6235-4fd7-ab47-321b49562994
- Runtime Navigation Area Modifiers: https://wouterweynants.gumroad.com/l/navigation · GPU-PCG: https://github.com/KisaragiTomi/GPU-PCG
- CircularDependenciesDetector: https://github.com/bstt/CircularDependenciesDetector · Unreal Directive Game Features: https://unrealdirective.com/articles/modular-game-features-what-you-need-to-know/ · Universal Foundry UBA local: https://www.universalfoundry.io/docs/documents/enable-uba-on-local-workstations-56/
- UE 5.9 preview reports: https://www.biunivoca.com/en/blog/unreal-engine-5-9-preview-coming-next-week · https://digg.com/ai/1qdr8rig · https://arcadianpixels.com/dev-log/unreal-engine-5-9-is-coming-web-returns-as-a-target
- UE 5.7 news: https://www.awn.com/news/unreal-engine-57-now-available · https://gamefromscratch.com/unreal-engine-5-7-released/

---

## Appendix A: Plugin maturity flags read from the installed engines

Source: `.uplugin` files under `C:\Program Files\Epic Games\UE_5.x\Engine\Plugins` [SRC]. "—" = no Beta/Experimental flag. "on" = `EnabledByDefault: true`.

| Plugin | Path (5.8) | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|---|
| SignificanceManager | Runtime/SignificanceManager | — | — | — |
| AnimationBudgetAllocator | Runtime/AnimationBudgetAllocator | — | — | — |
| MassEntity (plugin shell) | Runtime/MassEntity | Beta | Beta | *absent* |
| MassGameplay | Runtime/MassGameplay | Exp | Exp | Exp |
| MassAI | AI/MassAI | Exp | Exp | Exp |
| MassCrowd | AI/MassCrowd | Exp | Exp | Exp |
| ZoneGraph | Runtime/ZoneGraph | Exp | Exp | Exp |
| MassInsights | MassInsights | Beta, on | Beta, on | Beta, on |
| InstancedActors | Runtime/InstancedActors | Exp | Exp | Exp |
| MoverIntegrations | Experimental/MoverIntegrations | Exp | Exp | Exp |
| MetaHumanCrowd | MetaHuman/MetaHumanCrowd | *absent* | *absent* | Exp |
| Iris | Experimental/Iris | Exp | **Beta** | **Beta** |
| ReplicationGraph | Runtime/ReplicationGraph | Beta | Beta | Beta |
| NetworkPrediction | Runtime/NetworkPrediction | Beta | Beta | Beta |
| NetworkPredictionInsights | Runtime/NetworkPredictionInsights | — | — | — |
| Mover | Experimental/Mover | Exp | Exp | Exp |
| ChaosMover | Experimental/ChaosMover | Exp | Exp | Exp |
| LevelStreamingPersistence | Runtime/LevelStreamingPersistence | Exp | Exp | Exp |
| FastGeoStreaming | Experimental/FastGeoStreaming | Exp | Exp | Exp |
| WorldStreamingInsights | WorldStreamingInsights | *absent* | *absent* | Exp |
| WorldPartitionHLODUtilities | Editor/WorldPartitionHLODUtilities | —, on | —, on | —, on |
| PoseSearch | Animation/PoseSearch | — | — | — |
| Chooser | Chooser | — | — | — |
| MotionWarping | Animation/MotionWarping | Beta | Beta | Beta |
| AnimationWarping | Animation/AnimationWarping | — | — | — |
| AnimNext → UAF | Experimental/AnimNext (5.6), Experimental/UAF/UAF (5.7+) | Exp | Exp | Exp |
| GameplayAbilities | Runtime/GameplayAbilities | — | — | — |
| StateTree / GameplayStateTree | Runtime/StateTree, Runtime/GameplayStateTree | — | — | — |
| SmartObjects | Runtime/SmartObjects | — | — | — |
| WorldConditions | Runtime/WorldConditions | Exp | Exp | Exp |
| GameplayInteractions | Runtime/GameplayInteractions | Exp | Exp | Exp |
| EnvironmentQueryEditor | AI/EnvironmentQueryEditor | —, on | —, on | —, on |
| HTNPlanner | AI/HTNPlanner | Beta | Beta | Beta |
| LearningAgents | Experimental/LearningAgents | Exp | Exp | Exp |
| PCG | PCG | Beta | — | —, **on** |
| PCGFastGeoInterop | Experimental/PCGInterops/PCGFastGeoInterop | *absent* | Exp | Exp |
| PCGGeometryScriptInterop | PCGInterops/PCGGeometryScriptInterop | Beta | Beta | Beta |
| CQTest | Tests/CQTest | — | — | — |
| FunctionalTestingEditor | Tests/FunctionalTestingEditor | — | — | — |
| RuntimeTests | Tests/RuntimeTests | — | — | — |
| AutomationUtils | Experimental/AutomationUtils | Beta, on | Beta, on | Beta, on |
| Gauntlet | Experimental/Gauntlet | Beta | Beta | Beta |
| AutomatedPerfTesting | Performance/AutomatedPerfTesting | Exp | Exp | Exp |
| PerformanceMonitor | Performance/PerformanceMonitor | — | — | — |
| TraceUtilities | TraceUtilities | —, on | —, on | —, on |
| GameplayInsights | Animation/GameplayInsights | — | — | — |
| ChaosVD | ChaosVD | Beta, on | Beta, on | Beta, on |
| DataValidation | Editor/DataValidation | —, on | —, on | —, on |
| AssetManagerEditor | Editor/AssetManagerEditor | —, on | —, on | —, on |
| AssetReferenceRestrictions | Editor/AssetReferenceRestrictions | — | — | — |
| EditorScriptingUtilities | Editor/EditorScriptingUtilities | Beta | Beta | Beta |
| PluginAudit | Experimental/PluginAudit | Beta | Beta | Beta |
| PluginReferenceViewer | Experimental/PluginReferenceViewer | Beta | Beta | Beta |
| GameFeatures | Runtime/GameFeatures | Beta | Beta | Beta |
| ModularGameplay | Runtime/ModularGameplay | Beta | Beta | Beta |

## Appendix B: Engine cvar defaults read from source [SRC]

| Cvar | 5.6.1 | 5.7.4 | 5.8.3 | File |
|---|---|---|---|---|
| `tick.AllowBatchedTicks` | 0 | 0 | 0 | `Runtime/Engine/Private/TickTaskManager.cpp` |
| `tick.AllowBatchedTicksUnordered` | absent | 0 | 0 | same |
| `tick.AllowConcurrentTickQueue` / `tick.AllowAsyncTickDispatch` | 0 / 0 | 0 / 0 | 0 / 0 | same |
| `net.Iris.UseIrisReplication` | not checked | 0 | 0 | `Runtime/Net/Iris/Private/Iris/IrisConfig.cpp` |
| `a.EnableQueuedAnimEventsOnServer` | 1 | not checked | 1 | `Runtime/Engine/Private/Components/CharacterMovementComponent.cpp` |
| `pcg.FrameTime` | not checked | not checked | 5.0 ms | `Plugins/PCG/Source/PCG/Private/Graph/PCGGraphExecutor.cpp` |
| `pcg.RuntimeGeneration.NumGeneratingComponents` | not checked | not checked | 16 | `.../RuntimeGen/PCGRuntimeGenScheduler.cpp` |
| `pcg.RuntimeGeneration.Server.Mode` | not checked | not checked | 2 | `.../RuntimeGen/PCGGenSourceManager.cpp` |
| `UAISense_Sight` MaxTracesPerTick / MaxAsyncTracesPerTick / MaxTimeSlicePerTick | not checked | not checked | 6 / 10 / 0.005 s | `Runtime/AIModule/Private/Perception/AISense_Sight.cpp` |

---

## Addendum A: Time-sliced spawning and actor pooling (added during roadmap synthesis, 2026-11-19)

The original brief did not cover this idea. It was added because `02_technical_challenges.md` ranks "game-thread hitches when content enters the world" as the #1 recurring problem. In [T1-D], Epic staff also said a native time-sliced spawn API and built-in pooling don't exist.

**Engine [SRC 5.8.3]** (details in `04_local_engine_verification.md` §4b):
- There is no `SpawnActorAsync` or any time-sliced spawn API.
- The only pooling interface is Mass-specific: `IMassActorPoolableInterface`.
- The building blocks exist: `SpawnActorDeferred` / `FinishSpawning`, the public `AActor::IncrementalRegisterComponents`, and Experimental async physics-state creation (opt-in per component, with its own `p.Chaos.AsyncPhysicsStateTask.TimeBudgetMS`).

**Community / Fab:**

| Solution | What it states | What its public page does *not* state | Evidence |
|---|---|---|---|
| **SpawnBudget** (SimulatedFlow / Silvan Teufel; source-available, paid) | "A world spawner that cannot spike the frame": at most N spawns per frame and never more than a set number of ms, queued by importance. Three distance rings with hysteresis; per-class pooling | How the ms cap is enforced (checked between spawns, or cost predicted beforehand); phase splitting of one spawn; pooled-state reset verification; benchmarks; engine versions. Its docs wiki could not be read | [WEB] https://github.com/SimulatedFlow/ue-plugin-SpawnBudget (README opened; docs not accessible) |
| **PoolManager** (JanSeliv, UE 5.2–5.7) | Since 2023-11-25, "Take From Pool" spreads creation of large pools over several frames. Callback interface for take/return | Whether the spreading is count- or time-based; reset verification; benchmarks | [WEB] https://github.com/JanSeliv/PoolManager (README/changelog opened) |
| Flex Pool, UltraPool, Actor Pool Manager, Object Pool Creater (Fab) | Pre-loading pools in the background, growth policies, lease/TTL, interface-based reuse that skips constructors and BeginPlay | Time budgets per frame (not advertised) | (snippet only) Fab listings from search |
| Optimization Tools (Fab) | Includes actor pools and "Pool Ticking" that spreads ticks, not spawns | — | (snippet only) |
| bennystarfighter/UnrealEngine-ObjectPool | WorldSubsystem pool. Its README notes that pre-spawning causes "a big initial performance hit" | No time-slicing | (snippet only) GitHub |
| Epic forum, "Spread spawning across multiple frames?" | Spawning is game-thread-only. `SpawnActorDeferred` + a per-tick budget is the usual pattern. "To pool 300 actors you still need to spawn 300 actors" | — | (snippet only) https://forums.unrealengine.com/t/spread-spawning-across-multiple-frames/2001269 and https://forums.unrealengine.com/t/is-it-possible-to-safely-spawn-actors-off-the-game-thread/2592995 |

**Remaining gap (my assessment):**
- Time-sliced spawning and pooling **exist** (SpawnBudget, PoolManager), so the idea itself is not new.
- None of the public pages found publish controlled benchmarks or describe **verifying that pooled actors are fully reset**.
- None describes budgeting by a **predicted per-class spawn cost**, so that a spawn which cannot fit is never started. A check after each spawn lets one heavy actor overrun the budget; the same flaw Epic described for streaming in [T4-A].
- None describes a **per-phase cost breakdown** of a spawn (construction, component registration, physics state, BeginPlay).
- A project in this space must cite SpawnBudget and PoolManager and must not claim features they may have but don't document.
- **Gap strength: medium.**
