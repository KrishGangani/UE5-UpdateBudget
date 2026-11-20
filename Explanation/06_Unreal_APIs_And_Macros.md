# 06 · Unreal APIs And Macros: UpdateBudget

**Rule:**
- Only APIs this project **actually uses** appear here, each in the same step it enters the code.
- Each entry follows the 8-point format: what it is · why it exists · how it works · when to use it · when not to · common mistakes · a small example · how we use it.
- Verified against UE **5.8.4** source; file references are relative to `Engine/`.

| # | API | First used in |
|---|---|---|
| 1 | `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`, `FDefaultGameModuleImpl` / `FDefaultModuleImpl` | Phase 4 |
| 2 | `ModuleRules`: dependency lists and `PCHUsage` | Phase 4 |
| 3 | `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion` | Phase 4 |
| 4 | Descriptor `LoadingPhase` (`.uproject` / `.uplugin`) | Phase 4 |

---

## 1. `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`

1. **What it is.** Macros that generate a module's entry point (`Core/Public/Modules/ModuleManager.h:946–955, 1100–1136`).
2. **Why it exists.** The engine loads code as **modules**: DLLs in editor builds, static code in packaged builds. Each module needs a standard way to be found and started, so the module manager can call its `StartupModule()`/`ShutdownModule()`.
3. **How it works.**
   - In a modular build it exports an `InitializeModule` function, which the module manager calls after loading the DLL. It returns an instance of the class you name.
   - In a monolithic build it registers a static initializer instead.
   - The *primary* variant also sets up project-wide globals for packaged builds (`GInternalProjectName`, `GIsGameAgnosticExe`) and key and target-name registration.
4. **When to use it.**
   - Exactly one `IMPLEMENT_MODULE` (or the `GAME` variant) per module.
   - At least one `IMPLEMENT_PRIMARY_GAME_MODULE` per project.
5. **When not to.** Not twice in one module, which causes duplicate-symbol link errors. A plugin module uses `IMPLEMENT_MODULE`, not the primary game variant.
6. **Common mistakes.**
   - The module name argument doesn't match `Build.cs` and the `.uproject` ("module not found" at startup).
   - Writing an empty custom `IModuleInterface` class when `FDefaultModuleImpl` would do.
   - Thinking the third argument still sets the game name. In 5.8 it is ignored.
7. **Example.**
   ```cpp
   #include "Modules/ModuleManager.h"
   IMPLEMENT_MODULE(FDefaultModuleImpl, MyPluginModule);
   ```
8. **In this project.** `UpdateBudgetModule.cpp` uses the primary variant with `FDefaultGameModuleImpl`. `UpdateBudgetCoreModule.cpp` uses `IMPLEMENT_MODULE(FDefaultModuleImpl, UpdateBudgetCore)`. Neither module needs startup logic.

## 2. `ModuleRules`: dependencies and `PCHUsage`

1. **What it is.** The C# class in `<Module>.Build.cs` that tells UnrealBuildTool how to compile one module.
2. **Why it exists.** Unreal is hundreds of modules. Each one declares what it depends on, so UBT can work out include paths, link order and what to rebuild.
3. **How it works.**
   - `PublicDependencyModuleNames`: their include paths are given to us *and* to anyone who depends on us.
   - `PrivateDependencyModuleNames`: only to us.
   - Both link the dependency.
   - `PCHUsage = UseExplicitOrSharedPCHs` uses a shared precompiled header built for a common dependency set (e.g. `SharedPCH.UnrealEd.Project…` in our build log), unless a module names its own.
4. **When to use which.** Public **only** if one of *your public headers* includes that module's headers. Everything else is private.
5. **When not to.** Don't add modules "just in case". Every dependency costs build time and couples modules.
6. **Common mistakes.**
   - Making everything public, which spreads include paths and makes rebuilds wider.
   - A missing dependency, which shows up as `unresolved external symbol` link errors or `cannot open include file`.
   - A circular dependency between two modules (UBT reports it).
7. **Example.**
   ```csharp
   PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
   PrivateDependencyModuleNames.Add("UpdateBudgetCore");
   ```
8. **In this project.**
   - The game module depends privately on `UpdateBudgetCore`, and only its `.cpp` files use the plugin.
   - The plugin depends publicly on `Core`, `CoreUObject` and `Engine`, because its public headers will expose engine types.
   - `DeveloperSettings` is added in S6, when it is first needed.

## 3. `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion`

1. **What it is.** The C# class in `<Name>.Target.cs` that describes one executable to build: game, editor, client, server or program.
2. **Why it exists.** The same modules build into different executables, each with different settings and module sets.
3. **How it works.** UBT finds the class `<Name>Target` and builds its `ExtraModuleNames` plus the engine and enabled plugins. `DefaultBuildSettings` selects a versioned set of default build options. `IncludeOrderVersion` selects which engine version's header include order to use.
4. **When to use it.** One file per executable type. Here: `UpdateBudget` (game) and `UpdateBudgetEditor` (editor).
5. **When not to.** Don't add `Client`/`Server` targets until the project is networked (P5).
6. **Common mistakes.**
   - The class name doesn't follow `<Name>Target`.
   - Leaving an old `IncludeOrderVersion` after an engine upgrade, which hides missing-include bugs until they break later.
   - Expecting `Test` to build on a launcher engine. It doesn't ("Targets cannot be built in the Test configuration with this engine distribution", found in Phase 4).
7. **Example.** See `Source\UpdateBudget.Target.cs` (`05` Part A2).
8. **In this project.** `V7` and `Unreal5_8`, the same values as Epic's 5.8 `TP_Blank` template. Headline builds are packaged **Development**.

## 4. Descriptor `LoadingPhase`

1. **What it is.** A field on each module in `.uproject`/`.uplugin` that says when the module loads during engine startup.
2. **Why it exists.** Some modules must load before others: for example, before the first world is created, or before the editor UI.
3. **How it works.** The engine loads modules phase by phase (`PostConfigInit`, `PreDefault`, `Default`, `PostDefault`, …). A module that another loaded module depends on is loaded first regardless.
4. **When to use which.** `Default` for ordinary gameplay code. Earlier phases only when something must exist before engine systems start.
5. **When not to.** Don't pick an early phase "to be safe". Early modules can't use systems that aren't up yet.
6. **Common mistakes.** UClasses that a map or config needs aren't registered yet because the module loads too late (`PostDefault` or later), which causes "class not found" when loading.
7. **Example.** `{ "Name": "UpdateBudgetCore", "Type": "Runtime", "LoadingPhase": "Default" }`
8. **In this project.** Both modules use `Default`. The Phase 4 smoke-test log shows both loading before `LoadMap`, which is what world subsystems need.

---

## APIs added in step S1

| # | API | Where |
|---|---|---|
| 5 | `UActorComponent` lifecycle and `PrimaryComponentTick` | `UBudgetedUpdateComponent` |
| 6 | `UWorldSubsystem` / `UTickableWorldSubsystem` | `UUpdateBudgetSubsystem`, `UUBBenchmarkSubsystem` |
| 7 | `UCLASS` / `UENUM` / `UPROPERTY` / `GENERATED_BODY` and the specifiers used | all classes |
| 8 | `TObjectPtr` and UPROPERTY references (GC) | registry, component link, agents |
| 9 | `TArray`: `Add`, `RemoveAtSwap`, `IsValidIndex`, `Reserve` | registry |
| 10 | CSV profiler: `CSV_DEFINE_CATEGORY`, `CSV_CUSTOM_STAT` | subsystem `Tick` |
| 11 | `SpawnActorDeferred` / `FinishSpawning` | workload spawn |
| 12 | `CreateDefaultSubobject` / `ConstructorHelpers::FObjectFinder` | `AUBDemoAgent` |
| 13 | `FRandomStream`, `FParse::Value` / `FCommandLine`, `FCrc::TypeCrc32` | benchmark subsystem |
| 14 | `check` / `ensure`, `UE_LOG` + `DEFINE_LOG_CATEGORY_STATIC`, `RETURN_QUICK_DECLARE_CYCLE_STAT`, `UE_INLINE_GENERATED_CPP_BY_NAME` | various |
| 15 | `CSV_EVENT` / `CSV_METADATA` and `-csvStartOnEvent` / `-csvStopOnEvent` (S2) | benchmark director |
| 16 | `ACameraActor` + `APlayerController::SetViewTarget`; `FFileHelper::SaveStringToFile` + `FPaths::ProjectSavedDir`; `FPlatformMisc::RequestExit` (S2) | benchmark director |
| 17 | `USignificanceManager` (S3) | baseline B3 |
| 18 | `SetComponentTickInterval` vs `SetComponentTickIntervalAndCooldown` (S3) | baselines B2/B3 |
| 19 | `FSubsystemCollectionBase::InitializeDependency` (S3) | baseline driver |
| 20 | Dynamic delegates: `OnEndPlay`, `AddDynamic`, `UFUNCTION` (S3) | baseline driver |
| 21 | Small utilities from the S3 clean-up: `FMath::GetRangePct`, `TNumericLimits`, `GetCameraCacheView` / `FMinimalViewInfo`, `TArray::RemoveSingleSwap`, `MakeArrayView`, `ensureMsgf`, `FString::ToUpperInline` | baseline driver |

## 5. `UActorComponent` lifecycle and `PrimaryComponentTick`

1. **What it is.** The base class for reusable behaviour attached to an actor, plus its built-in tick function `PrimaryComponentTick` (an `FTickFunction`).
2. **Why it exists.** It splits an actor's behaviour into modules that the engine starts, ticks and stops for you.
3. **How it works.**
   - Start: `OnRegister` → (`InitializeComponent` if wanted) → `BeginPlay`.
   - Destroy: `EndPlay(Destroyed)` → `UninitializeComponent` → `OnUnregister`.
   - Streaming unload: `OnUnregister` happens first, then `EndPlay(RemovedFromWorld)` (`01b` §3).
   - The tick is registered only if `bCanEverTick` is true (default **false**). It starts enabled if `bStartWithTickEnabled` (default true). It runs in `TickGroup` (default **`TG_DuringPhysics`**) (`ActorComponent.cpp:548–550`).
4. **When to use it.** Behaviour that belongs to one actor and should be reusable across actor types.
5. **When not to.** World-wide services (use a subsystem). Logic that needs no per-actor state.
6. **Common mistakes.**
   - Forgetting `bCanEverTick = true` (the tick never runs).
   - Assuming the default tick group is PrePhysics.
   - Not calling `Super::BeginPlay` or `Super::EndPlay`.
   - Doing world registration in the constructor; there is no world yet, because the constructor also runs for the CDO.
7. **Example.** `PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickGroup = TG_PrePhysics;`
8. **In this project.** `UBudgetedUpdateComponent` pins the native tick to `TG_PrePhysics` (fair baselines). It registers in `BeginPlay`, unregisters in `EndPlay`, and ignores the engine's `DeltaTime` in favour of its own time contract.

## 6. `UWorldSubsystem` / `UTickableWorldSubsystem`

1. **What it is.** Engine-managed singletons, one per `UWorld`, created and destroyed with that world.
2. **Why it exists.** World-level services without placing a manager actor in every level.
3. **How it works.**
   - The world creates every subsystem class for which `ShouldCreateSubsystem` and `DoesSupportWorldType` are true.
   - Order: `Initialize` → `PostInitialize` → `OnWorldBeginPlay` (before actors' `BeginPlay`) … `Deinitialize` (after actors' `EndPlay`) (`01b` §3).
   - The tickable variant is also an `FTickableGameObject`. Its `Tick` runs **after `TG_PostPhysics`**, and it can't pick a tick group.
4. **When to use it.** World-wide registries, directors and managers whose lifetime should equal the world.
5. **When not to.** Cross-level state (use a `UGameInstanceSubsystem`). Work that must run in a specific tick group (use your own `FTickFunction`, as S5 will).
6. **Common mistakes.**
   - Forgetting `DoesSupportWorldType`, so the subsystem also runs in editor worlds.
   - Fetching another plugin's object in `Initialize` before it exists (the Significance Manager trap, `01b #10`).
   - Not calling `Super::Initialize`/`Super::Deinitialize` in a tickable subsystem, which breaks `IsAllowedToTick`.
7. **Example.** `UCLASS() class UMySubsystem : public UTickableWorldSubsystem { … virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMySubsystem, STATGROUP_Tickables); } };`
8. **In this project.** `UUpdateBudgetSubsystem` is tickable, to publish complete per-frame telemetry. `UUBBenchmarkSubsystem` is plain and spawns the workload in `OnWorldBeginPlay`. Both are Game/PIE only.

## 7. `UCLASS` / `UENUM` / `UPROPERTY` / `GENERATED_BODY`

1. **What it is.** Markers that the Unreal Header Tool reads to generate reflection data: type info, property lists and GC reference maps.
2. **Why it exists.** C++ has no runtime reflection. Unreal's editor, GC, serialization, Blueprint and replication all need to know a class's properties at runtime.
3. **How it works.** UHT parses the macros and writes `*.generated.h`/`.gen.cpp`. `GENERATED_BODY()` pastes the generated members into the class.
4. **When to use it.**
   - `UPROPERTY` for any UObject reference you hold (GC safety), and for anything to edit, save or replicate.
   - `UCLASS` for every UObject-derived class.
5. **When not to.** Plain hot-path data (e.g. our future `FBudgetClientState`) needs no reflection, and leaving it out keeps it a cheap plain struct.
6. **Common mistakes.**
   - A raw `UObject*` member without `UPROPERTY` that becomes dangling after GC.
   - Over-exposing with `BlueprintReadWrite`.
   - The `.generated.h` include not being last.
7. **Example.** `UPROPERTY(EditAnywhere, Category = "Workload", meta = (ClampMin = "0")) float PatrolRadius = 300.f;`
8. **In this project.**
   - `UCLASS(MinimalAPI, Abstract)` on the base component: type info is exported, members are exported one by one, and it can't be added directly.
   - `UPROPERTY(Transient)` on runtime links: GC-visible, never saved.
   - `EditAnywhere` + `ClampMin` on workload tuning.
   - `UENUM()` on `EUBBrainState`.
   - No `BlueprintReadWrite` anywhere yet, because nothing needs it (C++ vs Blueprint split, 02 §10).

## 8. `TObjectPtr` and UPROPERTY references

1. **What it is.** UE5's wrapper for UObject member pointers.
2. **Why it exists.** It enables editor access tracking and lazy loading in editor builds, while in packaged builds it compiles to a raw pointer.
3. **How it works.**
   - A `UPROPERTY` `TObjectPtr` is a strong reference GC can see: the target stays alive while referenced.
   - If the target is marked as garbage, GC sets the pointer to null when it runs (`GarbageCollection.cpp:2984`). Until then the pointer is non-null but points to garbage, so check `IsValid()` (`01b #8`).
4. **When to use it.** UObject *members* in UCLASS/USTRUCT types.
5. **When not to.** Function parameters and locals (use raw pointers). Non-owning observers of objects you must not keep alive (use `TWeakObjectPtr`).
6. **Common mistakes.**
   - Assuming GC nulls the pointer immediately.
   - Dereferencing a `TObjectPtr` repeatedly in a hot loop in editor builds (each access adds handle tracking); copy it to a raw local once.
7. **Example.** `if (UUpdateBudgetSubsystem* Current = Subsystem) { Current->RecordUpdate(*this, Previous); }`
8. **In this project.** The `Clients` array, the component's `Subsystem` link and `Agents`. Every access tolerates null, and hot paths read the pointer once.

## 9. `TArray`: `Add`, `RemoveAtSwap`, `IsValidIndex`, `Reserve`

1. **What it is.** Unreal's dynamic array: contiguous memory, like `std::vector`.
2. **Why it exists.** It is cache-friendly storage that works with the reflection system.
3. **How it works.**
   - `Add` appends and returns the index.
   - `RemoveAtSwap(i, EAllowShrinking::No)` moves the last element into slot *i*: O(1), and order is lost.
   - `Reserve(n)` pre-allocates.
   - `IsValidIndex(i)` bounds-checks.
4. **When to use which.** `RemoveAtSwap` when order doesn't matter. `Reserve` when the final size is known.
5. **When not to.** `RemoveAtSwap` when order matters (use `RemoveAt`, which is O(N)).
6. **Common mistakes.**
   - Forgetting to fix the stored index of the element that moved.
   - Removing while iterating forward.
   - Letting shrinking reallocate repeatedly.
7. **Example.** `Clients.RemoveAtSwap(Index, EAllowShrinking::No); if (Clients.IsValidIndex(Index) && Clients[Index]) { Clients[Index]->RegistryIndex = Index; }`
8. **In this project.** The registry (`RemoveClientAt`) and `Agents.Reserve(N)`.

## 10. CSV profiler: `CSV_DEFINE_CATEGORY`, `CSV_CUSTOM_STAT`

1. **What it is.** Macros that add our own columns to the CSV profiler's per-frame capture (`Core/Public/ProfilingDebugging/CsvProfiler.h`).
2. **Why it exists.** It puts project metrics in the same file and frame rows as the engine's `FrameTime`/`GameThreadTime`, so they can be compared frame by frame.
3. **How it works.**
   - `CSV_DEFINE_CATEGORY(UpdateBudget, true)` creates the category, enabled by default.
   - `CSV_CUSTOM_STAT(Category, Name, Value, Op)` records a value for the current frame. `Op` is `Set`, `Min`, `Max` or `Accumulate`, and the value can be `float`, `double` or `int32`.
   - Columns appear as `UpdateBudget/<Name>`.
   - It is compiled in Development and Test, not Shipping.
4. **When to use it.** Per-frame metrics you want next to engine timings.
5. **When not to.** Per-item recording in hot loops. On this machine the profiler measured **about 50 ns per custom stat**; aggregate in plain variables first.
6. **Common mistakes.**
   - Expecting CSV files from a Shipping build.
   - Reading the last two rows as data (they are a repeated header and metadata).
   - Assuming `-nullrhi` fills `GameThreadTime`. It doesn't: it stays 0 (S1 finding).
7. **Example.** `CSV_CUSTOM_STAT(UpdateBudget, UpdatesRun, FrameUpdates, ECsvCustomStatOp::Set);`
8. **In this project.** `Clients`, `UpdatesRun` and `MaxGapMs`, published once per frame from the subsystem's post-physics `Tick`.

## 11. `SpawnActorDeferred` / `FinishSpawning`

1. **What it is.** A two-step spawn (`World.h:3851`; `Actor.h:3117`).
2. **Why it exists.** To configure an actor *after* construction but *before* `BeginPlay`.
3. **How it works.** `SpawnActorDeferred` constructs the actor and stops. You set data. `FinishSpawning(Transform)` runs the construction script, registers components and calls `BeginPlay`.
4. **When to use it.** When `BeginPlay` logic depends on data the spawner provides.
5. **When not to.** When nothing needs setting first (plain `SpawnActor` is simpler).
6. **Common mistakes.**
   - Forgetting `FinishSpawning`, which leaves a half-built actor.
   - Not checking for a null return.
7. **Example.** `A* X = World.SpawnActorDeferred<A>(A::StaticClass(), FTransform(P)); X->Init(…); X->FinishSpawning(FTransform(P));`
8. **In this project.** Every agent's patrol is set before `BeginPlay`.

## 12. `CreateDefaultSubobject` / `ConstructorHelpers::FObjectFinder`

1. **What it is.** Constructor-only helpers: one creates a component every instance owns, the other finds an asset by path.
2. **Why it exists.** Default components are part of the class definition (the CDO), and the asset lookup happens once per class.
3. **How it works.** `CreateDefaultSubobject<T>(Name)` creates a subobject template that is copied to every instance. `static FObjectFinder<T>` resolves the asset path once; the `static` makes it run once per class.
4. **When to use it.** C++ actor composition, and default asset references.
5. **When not to.** Outside constructors (both assert). For assets the user should pick (use an `EditAnywhere` `TSoftObjectPtr`).
6. **Common mistakes.**
   - Duplicate subobject names.
   - A hard asset path that the cooker never sees, so the asset is missing in packaged builds. We fix this with `DirectoriesToAlwaysCook`.
7. **Example.** `static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));`
8. **In this project.** `AUBDemoAgent` gets a mesh and a brain, and uses the engine cube.

## 13. `FRandomStream`, `FParse::Value` / `FCommandLine`, `FCrc::TypeCrc32`

| API | What / why | In this project |
|---|---|---|
| `FRandomStream(Seed)`, `FRandRange(min, max)` | A seeded, self-contained random sequence. Reproducible, and not shared with other code (unlike `FMath::Rand`) | Agent placement and phase |
| `FParse::Value(Stream, TEXT("Key="), Out)` + `FCommandLine::Get()` | Reads `-Key=value` from the command line. Overloads exist for `int32`, `float`, `double`, `FString`… (`Misc/Parse.h`). It leaves `Out` untouched if the key is absent | `-UBAgents=`, `-UBSeed=`, `-UBWork=` |
| `FCrc::TypeCrc32(Value, Crc)` | The CRC of one value, chained through the seed. **Not for hash tables**: Epic recommends xxhash there (`Crc.h:34`). Fine as a determinism fingerprint | Start-position hash |

## 14. Small utilities

| API | What it is | Why it is used here |
|---|---|---|
| `check(expr)` | Assert. It stops the program in debug/Development and is compiled out in Shipping | Null `Client` is a programming error |
| `ensure(expr)` | Logs a callstack once, returns the result and continues | A missing world in `RunUpdate` is worth reporting, not crashing a benchmark |
| `DEFINE_LOG_CATEGORY_STATIC(Name, Log, All)` + `UE_LOG` | A log category visible only in one `.cpp` | The harness's own `LogUBBench` filter |
| `RETURN_QUICK_DECLARE_CYCLE_STAT(Name, Group)` | Declares and returns a stat ID inline | Required `GetStatId()` of a tickable object |
| `UE_INLINE_GENERATED_CPP_BY_NAME(File)` | Includes UHT's `.gen.cpp` into this file | Fewer translation units, faster builds |

## 15. `CSV_EVENT` / `CSV_METADATA` and event-driven capture

1. **What it is.**
   - `CSV_EVENT(Category, TEXT("Name"))` writes a named event on the current frame's row, in the `EVENTS` column as `Category/Name##timestamp`.
   - `CSV_METADATA(Key, Value)` writes a key/value pair into the capture's metadata row (keys arrive lower-case).
2. **Why it exists.** To mark *moments* (start, wave, end) and *context* (run tag, N, seed) inside the same file as the frame data.
3. **How it works.**
   - While no capture is running, the profiler still checks event text against `-csvStartOnEvent=` (exact text without the category, case-insensitive, `CsvProfiler.cpp:4769`) and starts capturing on a match.
   - During a capture, `-csvStopOnEvent=` queues a stop that is handled at the end of that frame.
   - Both depend on `CSV_PROFILER_ALLOW_DEBUG_FEATURES`, which the Development build has (verified by the S2 runs).
4. **When to use it.** For benchmark windows the game itself controls: no fragile frame counting in scripts.
5. **When not to.** Not in Shipping (compiled out). Not for per-object logging.
6. **Common mistakes.**
   - Expecting the start event in the capture. It *triggers* the capture, so it isn't recorded.
   - Doing expensive work on the stop frame (it is still captured).
   - Assuming `-csvExitOnCompletion` exists (it doesn't).
7. **Example.** `CSV_EVENT(UBBench, TEXT("BenchStart"));` with `-csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd`
8. **In this project.** The director's `BenchStart`, `Wave` and `BenchEnd` events, plus `UBRunTag`, `UBAgents`, `UBSeed`, `UBWork` and `UBWaveAt` metadata.

## 16. Camera, files and exit: `ACameraActor` + `SetViewTarget`, `FFileHelper`, `FPaths`, `RequestExit`

| API | What / why | Common mistake | In this project |
|---|---|---|---|
| `World.SpawnActor<ACameraActor>()` + `APlayerController::SetViewTarget(Camera)` | A plain engine camera actor. The player controller renders from its view target | Setting the view target once, then losing it when the controller possesses its pawn | Re-applied each frame if it changed. The camera flies a fixed analytic path |
| `FFileHelper::SaveStringToFile(String, *Path)` | Writes a text file and returns success | Ignoring the return value | The run report. A failure is logged as an Error |
| `FPaths::ProjectSavedDir()` | The project's `Saved/` folder, which in a packaged build sits beside the executable's content | Hard-coding absolute paths | `Saved/UBRuns/<tag>.csv` |
| `FPlatformMisc::RequestExit(false, CallSite)` | Asks the engine to shut down normally at the end of the frame. `true` would force an immediate exit | Exiting on the same frame a CSV capture stops, before the file is written | Called 3 frames after BenchEnd |

---

## APIs added in step S3 (baseline drivers)

## 17. `USignificanceManager` (plugin `SignificanceManager`)

1. **What it is.** Epic's framework for scoring how much each object matters to the viewers, then letting the game react (`Plugins/Runtime/SignificanceManager/.../SignificanceManager.h`).
2. **Why it exists.** Every large game needs "spend effort where the player is looking". This gives one place to compute significance for many objects, in parallel.
3. **How it works.**
   - One manager per game world. It is created on `OnPreWorldInitialization`, which happens **after** world subsystems initialise (`World.cpp:2447` vs `2449`), and removed on `OnPostWorldCleanup`.
   - `RegisterObject(Object, Tag, SignificanceFunction, PostType, PostFunction)` stores an `FManagedObjectInfo`. The object pointer is raw, and the manager keeps it alive, so the game must unregister.
   - **The game calls `Update(TArrayView<const FTransform> Viewpoints)` itself** ("Each user … is expected to call the Update function", header lines 26–27). It runs the significance functions in `ParallelFor`, on worker threads. Post functions run concurrently (`Concurrent`) or afterwards on the calling thread (`Sequential`).
4. **When to use it.** Many objects whose behaviour should scale with relevance (LOD of logic, effects or sound).
5. **When not to.** When you need a **time budget**: it has none (`01b` §5). For a handful of objects, a direct distance check is simpler.
6. **Common mistakes.**
   - Fetching it in a subsystem's `Initialize` (it doesn't exist yet).
   - Doing game-thread-only work, such as changing tick intervals, in a `Concurrent` post function.
   - Never unregistering destroyed objects (a raw-pointer use-after-free).
   - Unregistering in `Deinitialize`, which is too late: the manager is already gone.
7. **Example.** `SM->RegisterObject(Obj, Tag, [](FManagedObjectInfo* I, const FTransform& V){ return …; }, EPostSignificanceType::Sequential, [](FManagedObjectInfo* I, float Old, float New, bool bFinal){ … });`
8. **In this project.** Baseline **B3**: a significance in [0, 1] from distance, and a `Sequential` post function that maps it to a `TickInterval` band. It is fetched in `OnWorldBeginPlay`, unregistered per agent in `HandleAgentEndPlay` and globally in `OnWorldEndPlay`.

## 18. `SetComponentTickInterval` vs `SetComponentTickIntervalAndCooldown`

| Setter | What it does (`ActorComponent.cpp:1783–1791`, 5.8.4) | Consequence |
|---|---|---|
| `SetComponentTickInterval(f)` | Only writes `PrimaryComponentTick.TickInterval` | The new interval applies **after the current cooldown ends**. A far agent (0.5 s) that becomes near keeps waiting up to 0.5 s |
| `SetComponentTickIntervalAndCooldown(f)` | `UpdateTickIntervalAndCoolDown`: updates the reschedule entry, or removes the function from the cooling-down list and reschedules it (`TickTaskManager.cpp:2463–2477`) | Applies **at once**, at the cost of a list walk (H6). Once a tick function has had an interval, it stays on the cooldown path even at 0 |

**In this project:** both baselines use the `…AndCooldown` variant, and only when the band changes. That's what a team wanting responsive near agents would write.

## 19. `FSubsystemCollectionBase::InitializeDependency<T>()`

1. **What it is.** A call made inside a subsystem's `Initialize` that initialises another subsystem first (`Subsystems/SubsystemCollection.h:35–44`).
2. **Why it exists.** Subsystems that depend on each other need a guaranteed start order.
3. **How it works.** It initialises `T` immediately if it hasn't been already. For tickable world subsystems, **ticking is enabled in `Initialize`** (`SetTickableTickType`, `WorldSubsystem.cpp:94–101`), so initialisation order is also tick order.
4. **When to use it.** When one subsystem reads another's state every frame, or in its own setup.
5. **When not to.** For mutual dependencies (that's a design smell; merge them or use events).
6. **Common mistakes.** Calling it *after* `Super::Initialize`, which is too late to affect tick order.
7. **Example.** `void UMy::Initialize(FSubsystemCollectionBase& C) { C.InitializeDependency<UOther>(); Super::Initialize(C); }`
8. **In this project.** The baseline driver makes the benchmark subsystem tick first. Wave agents are then adopted on the frame they spawn, the same in every build.

## 20. Dynamic multicast delegates: `AActor::OnEndPlay` + `AddDynamic` + `UFUNCTION`

1. **What it is.** `OnEndPlay` is a *dynamic* (reflected, Blueprint-bindable) multicast delegate, `FActorEndPlaySignature (AActor*, EEndPlayReason::Type)` (`Actor.h:208, 2339`).
2. **Why it exists.** It lets other objects react when an actor leaves play, without the actor knowing about them (loose coupling).
3. **How it works.** `AddDynamic(this, &UClass::Handler)` binds by function **name** through reflection. That is why the handler must be a `UFUNCTION()`. Bindings to a garbage-collected listener are dropped safely.
4. **When to use it.** For cross-object notifications that Blueprint may also need, or when lifetime safety matters more than call speed.
5. **When not to.** In hot per-frame paths. Dynamic delegates are slower than native delegates (`DECLARE_MULTICAST_DELEGATE`) or a direct call.
6. **Common mistakes.**
   - Forgetting `UFUNCTION()` on the handler (it fails at runtime, or fails to compile).
   - Binding twice, which runs the handler twice.
   - Using it for per-frame work.
7. **Example.** `Agent->OnEndPlay.AddDynamic(this, &UUBBaselineDriverSubsystem::HandleAgentEndPlay);`
8. **In this project.** Agents that end play are removed from the baseline's list and unregistered from the Significance Manager. Each agent is bound once, at adoption.

## 21. Small utilities added in the S3 clean-up

| API | What it is (5.8.4) | Why it is used here | Common mistake |
|---|---|---|---|
| `FMath::GetRangePct(Min, Max, Value)` | `(Value − Min) / (Max − Min)`, returning 0 or 1 for an empty range (`Core/Public/Math/UnrealMathUtility.h:1054`) | The Mid-band fall-off of B3's significance, instead of a hand-written fraction | Forgetting that it does **not** clamp: values outside the range give results outside [0, 1] |
| `TNumericLimits<float>::Min()` | The smallest **positive** normal float, `FLT_MIN` (`Core/Public/Math/NumericLimits.h`). `Lowest()` is the most negative value | The lower clamp that keeps a Mid-band significance strictly above 0 | Reading `Min()` as "most negative", as in `std::numeric_limits<int>::min()`. For floats it is the smallest positive value |
| `APlayerCameraManager::GetCameraCacheView()` → `const FMinimalViewInfo&` | The camera's cached view of this frame: `Location`, `Rotation`, FOV and more. `GetCameraLocation()` / `GetCameraRotation()` just return its fields (`Engine/Private/PlayerCameraManager.cpp:774–782`) | One call builds the B2/B3 viewpoint | Expecting it to be valid before the camera manager has updated once (the first frame) |
| `TArray::RemoveSingleSwap(Item, EAllowShrinking::No)` | Finds the **first** match, moves the last element into its slot, returns the number removed (0 or 1) | B2 drops a brain that ended play; each brain is in the list once | Using it when order matters (swap changes it), or `RemoveSwap` when you know there is one match (that scans the whole array) |
| `MakeArrayView(Pointer, Count)` | A non-owning view over existing memory | Passes B3's single viewpoint to `USignificanceManager::Update(TArrayView<const FTransform>)` without building an array | Keeping the view after the memory it points to is gone (it owns nothing) |
| `ensureMsgf(expr, TEXT("…"), …)` | Like `ensure`, with a formatted message; logs a call stack the first time it fails and returns the result | Reports a broken "append-only" contract on the agent list instead of silently skipping agents | Putting side effects inside it, or using it for conditions that are expected at run time (use `if` and a log for those) |
| `FString::ToUpperInline()` | Upper-cases the string in place | The baseline name in the log and CSV metadata reads `B2`/`B3` whatever the command line said | Calling it only for comparisons: `FString ==` is already case-insensitive |
