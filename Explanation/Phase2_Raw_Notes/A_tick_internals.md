> **Raw Phase 2 research notes (A_tick_internals), UE 5.8.4, 2026-11-20.** Written by a research pass and kept as an evidence trail. Every claim cites a file:line or URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md) (marked ✔). Treat anything here that is not restated there as unverified.**

# P01 UpdateBudget: Phase 2 research, part A: tick internals (UE 5.8.4)

- Engine: `C:\Program Files\Epic Games\UE_5.8\Engine`. `Build/Build.version` reads `"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 4, "Changelist": 58902052`.
- All paths below are relative to `Engine/`. Line numbers are from the 5.8.4 launcher source.
- **[inferred]** marks conclusions drawn from following the code flow, not from a direct statement in the source.
- Read-only research. No engine file was modified.

---

## A1. The custom tick function pattern

### The base contract (FTickFunction)

| Fact | Source |
|---|---|
| `FTickFunction` is a USTRUCT with copy disabled | `Source/Runtime/Engine/Classes/Engine/EngineBaseTypes.h:182-185` `USTRUCT() struct FTickFunction { GENERATED_BODY()` |
| Subclasses should disable copy | `EngineBaseTypes.h:550-551` `// It is unsafe to copy FTickFunctions and any subclasses of FTickFunction should specify the type trait WithCopy = false` / `FTickFunction& operator=(const FTickFunction&) = delete;` |
| Base traits | `EngineBaseTypes.h:555-560` `struct TStructOpsTypeTraits<FTickFunction> ... WithCopy = false, WithPureVirtual = true` |
| The manager holds raw pointers, so do not destroy a registered function mid-frame | `EngineBaseTypes.h:179-180` `The tick manager stores raw pointers to these structs ... so registered functions cannot be safely destroyed during a tick frame.` |
| Pure virtuals to override | `EngineBaseTypes.h:524` `virtual void ExecuteTick(...) PURE_VIRTUAL(,);` and `:532` `virtual FString DiagnosticMessage() PURE_VIRTUAL(...)` |
| Optional override | `EngineBaseTypes.h:535-538` `virtual FName DiagnosticContext(bool bDetailed) { return NAME_None; }` |
| Register/unregister API | `EngineBaseTypes.h:341` `ENGINE_API void RegisterTickFunction(class ULevel* Level);` and `:349` `ENGINE_API void UnRegisterTickFunction();` |
| UnRegister does not cancel a tick already queued this frame | `EngineBaseTypes.h:345-346` `This does not cancel ticks that have already been scheduled in the current tick frame.` |

USTRUCT is **not** required. Two of the examples below (Mass, AsyncMessage) use plain C++ structs and delete copying themselves. You need USTRUCT plus `WithCopy = false` only if the struct goes through reflection, for example as a UPROPERTY.

### What `RegisterTickFunction` actually checks

`Source/Runtime/Engine/Private/TickTaskManager.cpp:2406-2421`:
```cpp
if (!IsTickFunctionRegistered())
    if(bAllowTickOnDedicatedServer || !(World && World->IsNetMode(NM_DedicatedServer)))
        ... FTickTaskManager::Get().AddTickFunction(Level, this); InternalData->bRegistered = true;
```
- **`bCanEverTick` and `bStartWithTickEnabled` are never read by the tick manager.** A grep of `TickTaskManager.cpp` finds `bCanEverTick` only in the constructor (`:2365`) and in `AddPrerequisite` (`:2483-2484`), and `bStartWithTickEnabled` only in the constructor (`:2366`).
  - Those flags are conventions applied by the **Actor and Component** registration code: `Actor.cpp:1690-1694` and `ActorComponent.cpp:1741-1748`.
- The default `TickState` is `Enabled` (`TickTaskManager.cpp:2374` `, TickState(ETickState::Enabled)`). So a custom tick function that is registered without a call to `SetTickFunctionEnable(false)` **starts ticking immediately**, even when `bStartWithTickEnabled == false`. **[inferred]**
  - Evidence: `FTickAnimationSharingFunction` and the engine's own `FTestTickFunction` (`TickTaskManager.cpp:2858-2864`) never set `bCanEverTick`, yet both register and tick.
- The registration guard (`TickTaskManager.cpp:2205`): `check(TickFunction->TickGroup >= 0 && TickFunction->TickGroup < TG_NewlySpawned);`
- The level must be visible to tick (`TickTaskManager.cpp:2238`): `if (Level && Level->bIsVisible && Level->TickTaskLevel)`. That is why every example uses `PersistentLevel`.
- `bAllowTickOnDedicatedServer` is checked **only at registration** (`:2412`).

### What happens if the owner dies without unregistering

- **The destructor unregisters automatically. No assert or ensure fires.** `TickTaskManager.cpp:2396-2398`: `FTickFunction::~FTickFunction() { UnRegisterTickFunction(); }`
- If the **level** is destroyed first, `ULevel::~ULevel` frees the `FTickTaskLevel` (`Level.cpp:470-472`). Its destructor clears `bRegistered` on every function it holds (`TickTaskManager.cpp:1219-1238`, e.g. `(*It)->InternalData->bRegistered = false;`), so a later `UnRegisterTickFunction` does nothing. **[inferred]**
- The only real danger is destroying the tick function **during** a tick frame after its task was queued: use-after-free, with no check. This is a hazard described by the header comments (`EngineBaseTypes.h:179-180`, `:345-346`), not by a runtime assert. **[inferred]**
- Corruption checks do exist when state is changed incorrectly:
  - `TickTaskManager.cpp:1851` `check(bFound); // otherwise you changed TickState while the tick function was registered. Call SetTickFunctionEnable instead.`
  - `TickTaskManager.cpp:1683` `check(!HasTickFunction(TickFunction));`
- Owners guard against a dangling back-pointer themselves:
  - Mass: `checkf(PhaseManager, ...left dangling after the FMassProcessingPhase owner got destroyed.)` (`Source/Runtime/MassEntity/Private/MassProcessingPhaseManager.cpp:75`).
  - AnimSharing: `ensure(Manager)` (see Example 2).

### Example 1: Mass, `FMassProcessingPhase` (owned by a UWorldSubsystem)

Ownership chain: `UMassSimulationSubsystem` → `UMassSubsystemBase` → `UWorldSubsystem`.
- `Source/Runtime/MassEntity/Public/MassSubsystemBase.h:36` `class UMassSubsystemBase : public UWorldSubsystem`
- The subsystem holds `TSharedRef<FMassProcessingPhaseManager> PhaseManager` (`Plugins/Runtime/MassGameplay/Source/MassSimulation/Public/MassSimulationSubsystem.h:96`).
- The manager holds the phases in an array: `FMassProcessingPhase ProcessingPhases[...]` (`MassProcessingPhaseManager.h:312`).

**Struct.** It is not a USTRUCT. It is a plain struct with copying deleted:
- `Source/Runtime/MassEntity/Public/MassProcessingPhaseManager.h:54` `struct FMassProcessingPhase : public FTickFunction`
- `:59-60` `FMassProcessingPhase(const FMassProcessingPhase& Other) = delete; FMassProcessingPhase& operator=(...) = delete;`
- `:64-66` declares the `ExecuteTick`, `DiagnosticMessage` and `DiagnosticContext` overrides.

**Fields set before registration:**
- `MassProcessingPhaseManager.cpp:60-61` `bCanEverTick = true; bStartWithTickEnabled = false;`
- `TickGroup` is set per phase in `Initialize` (`:203` `TickGroup = InTickGroup;`) from the table at `:44-51`: `TG_PrePhysics, TG_StartPhysics, TG_DuringPhysics, TG_EndPhysics, TG_PostPhysics, TG_LastDemotable`.
- `EndTickGroup`, `bTickEvenWhenPaused` and `bAllowTickOnDedicatedServer` are not set, so the defaults apply: `TG_PrePhysics`, `false` and `true`.
- The PrePhysics phase can optionally be made high priority (`:479-482` `Phase.SetPriorityIncludingPrerequisites(bHighPriority);`).

**Overrides:**
- `ExecuteTick` filters the tick type first (`:67` `if (ShouldTick(TickType) == false) return;`). The default mask is `(1 << LEVELTICK_All) | (1 << LEVELTICK_TimeOnly)` (`:62`).
- `DiagnosticMessage` returns `PhaseManager->GetName() + TEXT("[ProcessingPhaseTick]")` (`:191`).
- `DiagnosticContext` returns `TEXT("MassProcessingPhase")` (`:196`).

**When it registers.** `UMassSimulationSubsystem::OnWorldBeginPlay` (`MassSimulationSubsystem.cpp:126-135`) → `StartSimulation` → `PhaseManager->Start` (`:214`) → `EnableTickFunctions` (`MassProcessingPhaseManager.cpp:439`). There:
- `:485` `Phase.RegisterTickFunction(World.PersistentLevel);`
- `:486` `Phase.SetTickFunctionEnable(true);`

Game worlds register at **BeginPlay**, not at `Initialize`. In the editor, `PostInitialize` starts it deliberately (`MassSimulationSubsystem.cpp:120-121`).

**When it unregisters.** There is no explicit `UnRegisterTickFunction` anywhere in MassEntity or MassGameplay (grep result).
- `Stop()` only **disables** the phases (`MassProcessingPhaseManager.cpp:517-519` `Phase.SetTickFunctionEnable(false);`). It is called from `Deinitialize` (`MassSimulationSubsystem.cpp:92`) and `BeginDestroy` (`:50`).
- Final unregistration comes from `~FTickFunction` when the shared manager is destroyed. **[inferred]**

### Example 2: AnimationSharing, `FTickAnimationSharingFunction` (owned by a plain UObject manager)

**Struct:** `Plugins/Developer/AnimationSharing/Source/AnimationSharing/Public/AnimationSharingManager.h`
- `:483-486` `USTRUCT() struct FTickAnimationSharingFunction : public FTickFunction { GENERATED_USTRUCT_BODY()`
- `:500-505` `TStructOpsTypeTraits<FTickAnimationSharingFunction> ... WithCopy = false`
- `:629` the owner holds it as a plain member, not a UPROPERTY: `FTickAnimationSharingFunction TickFunction;`

**Overrides:** `Private/AnimationSharingManager.cpp`
- `:170-174` `if (ensure(Manager)) { Manager->Tick(DeltaTime); }`
- `:180` `return TEXT("FTickAnimationSharingFunction");`
- `:185` `return FName(TEXT("TickAnimationSharing"));`

**Fields:** none are set except `Manager`, so every default applies: `TG_PrePhysics`, `bCanEverTick == false`, enabled. It still ticks, which confirms that the manager ignores `bCanEverTick`.

**Lifetime.** The manager is created by `NewObject<UAnimationSharingManager>(InWorld)` (`AnimationSharingModule.cpp:37`), for game worlds only (`:46`).
- Register: `AnimationSharingManager.cpp:196-197` `TickFunction.Manager = this; TickFunction.RegisterTickFunction(GetWorld()->PersistentLevel);`
- Unregister: in `BeginDestroy` (`:146-147`) `TickFunction.UnRegisterTickFunction(); TickFunction.Manager = nullptr;`
- Components add this function as a tick prerequisite (`:1020` `BlendComponent->PrimaryComponentTick.AddPrerequisite(AnimSharingManager, AnimSharingManager->GetTickFunction());`).

### Example 3: Particles, `FParticleSystemWorldManagerTickFunction` (owned by a per-world non-UObject manager)

**Struct:** `Source/Runtime/Engine/Classes/Particles/ParticleSystemManager.h`
- `:51-54` `USTRUCT() struct FParticleSystemWorldManagerTickFunction : public FTickFunction { GENERATED_USTRUCT_BODY()`
- `:65-70` `WithCopy = false`
- `:173` `TArray<FParticleSystemWorldManagerTickFunction> TickFunctions;`, sized once before registration with `SetNum`.
- Because the manager keeps raw pointers, the array must never reallocate after registration. **[inferred]**

**Fields set before registration** (`Source/Runtime/Engine/Private/Particles/ParticleSystemManager.cpp:257-267`), one function per tick group:
```cpp
TickFunc.TickGroup = (ETickingGroup)TickGroup;  TickFunc.EndTickGroup = TickFunc.TickGroup;
TickFunc.bCanEverTick = true;  TickFunc.bStartWithTickEnabled = true;  TickFunc.bAllowTickOnDedicatedServer = false;
... TickFunc.RegisterTickFunction(InWorld->PersistentLevel);
```

**Overrides:**
- `:903-904` `check(Owner); Owner->Tick(TickGroup, DeltaTime, ...)`
- `:911` returns `"FParticleSystemManager::Tick(" + group name + ")"`
- `:916` returns `FName("ParticleSystemManager")`

**Lifetime:**
- Created in `OnWorldInit` (`:212` `new FParticleSystemWorldManager(World)`). It registers from its constructor.
- Deleted in `OnWorldCleanup`, `OnPreWorldFinishDestroy` or `OnWorldBeginTearDown` (`:222`, `:232`, `:242`).
- There is no explicit Unregister. The destructor of the `TArray` member calls `~FTickFunction`. **[inferred]**

### Example 4 (bonus, closest to our design): AsyncMessageSystem, owned by `UAsyncMessageWorldSubsystem`

`Plugins/Experimental/AsyncMessageSystem/Source/AsyncMessageSystem/Private/AsyncGameplayMessageSystem.cpp`

**Struct:**
- `:44` `struct FMessageSystemTickFunction final : public FTickFunction` (not a USTRUCT; heap-allocated through `MakeShared` at `:157`).
- It holds a `TWeakPtr` back to its owner (`:83`).

**Fields** (`:52-62`):
- `bCanEverTick = true; bStartWithTickEnabled = ...; bAllowTickBatching = true;`
- `bRunOnAnyThread = false;`
- `TickGroup = InGroup; EndTickGroup = TickGroup;`

**Overrides:**
- `:69-71` `if (TSharedPtr<...> MessageSys = WeakMessageSys.Pin()) MessageSys->ProcessMessagesForTickGroup(TickGroup);`
- `DiagnosticMessage` is at `:75-78`. `DiagnosticContext` is not overridden.

**Register:**
- `:139` `ULevel* TickLevel = OuterWorld->PersistentLevel;` then `:163` `Func->RegisterTickFunction(TickLevel);`
- The functions are chained with `:168` `Func->AddPrerequisite(TickLevel, *PreviousTickFunction);`
- Call path: `UAsyncMessageWorldSubsystem::Initialize` (`AsyncMessageWorldSubsystem.cpp:53-57`) → `CreateMessageSystem` → `Startup()` (`AsyncMessageSystemBase.h:349`) → `Startup_Impl` (`AsyncGameplayMessageSystem.cpp:97`).

**Unregister** (explicit): `:206` `Pair.Value->UnRegisterTickFunction();`
- Call path: `Deinitialize` (`AsyncMessageWorldSubsystem.cpp:60-64`) → `Shutdown()` → `Shutdown_Impl` (`AsyncMessageSystemBase.cpp:137`) → `DestroyTickFunctions`.

**Timing of world subsystem `Initialize` relative to the PersistentLevel:**
- For a new world, `PersistentLevel` is created in `InitializeNewWorld` (`World.cpp:2694`) before `InitWorld` → `InitializeSubsystems()` (`World.cpp:2447`). So it is valid in `Initialize`. **[inferred]**
- World subsystems are created for Editor worlds as well (`WorldSubsystem.cpp:31` `return WorldType == EWorldType::Game || WorldType == EWorldType::Editor || WorldType == EWorldType::PIE;`).
- The editor world ticks with `LEVELTICK_ViewportsOnly` (`Source/Editor/UnrealEd/Private/EditorEngine.cpp:1958`).
- Consequence: a tick function registered in `Initialize` **will also tick in the editor world**, unless `ExecuteTick` filters `TickType` (Mass `:67`; Actor `Actor.cpp:376`) or the function registers in `OnWorldBeginPlay` instead. **[inferred]**
- Deinit order: `OnPostWorldCleanup` is broadcast, then `SubsystemCollection.Deinitialize()` runs (`World.cpp:6482-6486`).

---

## A2. `TickInterval` mechanics (`TickTaskManager.cpp`)

### Storage

Each `FTickTaskLevel` keeps three lists:
- `:1941` `TSet<FTickFunction*> AllEnabledTickFunctions;`
- `:1943` `FCoolingDownTickFunctionList AllCoolingDownTickFunctions;`, a singly-linked list (`:1914-1936`, with `Head`).
- `:1945` `TSet<FTickFunction*> AllDisabledTickFunctions;`

Plus a pending array: `:1947` `TArrayWithThreadsafeAdd<FTickScheduleDetails> TickFunctionsToReschedule;`

Per-function data:
- `EngineBaseTypes.h:306-313`: `FTickFunction* Next;` and `float RelativeTickCooldown;` (*"relative to the element ahead of it in the cooling down list"*).
- `EngineBaseTypes.h:319`: `double LastIntervalTickSeconds;`

The cooldown list is a **sorted delta list**: each node stores its cooldown relative to the node before it.

### The per-frame lifecycle of an interval tick

1. **`FTickTaskLevel::StartFrame`** (`:1246-1279`) first calls `ScheduleTickFunctionCooldowns()` (`:1259`). It then walks the head of the list and enables every node whose cumulative cooldown is below the frame delta:
   ```cpp
   if (CumulativeCooldown + TickFunction->InternalData->RelativeTickCooldown >= Context.DeltaSeconds)
   { TickFunction->InternalData->RelativeTickCooldown -= (Context.DeltaSeconds - CumulativeCooldown); break; }
   ```
   (`:1266-1269`). The first node not yet due is decremented by the frame delta and the walk stops, so cooldown cost per frame is proportional to the number of nodes that are due. **[inferred]**
2. **`QueueAllTicks`** (`:1474-1514`).
   - Every enabled function is queued. A function that has `TickInterval > 0` is then **moved out of the enabled set** and rescheduled:
     `:1489-1492` `if (TickFunction->TickInterval > 0.f) { It.RemoveCurrent(); RescheduleForInterval(TickFunction, TickFunction->TickInterval); }`
   - Functions due from the cooldown list are queued and rescheduled **with credit for the overrun**:
     `:1506` `RescheduleForInterval(TickFunction, TickFunction->TickInterval - (Context.DeltaSeconds - CumulativeCooldown)); // Give credit for any overrun`
3. `RescheduleForInterval` only appends to the pending array and sets `bWasInterval` (`:1358-1361`).
4. **`EndFrame`** calls `ScheduleTickFunctionCooldowns()` (`:1654`). That function **sorts** the pending array by cooldown (`:1399-1402` `return A.Cooldown < B.Cooldown;`) and merges it into the delta list (`:1404-1468`). The merge walks the existing list, so it is O(list length + k log k) per frame. **[inferred]**

### When does an interval function tick for the first time?

**Immediately: on the first tick frame after registration or enabling. It does not wait one interval.**
- `AddTickFunction` puts an Enabled function into `AllEnabledTickFunctions` (`:1684-1686`). `QueueAllTicks` queues it before rescheduling it (`:1484-1492`).
- If it is registered or enabled *during* a tick frame, it also enters `NewlySpawnedTickFunctions` (`:1687-1689`). It then ticks in the next tick group of the same frame and is rescheduled for the full interval (`:1532-1541`).
- The header confirms this for enabling (`EngineBaseTypes.h:359` *"Enabling during a tick frame will tick it in the next tick group if it has not already ticked this frame."*).

### Phase alignment: **yes, aligned. There is no built-in stagger or randomisation.**

- `grep "Rand|FMath::Frand|stagger|Stagger|jitter"` on `TickTaskManager.cpp` returns nothing.
- N functions with the same interval that first tick in the same frame all call `RescheduleForInterval(TF, TickInterval)` with an identical `Cooldown` (`:1492` or `:1541`).
- In the merge, the first one gets `RelativeTickCooldown = CooldownTime - CumulativeCooldown` and every later one gets `0` (`:1452`, with `CumulativeCooldown` already equal to `CooldownTime`).
- The `StartFrame` walk therefore enables all of them in the same frame (relative 0 adds nothing to the cumulative total, `:1271-1275`).
- They are rescheduled with an identical overrun credit (`:1506`, same `CumulativeCooldown`), so they **stay aligned indefinitely**. **[inferred]**
- Expected result: frame-time spikes with period ≈ `TickInterval`, quantised to frames. This is **[inferred]** and must be measured in Phase 8, not asserted.
- Disable followed by enable re-phases a function. Disabling resets `LastIntervalTickSeconds = -1` (`:2452-2455`). Re-enabling puts it back in the enabled set, where it ticks at once and is rescheduled from that frame. **[inferred]**

### DeltaTime received by an interval tick

`FTickFunction::CalculateDeltaTime` (`:2825-2845`):
```cpp
const double CurrentWorldTime = (bTickEvenWhenPaused ? TickingWorld->GetUnpausedTimeSeconds() : TickingWorld->GetTimeSeconds());
if (InternalData->LastIntervalTickSeconds >= 0.0) { DeltaTime = CurrentWorldTime - InternalData->LastIntervalTickSeconds; }
InternalData->LastIntervalTickSeconds = CurrentWorldTime;
```
- **An interval function receives the world time since its own last tick**, not the frame delta.
  - Non-interval functions get the world frame delta (`:2827-2831`).
  - The **first** interval tick gets the frame delta, because `LastIntervalTickSeconds == -1`.
- `bWasInterval` is set during queueing (`:1360`), before the task executes, so even the first tick takes the interval branch. **[inferred]**
- World time is dilated game time, advanced before any tick group runs (`LevelTick.cpp:1596` `DeltaSeconds *= Info->GetEffectiveTimeDilation();`, `:1610` `TimeSeconds += DeltaSeconds;`).
- Actors and components then multiply by `CustomTimeDilation` (`Actor.cpp:379`; `Actor.h:4890-4891`).

### Changing the interval at runtime

- `UpdateTickIntervalAndCoolDown` (`:2463-2477`): if the function is registered, not disabled and `bWasInterval`, it first tries to update its pending reschedule entry (`UpdateRescheduleInterval`, a linear `FindByPredicate`, `:1349`). Otherwise it calls `RemoveTickFunction` and `RescheduleForInterval(this, TickInterval)`. **The new interval starts counting from now.**
- `RemoveTickFunction` on a cooling-down function does a **linear walk of the cooldown list** (`:1876-1902`). Disabling, unregistering or re-intervaling one cooling-down function is therefore O(N cooling-down). **[inferred]** This is a scalability factor for thousands of interval ticks.

### The 5.8 deprecation

`EngineBaseTypes.h:464-467`:
```cpp
double GetLastIntervalTickGameTime() const { return (InternalData ? InternalData->LastIntervalTickSeconds : -1.0); }
UE_DEPRECATED(5.8, "Renamed because function only returns the correct time for functions with a TickInterval")
double GetLastTickGameTime() const { ... }
```
- **`GetLastTickGameTime()` was renamed to `GetLastIntervalTickGameTime()`.**
- The field changed as well. UE 5.7 had `float LastTickGameTimeSeconds;` and `float GetLastTickGameTime()` (`UE_5.7/.../EngineBaseTypes.h:312`, `:424`). UE 5.8 has `double LastIntervalTickSeconds` (`EngineBaseTypes.h:319`), and the getter returns `double`.

---

## A3. Per-tick dispatch cost path

### Which CVars apply (`TickTaskManager.cpp`)

| CVar | Default | Line |
|---|---|---|
| `tick.AllowBatchedTicks` | `0` | `:59-62` *"If true, tick functions with bAllowTickBatching will be automatically combined into a single tick task."* |
| `tick.AllowBatchedTicksUnordered` | `0` | `:64-67` |
| `tick.AllowOptimizedPrerequisites` | `1` | `:69-72` |
| `tick.AllowConcurrentTickQueue` | `0` | `:75-78` (*"disabled by default in UE 5.5"*) |
| `tick.AllowAsyncTickDispatch` | `0` | `:80-83` |
| `tick.AllowAsyncComponentTicks` | `1` | `:54-57` (only matters for `bRunOnAnyThread`) |

All of them are read once per frame in `FTickTaskSequencer::StartFrame` (`:1091-1096`).

### Work per enabled tick function per frame, `AllowBatchedTicks=0` (the default)

1. **Count:** `return AllEnabledTickFunctions.Num() + CooldownTicksEnabled;` (`:1279`).
2. **Queue:** `QueueAllTicks` iterates the `TSet` (`:1481`). For each function, `QueueTickFunction` (`:2622-2711`):
   - two `std::atomic` frame-counter stores (`:2631`, `:2710`);
   - a prerequisite loop with a `TWeakObjectPtr::IsValid` call per prerequisite; stale prerequisites are removed (`:2638-2671`);
   - recursive queueing of any prerequisite not yet visited (`:2652`);
   - the actual start and end group are computed, including demotion (`:2674-2701`).
3. **Task creation, one per function** (`:872-888`, `QueueTickTask`):
   ```cpp
   FTickGraphTask* Task = TGraphTask<FTickFunctionTask>::CreateTask(Prerequisites, ENamedThreads::GameThread).ConstructAndHold(TickFunction, &UseContext);
   ```
   - Prerequisites become an `FGraphEventArray` built from each prerequisite's completion handle (`:849-855`).
   - `AddTickTaskCompletion` appends the task to `TickTasks[Start][End]` and appends its completion event to `TickCompletionEvents[End]` (`:672-683`).
4. **Dispatch:** `RunTickGroup` → `ReleaseTickGroup` → `DispatchTickGroup` → `TickArray[Index]->Unlock(CurrentThread);` for every task (`:1170-1204`).
5. **Execution:** `FTickFunctionTask::DoTask` (`:313-354`) runs `IsTickFunctionEnabled()`, `LIGHTWEIGHT_TIME_GUARD_BEGIN/END` and the call at `:334`, `Target->ExecuteTick(Target->CalculateDeltaTime(...), ...)`, then `ClearTaskInformation()`.
6. **Wait:** the game thread waits on `TickCompletionEvents[Block]` with `ProcessUntilTasksComplete` (`:1040`).

### Work per function with `AllowBatchedTicks=1`

- Steps 1–2 are unchanged: the prerequisite walk and group computation still happen per function.
- In `QueueOrBatchTickTask` (`:750-863`), each function does a **linear search of the existing batches** for a matching condition and compatible prerequisites (`:760-800`). It is then added with `BatchInfo->TickFunctions.Add(TickFunction);` (`:839`) and `SetTaskPointer` (`:840`).
- **One** `TGraphTask<FBatchTickFunctionTask>` is created per batch (`:827` or `:831`), with one completion event per batch (`:835`).
- Execution: `FBatchTickFunctionTask::DoTask` loops through the batch and makes the same per-function calls (`:408-428`).
- In short: batching removes per-function task creation, dispatch and completion events. It keeps per-function queueing logic and the virtual `ExecuteTick` call. **[inferred]**

### Batching eligibility (quoted)

1. `:752` `if (bAllowBatchedTicksForFrame && TickFunction->bAllowTickBatching)`
   - `bAllowTickBatching` defaults to `false` on `FTickFunction` (`:2368`).
   - It is set to `true` for actors (`Actor.cpp:280`) and components (`ActorComponent.cpp:551`).
2. `:711-715`
   ```cpp
   // Don't batch high priority ticks or ones that last more than a single tick group
   return !Condition.bRunOnAnyThread && !Condition.bHighPriority && Condition.StartGroup == Condition.EndGroup;
   ```
   This applies to the *actual* start and end groups, after prerequisite demotion (`FTickGroupCondition`, `:253-258`).
3. A batch is shared only when the `FTickGroupCondition` matches (`:762`) **and** the prerequisites match: either the batch requires the same prerequisite or the batch executes it, in ordered mode (`:767-793`).
4. Only the initial frame queue is batched. `FinishBatchedTicks` turns it off (`:729-730` `// Disable batching for the rest of the frame as we don't want to batch QueueNewlySpawned`).
5. Batching is not compatible with the concurrent queue (`:2060` `ensureMsgf(!CVarAllowBatchedTicks..., TEXT("Concurrent queuing is not compatible with batched ticks!"));`).

### What an Actor with an overridden but empty `Tick` still pays

All of steps 1–6 above, plus this call chain:
- `FActorTickFunction::ExecuteTick` (`Actor.cpp:372-380`):
  ```cpp
  if (IsValid(Target)) { if (TickType != LEVELTICK_ViewportsOnly || Target->ShouldTickIfViewportsOnly()) {
      FScopeCycleCounterUObject ActorScope(Target);
      Target->TickActor(DeltaTime*Target->CustomTimeDilation, TickType, *this);
  ```
- `AActor::TickActor` (`Actor.cpp:1997-2002`) calls `if (GetWorld()) { Tick(DeltaSeconds); }`.
- Your empty override then runs.
- If the override calls `Super::Tick`, `AActor::Tick` (`Actor.cpp:2008`) also does a class-flag check for BP/non-native classes before `ReceiveTick` and latent actions.

The work is: two virtual calls (`ExecuteTick`, `TickActor`) plus the virtual `Tick`, an `IsValid` check and a stats scope. `FScopeCycleCounterUObject` has a different definition per build config (`STATS`, lightweight stats or hitch detection, at `Source/Runtime/CoreUObject/Public/UObject/UObjectBaseUtility.h:986-989`, `:1073-1075`, `:1134-1137`). So its cost depends on the build configuration, and headline numbers must come from a Development or Test build. **[inferred]**

### `bCanEverTick = false` avoids registration entirely: **yes**

- `Actor.cpp:1690-1694`: `if(PrimaryActorTick.bCanEverTick) { ... PrimaryActorTick.RegisterTickFunction(GetLevel()); }`
- `ActorComponent.cpp:1741-1748`: `if(TickFunction->bCanEverTick && !IsTemplate()) { ... TickFunction->RegisterTickFunction(ComponentLevel); }`
- `InternalData` is allocated lazily, only inside `RegisterTickFunction` (`TickTaskManager.cpp:2414-2417`), so a non-ticking actor carries no `FInternalData`. **[inferred]**
- `SetActorTickEnabled` and `SetComponentTickEnabled` do nothing when `bCanEverTick` is false (`Actor.cpp:1756`, `ActorComponent.cpp:1757`).
- Disabled-but-registered functions sit in `AllDisabledTickFunctions` and are **not iterated per frame**. `QueueAllTicks` touches only the enabled set and the head of the cooldown list (`:1481-1513`). **[inferred]**
- Blueprint caveat: the Kismet compiler sets `bCanEverTick = true` when a BP has a **connected** Event Tick (`Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:5587-5592`). A disconnected Event Tick is detected and skipped (`:5560-5569`).

### Call paths (quoted)

- **Actor:** `FTickFunctionTask::DoTask` (`TickTaskManager.cpp:334`) → `FActorTickFunction::ExecuteTick` (`Actor.cpp:379` `Target->TickActor(DeltaTime*Target->CustomTimeDilation, TickType, *this);`) → `AActor::TickActor` (`Actor.cpp:2002` `Tick(DeltaSeconds);`) → `AActor::Tick`. For BP classes, `AActor::Tick` calls `ReceiveTick(DeltaSeconds);` (`:2013`) and then `ProcessLatentActions` (`:2025`).
- **Component:** `FActorComponentTickFunction::ExecuteTick` (`ActorComponent.cpp:1699-1701`) → `ExecuteTickHelper(Target, Target->bTickInEditor, DeltaTime, TickType, [this, TickType](float DilatedTime) { Target->TickComponent(DilatedTime, TickType, this); });`. The helper (`Actor.h:4877-4891`) checks `IsValid(Target)`, opens two `FScopeCycleCounterUObject` scopes, checks `Target->bRegistered` and the `ViewportsOnly` rule, then calls `ExecuteTickFunc(DeltaTime * TimeDilation)`. `UActorComponent::TickComponent` (`ActorComponent.cpp:1882-1888`) asserts `check(bRegistered);` and calls `ReceiveTick` for BP classes.

### Automatic prerequisites that add per-frame cost

- An attached scene component gets its parent as a prerequisite (`Source/Runtime/Engine/Private/Components/SceneComponent.cpp:2491` `PrimaryComponentTick.AddPrerequisite(Parent, Parent->PrimaryComponentTick); // force us to tick after the parent does`).
- Each prerequisite adds a weak-pointer check and a completion-handle fetch per frame (`TickTaskManager.cpp:2638-2668`, `:849-853`). **[inferred]**

---

## A4. Defaults

| Field | `FTickFunction` ctor (`TickTaskManager.cpp:2361-2376`) | `AActor::PrimaryActorTick` (`Actor.cpp:276-282`) | `UActorComponent::PrimaryComponentTick` (`ActorComponent.cpp:548-553`) |
|---|---|---|---|
| `TickGroup` | `TG_PrePhysics` (`:2362`) | `TG_PrePhysics` (`:276`) | **`TG_DuringPhysics`** (`:548`) |
| `EndTickGroup` | `TG_PrePhysics` (`:2363`) | (inherits) | (inherits) |
| `bCanEverTick` | `false` (`:2365`) | `false` (`:278`) | `false` (`:550`) |
| `bStartWithTickEnabled` | `false` (`:2366`) | `true` (`:279`) | `true` (`:549`) |
| `TickInterval` | `0.f` (`:2375`) | (inherits 0) | (inherits 0) |
| `bAllowTickBatching` | `false` (`:2368`) | `true` (`:280`) | `true` (`:551`) |
| `bAllowTickOnDedicatedServer` | `true` (`:2367`) | (inherits) | (inherits) |
| `TickState` | `Enabled` (`:2374`) | set Disabled by `SetTickFunctionEnable(false)` (`:282`) | set Disabled by `SetTickFunctionEnable(false)` (`:553`) |

- An `EndTickGroup` below `TickGroup` is harmless. The actual end group is `Max(MaxEndTickGroup, Max(EndTickGroup, MyActualTickGroup))` (`TickTaskManager.cpp:2687`). **[inferred]**
- Movement components override the component default and tick in `TG_PrePhysics` (`MovementComponent.cpp:28` `PrimaryComponentTick.TickGroup = TG_PrePhysics;`). So do skeletal meshes (`SkeletalMeshComponent.cpp:441`).
- An actor's tick function is registered just before `BeginPlay` in game worlds (`Actor.cpp:4814` `RegisterAllActorTickFunctions(true, false);`). It is registered at component registration only when `bAllowTickBeforeBeginPlay || !World->IsGameWorld()` (`Actor.cpp:6200-6204`).

### The setters

| Function | Exact behaviour |
|---|---|
| `AActor::SetActorTickEnabled(bool)` (`Actor.cpp:1754-1759`) | `if (PrimaryActorTick.bCanEverTick && !IsTemplate()) PrimaryActorTick.SetTickFunctionEnable(bEnabled);` |
| `AActor::SetActorTickInterval(float)` (`Actor.cpp:1767-1770`) | `PrimaryActorTick.TickInterval = TickInterval;`. A field write only. Header: *"Will not enable a disabled tick function. Takes effect on next tick."* (`Actor.h:2905`). An active cooldown keeps running; the new value is used at the next reschedule. |
| `UActorComponent::SetComponentTickEnabled(bool)` (`ActorComponent.cpp:1755-1760`) | `if (PrimaryComponentTick.bCanEverTick && !IsTemplate()) PrimaryComponentTick.SetTickFunctionEnable(bEnabled);` |
| `UActorComponent::SetComponentTickInterval(float)` (`ActorComponent.cpp:1783-1786`) | `PrimaryComponentTick.TickInterval = TickInterval;`. A field write only. Header: *"Takes effect on next tick."* (`ActorComponent.h:1017`) |
| `UActorComponent::SetComponentTickIntervalAndCooldown(float)` (`ActorComponent.cpp:1788-1791`) | `PrimaryComponentTick.UpdateTickIntervalAndCoolDown(TickInterval);`. Overwrites the current cooldown (see A2). Header: *"Takes effect imediately."* (`ActorComponent.h:1024`) |
| `SetActorTickIntervalAndCooldown` | **not found**. There is no actor equivalent in `Classes`/`Public`. Call `PrimaryActorTick.UpdateTickIntervalAndCoolDown` directly. |
| `FTickFunction::SetTickFunctionEnable` (`TickTaskManager.cpp:2439-2460`) | If the function is registered and its state changes, it is removed from its list, `TickState` is set and it is re-added. Disabling resets `LastIntervalTickSeconds`. Enabling a cooling-down function is a no-op (`:2443` checks `TickState == Disabled`). If unregistered, only `TickState` is set. |

---

## A5. The frame position of the tick groups (`Source/Runtime/Engine/Private/LevelTick.cpp`, `UWorld::Tick`)

Inside each level collection, when `bDoingActorTicks` is true (`:1650-1656`: `TickType != LEVELTICK_TimeOnly && !bIsPaused && ...`):

| Order | Code | Line |
|---|---|---|
| Physics setup and queueing | `SetupPhysicsTickFunctions(DeltaSeconds); ... FTickTaskManagerInterface::Get().StartFrame(this, DeltaSeconds, TickType, LevelsToTick);` | `:1740-1742` |
| `TG_PrePhysics` | `RunTickGroup(TG_PrePhysics);` | `:1750` |
| (collision tree) | `EnsureCollisionTreeIsBuilt();` | `:1753` |
| `TG_StartPhysics` | `RunTickGroup(TG_StartPhysics);` | `:1759` |
| `TG_DuringPhysics` | `RunTickGroup(TG_DuringPhysics, false); // No wait here...` | `:1765` |
| `TG_EndPhysics` | `RunTickGroup(TG_EndPhysics);` | `:1772` |
| `TG_PostPhysics` | `RunTickGroup(TG_PostPhysics);` | `:1778` |
| Latent actions, **TimerManager**, **TickObjects**, cameras, streaming (first collection only) | `GetTimerManager().Tick(DeltaSeconds);` / `FTickableGameObject::TickObjects(this, ...)` | `:1797`, `:1816`, `:1821` |
| `TG_PostUpdateWork` | `RunTickGroup(TG_PostUpdateWork);` | `:1877` |
| `TG_LastDemotable` | `RunTickGroup(TG_LastDemotable);` | `:1883` |
| End | `FTickTaskManagerInterface::Get().EndFrame();` | `:1886` |

- When paused, a separate pause frame runs instead (`:1784` `RunPauseFrame(...)`). It executes only `bTickEvenWhenPaused` functions, with no groups or dependencies (`TickTaskManager.cpp:1570-1571`).
- `TG_NewlySpawned` is not a real group. After each blocking group, functions spawned during it are drained, up to 101 iterations (`TickTaskManager.cpp:2140-2157`).
- Each `UWorld::RunTickGroup` also flushes grouped moves (`LevelTick.cpp:789` `ProcessPendingGroupMoves(true);`).

### What `TG_DuringPhysics` implies

- Enum docs (`EngineBaseTypes.h:85-98`):
  - `TG_PrePhysics`: *"Any item that needs to be executed before physics simulation starts."*
  - `TG_DuringPhysics`: *"Any item that can be run in parallel with our physics simulation work."*
  - `TG_PostPhysics`: *"Any item that needs rigid body and cloth simulation to be complete before being executed."*
- Physics is **kicked off** in `TG_StartPhysics`:
  - `PhysLevel.cpp:160-161` `StartPhysicsTickFunction.TickGroup = TG_StartPhysics; ...RegisterTickFunction(PersistentLevel);`
  - The tick calls `StartPhysicsSim` → `PhysScene->StartFrame()` (`PhysLevel.cpp:223`) → `Solver->AdvanceAndDispatch_External(UseDeltaTime)`, which collects completion events (`Source/Runtime/PhysicsCore/Private/ChaosScene.cpp:379-383`).
- Physics is **finished** in `TG_EndPhysics`, which waits for those events and then runs `FinishPhysicsSim` (`PhysLevel.cpp:269-282` `MyCompletionGraphEvent->DontCompleteUntil(... &UWorld::FinishPhysicsSim ... &PhysicsComplete ...)`).
- `TG_DuringPhysics` is released **without blocking**: `ReleaseTickGroup` runs `ProcessThreadUntilIdle(ENamedThreads::GameThread)` (`TickTaskManager.cpp:1061-1064`). Its ticks are waited for in the `TG_EndPhysics` block, which loops from `WaitForTickGroup` up to the current group (`:970`). **[inferred]**
- Prerequisite optimisation has a special case: `// There is no hard wait for DuringPhysics so always consider those` (`TickTaskManager.cpp:705-707`).
- Implications, all **[inferred]**:
  - While a `TG_DuringPhysics` tick runs, the Chaos solver step for this frame may be executing on worker threads.
  - This frame's simulation results (body transforms pulled in `FinishPhysicsSim`) are not available yet.
  - Game-thread changes to physics bodies made in this group cannot affect the step that is already dispatched; they apply to the next one.
  - Logic that moves, teleports or applies forces and expects them in *this* frame's simulation must run in `TG_PrePhysics`. That is why movement components use it.
- An **explicit engine assert or ensure forbidding body movement in `TG_DuringPhysics`: not found.** The restriction comes from the ordering, not from a check.

---

## A6. `FTickableGameObject` (`Source/Runtime/Engine/Public/Tickable.h`, `Private/Tickable.cpp`)

- **When:** once per world tick, after `TG_PostPhysics` and the TimerManager, and before `TG_PostUpdateWork` (`LevelTick.cpp:1816-1821`; table above).
- Objects with a null world tick in the engine-level call after all world ticks:
  - `GameEngine.cpp:1973` `FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, DeltaSeconds);`
  - `EditorEngine.cpp:2219`
- **No task graph.** `TickObjects` is a plain game-thread loop over **every** registered tickable (`Tickable.cpp:187-209`). Each one gets a virtual `GetTickableGameObjectWorld() == World` check, then `Tick`:
  ```cpp
  && ((TickableEntry.TickType == ETickableTickType::Always) || TickableObject->IsTickable())
  && (TickableObject->GetTickableGameObjectWorld() == World))
  ```
  So the cost is O(all tickables) per world per frame. **[inferred]**
- **`ETickableTickType`** (`Tickable.h:19-32`):
  - `Conditional`: call `IsTickable()` every frame.
  - `Always`: always tick.
  - `Never`: do not add to the array.
  - `NewObject`: query `GetTickableTickType()` at the start of the next tick (`Tickable.cpp:81-85`).
- **`SetTickableTickType(ETickableTickType)`** (`Tickable.h:206`; `Tickable.cpp:157-161` → `SetTickTypeForTickableObject` at `:19-65`):
  - `Never` removes the object, or nulls it and marks it for cleanup if called during ticking (`:33-38`).
  - A registered object takes the new type immediately, even in the current frame (`:55-56`).
  - An unregistered object is queued for the next frame (`:61-62`).
  - The destructor calls `SetTickableTickType(ETickableTickType::Never)` (`:147-153`).
- **Constructor option to start disabled** (`Tickable.h:158`): `ENGINE_API FTickableGameObject(ETickableTickType StartingTickType = ETickableTickType::NewObject);`
  - With `Never`, it skips registration entirely (`Tickable.cpp:135-144`). Otherwise it `ensure(IsInGameThread())` (`:139`).
  - Version check: **added in 5.7**. UE 5.6 has `ENGINE_API FTickableGameObject();` (`UE_5.6/.../Tickable.h:146`). UE 5.7 has the `StartingTickType` parameter (`UE_5.7/.../Tickable.h:151`).
  - The class comment says to use it for UObjects (`Tickable.h:135-138`: *"construct with ETickableTickType::Never and call SetTickableTickType to enable ticking later... set back to Never in BeginDestroy or earlier."*).
  - `UTickableWorldSubsystem` does exactly that: `: FTickableGameObject(ETickableTickType::Never)` (`WorldSubsystem.cpp:57-58`), enabled in `Initialize` (`:101`), disabled in `Deinitialize` (`:111`).
- **Can it choose a tick group?** **No.** `Tickable.h` has no tick-group, prerequisite or interval API (the whole file, 217 lines, was read). Its only timing slot is the fixed `TickObjects` call between `TG_PostPhysics` and `TG_PostUpdateWork`. Use a custom `FTickFunction` to choose a group. **[inferred from absence]**

---

## Engine-provided calibration tool (useful for Phase 8)

`TickTaskManager.cpp:2892`, `:2952-2956`: the console command `tick.AddTestTickFunctions` registers `NumTestTickFunctions = 10000` no-op `FTestTickFunction`s on the current level. `tick.AddIndirectTestTickFunctions` does the same with scattered heap allocations. `tick.RemoveTestTickFunctions` removes them.
- *"Add no-op ticks to test performance of ticking infrastructure."*
- The test functions do not set `bAllowTickBatching`, so they are never batched (`:2858-2864`).
- `CSV_CUSTOM_STAT(Basic, TicksQueued, ...)` reports the number of queued ticks per frame (`:2050`).

## Verdict on the caller's assumptions

- **(a) Actor ticks default to `TG_PrePhysics`: correct** (`Actor.cpp:276`). **Components default to `TG_DuringPhysics`** (`ActorComponent.cpp:548`), and that matters for any per-component baseline.
- **(b) A world-subsystem-owned `FTickFunction` can run gameplay logic in `TG_PrePhysics`: correct.** Mass does it (`MassProcessingPhaseManager.cpp:46`, `:485`). Caveats:
  - register on `PersistentLevel` (a visible level is required, `TickTaskManager.cpp:2238`);
  - `bStartWithTickEnabled` and `bCanEverTick` are **ignored** by `RegisterTickFunction`, so control the state with `SetTickFunctionEnable`;
  - world subsystems exist in Editor worlds, which tick with `LEVELTICK_ViewportsOnly`, so filter `TickType` or register in `OnWorldBeginPlay`;
  - `TG_PrePhysics` is the default group, so set it explicitly anyway for clarity.
- **(c) Same-frame interval ticks stay phase-aligned: correct [inferred]**, with no stagger or randomisation in the tick manager. Nuances:
  - they tick **immediately** on the first frame, then every interval;
  - overrun credit keeps them aligned;
  - they receive the *time since their own last tick* as DeltaTime, except the first tick, which gets the frame delta;
  - disable followed by enable re-phases a function;
  - removing or re-intervaling a cooling-down function is a linear list walk.
