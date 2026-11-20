# 04 · Local Engine Verification

**Date:** 2026-11-19
**Method:** I read the engine source headers, `.uplugin` files and `Build.version` files of the Unreal Engine builds installed on this machine (`C:\Program Files\Epic Games\UE_5.x`). Nothing here comes from memory or from a web page. Every fact can be re-checked with the command listed next to it.

Why this file exists: web documentation for Unreal often lags the code or describes a different version. The installed engine source is the ground truth for *this* machine.

---

## 0. Re-check on UE 5.8.4 (2026-11-20)

You updated `UE_5.8` from 5.8.3 to **5.8.4** (CL 58902052). Everything below was first read on 5.8.3. The facts the roadmap and Project 1 depend on were re-checked on 5.8.4 with a scripted grep. **All 15 are unchanged:**

1. `tick.AllowBatchedTicks` defaults to 0.
2. Actors and components set `bAllowTickBatching = true`.
3. `FTickableGameObject::TickObjects` runs after `TG_PostPhysics` and before `TG_PostUpdateWork`.
4. `UTickableWorldSubsystem` exists.
5. `FTickFunction::ExecuteTick` is pure virtual.
6. `AActor::IncrementalRegisterComponents` is public.
7. There is no `SpawnActorAsync`.
8. The Animation Budget Allocator has `AddExternalAnimationTimeSpentThisFrame`.
9. `USignificanceManager::Update(TArrayView<const FTransform>)` exists.
10. The EQS manager constructor sets `MaxAllowedTestingTime = 0.01f`.
11. World Partition streaming sources have `bUseVelocityContributionToCellsSorting`.
12. The Iris plugin is still Beta.
13. `net.Iris.UseIrisReplication` still defaults to 0.
14. Mover is still Experimental.
15. `-UseFixedTimeStep`, `-BENCHMARK` and `-FPS=` are still parsed.

Line numbers quoted below come from 5.8.3 and may be off by a few lines in 5.8.4.

**New fact read on 5.8.4:** UnrealBuildTool maps module names to `.Build.cs` files in a case-insensitive dictionary and skips a second module with the same name (`Engine/Source/Programs/UnrealBuildTool/Configuration/Rules/RulesAssembly.cs:199–206`). Module names must therefore be unique within a project. That is why Project 1's plugin is `UpdateBudgetCore` and its game module is `UpdateBudget`.

## 0b. Build toolchain facts (Phase 4 of P01, 2026-11-20)

| Fact | Evidence |
|---|---|
| UnrealBuildTool uses **MSVC 14.44.35229** (folder `MSVC\14.44.35207` is only the toolset *family*) and Windows SDK 10.0.22621.0 | The UBT build log line: "Using Visual Studio 14.44.35229 toolchain (…\MSVC\14.44.35207) and Windows 10.0.22621.0 SDK" |
| 14.44.35229 is **not banned**: the banned range ends at 14.44.35210, and 14.44.35207–14.44.99999 is preferred | `Engine/Config/Windows/Windows_SDK.json` (`BannedVisualCppVersions`, `PreferredVisualCppVersions`) |
| **The launcher engine can't build the Test configuration** | BuildCookRun output: "Targets cannot be built in the Test configuration with this engine distribution." → headline benchmarks use packaged **Development** |

## 1. Installed engines

| Folder | Version (from `Engine/Build/Build.version`) | Changelist |
|---|---|---|
| `UE_5.6` | 5.6.1 | 44394996 |
| `UE_5.7` | **5.7.4** (the first plan said "5.7"; the patch is 5.7.4) | 51494982 |
| `UE_5.8` | **5.8.4** (was 5.8.3, CL 58210709, when this file was first written) | 58902052 |

## 2. Benchmark machine

| Item | Value |
|---|---|
| CPU | Intel Core i7-14650HX: 16 cores (8 performance + 8 efficiency), 24 threads |
| GPU | NVIDIA GeForce RTX 5070 Laptop GPU (driver 32.0.16.1047), plus Intel UHD integrated |
| RAM | 31.6 GB usable (32 GB) |
| OS | Windows 11 Home Single Language, build 26200 |
| Form factor | **Laptop** |
| Storage | C: WD PC SN5000S NVMe SSD (engine). D: Kingston SNV3S2000G NVMe SSD (all projects), NTFS, 955 GB free on 2026-11-20 |

**What this means for benchmarking:**
- **Thermal and power state change results.** Every run must be on AC power, use the same Windows power mode, and come after the same warm-up. Record the power mode with each result.
- **Hybrid CPU.** Windows can move the game thread between performance and efficiency cores, which adds frame-time noise. Run several times and report the spread. If the noise is too high, a later decision is whether to pin affinity. Doing so would be stated, not hidden.
- **Two GPUs.** The standalone build must run on the RTX 5070, not the integrated Intel GPU. Check this with `stat rhi` or the log line that names the adapter.

## 3. Tick system facts (UE 5.8.3 source)

File: `Engine/Source/Runtime/Engine/Classes/Engine/EngineBaseTypes.h` (`FTickFunction`)

| Fact | Evidence |
|---|---|
| `FTickFunction::TickInterval` (float) exists | line ~258 |
| `FTickFunction::UpdateTickIntervalAndCoolDown(float)` exists | line ~375 |
| `FTickFunction::SetTickFunctionEnable(bool)` exists | line ~364 |
| `bAllowTickBatching` flag exists on every tick function | line ~224. Also present in 5.6.1 and 5.7.4 |
| Actors and components **opt in** to batching by default | `Actor.cpp:280` `PrimaryActorTick.bAllowTickBatching = true;` and `ActorComponent.cpp:551` |
| …but the engine **does not batch by default** | `TickTaskManager.cpp`: `tick.AllowBatchedTicks` default **0** |
| `tick.AllowBatchedTicksUnordered` (default 0) | Present in 5.7.4 and 5.8.3, **not in 5.6.1** |
| `tick.AllowAsyncComponentTicks` default 1 | Components with `bRunOnAnyThread` may run in parallel |
| A 5.8 deprecation exists in `FTickFunction` | `UE_DEPRECATED(5.8, "Renamed because function only returns the correct time for functions with a TickInterval")`. Confirms that tick APIs **change between versions** |

**Consequence for Project 1:** the "before" baseline must cover more than default ticking. It must also include the engine's own options: `TickInterval` tuning and `tick.AllowBatchedTicks 1`. Otherwise the comparison is not honest.

## 4. Built-in budget and significance systems (UE 5.8.3 source)

| System | Location | What it does | What it does *not* do |
|---|---|---|---|
| **Significance Manager** (`USignificanceManager`) | `Plugins/Runtime/SignificanceManager/.../SignificanceManager.h` | `RegisterObject(Object, Tag, SignificanceFunction, PostSignificanceType, PostSignificanceFunction)` and `Update(TArrayView<const FTransform> Viewpoints)`. It scores objects and calls your callback. | It has **no time budget**. It never limits how many ms gameplay spends, and you decide what to do with the score. |
| **Animation Budget Allocator** (`IAnimationBudgetAllocator`) | `Plugins/Runtime/AnimationBudgetAllocator/.../IAnimationBudgetAllocator.h` | A real **ms budget**: `SetGameThreadLastTickTimeMs`, `SetComponentSignificance(...)`, `SetParameters(FAnimationBudgetAllocatorParameters)`. | It works **only on `USkeletalMeshComponentBudgeted`**, so it covers skeletal-mesh animation only, not general gameplay logic. |

## 4b. Spawning, pooling and registration (UE 5.8.3 source)

Checked because the talks research ranks "hitches when content enters the world" as the #1 recurring problem, and Epic staff said a native time-sliced spawn and actor pooling don't exist yet (see `02_technical_challenges.md` [T1-D]).

| Question | Answer in 5.8.3 | Evidence |
|---|---|---|
| Is there a `SpawnActorAsync` or a time-sliced spawn API? | **No.** No match in `Source/Runtime/Engine` or `Plugins/Runtime` headers | `grep -rln "SpawnActorAsync\|AsyncSpawnActor\|TimeSlicedSpawn"` returns nothing |
| Is there a general actor pool? | **No general one.** The only pooling interface is Mass-specific: `IMassActorPoolableInterface` | `Plugins/Runtime/MassGameplay/Source/MassActors/Public/MassActorPoolableInterface.h` |
| Deferred spawn | `UWorld::SpawnActorDeferred<T>(...)` + `AActor::FinishSpawning(const FTransform&, ...)` | `World.h:3851`, `Actor.h:3117` |
| Incremental component registration | `AActor::IncrementalRegisterComponents(int32 NumComponentsToRegister, FRegisterComponentContext* = nullptr)` is **public**. Its comment says "used during level streaming" | `Actor.h:3267` (under `public:` from line 3122). Also present in 5.6.1 and 5.7.4 |
| Async physics-state creation | **Experimental** path: `UActorComponent` implements `IAsyncPhysicsStateProcessor`. The base `AllowsAsyncPhysicsStateCreation()` returns `false`, so components must opt in. Budget cvar `p.Chaos.AsyncPhysicsStateTask.TimeBudgetMS` (0 = unlimited) | `ActorComponent.h:160, 898`, `PhysicsEngine/Experimental/PhysScene_AsyncPhysicsStateJobQueue.cpp:14`. Present in 5.6.1, 5.7.4, 5.8.3. 5.8 adds `...PriorityExtraTimeBudgetMS` |

## 4c. Facts checked for Project 1's architecture and benchmark plan (UE 5.8.3 source)

| Question | Answer | Evidence |
|---|---|---|
| When do `FTickableGameObject` / `UTickableWorldSubsystem` tick? | **After `TG_PostPhysics`, before `TG_PostUpdateWork`.** That is **not** the same point as default Actor ticks (`TG_PrePhysics`) | `LevelTick.cpp`: `RunTickGroup(TG_PrePhysics)` :1750 … `RunTickGroup(TG_PostPhysics)` :1778 … `FTickableGameObject::TickObjects` :1821 … `RunTickGroup(TG_PostUpdateWork)` :1877 |
| `UTickableWorldSubsystem` exists | `class UTickableWorldSubsystem : public UWorldSubsystem, public FTickableGameObject`. `GetStatId()` is pure virtual | `Subsystems/WorldSubsystem.h:80, 92` |
| A custom tick function can be registered | `FTickFunction::ExecuteTick(...)` and `DiagnosticMessage()` are pure virtual. `RegisterTickFunction(ULevel*)` | `EngineBaseTypes.h:524, 532, 341` |
| `UDeveloperSettings` (Project Settings page) | `class UDeveloperSettings : public UObject` | `Runtime/DeveloperSettings/Public/Engine/DeveloperSettings.h:23` |
| High-resolution timer | `FPlatformTime::Seconds()` | `Windows/WindowsPlatformTime.h:23` |
| Deterministic frame stepping | `-BENCHMARK` sets `FApp::SetBenchmarking`; `-UseFixedTimeStep` (or deterministic mode) sets `FApp::SetUseFixedTimeStep`; `-FPS=` sets the fixed rate | `LaunchEngineLoop.cpp:2451, 2460, 4727` |
| CSV capture from the command line | `-csvCaptureFrames=`, `-csvCategories=`, `-csvGpuStats` are parsed | `CsvProfiler.cpp:5049, 5132, 5098` |
| Profiling macros | `CSV_DEFINE_CATEGORY`, `CSV_SCOPED_TIMING_STAT`, `CSV_CUSTOM_STAT`, `CSV_EVENT`; `TRACE_CPUPROFILER_EVENT_SCOPE`; `DECLARE_STATS_GROUP`, `DECLARE_CYCLE_STAT`, `SCOPE_CYCLE_COUNTER` | `CsvProfiler.h:50–155`, `CpuProfilerTrace.h:453`, `Stats/Stats.h` |
| Python for analysis scripts | Python 3.13.7 installed | `python --version` |

## 4d. Facts checked for other roadmap projects (UE 5.8.3 source)

| Question | Answer | Evidence |
|---|---|---|
| Can EQS share results between agents? | **No result sharing found.** `FEnvQueryInstanceCache` caches query *instances/templates* (sorted tests), not results. The time budget is `MaxAllowedTestingTime`: the config-struct default is `0.003f`, and the manager constructor sets `0.01f` | `AIModule/Classes/EnvironmentQuery/EnvQueryManager.h:36, 128–151`; `EnvQueryManager.cpp:182` |
| Do World Partition streaming sources already use velocity? | **Yes, for cell sorting.** `FWorldPartitionStreamingSource` has `FVector Velocity` and `bool bUseVelocityContributionToCellsSorting` | `Engine/Public/WorldPartition/WorldPartitionStreamingSource.h:419, 422` |

## 5. Plugin status flags (`.uplugin` `IsBetaVersion` / `IsExperimentalVersion`)

`.uplugin` flags are a strong hint, not Epic's official status label. The release notes are the authority (see `03_existing_solutions_gap_analysis.md`).

| Plugin | 5.6.1 | 5.7.4 | 5.8.3 |
|---|---|---|---|
| Iris | Experimental | Beta | Beta |
| ReplicationGraph | Beta | Beta | Beta |
| NetworkPrediction | Beta | Beta | Beta |
| Mover | Experimental | Experimental | Experimental |
| MassGameplay / MassCrowd | Experimental | Experimental | Experimental |
| MassEntity | Beta (plugin) | Beta (plugin) | No plugin: lives at `Engine/Source/Runtime/MassEntity` |
| StateTree / GameplayStateTree | no flag (stable) | no flag | no flag |
| SmartObjects | no flag | no flag | no flag |
| PCG | Beta | no flag | no flag |
| MotionWarping | Beta | Beta | Beta |
| PoseSearch / Chooser | no flag field | no flag field | no flag field |
| Gauntlet | Beta | Beta | Beta |
| CQTest | no flag | no flag | no flag |
| FunctionalTestingEditor | no flag | no flag | no flag |
| DataValidation | no flag | no flag | no flag |
| GameFeatures / ModularGameplay | Beta | Beta | Beta |
| SignificanceManager | stable | stable | stable |
| AnimationBudgetAllocator | stable | stable | stable |

## 6. Profiling tools present in the 5.8.3 install

- Unreal Insights (`UnrealEditor-TraceInsights*.dll` in `Engine/Binaries/Win64/`)
- CSV profiler (`Engine/Source/Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp`)
- CSV tools in `Engine/Binaries/DotNET/CsvTools/`: `CSVCollate`, `CSVFilter`, `CSVSplit`, `CSVStats`, `CSVToSVG`

## 7. Re-check commands

```bash
E="/c/Program Files/Epic Games/UE_5.8/Engine"
grep -n "bAllowTickBatching\|TickInterval" "$E/Source/Runtime/Engine/Classes/Engine/EngineBaseTypes.h"
sed -n '53,70p' "$E/Source/Runtime/Engine/Private/TickTaskManager.cpp"
grep -rn "bAllowTickBatching" "$E/Source/Runtime/Engine/Private/Actor.cpp"
grep -n "virtual" "$E/Plugins/Runtime/AnimationBudgetAllocator/Source/AnimationBudgetAllocator/Public/IAnimationBudgetAllocator.h"
```
