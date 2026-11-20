> **Raw Phase 2 research notes (C_framework_apis), UE 5.8.4, 2026-11-20.** Written by a research pass and kept as an evidence trail. Every claim cites a file:line or URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md) (marked ✔). Treat anything here that is not restated there as unverified.**

# P01 UpdateBudget: Phase 2 research, part C (framework APIs, UE 5.8.4 source)

Engine: `C:\Program Files\Epic Games\UE_5.8\Engine`. `Build/Build.version` says 5.8.4, CL 58902052, branch `++UE5+Release-5.8`.
All paths are relative to `Engine/`. **[inferred]** marks a conclusion drawn from code flow rather than read from a comment or doc.

---

## C1. `UWorldSubsystem` lifecycle

### Declared hooks (`Source/Runtime/Engine/Public/Subsystems/WorldSubsystem.h`, `Subsystem.h`)

| Hook | Where | Note |
|---|---|---|
| `ShouldCreateSubsystem(UObject* Outer) const` | Subsystem.h:56, WorldSubsystem.h:34 | Runs **on the CDO** before an instance exists (Subsystem.h:54: `Note: This function is called on the CDO prior to instances being created!`). The world version calls `DoesSupportWorldType`. |
| `Initialize(FSubsystemCollectionBase&)` | Subsystem.h:59 | |
| `PostInitialize()` | WorldSubsystem.h:37 | `/** Called once all UWorldSubsystems have been initialized */`. **Must call Super** (ensure at WorldSubsystem.cpp:48). |
| `PreDeinitialize()` | WorldSubsystem.h:40 | `/** Called once all UWorldSubsystems are about to be deinitialized */` |
| `OnWorldBeginPlay(UWorld&)` | WorldSubsystem.h:43 | **Must call Super** (ensure at WorldSubsystem.cpp:52). |
| `OnWorldEndPlay(UWorld&)` | WorldSubsystem.h:46 | `Called when world is ready to stop gameplay before the game mode transitions ... and call EndPlay on all actors` (but see the call site below: the comment is misleading) |
| `OnWorldComponentsUpdated(UWorld&)` | WorldSubsystem.h:49 | Called at the end of `UWorld::UpdateWorldComponents` (World.cpp:3013-3016). |
| `Deinitialize()` | Subsystem.h:62 | |
| `DoesSupportWorldType(EWorldType::Type) const` | WorldSubsystem.h:66 (protected) | |

**`UpdateStreamingState` is not a `UWorldSubsystem` virtual in 5.8.4.** The only streaming hook is the opt-in interface `IStreamingWorldSubsystemInterface::OnUpdateStreamingState()`:
- Declared at `Source/Runtime/Engine/Public/Streaming/StreamingWorldSubsystemInterface.h:21`: `virtual void OnUpdateStreamingState() { }`.
- Called from `UWorld::InternalUpdateStreamingState`, World.cpp:4893-4896: `SubsystemCollection.ForEachSubsystemWithInterface<UStreamingWorldSubsystemInterface>(... ->OnUpdateStreamingState(); });`.

### Default world types (WorldSubsystem.cpp:29-32)

```cpp
bool UWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::Editor || WorldType == EWorldType::PIE;
```

| World type | Default |
|---|---|
| Game | Yes |
| PIE | Yes |
| **Editor** | **Yes** |
| EditorPreview, GamePreview, GameRPC, Inactive, None | No |

The full enum is at `Source/Runtime/Engine/Classes/Engine/EngineTypes.h:1368-1395`.

> **Design consequence [inferred]:** by default our subsystem is also created in the **editor world**, where `Initialize` runs but `OnWorldBeginPlay` never does. Override `DoesSupportWorldType` to return Game || PIE only.

### Creation order (World.cpp)

1. `UWorld::InitWorld` → `InitializeSubsystems()` at World.cpp:2447. This runs `SubsystemCollection.Initialize(this)` (World.cpp:10589-10593), which calls `ShouldCreateSubsystem` on the CDO and then `Initialize` (SubsystemCollection.cpp:500, 526).
2. **Straight after:** `FWorldDelegates::OnPreWorldInitialization.Broadcast(this, IVS);` at World.cpp:2449.
3. Later in InitWorld: `PostInitializeSubsystems();` at World.cpp:2610. It calls `PostInitialize()` on every subsystem (World.cpp:10599-10603).
4. `UWorld::BeginPlay` → `OnWorldBeginPlay` (see below).
5. Late-created subsystems catch up. `FWorldSubsystemCollection::OnSubsystemAdded` (World.cpp:760-770) calls `PostInitialize` and `OnWorldBeginPlay` if the world is already past those points.

### `OnWorldBeginPlay` runs **before** actors' `BeginPlay`

World.cpp:6165-6174 (`UWorld::BeginPlay`):

```cpp
SubsystemCollection.ForEachSubsystem([this](UWorldSubsystem* WorldSubsystem)
{   WorldSubsystem->OnWorldBeginPlay(*this);
...
		GameMode->StartPlay();
```

The chain that reaches the actors:
- `AGameModeBase::StartPlay` → `GameState->HandleBeginPlay();` (GameModeBase.cpp:206).
- `AGameStateBase::HandleBeginPlay` → `GetWorldSettings()->NotifyBeginPlay();` (GameStateBase.cpp:209).
- `AWorldSettings::NotifyBeginPlay` → `for (FActorIterator It(World); It; ++It) ... It->DispatchBeginPlay(bFromLevelLoad);` (WorldSettings.cpp:370-374).

So **`OnWorldBeginPlay` runs strictly before every level-placed actor's `BeginPlay`**, and therefore before any component registers itself with us from `BeginPlay`.

### Teardown order and the `Deinitialize`-after-`EndPlay` guarantee

`UWorld::EndPlay` (World.cpp:6189-6219):
- It routes actor EndPlay **first**: `for (FActorIterator ActorIt(this); ActorIt; ++ActorIt) { ActorIt->RouteEndPlay(EndPlayReason); }` (6203-6206).
- **Then** it calls `WorldSubsystem->OnWorldEndPlay(*this);` (6210). The header comment at WorldSubsystem.h:45 ("before ... call EndPlay on all actors") does not match the code.
- Early-out: `if (!HasBegunPlay()) { return false; }` (6197-6201). A world that never began play gets no actor EndPlay and no `OnWorldEndPlay`.

`UWorld::CleanupWorldInternal` (World.cpp:6290-6491):
- `ClearWorldComponents();` (6361) unregisters all components, so `OnUnregister` runs.
- Then, when `bCleanupResources` is set: `PreDeinitialize()` (6476-6479), then `FWorldDelegates::OnPostWorldCleanup.Broadcast(...)` (6482), then `SubsystemCollection.Deinitialize();` (6486).

Call sites:

| Scenario | Sequence | Source |
|---|---|---|
| PIE end | `PlayWorld->EndPlay(EEndPlayReason::EndPlayInEditor);` → GameInstance Shutdown → `PlayWorld->CleanupWorld();` | `Source/Editor/UnrealEd/Private/PlayLevel.cpp:897, 912` |
| Map change (`LoadMap`) | the player's pawn and controller are **destroyed first** with `DestroyActor` (EndPlay reason `Destroyed`), then `WorldContext.World()->EndPlay(EEndPlayReason::LevelTransition);` → `WorldContext.World()->CleanupWorld();` | Runtime/Engine/Private/UnrealEngine.cpp:16247-16249, 16259, 16262 |
| Standalone quit | `World->EndPlay(EEndPlayReason::Quit);` → GameInstance Shutdown → `World->CleanupWorld();` | Runtime/Engine/Private/GameEngine.cpp:1373, 1380 |

**Conclusion [inferred]:** on PIE end, map change and quit, `Deinitialize` runs after every actor's and component's `EndPlay` (and after `OnUnregister`). The one exception is a world that never began play: it skips EndPlay entirely but still deinitializes.

`FSubsystemCollectionBase::Deinitialize` (SubsystemCollection.cpp:349-357) only calls `Deinitialize()` on subsystems that are still `IsValid`.

### Lifetime and GC of the subsystem

- World.cpp:1047: `This->SubsystemCollection.AddReferencedObjects(InThis, Collector);`.
- SubsystemCollection.cpp:400: `Collector.AddStableReferenceMap(SubsystemMap);`.
- So the world keeps the subsystem alive, and the subsystem's UPROPERTYs are traced by the GC.

### Bonus: where `UTickableWorldSubsystem::Tick` runs

`FTickableGameObject::TickObjects(this, TickType, bIsPaused, DeltaSeconds);` (Runtime/Engine/Private/LevelTick.cpp:1821). This is after `RunTickGroup(TG_PostPhysics)` (1778) and after `GetTimerManager().Tick` (1815), and before `RunTickGroup(TG_PostUpdateWork)` (1877).

The tickable subsystem is constructed with `ETickableTickType::Never`, switches to `Conditional` in `Initialize`, and goes back to `Never` in `Deinitialize` (WorldSubsystem.cpp:57-112).

---

## C2. `UActorComponent` lifecycle

### Key declarations (`Source/Runtime/Engine/Classes/Components/ActorComponent.h`)

| Function / flag | Line | Doc |
|---|---|---|
| `OnRegister()` | 830 | `Called when a component is registered, after Scene is set, but before CreateRenderState_Concurrent or OnCreatePhysicsState` |
| `OnUnregister()` | 835 | `Called after DestroyRenderState_Concurrent and OnDestroyPhysicsState are called.` |
| `InitializeComponent()` | 919 | `Requires component to be registered, and bWantsInitializeComponent to be true.` |
| `BeginPlay()` | 936 | `Called when the owning Actor begins play or when the component is created if the Actor has already begun play.` |
| `EndPlay(const EEndPlayReason::Type)` | 949 | `Called from AActor::EndPlay only if bHasBegunPlay is true` |
| `UninitializeComponent()` | 955 | `Called from AActor::EndPlay only if bHasBeenInitialized is true` |
| `TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)` | 976 | |
| `DestroyComponent(bool bPromoteChildren = false)` | 1328 | |
| `OnComponentCreated()` / `OnComponentDestroyed(bool bDestroyingHierarchy)` | 1331 / 1338 | |
| `uint8 bWantsInitializeComponent:1;` | 340 | `/** If true, we call the virtual InitializeComponent */`. The constructor never sets it, so it defaults to 0 **[inferred: zero-initialised UObject memory]** |
| `HasBegunPlay()` / `IsBeingDestroyed()` | 514 / 520 | |

### `PrimaryComponentTick` defaults (ActorComponent.cpp:548-553)

```cpp
PrimaryComponentTick.TickGroup = TG_DuringPhysics;
PrimaryComponentTick.bStartWithTickEnabled = true;
PrimaryComponentTick.bCanEverTick = false;
```

The constructor also sets `bAllowTickBatching = true`, `bRunTransactionally = true` and `SetTickFunctionEnable(false)`.

`ELevelTick` (EngineBaseTypes.h:69-79) has these values: `LEVELTICK_TimeOnly`, `LEVELTICK_ViewportsOnly`, `LEVELTICK_All`, and `LEVELTICK_PauseTick` ("Components don't tick").

### `EEndPlayReason` (EngineTypes.h:3670-3684)

`Destroyed`, `LevelTransition`, `EndPlayInEditor`, `RemovedFromWorld`, `Quit`. There are five values and no others.

### Startup order (spawn or level load)

1. **Spawn:** `AActor::PostSpawnInitialize` → `RegisterAllComponents()` (Actor.cpp:4336). Each component runs `RegisterComponentWithWorld` → `OnComponentCreated` if needed (ActorComponent.cpp:2026-2028) → `ExecuteRegisterEvents`, which triggers **OnRegister**.
2. `PostActorConstruction` → `PreInitializeComponents()` → `InitializeComponents()` (Actor.cpp:4440, 4449).
   - `InitializeComponents` activates components that have `bAutoActivate`.
   - It then calls `if (ActorComp->bWantsInitializeComponent && !ActorComp->HasBeenInitialized()) ActorComp->InitializeComponent();` (Actor.cpp:6403-6407).
3. `PostInitializeComponents()` → `DispatchBeginPlay()` runs if the world has begun play (Actor.cpp ≈4501-4529).
4. `AActor::BeginPlay` (Actor.cpp:4808-4848):
   - For each registered component: `Component->RegisterAllComponentTickFunctions(true); Component->BeginPlay();` (4824-4825). **Component BeginPlay happens before the actor's Blueprint `ReceiveBeginPlay()`** (4845).
   - Component tick functions in game worlds are registered **at BeginPlay, not at OnRegister**. ActorComponent.cpp:2035: `If not in a game world register ticks now, otherwise defer until BeginPlay.`

Order: **OnRegister → InitializeComponent → (ReadyForReplication) → BeginPlay**.

### Shutdown orders

**A. `UWorld::DestroyActor`** (Runtime/Engine/Private/LevelActor.cpp):
1. `ThisActor->Destroyed();` (926) → `RouteEndPlay(EEndPlayReason::Destroyed)` (Actor.cpp:3313).
2. `AActor::RouteEndPlay` → `EndPlay` (Actor.cpp:3229):
   - BP `ReceiveEndPlay` and `OnEndPlay.Broadcast` run first (3273-3274).
   - Then for each component that `HasBegunPlay()`: `Component->EndPlay(EndPlayReason)` (3279-3283).
   - Then `UninitializeComponents()` (Actor.cpp:3253-3254).
3. `ThisActor->UnregisterAllComponents();` (LevelActor.cpp:1052). This runs **OnUnregister**.
4. `ThisActor->MarkAsGarbage();` then `ThisActor->MarkComponentsAsGarbage();` (1055-1057).
   - For each component: `Component->OnComponentDestroyed(true); Component->MarkAsGarbage();` (Actor.cpp:6330-6331).

Order: **EndPlay(Destroyed) → UninitializeComponent → OnUnregister → OnComponentDestroyed(true) → garbage**.

**B. `UActorComponent::DestroyComponent`** (ActorComponent.cpp:2153-2205):
- `if (bHasBegunPlay) { EndPlay(EEndPlayReason::Destroyed); }`, then `if (bHasBeenInitialized) { UninitializeComponent(); }`.
- `if(IsRegistered()) { UnregisterComponent(); }` → `OnUnregister`.
- The component is removed from the owner's arrays, then `OnComponentDestroyed(false);` and `MarkAsGarbage();` (2201-2204, with the comment `// Finally mark pending kill, to NULL out any other refs`).
- Re-entrancy is guarded by `bIsBeingDestroyed` (2156-2161).

Order: **EndPlay(Destroyed) → UninitializeComponent → OnUnregister → OnComponentDestroyed(false) → garbage**.

**C. Level streaming unload (`UWorld::RemoveFromWorld`, World.cpp:4099+)**. The order is different here:
1. Components are unregistered first and incrementally: `Level->IncrementalUnregisterComponents(NumComponentsToUnregister, &Context)` (World.cpp:4199). That calls `Actor->UnregisterAllComponents(bForReregister)` (Level.cpp:2196), so **OnUnregister runs first**.
2. Only after that, `RouteActorEndPlayForRemoveFromWorld(Level, NumActorsToProcess)` (World.cpp:4250) → `Actor->RouteEndPlay(EEndPlayReason::RemovedFromWorld);` (Level.cpp:3939) → **EndPlay(RemovedFromWorld) → UninitializeComponent**.
3. Both steps are **time-sliced** (`GLevelStreamingComponentsUnregistrationGranularity`, `GLevelStreamingRouteActorEndPlayForRemoveFromWorldGranularity`, `IsTimeLimitExceeded`, World.cpp:4196-4207, 4247-4251). **[inferred]** So several frames can pass between a component's `OnUnregister` and its `EndPlay`.
4. No `OnComponentDestroyed` call is made. Later, `FLevelStreamingGCHelper::PrepareStreamedOutLevelForGC` marks every package object as garbage: `PackageObject->MarkAsGarbage();` (Runtime/Engine/Private/LevelStreamingGCHelper.cpp:202).

Order: **OnUnregister → (frames may pass) → EndPlay(RemovedFromWorld) → UninitializeComponent → garbage later**.

**D. World teardown (PIE end / LoadMap / Quit)**:
1. `UWorld::EndPlay` → every actor's `RouteEndPlay(reason)`, which runs **EndPlay(EndPlayInEditor | LevelTransition | Quit) → UninitializeComponent**.
2. `OnWorldEndPlay` runs.
3. `CleanupWorld` → `ClearWorldComponents()` → `Level->ClearLevelComponents()` → `Actor->UnregisterAllComponents()` (World.cpp:2922-2930; Level.cpp:1606). This runs **OnUnregister**.
4. Subsystems get `PreDeinitialize`, then `Deinitialize`.
5. World objects are marked garbage through `UWorld::MarkObjectsPendingKill()` (World.cpp:2804-2814). It is called from LoadMap (UnrealEngine.cpp:16282, 16291) and PIE teardown (PlayLevel.cpp:452, 461, 471).

No `OnComponentDestroyed` is called on this path **[inferred: not in the path]**.

Order: **EndPlay → UninitializeComponent → OnWorldEndPlay → OnUnregister → PreDeinitialize → Deinitialize**.

### Adding and destroying components at runtime, during another component's update

- **Add at runtime:** `NewObject` + `RegisterComponent()` → `RegisterComponentWithWorld` → `MyOwner->HandleRegisterComponentWithWorld(this)` (ActorComponent.cpp:2051).
  - If the owner has begun play: `InitializeComponent()` if wanted (Actor.cpp:6431-6433), then `RegisterAllComponentTickFunctions(true)` and `Component->BeginPlay();` (Actor.cpp:6443-6451).
  - So **BeginPlay runs synchronously inside `RegisterComponent()`**.
- **Destroy during another client's update:** `DestroyComponent` runs `EndPlay(Destroyed)` **synchronously** (ActorComponent.cpp:2163-2166), then marks the component garbage. The pointer stays non-null until the next GC, but `IsValid()` returns false right away (see C6).
- **Design consequence [inferred]:** if components register and unregister with the subsystem from `BeginPlay`/`EndPlay`, then gameplay code running inside a client update can add or remove registry entries **while the subsystem is iterating**. The registry needs one of these:
  - deferred add/remove queues applied between passes, or
  - index iteration that tolerates nulls/invalid entries plus swap-removal after the pass.
- Because of the streaming order above, the subsystem should also treat `!IsRegistered()` or `!IsValid()` entries as skip-and-remove.

---

## C3. `USignificanceManager` (`Plugins/Runtime/SignificanceManager`)

The plugin has `EnabledByDefault: false`, module `SignificanceManager`, Runtime, LoadingPhase `PreDefault` (`SignificanceManager.uplugin`). Its files are `SignificanceManager.h`, `OrderedBudget.h` and `SignificanceManager.cpp`. The game module must depend on `SignificanceManager` **[inferred: standard linking]**.

### Getting the instance; who creates it

- Static `Get`, SignificanceManager.h:148-155:
  ```cpp
  static UE_API USignificanceManager* Get(const UWorld* World);
  template<class T> static T* Get(const UWorld* World)
  { return CastChecked<T>(Get(World), ECastCheckedType::NullAllowed); }
  ```
- Backed by a module-owned map: `static TMap<const UWorld*, TObjectPtr<USignificanceManager>> WorldSignificanceManagers;` (SignificanceManager.cpp:57). The module is an `FGCObject` that adds references to the map (cpp:111-120).
- Creation: `FWorldDelegates::OnPreWorldInitialization.AddStatic(&FSignificanceManagerModule::OnWorldInit);` (cpp:99). In `OnWorldInit`, `if (World->IsGameWorld())` (cpp:124) holds for Game, PIE, GamePreview and GameRPC (World.cpp:9315).
- **The class is chosen by config:** `SignificanceManagerClass = LoadClass<USignificanceManager>(nullptr, *GetDefault<USignificanceManager>()->SignificanceManagerClassName.ToString());` (cpp:128). Then `WorldSignificanceManagers.Add(World, NewObject<USignificanceManager>(World, SignificanceManagerClass));` (cpp:136).
  - The property is `UPROPERTY(globalconfig, noclear, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="/Script/SignificanceManager.SignificanceManager", ...)) FSoftClassPath SignificanceManagerClassName;` (h:205-206), on `UCLASS(MinimalAPI, config=Engine, defaultconfig)` (h:29).
  - Default: `SignificanceManagerClassName = FSoftClassPath(GetClass());` (cpp:176), so the base class is created when nothing is configured.
  - To use a subclass, set `[/Script/SignificanceManager.SignificanceManager] SignificanceManagerClassName=...` in DefaultEngine.ini **[inferred from config=Engine + globalconfig]**.
- Server/client gating: `bCreateOnClient`/`bCreateOnServer` are both true by default (cpp:178-179, checked at 134).
- Destruction: `FWorldDelegates::OnPostWorldCleanup` → `WorldSignificanceManagers.Remove(World);` (cpp:100, 144).

> **Timing trap [inferred from World.cpp:2447 vs 2449 and cpp:99]:** `UWorld::InitWorld` calls `InitializeSubsystems()` (our `Initialize`) **before** it broadcasts `OnPreWorldInitialization`, and that broadcast is what creates the Significance Manager. So **`USignificanceManager::Get(World)` returns nullptr inside our `UWorldSubsystem::Initialize`**. Fetch it in `PostInitialize` or `OnWorldBeginPlay` instead.
>
> At teardown it is removed on `OnPostWorldCleanup` (World.cpp:6482), which happens **before** `SubsystemCollection.Deinitialize()` (6486). So `Get()` is also null in our `Deinitialize`. Unregister earlier (component EndPlay, or `OnWorldEndPlay`/`PreDeinitialize`).

### `RegisterObject` and the typedefs

SignificanceManager.h:35-41:
```cpp
typedef TFunction<float(UObject*, const FTransform&)> FSignificanceFunction;
typedef TFunction<void(UObject*, float, float, bool)> FPostSignificanceFunction;
typedef TFunction<float(FManagedObjectInfo*, const FTransform&)> FManagedObjectSignificanceFunction;
typedef TFunction<void(FManagedObjectInfo*, float, float, bool)> FManagedObjectPostSignificanceFunction;
```

`RegisterObject` uses the **`FManagedObject*`** variants, not the plain `FSignificanceFunction`/`FPostSignificanceFunction`, which are declared but not used by this API (h:121):
```cpp
UE_API virtual void RegisterObject(UObject* Object, FName Tag, FManagedObjectSignificanceFunction SignificanceFunction,
    EPostSignificanceType InPostSignificanceType = EPostSignificanceType::None, FManagedObjectPostSignificanceFunction InPostSignificanceFunction = nullptr);
```

Parameters:
- `Object`: the key, which must not already be registered (ensure plus delete at cpp:216-221).
- `Tag`: the bucket name, used for `GetManagedObjects(Tag)` and `UnregisterAll(Tag)`.
- `SignificanceFunction`: evaluated once per viewpoint. The max is kept by default; the min is kept if `bSortSignificanceAscending` (cpp:442-465).
- `InPostSignificanceType`: see below.
- `InPostSignificanceFunction`: an `ensure` fires if a function is passed with type `None`, or vice versa (h:77-85).

Other API: `UnregisterObject(UObject*)` (h:124), `UnregisterAll(FName)` (127), `GetSignificance(const UObject*)` (142, returns 0 when unmanaged), `QuerySignificance` (145) and `GetManagedObjects` (130/133).

The managed info stores a **raw `UObject*`** (h:96).

### `EPostSignificanceType` (h:44-52)

| Value | Comment | Behaviour (cpp) |
|---|---|---|
| `None` | "no post work" | |
| `Concurrent` | "can be done safely in parallel" | Called inside `UpdateSignificance`, i.e. **inside the ParallelFor on worker threads**: `if (PostSignificanceType == EPostSignificanceType::Concurrent) { PostSignificanceFunction(this, OldSignificance, Significance, false); }` (cpp:472-475) |
| `Sequential` | "must be done sequentially" | Called after the ParallelFor on the calling thread, in a loop (cpp:509-513) |

### Post-significance callback parameters `(FManagedObjectInfo*, float, float, bool)`

The typedef has no parameter names. Their meaning comes from the call sites:
- Update: `PostSignificanceFunction(ObjectInfo, SequentialPostWorkPair.OldSignificance, ObjectInfo->GetSignificance(), false);` (cpp:512). So the order is **(info, OldSignificance, NewSignificance, bFinal)**.
- Unregister: `ObjectInfo->PostSignificanceFunction(ObjectInfo, ObjectInfo->Significance, 1.0f, true);` (cpp:327, and the same in `UnregisterAll` at 353). So **`bFinal == true` on unregister, with New = 1.0f**.
- Register (Sequential only, and only if viewpoints are cached): `PostSignificanceFunction(ObjectInfo, 1.f, ObjectInfo->GetSignificance(), false);` (cpp:236).

### Who drives `Update`?

**The game does.** The plugin has no tick:
- Header comment, h:26-27: `Each user of the significance manager is expected to call the Update function from the appropriate location in the game code. GameViewportClient::Tick may often serve as a good place to do this.`
- The module's `StartupModule` only binds world-init/cleanup delegates, the HUD debug hook and console autocomplete (cpp:97-109).
- Signature: `UE_API virtual void Update(TArrayView<const FTransform> Viewpoints);` (h:118).

### Parallelism in `Update` (cpp:478-523)

- The registered list is copied first, so register/unregister during the update is safe for the iteration (cpp:494-497).
- Then: `ParallelFor(ObjArrayCopy.Num(), [&](int32 Index) { ... ObjectInfo->UpdateSignificance(Viewpoints,bSortSignificanceAscending); });` (cpp:499-507).
- **Significance functions and `Concurrent` post functions run on task-graph workers.** They must be thread-safe and must not touch game-thread-only UObject state **[inferred]**.
- After that: the Sequential post work runs, then every tag bucket is `StableSort`ed (cpp:518-521).
- Stats: the `STATGROUP_SignificanceManager` cycle stats include `STAT_SignificanceManager_Update` (cpp:66-73). Use them for baseline cost measurement.

### GC behaviour

`Collector.AllowEliminatingReferences(false);` around `AddReferencedObjects(ManagedObjects)` (cpp:287-292), with the comment `All managed objects should be removed from the significance manager before being marked for explicit destruction.`

**[inferred from GC code, see C6]** A managed object that is destroyed but never unregistered is **kept alive** (marked reachable through a non-killable reference) instead of being nulled. That is a leak, and its significance function keeps running on a garbage object.

### Usage examples in Engine/Plugins

- The only consumer outside the plugin is `Plugins/Developer/AnimationSharing/Source/AnimationSharing/Private/AnimationSharingManager.cpp`:
  - `SignificanceManager = USignificanceManager::Get<USignificanceManager>(World);` (967)
  - `ActorData.SignificanceValue = Data->SignificanceManager->GetSignificance(InActor);` (577)
- It **reads** significance only. **No `RegisterObject` or `Update` call exists anywhere in Engine/Plugins**, so there is no in-engine driver example.

### Bonus: `OrderedBudget.h`

`struct FOrderedBudget` (OrderedBudget.h:8) is a small helper that maps the "N-th closest item" index to a budget level from a spec string such as `"0,2,3,5"` (h:29-37). It is a count-based LOD budget, not a time (ms) budget. It is relevant prior art to cite in "existing solutions".

---

## C4. `UDeveloperSettings` (`Source/Runtime/DeveloperSettings`)

### Header (`Public/Engine/DeveloperSettings.h`)

`UCLASS(Abstract, MinimalAPI) class UDeveloperSettings : public UObject` (22-23).
- Virtuals: `GetContainerName()` (31), `GetCategoryName()` (33), `GetSectionName()` (35).
- `#if WITH_EDITOR` only: `GetSectionText`, `GetSectionDescription`, `SupportsAutoRegistration`, `DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSettingsChanged, UObject*, struct FPropertyChangedEvent&);`, `OnSettingChanged()` and `PostEditChangeProperty` (37-52).

### Defaults (`Private/Engine/DeveloperSettings.cpp`)

| Function | Lines | Behaviour |
|---|---|---|
| `GetContainerName` | 18-34 | `"Editor"` if `ClassConfigName` is `EditorSettings`/`EditorPerProjectUserSettings`, otherwise `"Project"` |
| `GetCategoryName` | 36-66 | `CategoryName` if set. Otherwise from the config name: Engine/Input → `Engine`, Editor* → `Editor`/`General`, **`Game` → `Game`**, else `Engine` |
| `GetSectionName` | 68-76 | `SectionName` if set, else `GetClass()->GetFName()` |

`PostEditChangeProperty` simply does `SettingsChangedDelegate.Broadcast(this, PropertyChangedEvent);` (89-92, editor-only). It does **not** call `Super::PostEditChangeProperty` **[observed; UObject's base does little, but note it]**.

### Registration and saving to `DefaultGame.ini`

- Auto-registration: `SettingsEditorModule.cpp:340-357` walks the CDOs of non-abstract `UDeveloperSettings` classes. `RegisterDeveloperSettings` reads `GetContainerName/GetCategoryName/GetSectionName` (`Source/Developer/SettingsEditor/Private/SettingsEditorModule.cpp:366-368`).
- Save: `Source/Developer/Settings/Private/SettingsSection.cpp:271-274`:
  ```cpp
  if (SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig))
  {   SettingsObject->TryUpdateDefaultConfigFile();
  ```
  - `UObject::TryUpdateDefaultConfigFile` (`Source/Runtime/CoreUObject/Public/UObject/Object.h:1306`): "Try to Saves just the section(s) for this class into the default ini file for the class (with just the changes from base)".
  - With `config=Game, defaultconfig`, values go to `Config/DefaultGame.ini` under `[/Script/<Module>.<ClassNameWithoutPrefix>]`. That naming is visible in `Config/BaseGame.ini`, e.g. line 73 `[/Script/EngineSettings.GeneralProjectSettings]` **[section-name convention inferred from those examples]**.

### Reading

`GetDefault<UMySettings>()`. The settings live on the CDO, which is loaded from config. Real example: `const UDataRegistrySettings* Settings = GetDefault<UDataRegistrySettings>();` (`Plugins/Runtime/DataRegistry/Source/DataRegistry/Private/DataRegistrySubsystem.cpp:265`).

### Real engine example (`config=Game, defaultconfig, meta=(DisplayName)`)

`Plugins/Runtime/DataRegistry/Source/DataRegistry/Public/DataRegistrySettings.h:16-17, 28-29, 43-45`:
```cpp
UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "Data Registry"))
class UDataRegistrySettings : public UDeveloperSettings
UPROPERTY(config, EditAnywhere, Category = "Data Registry") bool bInitializeAllLoadedRegistries = false;
```

It overrides `PostEditChangeProperty` under `#if WITH_EDITOR`. Other examples:
- `UAssetManagerSettings` (`Source/Runtime/Engine/Classes/Engine/AssetManagerSettings.h:66`)
- `UGameFeaturesSubsystemSettings` (`Plugins/Runtime/GameFeatures/.../GameFeaturesSubsystemSettings.h:11`)

### `UDeveloperSettingsBackedByCVars`: **exists in 5.8.4**

- Header: `Public/Engine/DeveloperSettingsBackedByCVars.h:19-20`, `UCLASS(Abstract, MinimalAPI) class UDeveloperSettingsBackedByCVars : public UDeveloperSettings`.
- `PostInitProperties`: `#if WITH_EDITOR if (IsTemplate()) { ImportConsoleVariableValues(); }` (cpp:12-22). It copies cvar values into properties that have `meta=(ConsoleVariable="name")`.
- `PostEditChangeProperty`: `ExportValuesToConsoleVariables(PropertyChangedEvent.Property);` (cpp:25-33).
  - That pushes the edited value into the cvar with `ECVF_SetByProjectSetting` (DeveloperSettings.cpp:138-193) and skips `ECVF_ReadOnly` cvars (146).
- In the editor, `SaveConfig`/`LoadConfig` use the cvar name as the ini key: `Key = CVarName; PortFlags |= PPF_ConsoleVariable;` (`Source/Runtime/CoreUObject/Private/UObject/Obj.cpp:3029-3034, 3769-3774`). This is inside `#if WITH_EDITOR`.

> **Surprise [inferred]:** all the cvar sync is `WITH_EDITOR` only. In a cooked or packaged build nothing applies that ini section to the cvars, unless the game calls `UE::ConfigUtilities::ApplyCVarSettingsFromIni(...)` itself.
>
> Epic's `UCQTestSettings` does exactly this (`Source/Developer/CQTest/Public/CQTestSettings.h:34-43`):
> `UE::ConfigUtilities::ApplyCVarSettingsFromIni(TEXT("/Script/CQTest.CQTestSettings"), *GEngineIni, ECVF_SetByProjectSetting);`
>
> The engine does it for its own sections in `LaunchEngineLoop.cpp:2817-2821`, e.g. `/Script/Engine.RendererSettings`.
>
> Signature: `CORE_API void ApplyCVarSettingsFromIni(const TCHAR* InSectionBaseName, const TCHAR* InIniFilename, uint32 SetBy, bool bAllowCheating=false, FName Tag=NAME_None);` (`Source/Runtime/Core/Public/Misc/ConfigUtilities.h:56`).

Another backed-by-cvars example: `UMoverDeveloperSettings` (`Plugins/Experimental/Mover/Source/Mover/Public/MoverDeveloperSettings.h:12-13`).

---

## C5. Console variables (`Source/Runtime/Core/Public/HAL/IConsoleManager.h`)

### `TAutoConsoleVariable<T>` vs `FAutoConsoleVariableRef`

**`TAutoConsoleVariable<T>`** (h:1969):
- The engine owns the storage. Reads go through `TConsoleVariableData<T>` with two shadow copies, `[0]` for the game thread and `[1]` for the render thread (h:1940-1941).
- Supported `T`: `bool`, `int32`, `float`, `FString`. Anything else hits `static_assert(sizeof(T) == 0, "Not supported");` (h:2002-2018).
- **Public constructors take the name first, flags last.** The flags-first constructors are private in 5.8 (they come before `public:` at h:1971-1993):
  ```cpp
  FORCEINLINE TAutoConsoleVariable(const TCHAR* Name, const T& DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default)
  FORCEINLINE TAutoConsoleVariable(const TCHAR* Name, const T& DefaultValue, const TCHAR* Help, const FConsoleVariableDelegate& Callback, uint32 Flags = ECVF_Default)
  ```
  (h:2027, 2044).
- There is also a getter/setter delegated form (h:2068) and `FCVarDeprecationArgs` overloads.
- The destructor clears the on-changed callback (h:2073-2076).

**`FAutoConsoleVariableRef`** (h:1706):
- `/** Autoregistering float, int, bool, FString REF variable class...this changes that value when the console variable is changed. */` (h:1703-1704).
- Binds to **your own** `int32&`/`float&`/`bool&`/`FString&`/`FName&` (FName has no callback overload).
- Public form: `FAutoConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default)` (h:1735).
- Reading is a plain C++ variable read, with no shadowing and no call **[inferred]**.
- Real engine example: `static FAutoConsoleVariableRef CVarSignificanceManagerObjectsToShow(TEXT("SigMan.ObjectsToShow"), GSignificanceManagerObjectsToShow, TEXT("..."), ECVF_Cheat);` (`Plugins/Runtime/SignificanceManager/.../SignificanceManager.cpp:526-531`).

### `ECVF_` flags that matter (h:63-187)

- `ECVF_Default = 0x0`.
- `ECVF_Cheat = 0x1`: "behave differently in a final release build. Then they are hidden in the console and cannot be changed by the user."
- `ECVF_ReadOnly = 0x4`: "cannot be changed by the user (from console). Changing from C++ or ini is still possible."
- `ECVF_RenderThreadSafe = 0x20`: "Maintains another shadow copy and updates the copy with render thread commands".
- `ECVF_Scalability = 0x40`: "should not be combined with ECVF_Cheat". Registration enforces `check(!(Obj->GetFlags() & ECVF_Cheat))` and not ReadOnly (ConsoleManager.cpp:3269-3275).
- `ECVF_SaveForNextBoot = 0x2000`.
- **SetBy priorities, weakest to strongest** (h:152-187): `SetByConstructor` < `SetByScalability` < `SetByGameSetting` < `SetByProjectSetting` < `SetBySystemSettingsIni` (the `[ConsoleVariables]` section) < `SetByPluginLowPriority` < `SetByDeviceProfile` < `SetByPluginHighPriority` < `SetByGameOverride` < `SetByConsoleVariablesIni` < `SetByHotfix` < `SetByPreview` < `SetByCommandline` < `SetByCode` < `SetByTemp` < `SetByConsole`.
  - Comment at h:152: "sorted in override order (weak to strong) ... It only affects the override behavior when calling Set()".

### Change callbacks

- `DECLARE_DELEGATE_OneParam( FConsoleVariableDelegate, IConsoleVariable* );` and `DECLARE_MULTICAST_DELEGATE_OneParam(FConsoleVariableMulticastDelegate, IConsoleVariable*);` (h:239, 242). Under `NO_CVARS` they become a null stub (h:383).
- `virtual void SetOnChangedCallback(const FConsoleVariableDelegate& Callback) = 0;` (h:687). The doc (h:678-686) says:
  - "Is even called if the value is the same as the value before. Will always be on the game thread."
  - "Setting other console variables in the delegate can cause infinite loops".
  - "you are not called on creation".
- `virtual FConsoleVariableMulticastDelegate& OnChangedDelegate() = 0;` (h:694).
- `ClearOnChangedCallback()` (h:692).

### `GetValueOnGameThread`

`TConsoleVariableData::GetValueOnGameThread` returns `ShadowedValue[0]` after `cvarCheckCode(ensure(GetShadowIndex() == 0));`, with the comment "compiled out in shipping" (h:1911-1917).

`TAutoConsoleVariable::GetValueOnGameThread` forwards to `Ref->GetValueOnGameThread()`, or falls back to a virtual `GetValue` (h:2078-2086).

**Do not call it from ParallelFor workers.** Use `GetValueOnAnyThread`, or read the value once on the game thread **[inferred]**.

### Setting a cvar from the command line

| Method | Mechanism | Priority | Build gating | Source |
|---|---|---|---|---|
| `-ExecCmds="upd.Foo 1, stat unit"` | Parsed in `UEngine::Init` into `GEngine->DeferredCommands` (comma-split, single quotes become double quotes), then run as console commands by `UEngine::TickDeferredCommands` on a later frame | console (`SetByConsole`) **[inferred: routed through console exec]** | `#if !(UE_BUILD_SHIPPING) \|\| ENABLE_PGO_PROFILE` | Runtime/Engine/Private/UnrealEngine.cpp:2543, 2552, 2772-2787; ParseExecCommands.cpp:11-83 |
| `-ini:Engine:[ConsoleVariables]:upd.Foo=1` | Command-line override of the Engine ini (format comment `-ini:IniName:[Section1]:Key=Value`). `[ConsoleVariables]` is applied by `LoadConsoleVariablesFromINI` | `ECVF_SetBySystemSettingsIni` | `ALLOW_INI_OVERRIDE_FROM_COMMANDLINE = (UE_SERVER \|\| !(UE_BUILD_SHIPPING))` | Core/Private/Misc/ConfigCacheIni.cpp:2317-2324, 6971; ConfigContext.cpp:1116-1118; Core/Public/Misc/ConfigCacheIni.h:57 |
| `-dpcvars=upd.Foo=1,upd.Bar=2` (also `-DPCVar=`, `-ForceDPCVars=`) | `DeviceProfileManager` applies them through `UE::ConfigUtilities::OnSetCVarFromIniEntry` | `SetByDeviceProfile`. `ForceDPCVars` uses `SetByCommandline`. A warning is logged if the existing priority is higher | (inside the DP manager) | Runtime/Engine/Private/DeviceProfiles/DeviceProfileManager.cpp:737-771 |
| `-cvarsini=<file>` | Applies the `[Startup]` section of that file with cheats allowed | `SetByConsoleVariablesIni` | `!DISABLE_CHEAT_CVARS && !UE_BUILD_SHIPPING` | ConfigCacheIni.cpp:6941-6968 |

### Do `ECVF_Cheat` cvars work in Test builds?

**Yes, by default.** `Source/Runtime/Core/Public/Misc/Build.h:435-440`:
```cpp
#define ALLOW_CHEAT_CVARS_IN_TEST 1
#define DISABLE_CHEAT_CVARS (UE_BUILD_SHIPPING || (UE_BUILD_TEST && !ALLOW_CHEAT_CVARS_IN_TEST))
```

When `DISABLE_CHEAT_CVARS` is set, `IsEnabled()` returns false for cheat cvars (IConsoleManager.h:468-473), and console input bails with `if (!CObj->IsEnabled()) return false;` (ConsoleManager.cpp:3122-3125). Both are `#ifndef`-guarded, so a Target.cs `GlobalDefinitions` entry can change them.

The comment at ConfigCacheIni.cpp:6949 ("not there for UE_BUILD_SHIPPING || UE_BUILD_TEST") is stale relative to `ALLOW_CHEAT_CVARS_IN_TEST=1` **[inferred]**.

> **Surprise: ECVF_Cheat cvars cannot be set from ini-style sources** (`[ConsoleVariables]`, `-ini:` overrides, `-dpcvars`):
> - `OnSetCVarFromIniEntry` has `bAllowCheating=false` by default (ConfigUtilities.h:43). It computes `bool bAllowChange = !bCheatFlag || bAllowCheating;` (ConfigUtilities.cpp:347) and otherwise fires `ensureMsgf(... "marked with ECVF_Cheat, this is only allowed in consolevariables.ini")` (ConfigUtilities.cpp:393-403).
> - If the cvar registers *after* the ini was read (as a game-module cvar may), the ini placeholder value is dropped for cheat cvars: `if(!Var->TestFlags(ECVF_Cheat) && !bScalabilityUpdate) { Var->Set(...) }` (ConsoleManager.cpp:3335-3339).
>
> **For benchmark-control cvars use `ECVF_Default` and set them with `-ExecCmds`**, or with `-ini:Engine:[ConsoleVariables]:...` for non-cheat cvars.

### Cvar deprecation (new mechanism)

`struct FCVarDeprecationArgs { const TCHAR* Version; EShadowCVarBehavior UsageBehavior = EShadowCVarBehavior::Ensure; const TCHAR* Message; }` (h:1691-1699). It is used as the first argument of overloaded `TAutoConsoleVariable`/`FAutoConsoleVariableRef` constructors. `FCVarDisableDeprecationWarningScope` is at h:1677. This deprecates the *cvar*, not C++ API. We don't need it.

---

## C6. GC and references

### Does GC see `UPROPERTY() TArray<TObjectPtr<UMyComponent>> Clients;` on a world subsystem?

Yes. The chain:
- World → `SubsystemCollection.AddReferencedObjects` (World.cpp:1047) → `Collector.AddStableReferenceMap(SubsystemMap);` (SubsystemCollection.cpp:400).
- The subsystem's class schema then traces its UPROPERTYs. A `TArray` of object pointers is the schema member type `ReferenceArray`: `case EMemberType::ReferenceArray: Dispatcher.HandleKillableArray(*(TArray<UObject*>*)MemberPtr, FMemberId(DebugIdx), Origin);` (`Source/Runtime/CoreUObject/Public/UObject/FastReferenceCollector.h:704`).
- This is a **strong** reference. A live, non-garbage component stays alive while it is in the array.

### Does GC null the entry after `DestroyComponent`/`MarkAsGarbage`? **Yes, at the next GC, by default in 5.8.4**

1. `MarkAsGarbage()` sets `RF_MirroredGarbage` and `GUObjectArray.IndexToObject(InternalIndex)->SetGarbage();` (`CoreUObject/Public/UObject/UObjectBaseUtility.h:207-215`). `DestroyComponent` calls it (ActorComponent.cpp:2204), and so do `MarkComponentsAsGarbage` (Actor.cpp:6331), `UWorld::MarkObjectsPendingKill` (World.cpp:2809) and streamed-out level GC prep (LevelStreamingGCHelper.cpp:202).
2. Garbage elimination is on by default:
   - `int32 GGarbageEliminationEnabled = UE_GARBAGE_ELIMINATION_ALLOWED;` with cvar `gc.GarbageEliminationEnabled` "If true, objects marked as Garbage will be automatically nulled and destroyed by Garbage Collector." (`CoreUObject/Private/UObject/ObjectBaseUtility.cpp:170-178`).
   - `UE_GARBAGE_ELIMINATION_ALLOWED` = `!UE_USE_COMPACT_SET_AS_DEFAULT || UE_ENABLE_COMPACTHASHTABLE_KEYSTORAGE` (cpp:168). Both defaults allow it: `UE_USE_COMPACT_SET_AS_DEFAULT 0` and `UE_ENABLE_COMPACTHASHTABLE_KEYSTORAGE 1` (`Core/Public/Containers/ContainerAllocationPolicies.h:1912, 1693`).
   - `BaseEngine.ini:1758-1759` leaves it commented: `; gc.GarbageEliminationEnabled=True`.
   - It can be switched off with `-DisableGarbageElimination` (ObjectBaseUtility.cpp:197-200).
3. GC selects the eliminating reachability pass: `(UObjectBaseUtility::IsGarbageEliminationEnabled() ? EGCOptions::EliminateGarbage : EGCOptions::None)` (GarbageCollection.cpp:5684).
4. A killable reference is nulled. In `CoreUObject/Private/UObject/GarbageCollection.cpp`:
   - `static constexpr EInternalObjectFlags KillFlag = EInternalObjectFlags::Garbage;` (3030).
   - `MayKill` → `(bAllowKill & (IsEliminatingGarbage(Options) || Origin == EOrigin::Blueprint)) ? EKillable::Yes : EKillable::No` (2994-2998).
   - Arrays: `HandleKillableArray` → `QueueArray(..., ProcessorType::MayKill(Origin, true))` (3226-3230) → `KillableBatcher.PushArray`.
   - Batched resolve: `else if (Metadata.Has(KillFlag)) { ... KillReference(*Reference.Mutable); }` (3089-3093).
   - `FORCEINLINE_DEBUGGABLE void KillReference(UObject*& Object) { Object = nullptr; }` (2984).

**Conclusions [inferred]:**
- The **array element becomes `nullptr`, but it is not removed**, so `Num()` is unchanged. The subsystem must compact nulls.
- Nulling only happens **when GC runs** (seconds later, `gc.TimeBetweenPurgingPendingKillObjects=61.1` in BaseEngine.ini:1749). Until then the pointer is non-null but points to a garbage object. **Check `IsValid()` on every access, or remove the entry in the component's `EndPlay`.**
- A reference that is **not** killable (for example the Significance Manager's `AllowEliminatingReferences(false)`) goes through `HandleValidReference` → `MarkAsReachableInterlocked_ForGC` (GarbageCollection.cpp:3121-3133). The garbage object is **kept alive** instead of nulled.

### `IsValid` / `IsValidChecked` (`CoreUObject/Public/UObject/Object.h`)

```cpp
UE_FORCEINLINE_HINT bool IsValid(const UObject *Test)
{	return Test && FInternalUObjectBaseUtilityIsValidFlagsChecker::CheckObjectValidBasedOnItsFlags(Test); }   // 1886-1889
inline bool IsValidChecked(const UObject* Test) { check(Test); return ...CheckObjectValidBasedOnItsFlags(Test); }  // 1897-1901
```

The check (Object.h:1856-1862): `return !ObjectItem->HasAnyFlags(EInternalObjectFlags::Garbage | EInternalObjectFlags::Unreachable);`. This is one `GUObjectArray.IndexToObject` lookup plus a flag test, and it is inline.

- `IsValidChecked` skips the null test but `check()`s it.
- `GetValid(const TObjectPtr<T>&)` returns `T*` or null (`ObjectPtr.h:1383-1388`).
- `IsValid(TObjectPtr)` works through TObjectPtr's implicit conversion to `T*` **[inferred]**.

### `TWeakObjectPtr::Get()` cost

- `FWeakObjectPtr::Get()` is `COREUOBJECT_API` (out-of-line, so a cross-DLL call in modular builds): `UObject* FWeakObjectPtr::Get() const { return Internal_Get(false); }` (`CoreUObject/Private/UObject/WeakObjectPtr.cpp:116-120`).
- `Internal_Get` → `Internal_GetObjectItem()`:
  - returns null if `ObjectSerialNumber == 0` or the index is negative;
  - otherwise does `GUObjectArray.IndexToObject(ObjectIndex)` plus a serial compare `ObjectItem->GetSerialNumber() == ObjectSerialNumber` (WeakObjectPtr.h:428-440, 478-495);
  - then calls `GUObjectArray.IsValid(ObjectItem, bEvenIfGarbage)`, which tests the `Unreachable | Garbage` flags (WeakObjectPtr.h:560-564; UObjectArray.h:1115-1122).
- The pointer itself is 8 bytes (`int32 ObjectIndex; int32 ObjectSerialNumber;`, WeakObjectPtr.h:567-573).
- **Cost [inferred]:** one non-inline call plus a chunked-array lookup into `GUObjectArray`, a serial-number compare and a flag test. Those are about two dependent loads into a global table that may miss cache for large client counts. It is cheap per call, but not free in a tight 10k-client loop. **Measure it rather than assume.**

---

## C7. Module API macro convention in 5.8

### The `UE_API` pattern

The pattern is the same in all of these headers:
- `Plugins/Runtime/SignificanceManager/Source/SignificanceManager/Public/SignificanceManager.h`: `#define UE_API SIGNIFICANCEMANAGER_API` (8), `UCLASS(MinimalAPI, config=Engine, defaultconfig)` + `class USignificanceManager : public UObject` (29-30), per-member `UE_API virtual void Update(...)` (118), `#undef UE_API` (212).
- `DataRegistrySettings.h:8, 16, 41, 49`.
- `MoverDeveloperSettings.h:9, 12, 17, 27`.
- `CQTestSettings.h:11, 28, 66, 69`.
- 1,520 headers under `Source/Runtime/Engine` + `Plugins/Runtime` contain `#define UE_API` (grep count).

**It is the prevailing Epic convention in 5.8 [inferred from prevalence]:**
- `UCLASS(MinimalAPI)` exports only the reflection boilerplate.
- Each non-inline member that other modules may call gets `UE_API`. `UE_API` is a local alias for `<MODULE>_API`, `#undef`'d at the end so it cannot leak into other headers in unity builds **[inferred]**.

No UBT code references `UE_API` (grep of `Source/Programs/UnrealBuildTool` found nothing). It is a plain preprocessor alias.

### What UHT expects

- UHT treats **any identifier ending in `_API`** as an API macro:
  - `token.Value.Span.EndsWith("_API", StringComparison.Ordinal)` (`Source/Programs/Shared/EpicGames.UHT/Tokenizer/UhtTokenReaderUtilityExtensions.cs:47`).
  - `class X_API UFoo` sets `classObj.ClassFlags |= EClassFlags.RequiredAPI;` (`Parsers/UhtClassParser.cs:127-130`).
  - So `class UE_API UFoo` would also be accepted **[inferred]**.
- `MinimalAPI`: `classObj.AddClassFlags(EClassFlags.MinimalAPI);` (`Specifiers/UhtClassSpecifiers.cs:103-106`). In generated code: `string api = classObj.ClassFlags.HasAnyFlags(EClassFlags.MinimalAPI) ? Module.Api : "NO_API ";` (`Exporters/CodeGen/UhtHeaderCodeGeneratorHFile.cs:833`).
- If the whole class is `RequiredAPI`, UHT does not add per-function `_API`, because "C++ will exports all methods automatically" (`Exporters/CodeGen/UhtHeaderCodeGenerator.cs:166-170`).

### UBT defines `<MODULE>_API`

- `ModuleApiDefine = Name.ToUpperInvariant() + "_API";` (`Source/Programs/UnrealBuildTool/Configuration/UEBuildModule.cs:239`).
- Possible values: `static readonly string[] ApiDefinitions = ["", "DLLEXPORT", "DLLIMPORT"];` (223).
- Selection logic (750-770): monolithic → `""` (unless compiled as a DLL with exports); a module compiled into a different binary → `DLLIMPORT` (index 2); its own binary → `DLLEXPORT` (index 1).

### Confirmation: plugin classes need `<MODULE>_API` to be linkable from the game module

In modular (editor) builds each module is a separate DLL. A non-inline member without `<MODULE>_API` is not exported, so calling it from another module gives an unresolved external at link time. Monolithic Game/Test builds would still link because the macro is empty there. So **mark everything another module calls**: either `class MYPLUGIN_API UFoo`, or `UCLASS(MinimalAPI)` plus per-member `MYPLUGIN_API`/`UE_API` **[inferred from the UBT values above; standard DLL semantics]**.

Example: `USignificanceManager::Get(const UWorld*)` is `static UE_API` (h:148), which is exactly why game code can call it. The inline template `Get<T>` needs no export (h:151-155).

**Recommendation for our project:** if UpdateBudget lives in the game module, `MYGAME_API` on the class is simplest. If it becomes a plugin, follow the 5.8 `UE_API` + `MinimalAPI` pattern.

---

## C8. Deprecations: `UE_DEPRECATED(5.7` / `UE_DEPRECATED(5.8` (searched unquoted and quoted versions)

| File:line | Deprecated | Replacement named | Relevant to us? |
|---|---|---|---|
| `Source/Runtime/Engine/Classes/Engine/EngineBaseTypes.h:466` (`FTickFunction`) | `double GetLastTickGameTime() const`, 5.8, "Renamed because function only returns the correct time for functions with a TickInterval" | `GetLastIntervalTickGameTime()` (h:464) | **Yes**, if we read tick timing |
| `EngineBaseTypes.h:1084` | `VMI_Lit_Wireframe` (EViewModeIndex, not FTickFunction), 5.8 | `VMI_Lit` + `FEngineShowFlags::SetMeshEdges` | No |
| `Source/Runtime/Engine/Classes/Components/ActorComponent.h:79` | `FRegisterComponentContext::SetAsyncRegisterLevelContext`, 5.8, "AsyncRegisterLevelContext has been removed." | none named | No |
| `ActorComponent.h:781` | `virtual void BeginReplication()`, 5.7 | override `OnReplicationStartedForIris` | No (not tick) |
| `ActorComponent.h:785` | `virtual void EndReplication()`, 5.7 | override `OnStopReplicationForIris` | No |
| `Source/Runtime/Engine/Classes/GameFramework/Actor.h`: 155, 1115, 1500, 2817, 2820, 3493, 3499, 3506 | `NumAlwaysVisibleComponents` (5.8), `DataLayers` (5.8), `GetDataLayerPropertyName` (5.8), `Set/GetBrowseToAssetOverride` (5.7), `BeginReplication`/`EndReplication`/`BeginReplication(Params)` (5.7) | `Has/Increment/DecrementAlwaysVisibleComponents`, `DataLayerAssets`, `UBrowseToAssetOverrideSubsystem`, `OnReplicationStartedForIris`/`OnStopReplicationForIris`/`FillReplicationParams` | **None are tick-related** |
| `Source/Runtime/Engine/Public/Subsystems/WorldSubsystem.h` | none | | |
| `Source/Runtime/Engine/Public/Subsystems/Subsystem.h` | none | | |
| `Source/Runtime/Engine/Public/Subsystems/SubsystemCollection.h:79` (extra file checked) | `FindAndPopulateSubsystemArrayInternal`, `UE_DEPRECATED("5.8", ...)` | `GetSubsystemArrayCopy` | Only if we enumerate subsystems |
| `Plugins/Runtime/SignificanceManager/.../SignificanceManager.h` | none | | |
| `Source/Runtime/DeveloperSettings/Public/Engine/DeveloperSettings.h` (and `DeveloperSettingsBackedByCVars.h`) | none | | |
| `Source/Runtime/Core/Public/HAL/IConsoleManager.h` | none (`UE_DEPRECATED`). There is a *cvar* deprecation mechanism, `FCVarDeprecationArgs` (h:1691) | | No |

Older deprecations seen nearby (for awareness): `Tickable.h:121` `UE_DEPRECATED(5.5, "Use IsTickable for conditional ticks and SetTickableTickType ...")` (this is `IsAllowedToTick`, also noted at WorldSubsystem.cpp:82).

---

## Summary of surprises

1. `UWorldSubsystem::UpdateStreamingState` does not exist in 5.8.4. Implement `IStreamingWorldSubsystemInterface::OnUpdateStreamingState` instead.
2. The default `DoesSupportWorldType` includes the **Editor** world.
3. `OnWorldEndPlay` runs **after** actor EndPlay, which contradicts its header comment.
4. The Significance Manager is created *after* our `Initialize` and destroyed *before* our `Deinitialize`.
5. Streaming unload runs `OnUnregister` **before** `EndPlay(RemovedFromWorld)`, time-sliced over frames.
6. A component's `BeginPlay`/`EndPlay` can fire synchronously in the middle of our update loop (runtime add/destroy).
7. GC nulls `TObjectPtr` array entries in place at the next GC. It does not remove them, and nothing happens before GC runs.
8. `UDeveloperSettingsBackedByCVars` syncs only in the editor. Packaged builds need `ApplyCVarSettingsFromIni`.
9. `ECVF_Cheat` cvars cannot be set from ini, `-ini:` or `-dpcvars`. They do work in Test builds by default (`ALLOW_CHEAT_CVARS_IN_TEST=1`).
10. Significance functions and `Concurrent` post callbacks run inside `ParallelFor` on worker threads.
11. `OrderedBudget.h` already offers a count-based LOD budget. This is prior art to cite.
