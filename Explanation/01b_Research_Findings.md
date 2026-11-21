# 01b · Research Findings (Phase 2): UpdateBudget

**Phase:** 2 (Research), completed 2026-11-20 · **Engine:** UE 5.8.4 (CL 58902052), read from `C:\Program Files\Epic Games\UE_5.8\Engine`

**Method:**
- Four investigations ran in parallel: tick internals, measurement tooling, gameplay-framework APIs, and existing work on the web.
- Every engine claim cites a path relative to `Engine/` plus a line number in 5.8.4.
- The claims that drive design decisions were **re-checked by hand** before being written here; they are marked ✔.
- Claims drawn from following code flow rather than a direct statement are marked **[inferred]**.
- Hypotheses still need a measurement and are marked **[to measure]**.

**Why this phase matters.** Phase 1 said what we *think* the problem is. Phase 2 checks what the engine *actually* does. Six of the findings below changed the design, and two changed the benchmark plan. Without this phase those would have become bugs or unfair comparisons.

---

## 1. What Phase 2 changed

| # | Finding | Consequence for UpdateBudget |
|---|---|---|
| 1 | **Components tick in `TG_DuringPhysics` by default; Actors in `TG_PrePhysics`** ✔ | The agent component explicitly ticks in `TG_PrePhysics`, the same group as the scheduler. Otherwise B0 and UB would run at different points in the frame, and the comparison would be unfair |
| 2 | **A custom `FTickFunction` starts ticking as soon as it is registered.** `RegisterTickFunction` ignores `bCanEverTick`, and the default `TickState` is `Enabled` ✔. **World subsystems also exist in Editor worlds** ✔ | Register the scheduler's tick in `OnWorldBeginPlay` (never in `Initialize`). Restrict `DoesSupportWorldType` to Game and PIE. Unregister explicitly in `Deinitialize` |
| 3 | **Interval ticks fire immediately when enabled, re-arm with "overrun credit", and nothing staggers them** ✔ | Hypothesis H4 (same-interval ticks spiking together) is now supported by the code **[inferred]**. Baseline B2 is designed to expose it **[to measure]** |
| 4 | **The engine already passes accumulated delta to interval ticks:** time since that function's own last tick ✔ | Accumulated delta is **not** new, and we don't claim it. Our scheduler must match the engine's behaviour so the comparison is like-for-like |
| 5 | **Disabling or re-timing a tick that is cooling down walks the whole cooling-down list** (`TickTaskManager.cpp:1876–1902, 2475`) | Baselines B2 and B3 pay this every time they re-tune thousands of intervals. It is part of their real cost, so it is measured, not hidden. Our scheduler never touches engine tick state per frame |
| 6 | **Without batching, every enabled tick function gets its own task and completion event every frame** (`:875, :683, :1184`). Batching has conditions (`:714, :752`) | Hypothesis H1 (dispatch overhead) can be calibrated with the engine's own `tick.AddTestTickFunctions` (10,000 no-op ticks) ✔ |
| 7 | **`OnWorldBeginPlay` runs before any actor's `BeginPlay`; `Deinitialize` runs after all `EndPlay`** ✔ [inferred for teardown] | The registry exists before the first agent registers and outlives the last one to unregister. That ordering is safe |
| 8 | **GC sets destroyed components in a `UPROPERTY` `TObjectPtr` array to null, but only when GC runs** ✔. Until then the pointer is non-null and points at garbage | Components unregister explicitly in `EndPlay`. The scheduler also guards each entry with `IsValid()` and compacts nulls |
| 9 | **Components can be added or destroyed synchronously during another component's update**, with `BeginPlay`/`EndPlay` called inline | Additions and removals made during the update loop are **deferred** and applied after the loop |
| 10 | **The Significance Manager doesn't tick; the game must call `Update` itself** ✔. It is **created after our subsystem's `Initialize`** ✔, runs significance functions in `ParallelFor` ✔, and keeps registered objects alive until they are unregistered | The B3 driver fetches it in `OnWorldBeginPlay`, calls `Update` once per frame, and unregisters every object in `EndPlay` |
| 11 | **Cheat cvars can't be set from ini files or `-dpcvars`** ✔ | Benchmark switches use `ECVF_Default`, and the benchmark director also reads its own `-UB…=` command-line values |
| 12 | **The Animation Budget Allocator already does four things we planned:** a closed-loop ms budget, significance ordering, a skip cap (`MaxTickRate = 10` frames), accumulated delta via `SetExternalDeltaTime`, plus smoothed per-component cost ✔ | **Repositioned.** UpdateBudget *adapts* the Animation Budget Allocator's model to general gameplay logic, with a staleness bound in **seconds**. Credit is given explicitly (§5) |

**Benchmark-plan changes:**

| # | Finding | Consequence |
|---|---|---|
| B-1 | **In fixed-step mode the engine sets the delta to 1/FPS and doesn't wait** (`UnrealEngine.cpp:3015–3023`) ✔. CSV `FrameTime` and `GameThreadTime` are taken from real timers ✔ | Headline comparisons use **fixed-step runs** (`-deterministic -FPS=60 -novsync`). Every configuration simulates identical frames, and only the cost differs. An uncapped real-time run confirms the result |
| B-2 | **No shipped tool prints p95/p99** (`CsvInfo` gives avg/min/max; PerfReportTool has no percentiles); `-csvExitOnCompletion` doesn't exist ✔; `-csvCaptureFrames=` includes loading frames | We write our own small stats script, `Tools\csv_stats.py`. Capture starts on a `CSV_EVENT` after warm-up: `-csvStartOnEvent=BenchStart -csvCaptureOnEventFrameCount=N -ExitAfterCsvProfiling` ✔ |

---

## 2. Tick system (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| Actor default group `TG_PrePhysics` | `Source/Runtime/Engine/Private/Actor.cpp:276` ✔ |
| Component defaults: `TickGroup = TG_DuringPhysics`, `bStartWithTickEnabled = true`, `bCanEverTick = false` | `Private/Components/ActorComponent.cpp:548–550` ✔ |
| Default `TickState` is `Enabled`; `RegisterTickFunction` checks only the dedicated-server flag | `Private/TickTaskManager.cpp:2374, 2406–2421` ✔ |
| Only Actor/Component setup reads `bCanEverTick` / `bStartWithTickEnabled` | `Actor.cpp:1690–1693`, `ActorComponent.cpp:1741–1747` |
| A world subsystem owning a `TG_PrePhysics` tick function, registered on `PersistentLevel`: Mass does exactly this | `Source/Runtime/MassEntity/Private/MassProcessingPhaseManager.cpp:485–486` ✔ |
| Interval: the first tick is queued immediately, then `RescheduleForInterval`; the re-arm gives overrun credit | `TickTaskManager.cpp:1489–1492, 1506` ✔ |
| Same cooldown → relative cooldown 0 → the functions stay adjacent and fire together; no stagger or randomisation anywhere | `:1452` ✔; a grep for random/stagger/jitter in the file is empty **[inferred]** |
| Interval tick delta = world time since its own last interval tick (the first tick gets the frame delta) | `:2834–2842` ✔ |
| Cancelling or re-timing a cooling-down function walks the list | `:1876–1902, 2475` |
| Batching requires `tick.AllowBatchedTicks=1` **and** `bAllowTickBatching`; not `bRunOnAnyThread`, not `bHighPriority`, and `StartGroup == EndGroup`; start-of-frame queue only | `:59–62, 714, 729–730, 752` |
| `~FTickFunction` unregisters itself; it must be registered on a visible level | `:2396–2398, 2238` |
| `FTickableGameObject::TickObjects` runs after `TG_PostPhysics`, and a tickable can't choose a tick group | `LevelTick.cpp:1821`; `Tickable.h` (no group API) |
| Editor worlds tick with `LEVELTICK_ViewportsOnly` | `Source/Editor/UnrealEd/Private/EditorEngine.cpp:1958` |
| `tick.AddTestTickFunctions` registers 10,000 no-op ticks for calibration | `TickTaskManager.cpp:2892, 2952` ✔ |
| 5.8 rename: `GetLastTickGameTime()` → `GetLastIntervalTickGameTime()` (now `double`) | `Classes/Engine/EngineBaseTypes.h:464–467` ✔ |

## 3. Gameplay framework (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| World init order: `InitializeSubsystems()` → `OnPreWorldInitialization` → … → `PostInitializeSubsystems()` | `Private/World.cpp:2447, 2449, 2610` ✔ |
| `OnWorldBeginPlay` runs before `GameMode->StartPlay()` (which calls actors' `BeginPlay`) | `World.cpp:6167, 6174` ✔; `WorldSettings.cpp:370–374` |
| World teardown: every actor's `EndPlay` → `OnWorldEndPlay` → `CleanupWorld` → … → `Deinitialize` | `World.cpp:6203–6210, 6361–6486` **[inferred order]** |
| Default world types: Game, Editor, PIE | `Private/Subsystems/WorldSubsystem.cpp:31` ✔ |
| `UWorldSubsystem` has **no** `UpdateStreamingState`; the streaming hook is `IStreamingWorldSubsystemInterface::OnUpdateStreamingState` | `WorldSubsystem.h` |
| Component start: `OnRegister` → `InitializeComponent` (only if `bWantsInitializeComponent`) → `BeginPlay` | `ActorComponent.cpp` (see the notes) |
| Destroy: `EndPlay(Destroyed)` → `UninitializeComponent` → `OnUnregister` → `OnComponentDestroyed`. **Streaming unload:** `OnUnregister` first, then `EndPlay(RemovedFromWorld)`, possibly frames apart **[inferred]** | `ActorComponent.cpp`, `Level.cpp` |
| `EEndPlayReason`: Destroyed, LevelTransition, EndPlayInEditor, RemovedFromWorld, Quit | `EngineTypes.h` |
| GC nulls array references to garbage: `KillReference(UObject*& Object) { Object = nullptr; }` | `CoreUObject/Private/UObject/GarbageCollection.cpp:2984` ✔; `FastReferenceCollector.h:704` |
| Significance Manager: created on `OnPreWorldInitialization` and destroyed on `OnPostWorldCleanup`; the game must call `Update`; `ParallelFor` inside | `Plugins/Runtime/SignificanceManager/.../SignificanceManager.cpp:99–100, 499` ✔; `SignificanceManager.h:24–27` ✔ |
| `OrderedBudget.h` exists in the same plugin: an existing **count-based** budget | `.../SignificanceManager/Public/OrderedBudget.h` ✔ |
| `ALLOW_CHEAT_CVARS_IN_TEST 1`; cheat cvars are refused from ini and `-dpcvars` | `Core/Public/Misc/Build.h:436` ✔; `Core/Private/Misc/ConfigUtilities.cpp:348` ✔ |
| `UDeveloperSettingsBackedByCVars` syncs cvars **only in the editor** | `DeveloperSettings` module (see the notes) |
| 5.8 export convention: `#define UE_API <MODULE>_API` + `UCLASS(MinimalAPI)` + `UE_API` on exported members + `#undef UE_API` | e.g. `SignificanceManager.h:8` ✔ |
| 5.8 deprecation: `FindAndPopulateSubsystemArrayInternal` → `GetSubsystemArrayCopy` | `Subsystems/SubsystemCollection.h:79` |

## 4. Measurement and tooling (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| `-deterministic` = `-UseFixedTimeStep -FixedSeed` | `Launch/Private/LaunchEngineLoop.cpp:2457–2462` ✔ |
| A fixed step sets the delta to the fixed value and never waits; `t.MaxFPS`, smoothing and `bUseFixedFrameRate` are skipped. Only VSync can throttle | `Engine/Private/UnrealEngine.cpp:3015–3023` ✔ |
| CSV global columns: `FrameTime`, `GameThreadTime`, `RenderThreadTime`, `GPUTime`, `RHIThreadTime`, `*_CriticalPath` | `LaunchEngineLoop.cpp:1518–1525` ✔; `CsvProfiler.cpp:4388` ✔ |
| Start capture on an event: `-csvStartOnEvent=`, `-csvCaptureOnEventFrameCount=`; exit: `-ExitAfterCsvProfiling` | `CsvProfiler.cpp:5078, 5081, 4309` ✔ |
| Output: `Saved/Profiling/CSV/Profile(YYYYMMDD_HHMMSS).csv`. The last two rows are a repeated header plus metadata, so the stats script must drop them | `CsvProfiler.cpp` (see the notes) |
| CSV profiler and Trace are compiled into Development and Test only, not Shipping | `CsvProfiler.h` (`CSV_PROFILER` define) |
| An open Unreal Insights makes every Development/Test run auto-trace; `-traceautostart=0` turns that off | `Core/Private/ProfilingDebugging/TraceAuxiliary.cpp:267–270, 2740–2757` ✔ |
| CSV tools need .NET 10, bundled at `Engine/Binaries/ThirdParty/DotNet/10.0` (set `DOTNET_ROOT`). `CsvInfo` gives avg/min/max; there are **no percentiles** | `Engine/Binaries/DotNet/CsvTools/` ✔ |
| `FStatUnitData` is smoothed and updates only while `stat unit` is visible. Raw `GGameThreadTime` / `GRenderThreadTime` / `RHIGetGPUFrameCycles()` are not | `RenderTimer.h` |
| `stat game` needs the stats system, which is **off in Test**. `stat unit`, `stat unitgraph`, `stat fps` and `dumpticks [grouped|enabled|disabled]` exist | `Stats` module (see the notes) |
| `GameUserSettings.ini` can override `r.VSync` / `t.MaxFPS` at startup, so it is reset before benchmark runs | `GameUserSettings.cpp:521–537` |
| Packaging: `RunUAT.bat BuildCookRun -project=… -noP4 -utf8output -unattended -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=…` Headline runs use `-clientconfig=Development`; **Test isn't available in the launcher engine** (found in Phase 4). Not `-client` | `Source/Programs/AutomationTool/.../ProjectParams.cs` (see the notes) |

**Engine timing modes in one picture:**

```text
                       DeltaTime given to gameplay      Engine waits?        Use in this project
Uncapped real-time     real frame time (varies)         no                   confirmation run
Fixed step (-deterministic -FPS=60)
                       exactly 1/60 s, every frame      no (runs flat out)   HEADLINE A/B runs + correctness
-BENCHMARK alone       1/30 s fixed step                no                   not used (audio/focus side effects)
```

## 5. Existing work, updated

| Solution | What it controls | ms budget? | Staleness bound? | Delta correction? | Published numbers (their claims) | Evidence |
|---|---|---|---|---|---|---|
| **Epic Animation Budget Allocator** | Skeletal-mesh animation only | **Yes, closed-loop** (smoothed, measured cost per component) | Partial: skips capped at `MaxTickRate` (10 **frames**); may exceed the budget; extra off-screen meshes switched off | **Yes**: accumulated, via `SetExternalDeltaTime` | Coconut Lizard, cooked Development: 256 AI, 60.8 → 15.1 ms of anim game-thread time | 5.8 docs opened; 5.8.4 source ✔ |
| Significance Manager | Scoring and sorting | No | No | n/a | — | docs, source ✔ |
| `OrderedBudget.h` (same plugin) | Count-based LOD budget | No (counts) | No | n/a | — | source ✔ |
| Engine `TickInterval` | One tick function | No | Interval only | **Yes** | — | source ✔ |
| Engine tick batching (5.5+) | Dispatch overhead | No | n/a | n/a | Epic Games Japan: ~0.6 ms per 2,000 ticks (~0.3 µs each), DevelopmentEditor | opened |
| Mass Simulation LOD / `FExecutionLimiter` | Mass entities only | No (entity count) | Per-LOD interval | Yes (snippet only) | — | docs |
| Tick Optimization Toolkit (Fab) | Tick interval or on/off by distance zone and visibility | No | No (zones can switch tick off) | Not documented | Author: "1024 AI … 108 ms → 18 ms", no hardware or version stated | official page opened |
| Optimization Tools (Fab) | Significance bands → intervals; round-robin pool; aggregation | No | No | Not documented | — | opened |
| EnhancedTick (GitHub) | Batching by type; LowPrio every 3rd frame | No | Fixed cadence only | No | — | README and source read |
| Tom Looman, `URogueDeferredTaskSystem` | One-shot deferred tasks | Yes, open-loop (1/120 s target, ≥1 ms) | No | No | — | source read |
| Rare, "Aggregating Ticks to Manage Scale in Sea of Thieves" (2019) | Type-batched tick managers (secondhand) | Unknown | Unknown | Unknown | Not obtained | talk listing only |

**The gap, stated conservatively:**
- UpdateBudget builds on Epic's Animation Budget Allocator and Significance Manager.
- Among the public UE tools reviewed, none applies a measured millisecond budget, significance priority and a **per-object staleness bound in seconds** to **general gameplay updates**.
- None publishes controlled measurements of how well it holds both the budget and the bound.
- Studio-internal systems aren't public, and a web search can't prove absence. So this is a statement about what we reviewed, never "nobody has done this".

**Why seconds rather than frames:** a 10-frame cap is 333 ms at 30 fps and 69 ms at 144 fps. A gameplay guarantee such as "a guard notices you within 0.5 s" must not depend on frame rate.

## 6. Hypothesis status after Phase 2

| # | Hypothesis | Status |
|---|---|---|
| H1 | Each tick has dispatch overhead | **Supported by code** (a task per tick function without batching). Calibrate with `tick.AddTestTickFunctions` **[to measure]** |
| H2 | Cost scales with count, not relevance | Unchanged **[to measure]** |
| H3 | Static intervals don't adapt to load | Unchanged **[to measure]** |
| H4 | Same-interval ticks clump into periodic spikes | **Supported by code** (no stagger; adjacent with relative cooldown 0) **[to measure]** |
| H5 | Scattered memory access adds cost | Unchanged **[to measure]**. A dense scheduler array only helps the scheduling part |
| **H6 (new)** | Re-tuning many intervals at once is itself costly (B2/B3) | **Supported by code** (a walk of the cooling-down list per change) **[to measure]** |

## 7. Carried into Phase 3

- Use the tick-function pattern: a USTRUCT deriving from `FTickFunction` with `TStructOpsTypeTraits` `WithCopy = false`, as Mass and the engine's test ticks do; register it on `PersistentLevel`.
- Start the native baseline component in `TG_PrePhysics`, with `bCanEverTick = true` and `bStartWithTickEnabled` depending on the mode.
- Make the registry safe: deferred add/remove, `IsValid` guard, null compaction, unregister in `EndPlay`.
- Read benchmark switches from both cvars (`ECVF_Default`) and the command line (`-UBMode=`, `-UBAgents=`, `-UBBudgetMs=`).
- Headline builds were planned as **Test**. **Phase 4 found the launcher engine can't build Test**, so headline runs use a **packaged Development** build (see `03` F3). Fixed-step mode, CSV capture starting on an event, and `-traceautostart=0`.
- Write the stats script: per-run median, p95 and p99; the median across runs plus the min–max range; drop the CSV's last two rows.

**Raw research notes**, with every quote, are in [Phase2_Raw_Notes/](Phase2_Raw_Notes/README.md) as an audit trail. This file is authoritative: everything the design depends on is restated above with its evidence.
