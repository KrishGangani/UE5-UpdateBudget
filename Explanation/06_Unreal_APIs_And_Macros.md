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
   - The game module depended privately on `UpdateBudgetCore` in Phase 4. Since S1 the dependency is **public**, because the public header `UBDemoBrainComponent.h` derives from the plugin's component (05 A3). `SignificanceManager` is private (S3): only a `.cpp` includes it.
   - The plugin depends publicly on `Core`, `CoreUObject` and `Engine`, because its public headers expose engine types.
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
| 22 | `TFunctionRef` and `TConstArrayView` (S4) | `FUpdateBudgetScheduler::Run` |
| 23 | Automation tests: `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `EAutomationTestFlags`, `TestEqual`/`TestTrue`, `WITH_DEV_AUTOMATION_TESTS`, `Automation RunTests` (S4) | scheduler tests, `Tools\run_tests.bat` |
| 24 | Sorting and numeric safety: `TArray::Sort` with a predicate, `FMath::IsFinite`, `TArray::Init` / `SetNumUninitialized` (S4) | scheduler |
| 25 | A custom `FTickFunction`: USTRUCT + `WithCopy = false`, `RegisterTickFunction` / `UnRegisterTickFunction` / `SetTickFunctionEnable`, `ExecuteTick` (S5) | `FUpdateBudgetTickFunction` |
| 26 | Console variables: `TAutoConsoleVariable`, `ECVF_Default`, `GetValueOnGameThread`, and setting one before start-up with `-ini:` (S5) | `ub.Enabled` |
| 27 | `FPlatformTime::Cycles64` / `ToMilliseconds64` / `GetSecondsPerCycle64` (S5) | `RunClient` |
| 28 | Engine test helpers: `FTestWorldWrapper`, `FTestConsoleVariable` (S5) | lifetime tests |
| 29 | The component's tick and registration controls: `SetComponentTickEnabled`, `bStartWithTickEnabled`, `OnRegister` / `OnUnregister`, `Activate`, `ReregisterComponent`, `UnregisterAllComponents`, `RouteEndPlay` (S5) | `UBudgetedUpdateComponent`, lifetime tests |
| 30 | Small utilities in S5: `IsValid`, `TArray::AddDefaulted_GetRef` / `Reset`, `TWeakObjectPtr`, `FCoreUObjectDelegates::GetPostGarbageCollect`, `UCLASS(NotBlueprintable, HideDropdown)` | subsystem, tests |
| 31 | `UDeveloperSettings`: `config = Game`, `defaultconfig`, `UPROPERTY(config)`, `GetDefault` (S6) | `UUpdateBudgetSettings` |
| 32 | Stats and timing scopes: `DECLARE_STATS_GROUP`, `DECLARE_CYCLE_STAT`, `SCOPE_CYCLE_COUNTER`, `CSV_SCOPED_TIMING_STAT` (S6) | `RunFrame`, `UpdateDeadlines` |
| 33 | The camera's view: `GetCameraCacheView`, `FMinimalViewInfo` (`FOV`, `AspectRatio`) (S6) | significance |
| 34 | Small utilities in S6: `AddOnScreenDebugMessage`, `UE_CLOG`, `FString::SanitizeFloat`, `FMath::Lerp` / `GetRangePct` / `Tan`, `GetDefault` | subsystem, significance |

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
5. **When not to.** Cross-level state (use a `UGameInstanceSubsystem`). Work that must run in a specific tick group (use your own `FTickFunction`, as S5 does: §25).
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

---

## APIs added in step S4 (the scheduling policy)

## 22. `TFunctionRef` and `TConstArrayView`

1. **What they are.**
   - `TFunctionRef<R(Args...)>` is a non-owning reference to any callable: a lambda, a function or a functor (`Core/Public/Templates/Function.h`).
   - `TConstArrayView<T>` (`TArrayView<const T>`) is a non-owning pointer + count over contiguous elements (`Core/Public/Containers/ArrayView.h`).
2. **Why they exist.** Code can take "something to call" or "some elements to read" without copying or allocating, and without caring whether the caller holds a `TArray`, a C array or a slice.
3. **How they work.**
   - `TFunctionRef` stores a pointer to the callable and a pointer to a small invoker function: no heap, no copy.
   - `TArrayView` stores a pointer and a size.
   - Both are valid only while what they point to lives.
4. **When to use them.** As **parameters** for callbacks and read-only arrays that are used during the call and not kept.
5. **When not to.** To store either beyond the call. Store a `TFunction` (owning, may allocate) or a `TArray` instead.
6. **Common mistakes.**
   - Keeping a `TFunctionRef` to a temporary lambda in a member: it dangles.
   - Building a view over a `TArray` and then adding to the array: the view may point to freed memory after reallocation.
7. **Example.** `double Sum(TConstArrayView<double> V, TFunctionRef<double(double)> F) { double S = 0; for (double X : V) S += F(X); return S; }`
8. **In this project.** `FUpdateBudgetScheduler::Run(TConstArrayView<FBudgetClientState> Clients, …, TFunctionRef<double(int32)> RunClient)`.
   - The subsystem (S5) passes its own state array and a lambda that runs and times one client, and tests pass fake ones.
   - Nothing is copied per frame.

## 23. Automation tests: `IMPLEMENT_SIMPLE_AUTOMATION_TEST` and friends

1. **What it is.** Unreal's built-in test framework (`Core/Public/Misc/AutomationTest.h`). A macro declares and registers a test class; `RunTest` holds the body.
2. **Why it exists.** Engine and game code can be tested inside the real engine, from the editor's Session Frontend or headless from the command line, with results in the log and a JSON report.
3. **How it works.**
   - `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClass, "Dotted.Name", Flags)` defines `FClass` and a static instance that registers itself.
   - The flags must contain **an application context** (`EditorContext`, `ClientContext`, …) and **exactly one filter** (`SmokeFilter`, `EngineFilter`, `ProductFilter`, …). Both are checked by `static_assert` (`AutomationTest.h:4127–4133`).
   - `TestEqual(What, Actual, Expected[, Tolerance])` / `TestTrue` / `TestFalse` record an error and continue.
   - Tests exist only when `WITH_DEV_AUTOMATION_TESTS` is 1. UBT sets it for every configuration except Test and Shipping (`UnrealBuildTool/Configuration/UEBuildTarget.cs:6316`).
   - Headless: `UnrealEditor-Cmd <project> -ExecCmds="Automation RunTests <filter>;Quit"`.
     - `RunTests` is parsed at `AutomationController/Private/AutomationCommandline.cpp:610` and `Quit` at `:726`.
     - When the queue empties, a failure sets a non-zero exit code (`:495–504`).
4. **When to use it.** Logic that can run without a level: here, the whole scheduling policy. (CQTest and Functional Tests exist for richer setups.)
5. **When not to.** Timing claims. Automation tests check rules; performance is measured with the benchmark protocol.
6. **Common mistakes.**
   - No filter flag, or two filter flags (compile error).
   - Floating-point `TestEqual` without a tolerance.
   - Assuming tests run in Shipping (they're compiled out).
   - Putting `Quit` in a separate `-ExecCmds` entry: it must be inside the same `Automation` command so it waits for the tests.
7. **Example.** `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest, "Game.Math.Add", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter) bool FMyTest::RunTest(const FString&) { TestEqual(TEXT("1+1"), 1 + 1, 2); return true; }`
8. **In this project.** Eight tests under `UpdateBudget.Scheduler.*` (05 E3), run headless by `Tools\run_tests.bat` through the job queue.

## 24. Sorting and numeric safety in the scheduler

| API | What it is (5.8.4) | Why it is used here | Common mistake |
|---|---|---|---|
| `TArray::Sort(Predicate)` | An in-place introsort; **not stable** | One sort by deadline per frame (v1) | Expecting equal elements to keep their order. The scheduler breaks ties by index so the run order is deterministic |
| `FMath::IsFinite(double)` | True unless the value is NaN or ±infinity (`Core/Public/Microsoft/MicrosoftPlatformMath.h:18`, `_finite`) | A NaN measured cost would poison the moving average | Thinking `FMath::Max(0.0, NaN)` removes a NaN: it returns NaN, because every comparison with NaN is false |
| `TArray::Init(Value, Count)` | Resizes to `Count` copies of `Value` | One cost prediction per category | Confusing it with `Reserve`, which allocates without adding elements |
| `FMath::Lerp(A, B, Alpha)` | `A + Alpha * (B - A)` (`Core/Public/Math/UnrealMathUtility.h:1123–1126`) | The moving-average step, by its name | Reaching for `FMath::WeightedMovingAverage` (same file, line 2494): it computes `Cur·W + Prev·(1 − W)` and re-clamps the weight on every call, so its result can differ in the last bit |
| `TArrayView::GetData()` | A raw pointer to the first element | The sort comparator reads deadlines without the per-access range check | Indexing a raw pointer with an index that isn't guaranteed in range: the check is gone, so out-of-range is undefined behaviour. Here every index is 0..N−1 by construction |
| `TArray::SetNumUninitialized(N, EAllowShrinking::No)` | Sets the size without constructing or zeroing elements, and keeps the memory when shrinking | The sort buffer is fully rewritten every frame, so zeroing would be wasted work | Reading elements before writing them: they hold garbage |

---

## APIs added in step S5 (tick function, registry, budgeted path)

## 25. A custom `FTickFunction`

1. **What it is.** The engine's unit of per-frame work. Actors and components each own one (`PrimaryActorTick`, `PrimaryComponentTick`); any code can define its own by deriving from `FTickFunction` (`Engine/Classes/Engine/EngineBaseTypes.h:182–562`).
2. **Why it exists.** The tick task manager orders every tick function of a world by **tick group** (`TG_PrePhysics`, `TG_DuringPhysics`, …), prerequisites and priority, and runs it once per frame. A custom one lets a system run at a chosen point in the frame, which a tickable object can't do: `FTickableGameObject::Tick` always runs after `TG_PostPhysics` (`01b` §2).
3. **How it works.**
   - Override `ExecuteTick(float DeltaTime, ELevelTick, ENamedThreads::Type, const FGraphEventRef&)` (`:524`, pure virtual) and `DiagnosticMessage()` (`:532`, pure virtual); `DiagnosticContext(bool)` is optional (`:535`).
   - `RegisterTickFunction(ULevel*)` (`:341`) adds it to that level's lists; `UnRegisterTickFunction()` (`:349`) removes it; the destructor also unregisters (`TickTaskManager.cpp:2396–2399`).
   - Registration ignores `bCanEverTick` and `bStartWithTickEnabled` (`TickTaskManager.cpp:2406–2427`); the default state is **enabled** (`:2374`), so it ticks from the next frame.
   - `SetTickFunctionEnable(bool)` (`EngineBaseTypes.h:364`) moves a registered function between the enabled and disabled sets (`TickTaskManager.cpp:2439–2461`). Each frame the manager walks only the enabled set (`:1479–1485`), so a disabled function costs nothing per frame. On an unregistered function it only stores the state.
   - A function registered **during** a tick frame runs in the next tick group of the same frame (`EngineBaseTypes.h:334–341`).
   - The manager runs each function at most once per value of `GFrameCounter` (`TickTaskManager.cpp:1479–1485`, `2622–2631`).
   - The struct must never be copied: the manager keeps raw pointers to it. `FTickFunction` deletes its copy assignment (`EngineBaseTypes.h:550–551`), and a USTRUCT subclass declares `TStructOpsTypeTraits<…>::WithCopy = false` (`FActorTickFunction`: `:599–608`).
4. **When to use it.** A world-level system that must run in a specific tick group, once per frame, on the game thread: schedulers, batch managers (Mass and Niagara do this: `MassProcessingPhaseManager.cpp:485–486`, `NiagaraWorldManager.cpp:452–463`).
5. **When not to.** Per-actor work (use the actor's or component's own tick). Work that may run anywhere after physics (a tickable subsystem is simpler).
6. **Common mistakes.**
   - Registering in a subsystem's `Initialize`: it starts ticking at once, even in editor worlds (`01b #2`).
   - Forgetting to unregister before the owner goes, or copying the struct (dangling pointers in the tick manager).
   - Setting `bStartWithTickEnabled` and expecting it to matter for a hand-registered function. Only actor and component setup read it (`ActorComponent.cpp:1739–1750`).
   - Calling the owner's per-frame function from anywhere else as well: the work then runs twice.
7. **Example.**
   ```cpp
   USTRUCT() struct FMyTick : public FTickFunction
   {
       GENERATED_BODY()
       UMySubsystem* Owner = nullptr;
       virtual void ExecuteTick(float Dt, ELevelTick, ENamedThreads::Type, const FGraphEventRef&) override { Owner->Step(Dt); }
       virtual FString DiagnosticMessage() override { return TEXT("UMySubsystem::Step"); }
   };
   template<> struct TStructOpsTypeTraits<FMyTick> : TStructOpsTypeTraitsBase2<FMyTick> { enum { WithCopy = false }; };
   // OnWorldBeginPlay:  Tick.Owner = this; Tick.SetTickFunctionEnable(true); Tick.RegisterTickFunction(World.PersistentLevel);
   // Deinitialize:      Tick.UnRegisterTickFunction();
   ```
8. **In this project.** `FUpdateBudgetTickFunction` (05 F1) runs `UUpdateBudgetSubsystem::RunFrame` in `TG_PrePhysics`. It is registered in `OnWorldBeginPlay` in every mode and enabled only in budgeted mode, so in the native baselines it costs nothing.

## 26. Console variables: `TAutoConsoleVariable`, and setting one before start-up

1. **What it is.** A named runtime setting that can be changed from the console, config files and the command line. `TAutoConsoleVariable<T>` registers one when its static object is constructed (`Core/Public/HAL/IConsoleManager.h:1969–2028`).
2. **Why it exists.** Tuning and switching behaviour without rebuilding, and one place where every source of a value (code default, ini, device profile, console) is ranked by priority.
3. **How it works.**
   - `TAutoConsoleVariable<int32> CVar(TEXT("ub.Enabled"), 0, TEXT("help"), ECVF_Default);`
   - `GetValueOnGameThread()` returns the game thread's copy; there is a separate render-thread copy (`IConsoleManager.h:1911–1928`).
   - **Ini files:** the `[ConsoleVariables]` section of `Engine.ini` is applied at start-up (`Core/Private/Misc/ConfigCacheIni.cpp:6971`). A variable that doesn't exist yet, for example one a plugin registers later, gets a placeholder (`Core/Private/Misc/ConfigUtilities.cpp:419–425`), and its value is taken over when the real variable registers (`Core/Private/HAL/ConsoleManager.cpp:3328–3345`).
   - **Command line:** `-ini:Engine:[ConsoleVariables]:ub.Enabled=1` writes into that section before it is applied (`ConfigCacheIni.cpp:1715–1722`, `2317–2405`). This override exists in every build except Shipping (`ConfigCacheIni.h:57`).
   - `-ExecCmds="…"` is different: the commands are queued as deferred commands (`Engine/Private/UnrealEngine.cpp:2552`) and run during the engine tick (`:2772–2810`), after the first map has begun play.
   - Cheat variables (`ECVF_Cheat`) refuse values from ini files (`01b #11`).
4. **When to use it.** Switches and tuning values that a developer or a benchmark script changes at run time.
5. **When not to.** Designer-facing data that belongs in Project Settings (a `UDeveloperSettings` class, S6). Values read in a hot loop: read the cvar once per frame and keep the value.
6. **Common mistakes.**
   - Setting a value with `-ExecCmds` that the code reads at begin play: it arrives a frame too late, and nothing reports it.
   - Marking a benchmark switch `ECVF_Cheat`, so the ini route silently refuses it.
   - Reading `GetValueOnGameThread()` from a worker thread (it `ensure`s).
7. **Example.** `static TAutoConsoleVariable<int32> CVarMyMode(TEXT("my.Mode"), 0, TEXT("0 off, 1 on"), ECVF_Default);` then `const bool bOn = CVarMyMode.GetValueOnGameThread() != 0;` and, to start a run with it on: `MyGame.exe -ini:Engine:[ConsoleVariables]:my.Mode=1`.
8. **In this project.** `ub.Enabled` (05 F4) selects native or budgeted mode, read once per world in `OnWorldBeginPlay`. The UB benchmark configuration (S6) sets it with `-ini:`; B1 keeps using `-ExecCmds` for `tick.AllowBatchedTicks`, which the tick manager reads every frame, so the one-frame delay doesn't matter there.

## 27. `FPlatformTime::Cycles64` and friends

1. **What it is.** The platform's high-resolution timer. On Windows `Cycles64()` returns the raw `QueryPerformanceCounter` value (`Core/Public/Windows/WindowsPlatformTime.h:39–44`). `GetSecondsPerCycle64()` is its tick length, `ToMilliseconds64(Cycles)` converts a difference (`Core/Public/GenericPlatform/GenericPlatformTime.h:195–205`).
2. **Why it exists.** Measuring short intervals cheaply and portably, without floating-point drift.
3. **How it works.** Take two readings and subtract; the difference is an integer number of timer ticks. `FPlatformTime::Seconds()` reads the same counter but converts at once and adds a fixed offset of 16,777,216 s (`WindowsPlatformTime.h:22–29`, `:46–49`), which costs low-order precision in a difference.
4. **When to use it.** Timing one piece of code at run time, where the result feeds a decision (as here) or a custom statistic.
5. **When not to.** Profiling: use `TRACE_CPUPROFILER_EVENT_SCOPE`, cycle stats or CSV timing scopes, which the tools understand. Gameplay time: use `UWorld::GetTimeSeconds()` (paused and dilated correctly).
6. **Common mistakes.**
   - Assuming nanosecond resolution: the counter's frequency is whatever the OS reports, so the tick length should be checked (this project logs it).
   - Converting each reading to seconds before subtracting.
   - Timing something so short that the two readings cost more than the work.
7. **Example.** `const uint64 Start = FPlatformTime::Cycles64(); DoWork(); const double Ms = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Start);`
8. **In this project.** `RunClient` times each scheduled update (05 F4); the cost feeds the policy's moving average and the budget. The subsystem logs the timer's tick length once per world (09, S5).

## 28. Engine test helpers: `FTestWorldWrapper` and `FTestConsoleVariable`

1. **What they are.** Utilities in the Engine module for tests that need a real world or a temporary cvar value (`Engine/Public/Tests/AutomationCommon.h:30–72` and `:80–104`, exported with `ENGINE_API`, outside any `WITH_AUTOMATION_TESTS` guard).
2. **Why they exist.** Building a playable world by hand has many steps that are easy to get wrong; Epic's own tick tests use the wrapper (`Plugins/Tests/RuntimeTests/Source/RuntimeTests/Private/EngineRuntimeTests.cpp:159–162`).
3. **How they work.**
   - `CreateTestWorld(EWorldType::Game)` creates a game instance, the world (rooted, never ticked by the engine loop) and a world context (`AutomationCommon.cpp:59–115`).
   - `BeginPlayInTestWorld()` spawns the game mode (`SetGameMode`, `:169`), then `InitializeActorsForPlay` and `UWorld::BeginPlay` (`:185`). With a game mode, `GameMode->StartPlay()` dispatches every actor's `BeginPlay` (`World.cpp:6171–6174`, `GameStateBase.cpp:205–211`, `WorldSettings.cpp:363–379`). Without one, `UWorld::BeginPlay` still runs the subsystems' `OnWorldBeginPlay` (`World.cpp:6165–6169`) but no actor begins play.
   - `TickTestWorld(DeltaTime)` calls `UWorld::Tick(LEVELTICK_All, …)` and then advances `GFrameCounter` (`:220–235`), because tick functions run once per frame number (§25).
   - `EndPlayInTestWorld()` routes `EndPlay(Quit)` and restores the frame counter (`:199–218`); `DestroyTestWorld(bool bForceGarbageCollect)` shuts everything down and can run a full GC (`:117–147`); the destructor destroys the world without GC.
   - Failures are collected, not logged; `ForwardErrorMessages(Test)` turns them into test errors.
   - `FTestConsoleVariable(Name)`: `Set(Value)` remembers the original value once, and the destructor (or `Restore()`) puts it back (`AutomationCommon.cpp:274–339`). It keeps the variable's current priority.
4. **When to use them.** Automation tests that must exercise real actor lifetimes, ticking and subsystems.
5. **When not to.** Pure logic (test it without a world, as the S4 policy tests do). PIE behaviour: the wrapper doesn't support PIE worlds (`AutomationCommon.cpp:84–88`).
6. **Common mistakes.**
   - Ticking a hand-made world twice without advancing `GFrameCounter`: the second tick runs no tick functions.
   - Reading a destroyed object after a world tick: `UWorld::Tick` may collect garbage at its end (`LevelTick.cpp:1968–1971`).
   - Restoring a cvar before the world that depends on it is gone.
7. **Example.**
   ```cpp
   FTestConsoleVariable Mode(TEXT("my.Mode"));  Mode.Set(TEXT("1"));
   FTestWorldWrapper Wrapper;
   Wrapper.CreateTestWorld(EWorldType::Game);  Wrapper.BeginPlayInTestWorld();
   Wrapper.GetTestWorld()->SpawnActor<AMyActor>();
   Wrapper.TickTestWorld(1.f / 60.f);
   Wrapper.ForwardErrorMessages(this);
   ```
8. **In this project.** `FTestBudgetWorld` in the lifetime tests (05 F6) wraps both, with the cvar declared first so it is restored last.

## 29. The component's tick and registration controls

| API (5.8.4) | What it does | Why it matters here | Common mistake |
|---|---|---|---|
| `UActorComponent::SetComponentTickEnabled(bool)` (`ActorComponent.cpp:1755–1761`) | Enables or disables `PrimaryComponentTick` (if it can ever tick and isn't a template) | Budgeted mode turns the native tick off, so the native modes' dispatch cost isn't paid in UB | Expecting it to survive re-registration: see the next row |
| `FTickFunction::bStartWithTickEnabled` | Read whenever a component's tick function is registered: `SetTickFunctionEnable(bStartWithTickEnabled \|\| IsTickFunctionEnabled())` (`ActorComponent.cpp:1739–1750`, line 1747) | Budgeted mode clears it, or a re-registered component would tick natively again | Treating it as "initial state only": it is re-applied on every registration |
| `OnRegister()` / `OnUnregister()` (protected virtuals, `ActorComponent.h:830`, `835`) | Called when the component joins or leaves its world: on spawn, on re-registration, and (unregister) during streaming removal | The client's registry membership follows these, so it stops exactly when the native tick does | Doing play-time work in `OnRegister` without checking `HasBegunPlay()`: it also runs before `BeginPlay`, and in editor worlds |
| `Activate(bool bReset)` (`ActorComponent.cpp:2815–2824`) / `Deactivate()` (`:2826–2835`) | Turn the component's tick on or off and set its active flag | `Activate()` would bring back a scheduler-driven client's native tick; the `TickComponent` guard handles it. `Deactivate()` isn't mirrored by the scheduler (02 §12 item 8) | Assuming activation is only a flag: it changes ticking |
| `ReregisterComponent()` (`ActorComponent.cpp:2554–2566`) | Unregisters and registers again (through `FComponentReregisterContext`), without `EndPlay`/`BeginPlay` | Exercised by the `NoDoubleUpdate` test | Expecting `BeginPlay` to run again |
| `AActor::UnregisterAllComponents(bool bForReregister = false)` (`Actor.h:3250`) | Unregisters every component of the actor | What streaming removal calls first (`Level.cpp:2196`); the `LevelUnloadOrder` test calls it the same way | Assuming the actor has ended play afterwards: it hasn't |
| `AActor::RouteEndPlay(EEndPlayReason)` (`Actor.h:3381`, `Actor.cpp:3221–3257`) | Calls `EndPlay` once (if begun play), then uninitialises components | The second step of streaming removal, called with `RemovedFromWorld` in the test | Calling `EndPlay` directly, which skips the begun-play check and component uninitialisation |

## 30. Small utilities in S5

| API (5.8.4) | What it is | Why it is used here | Common mistake |
|---|---|---|---|
| `IsValid(const UObject*)` (`CoreUObject/Public/UObject/Object.h:1886`) | True if the pointer is non-null and the object isn't flagged as garbage or unreachable | The guard before each scheduled update, and the test for entries to remove after the loop | Testing only `!= nullptr`: a destroyed object stays non-null until GC runs (`01b #8`) |
| `TArray::AddDefaulted_GetRef()` (`Array.h:3067`) | Appends a default-constructed element and returns a reference to it | Adds a client's hot state and sets its `LastUpdateTime` in one step | Keeping the reference after another `Add`: the array may reallocate |
| `TArray::Reset(NewSize = 0)` (`Array.h:2469`) | Empties the array but keeps its allocation | `PendingAdds` is reused every frame without reallocating | Using `Empty()` in a per-frame path, which frees and later reallocates |
| `TWeakObjectPtr<T>` | A pointer that doesn't keep its object alive and reads as null once the object is garbage or gone | The teardown test proves the subsystem was collected, and reads only components still alive | Using it to keep an object alive (it doesn't), or a raw pointer where an object may die between frames |
| `FCoreUObjectDelegates::GetPostGarbageCollect()` + `AddUObject(this, &Class::Method)` → `FDelegateHandle`; `Remove(Handle)` (`CoreUObject/Public/UObject/UObjectGlobals.h:3490`) | A multicast delegate broadcast after every garbage collection (`GarbageCollection.cpp:5943`). `AddUObject` binds a member function and won't call it once the object is gone | The subsystem removes the entries GC nulled, in every mode (05 F4). `UWorld` and `UNetDriver` bind the same way (`World.cpp:1765`, `NetDriver.cpp:839`) | Forgetting to `Remove` the handle when the owner shuts down, or changing containers that another loop is reading from inside the callback (here: deferred if the scheduler is running) |
| `UCLASS(NotBlueprintable, HideDropdown)` | No Blueprint subclasses; hidden from class pickers | The test classes ship in the module but never appear in the editor's lists | Leaving test classes visible, so designers can pick them |

---

## APIs added in step S6 (significance, settings, telemetry)

## 31. `UDeveloperSettings`: Project Settings backed by an ini file

1. **What it is.** An abstract UObject base class (`Runtime/DeveloperSettings/Public/Engine/DeveloperSettings.h:21–23`) whose subclasses appear in the editor's Project or Editor Settings automatically.
2. **Why it exists.** A system needs designer-editable, version-controlled configuration without writing any settings UI.
3. **How it works.**
   - `UCLASS(config = Game, defaultconfig)`: `config = Game` names the ini file; `defaultconfig` makes the editor save into `Config/DefaultGame.ini`, the committed project file.
   - `UPROPERTY(config, EditAnywhere)` marks each saved field.
   - The class default object loads the values when the class is created, so `GetDefault<USettings>()` returns them at run time.
   - The container (Project or Editor) and category come from the config name; the section name defaults to the class name (`DeveloperSettings.cpp:18–76`).
   - **Arrays:** if the ini has any entries for an array property, the array is emptied and filled from the ini; if it has none, the C++ default stays (`CoreUObject/Private/UObject/Obj.cpp:3091–3095`).
   - Needs the `DeveloperSettings` module as a dependency.
4. **When to use it.** Per-project tuning that designers change and that belongs in source control.
5. **When not to.** Values that change every frame or per instance (use properties on the component). Per-user preferences (use `config = EditorPerProjectUserSettings`). `UDeveloperSettingsBackedByCVars` syncs cvars only in the editor (`01b #11`), so packaged runs would differ.
6. **Common mistakes.**
   - Forgetting `config` on the `UPROPERTY`, so the value is never saved.
   - Expecting `+Array=` lines in the ini to add to the C++ default: any entry replaces the default array.
   - Reading the settings in a hot loop through `GetDefault` every time instead of once.
7. **Example.** `UCLASS(config = Game, defaultconfig, meta = (DisplayName = "My System")) class UMySettings : public UDeveloperSettings { GENERATED_BODY() public: UPROPERTY(config, EditAnywhere, Category = "Budget") float BudgetMs = 2.f; };` and `const float Budget = GetDefault<UMySettings>()->BudgetMs;`
8. **In this project.** `UUpdateBudgetSettings` (05 G2): the budget and the significance categories, read once per world in `OnWorldBeginPlay`.

## 32. Stats and timing scopes: `DECLARE_STATS_GROUP`, `DECLARE_CYCLE_STAT`, `SCOPE_CYCLE_COUNTER`, `CSV_SCOPED_TIMING_STAT`

1. **What they are.** Two instrumentation systems. Cycle stats feed `stat <Group>` on screen and Unreal Insights (`Core/Public/Stats/Stats.h`, inside `#if STATS` at :31: `DECLARE_CYCLE_STAT` :130, `DECLARE_STATS_GROUP` :209, `SCOPE_CYCLE_COUNTER` :229; lighter or empty versions follow for builds without stats, :454–711). CSV timing stats write a column per scope into the CSV capture (`ProfilingDebugging/CsvProfiler.h`).
2. **Why they exist.** To time a scope in the build that is measured, with tools that already exist.
3. **How they work.**
   - Cycle stats: declare a group and a stat once per file, then `SCOPE_CYCLE_COUNTER(STAT_X)` times the enclosing scope. The stats system is compiled out of Test and Shipping (`01b` §4).
   - CSV: `CSV_SCOPED_TIMING_STAT(Category, Name)` records the scope's time per frame. The column is named `Category/<Thread>/Name`, here `UpdateBudget/GameThread/RunFrame`, because the profiler inserts the thread name (S3 finding). The CSV profiler's start-up benchmark put a timing stat at about 74 ns (S1).
4. **When to use them.** Once-per-frame or rare scopes whose cost must be visible: here the whole budgeted path and its significance loop.
5. **When not to.** Inside a per-update loop: thousands of scopes per frame would themselves cost measurable time (S1's reason for publishing once per frame).
6. **Common mistakes.**
   - Looking for `UpdateBudget/RunFrame` in the CSV: the thread name is part of the column.
   - Declaring the same stat in two files (a link error).
7. **Example.** `DECLARE_CYCLE_STAT(TEXT("Step"), STAT_MyStep, STATGROUP_Game); void Step() { SCOPE_CYCLE_COUNTER(STAT_MyStep); CSV_SCOPED_TIMING_STAT(MyCategory, Step); … }`
8. **In this project.** `RunFrame` and `Deadlines` in both systems (05 G4). `RunFrame` minus `UpdateMs` is the scheduler's own overhead.

## 33. The camera's view: `APlayerCameraManager::GetCameraCacheView`, `FMinimalViewInfo`

1. **What it is.** The player camera manager caches the final view of the last camera update: location, rotation, horizontal `FOV` in degrees and `AspectRatio` (`Engine/Classes/Camera/PlayerCameraManager.h:331`; `Engine/Classes/Camera/CameraTypes.h:48–50`, `:96–98`).
2. **Why it exists.** Gameplay and rendering need one agreed view per player without recomputing it.
3. **How it works.** `UWorld::Tick` updates every player's camera manager **after** all tick groups and tickable objects (`LevelTick.cpp:1821` then `:1828–1847`). During `TG_PrePhysics` the cache therefore holds last frame's view. `FOV` is horizontal; the vertical extent follows from the aspect ratio. `FMinimalViewInfo`'s default `AspectRatio` is 4:3 (`CameraTypes.h:179`).
4. **When to use it.** Relevance, significance and sensing code that needs "where is the player looking".
5. **When not to.** Split-screen or server code that needs several views (02 §11).
6. **Common mistakes.**
   - Expecting this frame's camera during a tick group: it is one frame old.
   - Testing visibility with a cone of half the **horizontal** FOV: on a wide screen the corners fall outside it (found by the S6 code review).
   - Normalising every direction for an angle test: compare squared dot products instead (04 §22).
7. **Example.** `if (const APlayerCameraManager* Camera = PC->PlayerCameraManager.Get()) { const FMinimalViewInfo& View = Camera->GetCameraCacheView(); const FVector Forward = View.Rotation.Vector(); … }`
8. **In this project.** `UUpdateBudgetSubsystem::UpdateDeadlines` builds an `FUpdateBudgetView` from it once per frame (05 G4), the same view the brains (B4) and the B2/B3 drivers (D1) use.

## 34. Small utilities in S6

| API (5.8.4) | What it is | Why it is used here | Common mistake |
|---|---|---|---|
| `UEngine::AddOnScreenDebugMessage(uint64 Key, float TimeToDisplay, FColor, const FString&, …)` (`Engine/Classes/Engine/Engine.h:2196–2197`) | Draws a line of text on screen. A key other than −1 replaces the previous message with the same key | `ub.Debug 1`: one line per world, keyed by the subsystem's `GetUniqueID()`, shown for one frame (time 0) | Key −1 every frame: the messages stack up |
| `UE_CLOG(Condition, Category, Verbosity, Format, …)` | `UE_LOG` that runs only when the condition is true | The begin-play warning for an inverted category | Putting work with side effects in the condition |
| `FString::SanitizeFloat(double)` | A float as text without trailing zeros | The `UBMaxStaleness` CSV metadata value | Using it where a fixed number of decimals matters |
| `FMath::Lerp`, `FMath::GetRangePct`, `FMath::DegreesToRadians`, `FMath::Tan` (`Core/Public/Math/UnrealMathUtility.h`) | Linear interpolation, the position of a value within a range, degree conversion, tangent | The significance falloff, the max staleness and the view cone (05 G1) | `GetRangePct` with Min = Max divides by zero; the category code never calls it then |
| `GetDefault<T>()` | The class default object of `T`; for a config class it holds the loaded ini values | Reading `UUpdateBudgetSettings` once per world | Modifying it at run time (it is shared by everything that reads it) |
