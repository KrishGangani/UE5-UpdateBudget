# 05 · Line By Line Code Explanation: UpdateBudget

**Rule:** every file that exists in `UpdateBudget\` is explained here, line by line, in the same step it is written (Hard rule 13). Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md).

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F3 (foundation) | `.uproject`, two `Target.cs`, two `Build.cs`, two module `.cpp`, `.uplugin`, config `.ini`, `Tools\*.bat` |
| B | S1 (workload, native path only; B6 updated in S2) | `BudgetedUpdateComponent`, `UpdateBudgetSubsystem`, `UBDemoBrainComponent`, `UBDemoAgent`, `UBBenchmarkSubsystem` |
| C | S2 (measurement tools) | `Tools\csv_stats.py`, `Tools\run_bench.py` |
| D | S3 (baselines B2/B3) | `UBBaselineDriverSubsystem`, build files, `UBBenchmarkSubsystem.h` contract |
| E | S4 (scheduling policy) | `UpdateBudgetScheduler`, scheduler tests, `Tools\run_tests.bat`, `run_bench.py` pinning, `analyse_baselines.py` |
| F | S5 (tick function, registry, budgeted path) | `UpdateBudgetTickFunction`, `UpdateBudgetSubsystem` (rewritten), `BudgetedUpdateComponent` (lifecycle and mode), lifetime tests and their test types |

---

## Part A · Foundation (Phase 4)

### A1. `UpdateBudget.uproject`: the project descriptor

```json
{
	"FileVersion": 3,
	"EngineAssociation": "5.8",
	"Category": "",
	"Description": "P01 UpdateBudget: ...",
	"Modules": [ { "Name": "UpdateBudget", "Type": "Runtime", "LoadingPhase": "Default" } ],
	"Plugins": [
		{ "Name": "UpdateBudgetCore", "Enabled": true },
		{ "Name": "SignificanceManager", "Enabled": true },
		{ "Name": "AndroidFileServer", "Enabled": false }
	]
}
```

| Line | What it does, and why |
|---|---|
| `"FileVersion": 3` | The descriptor schema version the engine expects. 3 is the current value in UE 5.8: Epic's own `TP_Blank` template uses it. A wrong value makes the editor refuse or upgrade the file |
| `"EngineAssociation": "5.8"` | Tells the Epic launcher and the "Generate project files" shell action which installed engine opens this project. A launcher build is identified by its version string. If this were blank (as in the raw template), double-clicking the file would prompt for an engine |
| `"Description"` | Shown in the project browser. Cosmetic |
| `"Modules"` | Every C++ module the **project** owns. UnrealBuildTool compiles exactly these, plus plugin modules |
| `"Name": "UpdateBudget"` | The module name. It must match the folder `Source\UpdateBudget\` and the class name in `UpdateBudget.Build.cs`. Module names are unique per project (UBT keys them in one dictionary, `RulesAssembly.cs:199–206`) |
| `"Type": "Runtime"` | Built into game **and** editor targets. *Alternative:* `Editor` (editor-only), which would break packaged builds that need this code |
| `"LoadingPhase": "Default"` | The module loads at the normal point, after engine startup and before the first world exists. That is early enough for its UClasses (including world subsystems) to be registered before the map loads |
| `"SignificanceManager"` (S3) | Baseline B3 needs Epic's Significance Manager, an engine plugin that is off by default (D2) |
| `"AndroidFileServer": false` (S4) | An engine plugin that is **on by default** (`EnabledByDefault: true` in its `.uplugin`). Its editor half writes a settings block with a random `SecurityToken` into `DefaultEngine.ini` on the first editor run. The token only guards Android device file transfers, which this Windows-only project never uses, but a public repository shouldn't carry an engine-generated security token, so the plugin is disabled and the block deleted |
| `"Plugins"` / `"UpdateBudgetCore"` | Explicitly enables the project's own plugin. The plugin is `EnabledByDefault`, so it would load anyway, but stating it here makes the dependency visible in one place and keeps it enabled even if that default changes |

### A2. `Source\UpdateBudget.Target.cs` and `UpdateBudgetEditor.Target.cs`: build targets

```csharp
using UnrealBuildTool;

public class UpdateBudgetTarget : TargetRules
{
	public UpdateBudgetTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("UpdateBudget");
	}
}
```

| Line | What it does, and why |
|---|---|
| `using UnrealBuildTool;` | Target files are **C#**, compiled by UBT itself, not C++. This brings `TargetRules`, `TargetType` and so on into scope |
| `public class UpdateBudgetTarget : TargetRules` | UBT finds targets by class name: `<Name>Target`. `UpdateBudgetTarget` defines the target called `UpdateBudget`. That is why `build.bat UpdateBudget …` works |
| `(TargetInfo Target) : base(Target)` | UBT passes in the platform, configuration and architecture being built. The base constructor stores them |
| `Type = TargetType.Game` | A standalone game executable: no editor code, and the target that gets packaged. The editor file uses `TargetType.Editor`, which builds DLLs loaded by `UnrealEditor.exe` |
| `DefaultBuildSettings = BuildSettingsVersion.V7` | Opts into the newest default compiler and build settings for this engine. **V7 is what Epic's 5.8 template uses.** Older values keep older defaults for backward compatibility |
| `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8` | Uses 5.8's engine header include order (Epic trims transitive includes each version). Pinning it means a future engine upgrade doesn't silently change what our code includes |
| `ExtraModuleNames.Add("UpdateBudget")` | Tells the target which of the project's modules to build. Plugin modules come in through the plugin system |

**Why two target files?** The editor and a packaged game are different executables with different module sets. UBT needs one target rule per executable type.

### A3. `Source\UpdateBudget\UpdateBudget.Build.cs`: the game module's build rules

```csharp
public class UpdateBudget : ModuleRules
{
	public UpdateBudget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// UpdateBudgetCore is public: UBDemoBrainComponent.h (a public header) derives from its UBudgetedUpdateComponent.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UpdateBudgetCore" });
	}
}
```

| Line | What it does, and why |
|---|---|
| `class UpdateBudget : ModuleRules` | The class name **must equal the module name**. UBT maps each `*.Build.cs` filename to its module |
| `ReadOnlyTargetRules Target` | A module can read target settings (platform, configuration) but not change them, which keeps module rules from fighting each other |
| `PCHUsage = UseExplicitOrSharedPCHs` | Use a **shared precompiled header** provided by the engine (the build log shows `SharedPCH.UnrealEd.Project…`), unless the module names its own. Precompiled headers make each `.cpp` compile faster. *Rejected:* `NoPCHs` (slow), or a custom private PCH (unneeded for a small module) |
| `PublicDependencyModuleNames …` | Modules whose **headers our public headers include**. Anything depending on *our* module then gets these include paths too. `Core` (containers, strings, logging), `CoreUObject` (UObject, reflection, GC), `Engine` (actors, components, worlds) |
| `"UpdateBudgetCore"` (public) | **Rule of thumb: private unless a public header needs it.** In Phase 4 the plugin was a *private* dependency. In S1, `Public/UBDemoBrainComponent.h` derives from the plugin's `UBudgetedUpdateComponent`, so a public header now includes a plugin header, and the rule says **public**. Getting this wrong "works" today only because no other module includes our headers. It breaks the moment one does. Over-using public dependencies spreads include paths and slows builds, so the rule is applied header by header |
| *Not included:* `InputCore`, `EnhancedInput` | The template adds them, but this project reads no player input in code. Keeping the dependency list honest is part of modular architecture |

### A4. `Source\UpdateBudget\Private\UpdateBudgetModule.cpp`

```cpp
#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, UpdateBudget, "UpdateBudget");
```

| Line | What it does, and why |
|---|---|
| `#include "Modules/ModuleManager.h"` | Declares `IMPLEMENT_PRIMARY_GAME_MODULE` and `FDefaultGameModuleImpl`. Include the specific header, not a giant one (IWYU, "include what you use") |
| `IMPLEMENT_PRIMARY_GAME_MODULE(…)` | Generates the module's entry point: `IMPLEMENT_GAME_MODULE` plus signing-key, encryption-key and target-name registration (`Core/Public/Modules/ModuleManager.h:1100–1136`, 5.8.4). **In monolithic (packaged) builds** it also defines the globals `GInternalProjectName` (from `UE_PROJECT_NAME`, which UBT passes in from the `.uproject`) and `GIsGameAgnosticExe = false`. In modular (editor) builds the name comes from the command line. **Primary** means "the project's main game module". A project needs at least one. *Without it:* the module never registers, and a packaged game has no project-name global to start from |
| `FDefaultGameModuleImpl` | An engine-provided module class with empty `StartupModule`/`ShutdownModule`. We have no startup logic, so writing our own class would be an extra class with no reason |
| `UpdateBudget` | The module name. It must match `.uproject` and `Build.cs` |
| `"UpdateBudget"` | The old "game name" argument. **In 5.8 it is ignored:** the monolithic overloads name it `DEPRECATED_GameName`, and Epic's header comment says manually specifying a name "is no longer necessary". We pass the project name anyway, because the macro still takes three arguments, and it matches the template |

**Why there is no `UpdateBudget.h`:** the template ships an empty module header that nothing includes. An empty header adds nothing, so we leave it out.

### A5. `Plugins\UpdateBudgetCore\UpdateBudgetCore.uplugin`: the plugin descriptor

| Field | Value | Why |
|---|---|---|
| `FileVersion`, `Version`, `VersionName` | 3, 1, "1.0" | Same schema as Epic's `SignificanceManager.uplugin` in 5.8 |
| `FriendlyName`, `Description`, `Category` | "Update Budget Core", …, "Performance" | Shown in the editor's Plugins window. Same category as Epic's Significance Manager |
| `EnabledByDefault` | `true` | Any project that copies the plugin gets it working without editing the `.uproject`. Later projects (P2, P7) reuse it this way |
| `CanContainContent` | `false` | Code-only. Saying so avoids an empty content mount point |
| `IsBetaVersion`, `IsExperimentalVersion` | `false`, `false` | No warning badge. Honest, because the plugin is stable code, though not "production-ready" (Hard rule 11) |
| `Installed` | `false` | It is part of the project, not installed into the engine |
| `Modules[0]` | `UpdateBudgetCore`, `Runtime`, `Default` | One runtime module, loaded at the default phase. Before the game module needs it, the dependency forces it to load anyway |

### A6. `UpdateBudgetCore.Build.cs` and `Private\UpdateBudgetCoreModule.cpp`

- **`Build.cs`:** the same pattern as A3, with `Core`, `CoreUObject` and `Engine` as **public** dependencies. The plugin's public headers will include engine types (`UActorComponent`, `UWorldSubsystem`), so any module that includes them needs those include paths. `DeveloperSettings` is added in S6, when the settings class exists, not before. (A log category `LogUpdateBudget` was added in S1 and **removed by the S1 simplify audit**: nothing logged to it yet. It returns when the scheduler first logs, in S5.)
- **`IMPLEMENT_MODULE(FDefaultModuleImpl, UpdateBudgetCore);`** is the non-primary version of A4: an ordinary module entry point. `FDefaultModuleImpl` is the empty default implementation.

The build log line `[Adaptive Build] Excluded from UpdateBudgetCore unity file: UpdateBudgetCoreModule.cpp` is normal. UBT compiles recently changed files on their own instead of inside the "unity" batch file, to speed up iteration.

### A7. `Config\DefaultEngine.ini`

| Section / key | Why |
|---|---|
| `GameMapsSettings: GameDefaultMap / EditorStartupMap = /Engine/Maps/Templates/Template_Default` | A shipped engine map with a floor and lights. No map has to be authored in the editor, so the project stays fully buildable from text files. The template's `OpenWorld` map was rejected: it is World Partition, and streaming would add measurement noise |
| `[/Script/Engine.Engine] bSmoothFrameRate=False, bUseFixedFrameRate=False` | Both are already the engine defaults (`BaseEngine.ini`). They are **restated** so nobody can turn on smoothing or a frame cap later without it showing in a diff. Either one would make frame time stop reflecting real work |
| `[SystemSettings] r.VSync=0` | Already the engine default. Restated for the same reason: VSync makes Present wait for the monitor |
| `RendererSettings`, `WindowsTargetSettings` | Copied from Epic's 5.8 template: DX12 with Shader Model 6, virtual shadow maps, no static lighting. These are standard modern defaults, not benchmark choices |

`DefaultGame.ini` sets the project name and description, plus (since S1) `+DirectoriesToAlwaysCook=(Path="/Engine/BasicShapes")`. The agent's cube mesh is referenced **only from C++** (`ConstructorHelpers`). No map references it, so without this line the cooker would leave it out and packaged agents would be invisible. `DefaultInput.ini` is Epic's template file, unchanged.

**Engine-written block (removed in S4):** on the first editor run, the engine's `AndroidFileServerEditor` plugin had appended an `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` section, with a random `SecurityToken`, to `DefaultEngine.ini`. It configures Epic's Android device file server and does nothing on Windows. Since S4 the plugin is disabled in the `.uproject` (A1) and the block is gone, so the token is no longer written or committed. The earlier commits still contain the old token; it guards nothing here, because no Android build of this project exists.

### A8. `Tools\build.bat`, `smoke_game.bat`, `package.bat`

| Script | Key lines | Why |
|---|---|---|
| `build.bat [Target] [Config]` | `call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project=… -WaitMutex -NoHotReloadFromIDE` | A reproducible command-line build. `-WaitMutex` waits if another build is already running instead of failing. `%~dp0` is the script's own folder, so the script works from any working directory |
| `smoke_game.bat` | `UnrealEditor-Cmd.exe <uproject> -game -nullrhi -nosound -unattended -traceautostart=0 -ExecCmds="quit" -abslog=…` | Runs the uncooked game with no window and no GPU, loads the default map, then quits. `-traceautostart=0` stops an open Insights from tracing it (`01b` §4) |
| `package.bat [Config]` | `RunUAT.bat BuildCookRun … -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory=…` | Builds, cooks, stages, paks and archives a standalone game. **The default is Development**, because the launcher engine can't build Test (Phase 4 finding) |

**Phase 4 results (2026-11-20):**
- The editor target builds with **0 errors and 0 warnings** (MSVC 14.44.35229).
- The smoke test loads `Template_Default` in 0.15 s, mounts and loads both modules, and exits with code 0. The log has 0 errors; its warnings are all engine-side `LogEditorDataStorageUI`, from running the editor binary in `-game`.
- The packaged build result is recorded in `09_Testing_And_Benchmarks.md`.

---

## Part B · Step S1: the workload, native path only

**State after S1 (2026-11-20):**
- The workload runs on **native component ticks**; there is no scheduler yet, on purpose, because the baselines must be captured first.
- The code passed a `code-review` pass (9 findings, all fixed or documented) and a 4-angle `simplify` audit (13 changes).
- It builds with 0 errors and 0 warnings.

### B1. `BudgetedUpdateComponent.h` (plugin, public) *(as of S1; S5 additions in F3)*

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#pragma once` | Include the header once per translation unit. *Alternative:* `#ifndef` guards, which do the same thing more verbosely. Unreal uses `#pragma once` everywhere |
| `#include "Components/ActorComponent.h"` | The base class must be complete to derive from it, so a forward declaration is not enough here |
| `#include "BudgetedUpdateComponent.generated.h"` | Code generated by UHT, the Unreal Header Tool, for this file's reflection. **Must be the last include.** UHT checks this and fails the build otherwise |
| `#define UE_API UPDATEBUDGETCORE_API` … `#undef UE_API` | The 5.8 export pattern (`01b` §3). `UPDATEBUDGETCORE_API` becomes `__declspec(dllexport)` while building the plugin DLL and `dllimport` for modules using it. `UE_API` is a short local alias, undefined at the end so it can't leak into other headers. **Without it** the game module gets "unresolved external symbol" link errors for the constructor, `RunUpdate`, `BeginPlay` and so on |
| `class UUpdateBudgetSubsystem;` | A forward declaration. The header only needs the *name* (for a pointer and a `friend`), not the full class, so it avoids an include cycle and keeps rebuilds small |
| `UCLASS(MinimalAPI, Abstract, ClassGroup = (UpdateBudget))` | `UCLASS` registers the class with the reflection system, so it can be a UPROPERTY target, garbage-collected and spawned. `MinimalAPI` exports only the class's type information (`StaticClass`, casts), not every function; the members we need are exported one by one with `UE_API`, which keeps the DLL's export table small. `Abstract` means it can't be added to an actor directly: you must subclass it, because the base has no gameplay. `ClassGroup` is the editor's "Add Component" category |
| `class UBudgetedUpdateComponent : public UActorComponent` | A **component** rather than an interface: it must hold data (`LastUpdateTime`, registry link), and it attaches to any actor (02 §4). `U` prefix: UObject-derived |
| `GENERATED_BODY()` | Expands to the code UHT generated: constructors used by reflection, `StaticClass()`, the `Super` typedef and more. Every `UCLASS` needs it |
| `UE_API UBudgetedUpdateComponent();` | The constructor. It sets the tick defaults (B2). It runs for the class default object (CDO) and for every instance |
| `UE_API void RunUpdate();` | **The one entry point for an update, used by every caller**: native tick now, the scheduler in S5. Callers never call `OnBudgetedUpdate` directly, so time accounting can't be skipped |
| `double GetLastUpdateTime() const` | `const`: reading it cannot change the object. `double`, because `UWorld::GetTimeSeconds()` returns double in UE5; float would lose milliseconds after a few hours of uptime |
| `BeginPlay` / `EndPlay` / `TickComponent` `override` | Engine lifecycle hooks (06 #5). `override` makes the compiler check that the signature really matches a base virtual. A typo would otherwise silently add a new function that is never called |
| `virtual void OnBudgetedUpdate(float DeltaSeconds) {}` | The customisation point. `protected`, because only the class itself and subclasses call it. An empty default instead of pure virtual, because UObjects need a constructible CDO. `float`, because gameplay math is float; the time *bookkeeping* stays double |
| `friend class UUpdateBudgetSubsystem;` | Only the subsystem may write the link fields. A friend keeps them private to everyone else. *Rejected:* public setters, which would let any code corrupt the registry |
| `UPROPERTY(Transient) TObjectPtr<UUpdateBudgetSubsystem> Subsystem;` | `UPROPERTY` makes the reference visible to GC. `Transient` means never saved to disk, since it is a runtime-only link. `TObjectPtr` is UE5's UObject pointer wrapper (06 #8) |
| `int32 RegistryIndex = INDEX_NONE;` | The slot in the subsystem's array, enabling O(1) removal. `INDEX_NONE` (−1) means "not registered". Not a UPROPERTY: it is plain bookkeeping and nothing reflects it |
| `double LastUpdateTime = -1.0;` | Negative means "never updated". That one state also defines "first update" for the time contract |

### B2. `BudgetedUpdateComponent.cpp` *(as of S1; S5 changes in F3)*

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include UE_INLINE_GENERATED_CPP_BY_NAME(BudgetedUpdateComponent)` | Pulls the UHT-generated `.gen.cpp` into this file instead of compiling it separately: fewer translation units, faster builds. 5.x convention |
| Constructor: `PrimaryComponentTick.bCanEverTick = true;` | Components default to **not** ticking (`ActorComponent.cpp:550`). Without this line the native baselines would never run. `bStartWithTickEnabled` is already `true` by default, so it isn't repeated (simplify audit) |
| `PrimaryComponentTick.TickGroup = TG_PrePhysics;` | Components default to `TG_DuringPhysics` (`ActorComponent.cpp:548`). The scheduler runs in `TG_PrePhysics`, so pinning both there keeps B0–B3 and UB at the same point in the frame. **If removed:** the native baselines would run during physics, a different frame position, and the comparison would be unfair |
| `BeginPlay`: `Super::BeginPlay();` first | The engine's own begin-play work (it sets `bHasBegunPlay` and fires the Blueprint event) must happen before ours |
| `UWorld::GetSubsystem<UUpdateBudgetSubsystem>(GetWorld())` | The static, null-safe overload (`World.h:4331`). It returns null in editor or preview worlds, where our subsystem doesn't exist (`DoesSupportWorldType`). There the component just ticks natively with no registry. *Rejected:* a nested `if (UWorld*)`, which said the same thing in more code (simplify audit) |
| `Found->Register(this);` | The subsystem sets **both** link fields (altitude audit): one owner for the invariant "registered ⇔ `Subsystem` and `RegistryIndex` are set" |
| `EndPlay`: `if (UUpdateBudgetSubsystem* Current = Subsystem)` | Copies the `TObjectPtr` into a raw local once. In editor builds a `TObjectPtr` dereference adds handle tracking, so doing it once is cheaper (efficiency audit) |
| `Current->Unregister(this);` **before** `Super::EndPlay` | Leave the registry while still fully valid. **If not called:** the registry keeps a pointer to a dying component until GC nulls it, and with a scheduler (S5) that is a use-after-end-play hazard. GC nulling is not timely (`01b #8`) |
| `TickComponent`: `Super::TickComponent(...)` | Keeps engine behaviour (`check(bRegistered)`; fires the Blueprint Tick event for Blueprint subclasses, `ActorComponent.cpp:1882–1888`). It costs about 1–2 ns and **only exists on the native path**: the one per-update infrastructure difference between modes, documented for honesty |
| `RunUpdate();` *(DeltaTime not forwarded)* | **The time contract** (B3). The engine's `DeltaTime` would differ by mode (a frame delta, or time since an interval tick) and includes per-actor time dilation. Using it would give two clocks. The S1 code review found exactly that bug |
| `RunUpdate`: `if (!ensure(World)) return;` | `ensure` logs a callstack once and continues, whereas `check` would crash. A missing world is a caller bug worth reporting, not a reason to crash a benchmark |
| `const double PreviousUpdateTime = LastUpdateTime; LastUpdateTime = World->GetTimeSeconds();` | Stamp first, so the subsystem sees the new time through `GetLastUpdateTime()`. `GetTimeSeconds()` is world time: paused time excluded, global time dilation included |
| `Current->RecordUpdate(*this, PreviousUpdateTime);` | **The single per-update sync point** (altitude audit). It counts every update, including the first; the S1 review found the first update wasn't counted. It is passed facts (the client and its previous time), not an encoded value, so S5 can also refresh the scheduler's hot copy of this client's time here, in every mode |
| `DeltaSeconds = (PreviousUpdateTime < 0.0) ? 0.0 : LastUpdateTime - PreviousUpdateTime;` | 0 on the first update, then the exact time since the previous one, from one clock |
| `OnBudgetedUpdate(static_cast<float>(DeltaSeconds));` | An explicit narrowing cast, so the double → float conversion is visible in the code |

### B3. `UpdateBudgetSubsystem.h` / `.cpp` (plugin) *(as of S1; rewritten in S5, F4: the rows below still hold unless F4 says otherwise)*

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "BudgetedUpdateComponent.h"` | Needed because `RecordUpdate` is defined **inline in this header** and calls `Client.GetLastUpdateTime()`. No cycle: the component header only forward-declares the subsystem |
| `class UUpdateBudgetSubsystem : public UTickableWorldSubsystem` | A world subsystem has the world's lifetime (`01b` §3). The *tickable* variant gives a `Tick` after `TG_PostPhysics` (`LevelTick.cpp:1821`), when every PrePhysics update of the frame is finished, so the per-frame totals are complete (02 §13) |
| `Register(Client)`: `check(Client)` | Passing null is a programming error. `check` stops in Development builds and compiles out in Shipping |
| `if (Client->RegistryIndex != INDEX_NONE) return;` | Idempotent: registering twice is harmless |
| `Client->Subsystem = this; Client->RegistryIndex = Clients.Add(Client);` | `TArray::Add` returns the new element's index, which is stored for O(1) removal |
| `Unregister`: `if (Clients.IsValidIndex(Index) && Clients[Index] == Client) RemoveClientAt(Index);` | Only remove if the slot really holds this client, which protects against stale indices. The link fields are cleared either way |
| `RemoveClientAt`: `Clients.RemoveAtSwap(Index, EAllowShrinking::No);` | **Swap-remove:** the last element moves into the hole, O(1). *Rejected:* `RemoveAt`, which shifts everything down, O(N), and would make every despawn cost proportional to the agent count. The trade-off is that order isn't preserved, which is fine because nothing depends on registry order. `EAllowShrinking::No` avoids reallocating the array while agents come and go |
| `if (Clients.IsValidIndex(Index) && Clients[Index]) Clients[Index]->RegistryIndex = Index;` | The moved element's stored index must be updated, or its next removal would hit the wrong slot. **The null check:** GC can null an entry whose component was collected without `EndPlay` (`01b #8`). The S1 review found the crash path. One helper now serves every removal path, including those S5 adds (altitude audit) |
| `RecordUpdate(...)` (inline) | `++FrameUpdates` counts every update. The gap is recorded only when there was a previous update. **Inline** because it runs thousands of times per frame and is only a few instructions; an out-of-line cross-DLL call would add call overhead to every update (efficiency audit) |
| `Deinitialize`: detach any remaining client, `Clients.Empty()`, then `Super::Deinitialize()` | Normally every client has already unregistered (`EndPlay` runs before `Deinitialize`, `01b #7`). This is the safety net, so no component keeps a strong reference to a dead subsystem. `Super` must be called **last**, because the tickable base clears its "initialised" flag there |
| `Tick`: three `CSV_CUSTOM_STAT(UpdateBudget, …, ECsvCustomStatOp::Set)` | One value per frame per column: `UpdateBudget/Clients`, `UpdateBudget/UpdatesRun`, `UpdateBudget/MaxGapMs`. **Why not one CSV call per update?** The CSV profiler's own startup benchmark measured about 50 ns per custom stat on this machine. 4,000 calls per frame would add about 0.2 ms of pure instrumentation |
| `GetStatId`: `RETURN_QUICK_DECLARE_CYCLE_STAT(UUpdateBudgetSubsystem, STATGROUP_Tickables)` | Required by `FTickableGameObject` (pure virtual). It gives this tick a named entry in `stat` and Insights |
| `DoesSupportWorldType` → `Game || PIE` (`protected`) | The engine default also includes `Editor` worlds (`WorldSubsystem.cpp:31`). The override keeps the subsystem, and in S5 its tick function, out of the editor |

### B4. `UBDemoBrainComponent.h` / `.cpp` (game module): the workload

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `UENUM() enum class EUBBrainState : uint8` | `enum class`: scoped and strongly typed, no implicit int conversion. `uint8` keeps it 1 byte. `UENUM` makes it reflectable for debugging and future UPROPERTYs |
| `UCLASS(ClassGroup = (UpdateBudget))` without `Abstract` | This is the concrete subclass agents use |
| `InitPatrol(Center, Phase)` | Called **between** `SpawnActorDeferred` and `FinishSpawning`, so the patrol exists before `BeginPlay` and before the first tick |
| `UPROPERTY(EditAnywhere, Category = "Workload", meta = (ClampMin = "0")) float PatrolRadius = 300.f;` | `EditAnywhere`: tunable per instance and on the class default in the editor. `Category`: grouping in the Details panel. `ClampMin`: the editor refuses negative values. `float`, because centimetre-scale gameplay math is float. **300 cm** gives a 6 m wide circle: big enough to see motion from the camera, small enough for dense grids. **Not `BlueprintReadWrite`:** nothing in Blueprint needs it, and exposing it would invite changes mid-benchmark |
| `PatrolAngularSpeed = 0.5f` (rad/s) | 0.5 rad/s × 3 m = 1.5 m/s, a walking pace. One lap takes about 12.6 s, so a 60 s capture covers about 5 laps |
| `AlertRadius = 1500.f`, `AlertConeCos = 0.5f` | 15 m awareness and a 60° half-angle cone. This gives a realistic mix of Patrol and Alert as the camera passes. The cone is stored as a **cosine** so the per-update test needs no `acos` |
| `ComputeIterations = 128` | A stand-in for heavier logic, **calibrated in S2** (09, S2-f): at N = 1,000 it makes B0 game-thread bound (median 5.21 ms, against 2.46 ms with no compute, where the frame was render-bound), so a game-thread improvement shows up in frame time. About 20 ns per iteration per agent on this machine |
| `GetPatrolAngle(double)` (private, inline) | One formula, used by `GetPatrolPosition` and `OnBudgetedUpdate`. Previously it was copied into two functions (simplify audit) |
| `OnBudgetedUpdate`: `PatrolTime += DeltaSeconds;` | Gameplay that integrates time, the kind of code that needs the accumulated-delta contract. An agent updated after 0.3 s walks 0.3 s worth |
| `Cos`/`Sin` once → `Position` and `Forward(-Sin, Cos, 0)` | Two trig calls per update instead of four. `Forward` is the derivative of the circle (the counter-clockwise tangent). Done before S2 calibration, so the calibrated cost measures the final code |
| `GetWorld()->GetFirstPlayerController()` … `PlayerCameraManager.Get()` | "Where is the player?" is asked **per agent, as real perception code would**. The efficiency audit priced it at about 10–20 ns per update, the same in every mode. It is kept on purpose: caching it once per frame would make the workload unrealistically cheap. `.Get()` turns the `TObjectPtr` into a raw pointer once |
| `DistanceSquared < Square(AlertRadius)` | Squared distances avoid a square root in the common "far away" case |
| `Dot(Forward, ToPlayer) > AlertConeCos * Sqrt(DistanceSquared)` | The same test as `Dot(Forward, Normalized(ToPlayer)) > cos`, without a division (efficiency audit). The square root is only computed when the agent is near enough to matter (`&&` short-circuits) |
| `if (NewState != State) { State = NewState; ++StateChanges; }` | Counts transitions, used later to report decision churn |
| `GetOwner()->SetActorLocationAndRotation(Position, FacingDirection.Rotation())` | **One** transform update per call, not separate location and rotation calls that would each update the component and its render state |
| Compute loop with `Sink` → `ComputeSink` | A fixed amount of floating-point work. It writes to a member, so the compiler cannot prove the result unused and delete the loop. Without the store, an optimising build could remove the "work" entirely and the benchmark would measure nothing |

### B5. `UBDemoAgent.h` / `.cpp` (game module)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `PrimaryActorTick.bCanEverTick = false;` | The actor itself does nothing per frame; all gameplay is in the brain. Its tick function is **never registered** (`Actor.cpp:1690`), so it adds no tick-manager cost |
| `CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))` | Creates a default subobject: a component every instance gets, created in the constructor. The name must be unique within the class |
| `RootComponent = Mesh;` | The mesh defines the actor's transform. `SetActorLocationAndRotation` moves it |
| *(no `SetMobility`)* | Scene components default to `Movable` (`SceneComponent.cpp:124`), which a moving agent needs. The explicit call was redundant (simplify audit) |
| `SetCollisionEnabled(NoCollision)`, `SetGenerateOverlapEvents(false)` | The workload measures **gameplay logic**, not physics. No collision means no physics bodies are created and moving costs no physics updates |
| `SetCastShadow(false)` | Cuts GPU shadow cost for thousands of moving cubes. The benchmark targets the game thread |
| `static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));` | Loads the engine cube once. `static` means the lookup runs once for the class, not per instance. **Only valid inside a constructor.** The asset is cooked through `DirectoriesToAlwaysCook` (A7) |
| `CreateDefaultSubobject<UUBDemoBrainComponent>(TEXT("Brain"))` | The workload component. It registers with the subsystem in its own `BeginPlay` |

### B6. `UBBenchmarkSubsystem.h` / `.cpp` (game module): the director *(as of S2)*

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `class UUBBenchmarkSubsystem : public UTickableWorldSubsystem` | **Changed from the plan** (02 §13): the project has no hand-authored map, so nothing would place a director or spawner actor. A world subsystem has exactly the world's lifetime and replaces both. *Tickable* since S2, because it drives the camera and the run timeline every frame. That work is benchmark control, not measured gameplay, and it is identical in every configuration |
| `struct FRunSettings { … }` (private, plain struct) | The `-UB…=` values, read **once** into one place, with their defaults beside them. A plain struct, not a USTRUCT: nothing reflects it |
| `enum class ERunPhase : uint8 { Idle, WarmUp, Capture, Done }` | The run timeline as a small state machine. `Idle` means no workload was requested, so the subsystem does nothing (safe in normal play) |
| `static FRunSettings ReadRunSettings()` | Reads the command line once (simplify audit). `FParse::Param(…, TEXT("UBExit"))` is a flag with no value. `FParse::Value` leaves a field untouched when the key is absent, so the defaults stand |
| `ValidateRunSettings()` | Logs an **Error** when the capture is ≤ 0 s, the warm-up is < 0 s, or `-UBWaveAt` falls outside the capture. A misconfigured run then fails the batch (`run_bench.py` counts log errors) instead of silently measuring the wrong thing (S2 review) |
| `EffectiveComputeIterations` | Read back from the first agent's brain and written to the CSV metadata and report. "−1 = default" would hide a recalibration of the default (S2 review) |
| `OnWorldBeginPlay`: spawn, `SpawnActor<ACameraActor>()`, `Phase = WarmUp` | Runs before any actor's `BeginPlay` (`World.cpp:6167` vs `6174`). The camera is a plain engine `ACameraActor`, so no new class is needed |
| `CSV_METADATA(TEXT("UBRunTag"), …)` and the rest | Writes the run description **into the CSV file itself** (keys arrive lower-case, e.g. `ubruntag`). A capture can never be separated from what produced it |
| `SpawnAgents(World, Count, Seed)` | The S1 spawner, now able to append, which the wave needs. `ArenaHalfExtent` keeps the largest grid so the camera path covers it. `Phase0` is renamed from `Phase` to avoid shadowing the run phase |
| `UpdateCamera(Elapsed - WarmupSeconds)` | A circle at half the arena's half-width (at least 15 m), 4 m up, pitched down 15°, **one lap per capture window**. The angle is **0 at the planned BenchStart** (negative during warm-up), so the captured path never depends on the warm-up length; the S2 review found the earlier begin-play timing did. It is analytic, so every run sees exactly the same path. Agents near the camera change continuously, which is what gives the scheduler's significance something real to react to |
| `if (PC && PC->GetViewTarget() != Camera) PC->SetViewTarget(Camera);` every frame | The player controller may possess its default pawn after begin play, which resets the view target. Re-applying it every frame is robust and costs one pointer compare |
| `Tick` → `WarmUp`: at `Elapsed >= WarmupSeconds` → `CSV_EVENT(UBBench, TEXT("BenchStart"))` | The CSV profiler compares event **text** (without category) case-insensitively with `-csvStartOnEvent=` (`CsvProfiler.cpp:4769`). So this event **starts the capture**: warm-up frames are never in the file |
| `Capture`: the optional wave | At `WaveAt` seconds into the capture, `N` more agents spawn with seed + 1, doubling the load, and a `Wave` event marks the frame. Used for the "load doubles" stress test (01 §6.1) |
| `Capture`: at `CaptureSeconds` → `CSV_EVENT(BenchEnd)`, `Phase = Done` | `-csvStopOnEvent=BenchEnd` stops the capture **at the end of this frame** (the stop is queued and handled in `EndFrame`). The game owns the whole window, so the run script needs no frame count |
| `Done`: `WriteRunReport()` on the **first** Done frame | Written *after* the capture has stopped. The S2 review found that writing it on the BenchEnd frame put an N-dependent string build and disk write inside the last measured frame |
| `Done`: `RequestExit(false, …)` after `ExitDelayFrames = 3` | The CSV profiler writes its file the frame after capture ends (log: "Capture Ended. Writing CSV"). Waiting three frames lets it finish before shutdown. `false` means a normal, non-forced exit |
| `WriteRunReport()` | One CSV line per agent: final position, patrol time, last update time, **age at report** (−1 if never updated), state changes. A `#` header line records the run settings, the BenchStart time, the report time and the **effective** compute iterations. It feeds SC6 (positions vs the reference) and the end-of-run staleness check that catches agents that were *never* updated. `Report.Reserve(...)` avoids repeated string growth for thousands of lines |
| `IsValid(Agent)` before use | Agents can be destroyed during a run. The S1 review nit is now enforced where the list is used |
| `FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UBRuns"), Tag + ".csv")` | In the packaged build this resolves to `Build\Windows\UpdateBudget\Saved\UBRuns\`, where `run_bench.py` collects it |

---

## Part C · Step S2: the measurement tools (`Tools\`)

### C1. `csv_stats.py`

| Part | What, why |
|---|---|
| `csv.field_size_limit(1 << 30)` | Unreal's metadata row has one very long field (the command line and every metadata pair). Python's default 128 KB limit raised an error on the first real capture |
| `load_capture` | A finalised capture ends with the **full header** and a `[key],value,…` metadata row. Both are dropped, and **the end header is used for column lookup**: with `HasHeaderRowAtEnd`, columns can appear mid-capture, so the first header is only a *prefix* (seen on a real capture: 282 vs 286 columns). Finalised means the last row starts with `[` and the first header is a prefix of the end header; otherwise every row is data and the result is flagged `finalised: False`. **Two bugs found and fixed in S2:** (1) metadata keys were emptied (stripping up to `]`), caught by `tag=?`; (2) the first finalised check compared the two headers for equality, which rejected valid captures |
| `frame_events` / `--split-event` | The `EVENTS` column holds `Category/Name##timestamp;…`. A wave run is split at the `Wave` frame into **pre**, **event frame** and **post** statistics, so steady state at the new load isn't mixed with the spawn hitch (S2 review) |
| `percentile` | Linear interpolation between closest ranks: position = p/100 × (n−1), the same as numpy's default. Documented in the file so any reader can reproduce a number |
| `summarize` | n, mean, min, median, p95, p99, max |
| `--selftest` | Checks the maths against hand-calculated values (1…10: median 5.5, p95 9.55, p99 9.91) before any benchmark trusts it |
| `--skip N` | Drops the first N frames. Only needed for captures that start at boot. Event-started captures use 0 |

### C2. `run_bench.py`

| Part | What, why |
|---|---|
| `COMMON_ARGS` | The same flags for every run: 1280×720 windowed on the **real GPU** (`GameThreadTime` is 0 under `-nullrhi`, S1 finding), fixed step `-deterministic -FPS=60` (identical simulated frames in every configuration, `01b` B-1), `-novsync`, `-traceautostart=0`, `-UBExit`, and the event-driven capture window |
| `CONFIGS` | Config name → extra arguments. B0 = nothing; B1 = `-ExecCmds="tick.AllowBatchedTicks 1"`; B2 = `-UBBaseline=B2`; B3 = `-UBBaseline=B3` (added in S3, Part D). **UB is added only when the code implementing it exists** (S6). A config is never listed before its code exists, so a batch can't silently measure B0 under another name |
| `environment_record()` | CPU, GPUs and drivers, RAM, OS build, **power plan and AC power** (thermal noise control on a laptop), engine version and changelist, build configuration, compiler, resolution, flags. Stored with every batch ([Benchmarks/README.md](../Benchmarks/README.md)) |
| Command built as **one string** | So `-ExecCmds="…"` reaches Unreal exactly as written. Python's list quoting would have wrapped the whole argument in quotes, which Unreal's parser doesn't expect |
| `GameUserSettings.ini` deleted before each run | It can override VSync and frame caps at startup (`01b` §4) |
| Interleaving | The config order **rotates each round** (B0,B1 / B1,B0 / …), so laptop thermal drift doesn't systematically favour one configuration |
| Names | `<batch>_<Config>[_W<work>]_N<agents>_r<run>`, with `<batch>` = `YYYY-MM-DD_HHMMSS[_label]`. **Raw files are never overwritten** (`unique_path` raises). The S2 review found that date-only names let a second batch on the same day silently replace the first |
| Priming run | One discarded run before the batch warms file, driver and PSO caches, so the first measured run isn't the only cold one (S2 review). The number of runs per combination is a multiple of the number of configs, so with the rotation each config leads equally often (E5) |
| Artefact collection | Only files written **after this run started** are taken: the CSV, the per-agent report, and the log, whose modification time is checked so a run that never started can't inherit the previous log (S2 review). Analysis errors are caught per run. **The manifest is rewritten after every run**, so a crash loses nothing |
| Timeout | `warmup + capture + 180 s`; a hung run is recorded (`exit_code: null`) instead of hanging the batch |
| Failure rule | A run fails if it has a non-zero exit, a timeout, no or partial capture, a missing log, log errors, or an analysis error. Failed runs are listed as **ATTENTION** and the script exits non-zero. Each run's **PSO events** are counted, so shader-compile noise is visible |
| `write_summary` | For each (config, work, N): the **median across runs** of each run's median, p95 and p99, with the **[min–max] across runs**, as `Benchmarks\README.md` defines |

---

## Part D · Step S3: the native baselines B2 and B3

Two baselines that a competent team would write without UpdateBudget. They exist so the scheduler is compared with good native practice, not only with "every agent ticks every frame" (01 §5, success criterion SC4). Both use the same bands, the same view and the same setter, and both are timed.

### D1. `UBBaselineDriverSubsystem.h` / `.cpp` (game module)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `class UUBBaselineDriverSubsystem : public UTickableWorldSubsystem` (`UCLASS()`, no `_API`) | The world's lifetime, and a `Tick` after `TG_PostPhysics`. Not exported: only this module uses it. **Why a subsystem and not an actor:** the project has no hand-authored map (02 §13), and a subsystem exists in every game world automatically |
| `DoesSupportWorldType` → `Game \|\| PIE` | The same rule as the other subsystems: never in editor worlds |
| `Initialize`: `Benchmark = Collection.InitializeDependency<UUBBenchmarkSubsystem>();` **before** `Super::Initialize` | Tickable world subsystems join the tick list inside `Initialize` (`WorldSubsystem.cpp:94–101`), so initialisation order is tick order (06 §19). The benchmark subsystem therefore ticks first: it spawns the wave, and this driver adopts the new agents **in the same frame**, identically in every build. The returned pointer is kept (efficiency audit), so there is no subsystem lookup per frame |
| `UPROPERTY(Transient) TObjectPtr<UUBBenchmarkSubsystem> Benchmark;` | `UPROPERTY` so the garbage collector sees the reference. `Transient`: runtime state, never saved |
| `namespace UBBaseline { constexpr double NearDistance = 1500.0; FarDistance = 6000.0; MidInterval = 0.1f; FarInterval = 0.5f; }` | The band edges are **the same 15 m / 60 m that UpdateBudget uses** (02 §7), so the comparison is like for like. `constexpr`: typed compile-time constants with no storage. Distances in centimetres (Unreal units) as `double`, because 5.x world positions are double |
| `constexpr float BelowOne = 0.99999994f;` | The largest float below 1. A Mid-band significance like 1 − 1e-9 is below 1 in `double` but **rounds to exactly 1.0f**, which would put the agent in the Near band. The clamp to `BelowOne` prevents it |
| `const FName SignificanceTag(TEXT("UBAgent"));` | The Significance Manager groups objects by tag. `UnregisterAll(Tag)` uses it at world end |
| `enum class EBand : uint8 { Near, Mid, Far }` (private) | The three bands as a type, so both baselines speak the same language and a typo can't produce a fourth band |
| `BandForDistanceSquared(DistanceSquared)`: `<= Square(Near)` → Near; `< Square(Far)` → Mid; else Far | **The one edge test** both baselines use (altitude audit). Squared distances need no square root. The edges are explicit: exactly 15 m is Near, exactly 60 m is Far. Before the audit B2 compared squared distances and B3 compared distances, two tests that agreed only by rounding luck |
| `BandForSignificance(Significance)`: `>= 1` → Near; `> 0` → Mid; else Far | The inverse mapping, for B3: the Significance Manager hands back a number, and this turns it back into a band |
| `IntervalForBand(Band)`: `switch` → `0.f` / `MidInterval` / `FarInterval` | `TickInterval = 0` means "tick every frame". 0.1 s means about every 6th frame at 60 FPS, and 0.5 s about every 30th |
| `SignificanceForDistanceSquared`: Near → `1.f`, Far → `0.f`, Mid → `FMath::Clamp(static_cast<float>(1.0 - FMath::GetRangePct(Near, Far, FMath::Sqrt(DistanceSquared))), TNumericLimits<float>::Min(), BelowOne)` | A linear fall-off across the Mid band gives the manager a meaningful **order** (it sorts objects by significance). `GetRangePct` is the engine's `(Value − Min) / (Max − Min)` (reuse audit). The square root is taken only in the Mid band. The clamp keeps Mid strictly inside (0, 1) after the double → float conversion (`TNumericLimits<float>::Min()` is the smallest positive float), so `BandForSignificance` returns Mid for it: **B3's bands equal B2's by construction** |
| `OnWorldBeginPlay`: `FParse::Value(FCommandLine::Get(), TEXT("UBBaseline="), Requested); Requested.ToUpperInline();` | The mode is read **once**, at world start, and never changes during a run. `FString ==` already compares case-insensitively; the upper-case copy is for the log and the CSV metadata |
| B3: `if (USignificanceManager::Get(&InWorld))` | `Get` is a map lookup by world. The manager is created in `OnPreWorldInitialization`, after world subsystems initialise, so it can't be fetched in `Initialize` (06 §17). It exists by begin play |
| `UE_LOG(…, Error, …)` for a missing manager or an unknown value | A misconfigured run **fails the batch** (`run_bench.py` counts log errors) instead of silently running B0 under the name B3 |
| `CSV_METADATA(TEXT("UBBaseline"), *Requested);` + `Display` log | The capture records which baseline produced it. Verified in the S3 smoke captures: metadata `ubbaseline = B2 / B3`, and the log line `Baseline B2 active.` |
| `OnWorldEndPlay`: `UnregisterAll(SignificanceTag)`, `Brains.Empty()`, `Mode = None`, then `Super` | Runs after every actor's `EndPlay` (`World.cpp:6203–6210`), so the agents have normally unregistered themselves already. This is the safety net, run **while the manager still exists**: it is removed in `OnPostWorldCleanup`, before `Deinitialize`, which would be too late |
| `GetSignificanceManager()` → the manager in B3 mode, else `nullptr` | One rule, used at every call site (simplify audit). **Looked up each time, not cached:** the manager is removed during world cleanup, so a cached raw pointer could dangle. The lookup is one map find per call site per frame |
| `GetViewTransform`: first player controller → `PlayerCameraManager` → `GetCameraCacheView()` → `FTransform(ViewInfo.Rotation, ViewInfo.Location)` | The **same view the brains sense from**, so every consumer sees an identical camera. `GetCameraLocation()` and `GetCameraRotation()` both just return fields of this cached view (`PlayerCameraManager.cpp:774–782`), so one call replaces two virtual calls (reuse audit). Returns `false` before a camera exists, and that frame's re-tune is skipped |
| `SetIntervalIfChanged`: `if (Brain.GetComponentTickInterval() != Interval) { Brain.SetComponentTickIntervalAndCooldown(Interval); ++FrameIntervalChanges; }` | The setter that applies **at once** (06 §18): the plain setter would make a far agent that comes close wait out its old 0.5 s cooldown. Called **only on change**, because each call reschedules the tick function in the cooldown list (H6). Calling it every frame for every agent would make the baseline a strawman. The count is published as `UBBaseline/IntervalChanges` |
| `UFUNCTION() void HandleAgentEndPlay(AActor*, EEndPlayReason::Type)` | Bound to each agent's `OnEndPlay` with `AddDynamic`, which binds by name through reflection, so it must be a `UFUNCTION` (06 §20). **B3:** `UnregisterObject(Brain)`, because the manager holds a raw pointer and would keep calling our callbacks on a destroyed object. **B2:** `Brains.RemoveSingleSwap(Brain, EAllowShrinking::No)`, which stops at the first match (each brain is listed once) and fills the hole with the last element instead of shifting the array |
| `AdoptNewAgents`: `const TArray<…>& Agents = Benchmark->GetAgents();` and `if (Agents.Num() == AdoptedAgents …) return;` | A reference, not a copy. On a normal frame the whole adoption costs one comparison |
| `!ensureMsgf(Agents.Num() > AdoptedAgents, …)` | Agents are adopted **by index**, which is correct only because the benchmark's list is append-only. That contract is now written on `GetAgents()` (D3). If a future change ever compacted the list, adoption would silently skip agents and the baseline would be wrong; the `ensure` reports it instead (altitude audit). An `ensure` logs a call stack once and continues; it isn't a crash |
| The significance lambda: `[](FManagedObjectInfo* Info, const FTransform& View) -> float { … SignificanceForDistanceSquared(FVector::DistSquared(Owner->GetActorLocation(), View.GetLocation())); }` | **Runs on worker threads** (`Update` uses `ParallelFor`). It captures nothing and only reads transforms, at a point in the frame (after `TG_PostPhysics`) where nothing writes them |
| The post lambda: `[this](Info, OldSignificance, NewSignificance, bFinal)` with `EPostSignificanceType::Sequential` | `Sequential` runs on the **game thread** after the parallel pass, where changing tick intervals is allowed; `Concurrent` would run it on worker threads. `bFinal` is `true` only for the call made inside `UnregisterObject` (`SignificanceManager.cpp:325–327`), which is ignored |
| `if (!bFinal && NewBand != BandForSignificance(OldSignificance))` | Only a **band change** touches the component (efficiency audit). That's what a competent Significance Manager user writes, because the manager hands over both values. It holds from the very first call: the manager starts every object at significance 1 (`SignificanceManager.h:72`) and registration passes `Old = 1` (`.cpp:236`). That is the Near band, which matches a new brain's interval of 0 |
| Both lambdas are defined **once**, before the loop | Each `RegisterObject` copies them into its own `TFunction`. Defining them inside the loop rebuilt the same objects per agent (simplify audit) |
| `if (!SignificanceManager) Brains.Reserve(Agents.Num());` | B2 only: one allocation for the initial set, and one for the wave, instead of repeated growth |
| The loop from `AdoptedAgents`: `IsValid(Agent)` → `GetBrain()`; `OnEndPlay.AddDynamic(…)`; B3 `RegisterObject(Brain, Tag, …, Sequential, …)`, else `Brains.Add(Brain)` | Each agent is bound **once**, when it's adopted. B3's brains live in the manager, B2's in `Brains`, so neither mode pays for the other's bookkeeping |
| `Retune()`: no view → return; B3 → `SignificanceManager->Update(MakeArrayView(&View, 1)); return;`; B2 → loop over `Brains` | Early returns keep each mode's path flat (simplify audit). `MakeArrayView(&View, 1)` passes the single viewpoint as an array view without building an array. Epic's design expects **the game** to call `Update` (header lines 26–27). The B2 loop: `IsValid(Brain)`, then distance² to the camera → band → interval → `SetIntervalIfChanged` |
| `Tick`: `if (Mode == EMode::None) return;` | In B0, B1 and UB the driver costs one comparison per frame |
| `{ CSV_SCOPED_TIMING_STAT(UBBaseline, Register); AdoptNewAgents(); }` | One-off registration (the initial set and the wave frame) is timed **separately** from the per-frame re-tuning, so a spawn frame doesn't pollute the steady-state cost. In the CSV the column is **`UBBaseline/GameThread/Register`**: the profiler inserts the thread name for timing stats (seen in the S3 smoke captures) |
| `FrameIntervalChanges = 0; { CSV_SCOPED_TIMING_STAT(UBBaseline, Retune); Retune(); }` | The re-tuning is part of these approaches' real cost, so it is measured (H6). CSV column **`UBBaseline/GameThread/Retune`**. In B3 this scope includes the manager's parallel pass and the post callbacks |
| `CSV_CUSTOM_STAT(UBBaseline, IntervalChanges, FrameIntervalChanges, ECsvCustomStatOp::Set);` | One value per frame: how many agents changed band. Together with `Retune` it separates "how often" from "how expensive" |
| `GetStatId` → `RETURN_QUICK_DECLARE_CYCLE_STAT(…, STATGROUP_Tickables)` | Required by `FTickableGameObject`; gives the tick a named entry in `stat` and Insights |
| **Not in this class:** the cost of the engine's cooldown list | An agent that has ever had a non-zero interval stays on the tick manager's cooling-down list and is rescheduled every frame (`TickTaskManager.cpp:1538`), even after returning to 0. That cost is inside `GameThreadTime` (so the comparison stays fair) but outside `Retune`. It is examined with Unreal Insights (09, H6) |

### D2. Build and project files for S3

| File | Line | Why |
|---|---|---|
| `UpdateBudget.uproject` | `{ "Name": "SignificanceManager", "Enabled": true }` | The Significance Manager is an engine **plugin**, off by default. Without it the module can't load |
| `UpdateBudget.Build.cs` | `PrivateDependencyModuleNames.AddRange(new string[] { "SignificanceManager" });` | Private: only `.cpp` files include its header, so modules that depend on ours don't inherit it |
| `Tools\run_bench.py` | `"B2": "-UBBaseline=B2"`, `"B3": "-UBBaseline=B3"` | One command-line value selects the mode. Everything else on the command line is identical to B0 |

### D3. `UBBenchmarkSubsystem.h`: the agent-list contract

| Line | Why |
|---|---|
| `/** The spawned agents, in spawn order. Append-only: an index stays valid for the world's lifetime … */ const TArray<TObjectPtr<AUBDemoAgent>>& GetAgents() const` | A contract that another class relies on is **written where the data lives**, not only where it's used (altitude audit). Destroyed agents stay in the list as invalid entries; every user checks `IsValid()` |

---

## Part E · Step S4: the scheduling policy, and its tests

The policy is plain C++: no UObject, no world, no engine tick. It is given numbers (deadlines, a time, a budget) and a function that runs one client, so every rule can be tested with fake clients and fake costs (02 §4.1). The subsystem that feeds it real clients arrives in S5.

### E1. `UpdateBudgetScheduler.h` (plugin, public)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "Containers/Array.h"`, `"Containers/ArrayView.h"`, `"Templates/Function.h"` | Only what the header uses (`TArray`, `TConstArrayView`, `TFunctionRef`). No `CoreMinimal.h`, no `UObject` headers: the policy has nothing to do with reflection, which keeps it cheap to include and easy to test |
| `#define UE_API UPDATEBUDGETCORE_API` … `#undef UE_API` | The same export pattern as the component (A6, 5.8 convention): each exported function is marked, and the short name is undefined at the end so it can't leak into files that include this one |
| `struct FBudgetClientState { double Deadline = 0.0; int32 CategoryIndex = 0; };` | **The hot data.** A dense array of these is all the policy reads, so the scheduling loop touches no UObject memory (H5). `double`, because world time is `double` in 5.x and seconds since start lose float precision after a few hours. Only the two fields the policy reads exist; `LastUpdateTime` and `MaxStaleness` come in S5/S6 with the code that sets them (02 §4.1) |
| `struct FBudgetFrameStats { SpentMs, Runs, MandatoryRuns, Deferred, bOverBudget }` | One frame's outcome, returned by value (five fields, no allocation). `MandatoryRuns` and `bOverBudget` are what SC1 needs: frames where mandatory work forced an overrun are counted separately. `Deferred` shows how much work waited |
| `using FRunClient = TFunctionRef<double(int32 ClientIndex)>;` | **How the policy runs a client without knowing what a client is.** `TFunctionRef` is a non-owning reference to any callable: no heap allocation, no copy, only valid during the call (06 §22). The callable returns the measured cost in ms: the subsystem times the real update, and tests return made-up costs |
| `explicit FUpdateBudgetScheduler(int32 NumCategories = 1, double InitialCostMs = 0.005, double SmoothingAlpha = 0.2)` | `explicit` stops an `int` silently converting into a scheduler. **0.005 ms** is about one demo update on this machine (09, S2-f: about 5.2 µs), so the first frame's prediction is realistic. **0.2** is the design's smoothing weight (02 §7) |
| `Run(TConstArrayView<FBudgetClientState> Clients, double NextFrameTime, double BudgetMs, FRunClient RunClient)` | A **view**, not a copy: the subsystem's array is read in place. `NextFrameTime` = now + this frame's delta (02 §6): "would this client be late if it waited one frame?" needs the time of the next chance to run |
| `GetPredictedCostMs(CategoryIndex)` | Read-only access for tests and, later, the debug view |
| `static double SanitiseCostMs(double)` (private) | **Every** cost that enters a prediction passes here: the measured ones in `Run` and the starting one in the constructor (simplify, altitude angle: the constructor was a second way in for NaN or infinity). Negative and non-finite values count as 0 |
| `ClampCategory(...)` (private, inline) | An index the scheduler doesn't have falls back to category 0, so a settings change can never index out of range |
| `TArray<int32> Order;` | The sort buffer is a **member**, reused every frame. After the first frame (or when N grows) `SetNumUninitialized` reuses the same memory, so the steady state allocates nothing |

### E2. `UpdateBudgetScheduler.cpp`

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| Constructor: `Alpha(FMath::Clamp(SmoothingAlpha, 0.0, 1.0))`; `PredictedCostMs.Init(SanitiseCostMs(InitialCostMs), FMath::Max(1, NumCategories))` | Bad arguments are made safe instead of producing negative or NaN predictions or an empty array. `Init(Value, Count)` fills the array with `Count` copies |
| `Order.SetNumUninitialized(Clients.Num(), EAllowShrinking::No);` then `Order[Index] = Index` | Sizes the buffer without zero-filling (every slot is written on the next line) and without giving memory back when N shrinks |
| `const FBudgetClientState* const State = Clients.GetData();` then `Order.Sort([State](int32 A, int32 B) { … DeadlineA < DeadlineB \|\| (DeadlineA == DeadlineB && A < B); })` | **Earliest deadline first.** One O(N log N) sort per frame (v1, 02 §12 item 6; Phase 8 measures it at N = 4,000).<br>**Raw pointer reads** (simplify, efficiency angle): every index is in [0, N) by construction, so the view's per-access range check, which is compiled into Development builds, only added cost. That was about 15% of the sort time, roughly 45–75 µs per frame at N = 4,000 on a clang copy of UE's introsort; to be confirmed in the MSVC build in Phase 8. Each deadline is now read once per comparison.<br>**Ties are broken by index**, because `TArray::Sort` is not stable: without it, two clients with the same deadline could swap places between runs and break determinism |
| `while (NumMandatory < Order.Num() && Clients[Order[NumMandatory]].Deadline < NextFrameTime) ++NumMandatory;` | Because the order is sorted, **the mandatory clients are exactly a prefix**: one sort gives both sets. Counted **before** anything runs, because `RunClient` moves the deadline of the client it updates. The test is `<`, not `<=`: a client due exactly at the next frame can still run on time then |
| `if (Position >= NumMandatory && Stats.SpentMs + PredictedCostMs[Category] > BudgetMs) break;` | **The budget cut-off.** Don't start an update that is predicted not to fit. It is `break`, not `continue`: skipping one client to run a cheaper one with a *later* deadline would break deadline order. Mandatory clients skip this test, so they always run (no starvation) |
| `const double CostMs = SanitiseCostMs(RunClient(ClientIndex));` | Runs the client. A negative measured cost (a clock oddity) counts as 0 rather than *giving back* budget. **A non-finite one counts as 0 too** (code review): `FMath::Max(0.0, NaN)` is NaN, a NaN prediction would stay NaN for good, and every later `Spent + NaN > Budget` is false, which would silently switch the budget off |
| `PredictedCostMs[Category] = FMath::Lerp(PredictedCostMs[Category], CostMs, Alpha);` | **The moving average**: move a fraction α of the way towards the newest measurement. `FMath::Lerp(A, B, α)` is exactly `A + α · (B − A)` (`Math/UnrealMathUtility.h:1123–1126`), the same as `(1 − α) · old + α · new` with one multiply, so the name now says what it does (simplify, reuse angle). With α = 0.2 a step change is about 89% absorbed after 10 updates (1 − 0.8¹⁰). Mandatory runs also teach the prediction |
| `Stats.MandatoryRuns = NumMandatory;` | Set once: the loop only ever stops at an optional client, so every mandatory client has run. A per-iteration counter would always end equal to `NumMandatory` (simplify) |
| `Stats.Deferred = Clients.Num() - Stats.Runs; Stats.bOverBudget = Stats.SpentMs > BudgetMs;` | Everything not run waits for a later frame. Over budget is a fact about the frame, whatever caused it |

### E3. `Private\Tests\UpdateBudgetSchedulerTests.cpp`

| Part | What it proves |
|---|---|
| `#if WITH_DEV_AUTOMATION_TESTS` … `#endif` | Tests compile into Development builds and disappear from Test and Shipping (`UEBuildTarget.cs:6316`), so they cost a shipped game nothing |
| `constexpr EAutomationTestFlags Flags = EditorContext \| ClientContext \| ProductFilter;` (in `UpdateBudgetTestFlags.h` since S5) | **Where** a test may run (editor and game) and **which group** it belongs to (project tests). The macro refuses at compile time a test without exactly one filter flag (`AutomationTest.h:4127–4133`) |
| `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClass, "UpdateBudget.Scheduler.Name", Flags)` + `bool FClass::RunTest(const FString&)` | Declares a test class and registers it under a dotted name, which is what `Automation RunTests UpdateBudget.Scheduler` filters on. `TestEqual` / `TestTrue` record a failure and keep going, so one run reports every broken rule |
| `NoClients` | Zero clients: nothing runs, nothing breaks, no over-budget flag |
| `DeadlineOrder` | Five clients with a tie: run order is `1, 3, 4, 2, 0`, earliest deadline first, the tie broken by index |
| `BudgetCutOff` | Ten 0.1 ms clients and a 0.45 ms budget: exactly the four earliest run, 0.4 ms spent, six deferred, not over budget |
| `MandatoryBeatsBudget` | Five clients all due before the next frame and a budget for one: all five run, and the overrun is reported |
| `ZeroBudget` | A budget of 0 still runs every mandatory client, most overdue first, and nothing else |
| `CostPrediction` | One step of the moving average is exact (1.0 → 0.8); a steady cost is learned (error far below 10⁻⁹ after 100 frames); a negative measurement counts as 0; **a NaN measurement leaves the prediction and the spent time finite, and the budget cut-off still works on the next frames** (`std::numeric_limits<double>::quiet_NaN()`, because `TNumericLimits` has no NaN in 5.8) |
| `CategoryFallback` | An unknown category reads and teaches category 0, and nothing crashes |
| `NoStarvation` | **The guarantee itself, under real overload**, simulated: 200 clients (max staleness 0 s, 0.1 s and 0.5 s) for 600 fixed 60 Hz frames.<br>**The overload:** a 0.3 ms budget at 0.01 ms per update. The 20 near clients take 0.2 ms, leaving room for 10 optional updates per frame against a demand of about 16.7 (80/6 + 100/30). The code review found that the first version (0.5 ms) never actually overloaded, so the test now **asserts that mid/far clients were forced to run**.<br>**The checks:** no client ever waits longer than its max staleness, or one frame when that is shorter; every client is updated; near clients run every frame; and every frame that ran optional work stayed inside the budget.<br>**Why the bound holds:** a client becomes mandatory on the first frame where waiting would make it late. On the previous frame it was not yet due, so it always runs at or before its deadline |

### E4. `Tools\run_tests.bat`

| Line | Why |
|---|---|
| `UnrealEditor-Cmd.exe "<project>" -ExecCmds="Automation RunTests %FILTER%;Quit"` | The automation commands are parsed in `AutomationCommandline.cpp` (`RunTests` at :610, `Quit` at :726). `Quit` waits until the test queue is empty, then exits with **a non-zero code if any test failed** (:495–504, the log line `TEST COMPLETE. EXIT CODE`). So the script's exit code is the verdict |
| `-unattended -nullrhi -nosound -nosplash -nopause` | No dialogs, no GPU, no audio: pure logic tests need none of them, and they run unattended through the job queue |
| `-ReportExportPath="…\Saved\Automation\Reports"` | Writes a JSON report (`AutomationControllerManager.cpp:233`) into the gitignored `Saved\` folder; the results are copied into 09. The older name `-ReportOutputPath=` still works but logs a deprecation warning (`:213–219`), which the first S4 run showed |

### E5. `Tools\run_bench.py`: changes made in S3/S4

| Part | What, why |
|---|---|
| `performance_core_mask()` | Asks Windows which logical CPUs belong to the **performance cores**: `GetLogicalProcessorInformationEx(RelationProcessorCore)` returns one record per physical core, with an `EfficiencyClass` byte (offset 9) and a group affinity mask (offset 32). P-cores have the higher class. On this i7-14650HX the result is `0xFFFF`: 8 P-cores × 2 hardware threads = logical CPUs 0–15. It returns `None` on a non-hybrid CPU, so the script still runs anywhere |
| `KERNEL32 = ctypes.WinDLL("kernel32")` with its prototypes, set once behind `sys.platform == "win32"` | One place for every Windows call. `restype`/`argtypes` are declared, so 64-bit handles and masks aren't truncated by ctypes' default `int`. Off Windows, `KERNEL32` is `None` and the pinning is skipped |
| `pin_self(mask)`, called once in `main()` before anything is recorded or started | `SetProcessAffinityMask(GetCurrentProcess(), mask)`. **Every process started afterwards inherits the mask from its first instruction** (simplify, altitude angle; verified on this machine: a child of a process pinned to `0xFFFF` reports `0xffff`). This replaced pinning each game after `Popen`, which left a start-up window and needed a per-run failure state. If the pin fails on a hybrid CPU, the batch refuses to start |
| `applied_mask()` | Reads back the mask Windows actually applied (`GetProcessAffinityMask`), so the environment record states what happened, not what was asked for |
| `EXE` = `Build\Windows\UpdateBudget\Binaries\Win64\UpdateBudget.exe` | The **real** game executable, launched directly. The packaged bootstrap one folder up only starts this one. Launching it directly makes the game a direct child, so its **exit code is real** and a hung or interrupted run is always killed (`taskkill /T` in a `finally` block). The first pinned version used `cmd /c start /wait /affinity`, which **always returns 0**. P03 found this while testing the same idea, and it would have hidden every failed run from `is_failure` |
| `"cpu_affinity"` in the environment record | The mask applied to the whole batch, read back after pinning. Every run of the batch inherits it |
| `MIN_RUNS = 4`; `--runs` default `math.ceil(MIN_RUNS / len(configs)) * len(configs)` | The smallest multiple of the number of configs that is at least 4: 4 for B0–B3, 4 for two configs, 5 with UB. With the rotation, every config sits in every order position equally often. A `--runs` that isn't a multiple prints a warning (code review: the first version gave a one-config batch a single run). The rule is stated once, in the `--runs` help (built from `MIN_RUNS`) and `Benchmarks/README.md`; the module docstring points there |
| `SetThreadExecutionState(ES_CONTINUOUS \| ES_SYSTEM_REQUIRED \| ES_DISPLAY_REQUIRED)` | While the batch process lives, Windows keeps the display and system awake, as it does for a video player. A locked or occluded desktop could throttle a windowed game's presentation. **No power setting is changed**, and the request ends with the process |

### E6. `Tools\analyse_baselines.py`

| Part | What, why |
|---|---|
| Reads the batch manifest, then each run's raw CSV through `csv_stats.load_capture` | The same loader as the summaries; raw files are never modified. Unfinished captures are skipped. **Per-run `GameThreadTime` and `UpdatesRun` statistics are taken from the manifest**, which `run_bench.py` computed with the same `csv_stats` code, so the summary and the analysis can't drift apart (simplify, reuse angle; bit-identical on all 64 runs). The CSV is read only for what the manifest doesn't hold: the per-frame series and the driver columns, through `csv_stats.column_values` and `csv_stats.summarize` |
| Groups by `(config, work, agents)`, then re-keys by `(config, N)` | The same key as `run_bench.py`'s summary, so different workloads are never pooled (code review). A batch with several workloads is refused, after which the workload is the same everywhere and the tables loop over `(config, N)` directly. `table()` and `col()` build every header, separator and cell, so a column count can't get out of step with its separator |
| **H1** table: B0 − B1 per run, paired **by run index** | Runs of the same round ran next to each other, so pairing them cancels slow drift. Pairing is by the run number both configs have, not by list position: a skipped run would otherwise shift every later pair (code review) |
| **H4** table: autocorrelation of `UpdatesRun` at lags 6 and 30, and max / median | At the fixed 60 Hz step, lag 6 = 0.1 s and lag 30 = 0.5 s, the two band intervals. Values near 1 mean the load repeats with that period: agents given the same interval at the same moment stay in phase |
| **H6** table: `UBBaseline/GameThread/Retune` median and p99, interval changes per frame | The baselines' own re-tune cost. The tick manager's cooling-down list cost is inside `GameThreadTime`, not in this column |
| Optional drift check (`split_round`) | Rounds 1..split against the later rounds: shows whether the machine itself changed during a batch |

---

## Part F · Step S5: the tick function, the registry and the budgeted path

S5 connects the S4 policy to real components. After S5, `ub.Enabled 1` makes the scheduler run every update; until S6 adds significance, every client's max staleness is 0, so each client is still updated exactly once per frame (02 §6, S5 notes). The lifetime rules come from reading the 5.8.4 source, and each one has a test (F6).

### F1. `UpdateBudgetTickFunction.h` / `.cpp` (plugin, public)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "Engine/EngineBaseTypes.h"` | Declares `FTickFunction`, `ELevelTick`, `ETickingGroup` and `TStructOpsTypeTraits`. The base must be complete to derive from it |
| `USTRUCT() struct FUpdateBudgetTickFunction : public FTickFunction` + `GENERATED_BODY()` | `FTickFunction` is itself a USTRUCT (`EngineBaseTypes.h:182–183`), and the engine's own tick functions derive from it as USTRUCTs (`FActorTickFunction`, `:565–608`), as Niagara's does (`NiagaraWorldManager.h:47–69`). It is **not** a UPROPERTY of the subsystem: nothing needs to reflect or serialise it. It is a USTRUCT so it matches its base and the engine's pattern |
| `UE_API FUpdateBudgetTickFunction();` and `UE_API` on the three overrides | The 5.8 export pattern (A6): the members are exported one by one, like `ENGINE_API` on `FActorTickFunction`'s members |
| `UUpdateBudgetSubsystem* Subsystem = nullptr;` | A raw back-pointer to the owner, exactly like `FActorTickFunction::Target` (`EngineBaseTypes.h:570–575`). It needs no GC reference: the subsystem *contains* this struct, so the struct can't outlive it |
| `ExecuteTick(float DeltaTime, ELevelTick, ENamedThreads::Type, const FGraphEventRef&) override` | The one function the tick manager calls (`EngineBaseTypes.h:524`, pure virtual). `DeltaTime` is the world's delta for this frame: for a function with no interval, `CalculateDeltaTime` returns it unchanged (`TickTaskManager.cpp:2825–2846`) |
| `if (ensure(Subsystem)) Subsystem->RunFrame(DeltaTime);` | The subsystem sets the pointer before it registers the function, so a null here is a bug. `ensure` reports it once and skips the frame instead of crashing. `RunFrame` is private; this struct is the subsystem's `friend`, so nothing else can run a frame of updates |
| `DiagnosticMessage()` → `"UUpdateBudgetSubsystem::RunFrame"`; `DiagnosticContext()` → `"UpdateBudget"` | Pure virtual in the base (`:532`). The tick manager prints them in `dumpticks` and in cycle reports. `DiagnosticContext` returns a `static const FName`, built once instead of on every call |
| Constructor: `TickGroup = TG_PrePhysics; EndTickGroup = TG_PrePhysics;` | Both are already the defaults (`TickTaskManager.cpp:2361–2363`); they are **stated** because the fair comparison depends on them (the native baselines tick in `TG_PrePhysics`, B2). `EndTickGroup` = start group means "must finish within the group it started in", which a synchronous game-thread function always does |
| *Not set:* `bCanEverTick`, `bStartWithTickEnabled` | `RegisterTickFunction` reads neither (`TickTaskManager.cpp:2406–2427`): only actor and component setup do (`ActorComponent.cpp:1739–1750`). Setting them would suggest they matter. The subsystem enables or disables the function explicitly |
| `template<> struct TStructOpsTypeTraits<FUpdateBudgetTickFunction> … { enum { WithCopy = false }; };` | Tells the reflection system this struct can't be copied. `FTickFunction` deletes its copy assignment because the tick manager holds raw pointers to registered functions (`EngineBaseTypes.h:550–551`). **Without it** the generated struct operations would try to copy the struct and fail to compile |

### F2. `UpdateBudgetScheduler.h`: one new field

| Line | Why |
|---|---|
| `double LastUpdateTime = -1.0;` in `FBudgetClientState` | The per-frame deadline loop needs each client's last update time. Reading it from the component would touch N UObjects per frame (H5), so `RecordUpdate` mirrors it here, in every mode. **Negative = never updated**: then `Deadline = LastUpdateTime + MaxStaleness` is below any world time, so a new client is mandatory in its first frame, like a native tick that runs on the first frame. It is the **last** member, so the S4 tests' aggregate initialisation `{ Deadline, 0 }` is unchanged (04 §16). The struct grows from 16 to 24 bytes |

### F3. `BudgetedUpdateComponent.h` / `.cpp`: lifecycle and mode (S5 changes)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `bool IsRegisteredWithBudget() const` / `bool IsDrivenByScheduler() const` | Read-only state for tests and, in S7, the debug view. They expose facts, not the link itself |
| `BeginPlay`: `Super::BeginPlay(); JoinBudget();` | Unchanged behaviour, now through one helper. The component's tick function is already registered at this point (`Actor.cpp:4822–4825` registers it right before `BeginPlay`), which matters for F4's `Register` |
| `EndPlay`: `LeaveBudget(); Super::EndPlay(…)` | As in S1; `LeaveBudget` is safe to call twice, because `OnUnregister` may already have run |
| `OnRegister()` override: `Super::OnRegister(); if (HasBegunPlay()) JoinBudget();` | **Re-registration while playing** (`ReregisterComponent`, or a re-attach) unregisters and re-registers the component without a second `BeginPlay`; the engine re-registers its tick functions in `HandleRegisterComponentWithWorld` (`Actor.cpp:6443–6451`). Joining here keeps the budget in step. A **first** registration happens before `BeginPlay` (`HasBegunPlay()` is false), so it doesn't join twice |
| `OnUnregister()` override: `LeaveBudget(); Super::OnUnregister();` | **Streaming removal** unregisters every actor's components (`Level.cpp:2184–2213`, calling `UnregisterAllComponents`) and routes `EndPlay(RemovedFromWorld)` only afterwards (`World.cpp:4196–4208` before `4244–4251`). Both steps are time-sliced, so they can be frames apart. The native tick is unregistered at the first step (`ActorComponent.cpp:2147`). Without this override, budgeted mode would keep updating a component its level had already taken out of the world. Leaving **before** `Super` matches `EndPlay`: leave while still fully valid |
| `JoinBudget()` / `LeaveBudget()` (private) | One place for each direction, used by four lifecycle hooks. `LeaveBudget` copies the `TObjectPtr` into a raw local once (B2's efficiency note) |
| `SetDrivenByScheduler(bool)` (private; only the subsystem calls it, as a friend) | Sets the mode flag, `PrimaryComponentTick.bStartWithTickEnabled = !bDriven`, and `SetComponentTickEnabled(!bDriven)`. **Why both tick calls:** `SetComponentTickEnabled` moves the tick function between the tick manager's enabled and disabled sets (`TickTaskManager.cpp:2439–2461`; a disabled function costs nothing per frame, only the enabled set is walked, `:1479–1485`). But whenever the tick function is registered again, the engine enables it if `bStartWithTickEnabled` is set (`ActorComponent.cpp:1747`). Without the second line, a re-registered component would tick natively **and** be scheduled: two updates per frame |
| `TickComponent`: `if (bDrivenByScheduler) { SetComponentTickEnabled(false); return; }` **before** `Super` | The safety net. `Activate()` re-enables the tick (`ActorComponent.cpp:2815–2824`), and so could any other code. The guard turns it off again on its first native call and returns without updating, so the client is still updated exactly once per frame, by the scheduler. Turning off your own tick function from inside it is allowed: "Disabling during a tick frame will stop it from executing unless it is already in progress" (`EngineBaseTypes.h:357–364`). Cost in the native modes: one `bool` test per tick |
| `bool bDrivenByScheduler = false;` | Plain bookkeeping, not a UPROPERTY: it is runtime state set by the subsystem |

### F4. `UpdateBudgetSubsystem.h` / `.cpp`: the mechanism (rewritten in S5)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "UpdateBudgetScheduler.h"`, `"UpdateBudgetTickFunction.h"` | The subsystem holds both by value, so their full definitions are needed |
| `static TAutoConsoleVariable<int32> CVarUpdateBudgetEnabled(TEXT("ub.Enabled"), 0, TEXT("…"), ECVF_Default);` | The mode switch (02 §4.1), introduced in S5 (02 §13). `int32` like the engine's own switches (`tick.AllowBatchedTicks`, `TickTaskManager.cpp:59–62`). **`ECVF_Default`, not `ECVF_Cheat`:** cheat cvars can't be set from ini files (`01b #11`), and a budgeted run sets this one through the ini system (`-ini:Engine:[ConsoleVariables]:ub.Enabled=1`, 06 §26). Default 0 keeps every existing baseline unchanged |
| `namespace UpdateBudget { constexpr double DefaultBudgetMs = 2.0; constexpr double UnscoredMaxStalenessSeconds = 0.0; }` | Two S5 stand-ins with names that say so. 2.0 ms is the design default (02 §7); S6 replaces it with the settings value and `ub.BudgetMs`. **Max staleness 0** makes every client due every frame, so S5's budgeted path does exactly B0's work, which is what S5 checks; S6 replaces it with significance |
| `DEFINE_LOG_CATEGORY_STATIC(LogUpdateBudget, Log, All);` | The plugin's log category returns (A6 said it would when the code first logs). `STATIC` keeps it private to this file |
| `OnWorldBeginPlay`: `bBudgeted = CVar…GetValueOnGameThread() != 0;` | The mode is read **once per world**, before any actor's `BeginPlay` (`World.cpp:6165–6174`), so every client registers into a known mode. S7 adds the live switch |
| The loop `ApplyMode(*Client)` over `Clients` | Normally empty here. If something registered before the mode was read, it gets the mode too, so it can never be updated both natively and by the scheduler. S7's live switch will reuse the same helper |
| `TickFunction.Subsystem = this; TickFunction.SetTickFunctionEnable(bBudgeted); TickFunction.RegisterTickFunction(InWorld.PersistentLevel);` | Registered in **every** mode, so S7's switch only has to enable it; disabled, it costs nothing per frame. **The enable state is set first**: on an unregistered function `SetTickFunctionEnable` only stores the state (`TickTaskManager.cpp:2457–2460`), and registration then puts it straight into the right set, as Niagara does (`NiagaraWorldManager.cpp:462–463`). **Registered in `OnWorldBeginPlay`, never `Initialize`:** a registered function starts ticking at once (`01b #2`). On the **persistent level**, because a tick function lives in one level's tick list, and the persistent level lives as long as the world (Mass: `MassProcessingPhaseManager.cpp:485–486`) |
| `PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UUpdateBudgetSubsystem::HandlePostGarbageCollect);` | **Added after the S5 code review.** GC nulls the entry of a component collected without `EndPlay` (`01b #8`). Before this hook only the scheduler's loop ever found such entries, so in the native modes a dead slot stayed for the rest of the run and the CSV `Clients` count was too high. The delegate fires after every collection (`UObjectGlobals.h:3490`, broadcast at `GarbageCollection.cpp:5943`), the engine's own `UWorld` and `UNetDriver` bind to it the same way (`World.cpp:1765`, `NetDriver.cpp:839`), and `Deinitialize` removes it. One O(N) `IsValid` pass per collection, roughly once a minute, in every mode |
| `HandlePostGarbageCollect()`: `if (bRunningScheduler) bHasInvalidClients = true; else RemoveInvalidClients();` | Collections run between frames (`LevelTick.cpp:1968–1971`), so normally the arrays can change at once. If an update forced a collection inside the loop, the entries are only marked, and the loop removes them when it ends: the arrays never change under the policy |
| `UE_LOG(LogUpdateBudget, Display, "… mode (ub.Enabled %d). Update timer resolution: %.1f ns.", …)` | One line per world says which mode is running, so a benchmark log proves its mode (09, S5). `GetSecondsPerCycle64()` × 10⁹ is the tick length of the timer used in `RunClient`, recorded so the precision of every measured cost is known |
| `Register`: `if (Client->Subsystem) { ensure(Client->Subsystem == this); return; }` | Idempotent: registering twice, or while the add is still pending, does nothing. A link to a **different** subsystem would be a bug, which the `ensure` reports |
| `ApplyMode(*Client);` **before** the add; `ApplyMode`: `if (Client.IsDrivenByScheduler() != bBudgeted) Client.SetDrivenByScheduler(bBudgeted);` | **One rule in one place** (simplify: `OnWorldBeginPlay` and `Register` had two forms of it). Applied at once, even when the add itself is deferred. A component spawned inside an update has just had its native tick registered and enabled, which would run it in a later tick group of this frame (`EngineBaseTypes.h:334–341`). Disabling it also removes it from that newly-spawned list (`TickTaskManager.cpp:1906–1909`). **Compared, not only set in budgeted mode** (S5 code review): a client that was scheduler-driven in another world and joins a native one must get its tick back, or nothing would ever update it. In the normal native case the comparison is false and nothing is done |
| `if (bRunningScheduler) PendingAdds.Add(Client); else AddClient(Client);` | **Why defer:** the policy reads `States` through a raw pointer (05 E2). `TArray::Add` may reallocate, and the policy would then read freed memory. `PendingAdds` is a UPROPERTY, so GC sees the waiting clients |
| `Unregister`: `if (Index == INDEX_NONE) PendingAdds.RemoveSingleSwap(Client, …)` | A client that registered and unregistered inside the same loop never reaches the arrays: the add and the removal cancel out |
| `else if (… Clients[Index] == Client) { if (bRunningScheduler) { Clients[Index] = nullptr; bHasInvalidClients = true; } else RemoveClientAt(Index); }` | **Deferred removal nulls the slot**: no element moves, so every index the policy holds stays valid, and the loop's `IsValid` check skips the slot. Between frames, the O(1) swap-remove runs at once |
| `DetachClient(Client);` at the end of `Unregister`; `DetachClient` is `static` and accepts null | The link is cleared **in every case**, so a component never points at a registry it isn't in. **Null-tolerant** (simplify): every caller that walks the arrays may meet an entry GC has already cleared, and the four `if (Client)` checks that came with it are gone |
| `AddClient`: `Client->RegistryIndex = Clients.Add(Client); States.AddDefaulted_GetRef().LastUpdateTime = Client->GetLastUpdateTime();` | Both arrays grow together, so the index is shared. The hot state starts from the client's **own** last update time: a client re-registered after updates keeps its staleness, and one that waited in `PendingAdds` loses nothing. `AddDefaulted_GetRef` (`Array.h:3067`) constructs the element in place and returns it |
| `RemoveClientAt`: `Clients.RemoveAtSwap(…); States.RemoveAtSwap(…);` then fix the moved entry's `RegistryIndex` | The S1 swap-remove, now on both arrays with the same index, so they stay parallel. The null check on the moved entry is kept (S1 review) |
| `RemoveInvalidClients()`: backwards loop, `if (!IsValid(Client)) { DetachClient(Client); RemoveClientAt(Index); }` | Removes the slots nulled during the loop **and** any client that became invalid without `EndPlay` (garbage, or nulled by GC). **Backwards**, because a swap moves the last element into `Index`, and the last element has already been checked. A garbage component's link is cleared, so a later `EndPlay` finds nothing to do |
| `RunFrame`: `for (FBudgetClientState& State : States) State.Deadline = State.LastUpdateTime + UnscoredMaxStalenessSeconds;` | **Step 1** of 02 §6: deadlines from the dense array only. S6 adds significance here. One edge (S5 code review): in a frame where world time doesn't advance (`DeltaSeconds` = 0), a client updated last frame has `Deadline == NextFrameTime`, which is not mandatory under S4's strict `<`, so only the budget decides who runs. Nothing is overdue in such a frame (an update would receive 0 s), and fixed-step and normal play never produce one; kept as is |
| `bRunningScheduler = true; Scheduler.Run(States, Now + DeltaSeconds, DefaultBudgetMs, [this](int32 Index) { return RunClient(Index); }); bRunningScheduler = false;` | **Step 2.** `States` converts to the `TConstArrayView` the policy takes, without a copy. `Now + DeltaSeconds` is the next frame's start (02 §6). The lambda converts to the policy's `TFunctionRef` (06 §22). The S4 stats are not yet used; S6 publishes them |
| `if (bHasInvalidClients) RemoveInvalidClients();` then `AddClient` for each valid entry of `PendingAdds` (an invalid non-null one is detached), then `PendingAdds.Reset()` | **Step 3**, right after the loop: nothing reads the arrays any more. The removal pass runs only when something was invalid, so a normal frame doesn't pay an O(N) scan. A waiting client that became garbage is **detached** rather than dropped silently (S5 code review), so it can't keep reporting itself as registered. `Reset` keeps the allocation for the next frame (`Array.h:2469`) |
| `RunClient`: `if (!IsValid(Client)) { bHasInvalidClients = true; return 0.0; }` | The **`IsValid` guard** (`01b #8`): null (unregistered during this loop, or nulled by GC) or garbage (`Object.h:1886`). It costs the budget nothing |
| `const uint64 StartCycles = FPlatformTime::Cycles64(); Client->RunUpdate(); return FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);` | The measured cost the budget is spent on. `Cycles64()` is `QueryPerformanceCounter` on Windows (`WindowsPlatformTime.h:39–44`); `ToMilliseconds64` converts with the measured frequency (`GenericPlatformTime.h:201–205`). Integer cycles avoid `Seconds()`'s large offset (`WindowsPlatformTime.h:22–29`). **Two timer reads per update** are the budgeted path's own overhead; one read per update is a Phase 8 candidate (11) |
| `RecordUpdate` (inline): `States[Client.RegistryIndex].LastUpdateTime = UpdateTime;` when the index isn't `INDEX_NONE` | **The sync point the S1 design reserved** (B2). It runs in every mode, so the cost is equal and a later switch (S7) starts from correct times. `INDEX_NONE` means the client is waiting in `PendingAdds`, for example if it ran its own update in `BeginPlay`; `AddClient` copies the time when the add happens |
| `Deinitialize`: `TickFunction.UnRegisterTickFunction();` and the GC delegate removed first, then one loop `for (… Registry : { &Clients, &PendingAdds })` detaches every client in both arrays, then all three arrays are emptied, then `Super` | The tick function must not stay registered once the subsystem is going (`01b` §2). `~FTickFunction` would unregister it too (`TickTaskManager.cpp:2396–2399`), but only when the memory is freed, after GC. Deinitialize runs during world cleanup (`World.cpp:6486`) |
| `friend struct FUpdateBudgetTickFunction;` and `RunFrame`/`RunClient` private | Only the tick function can run a frame. A second `RunFrame` call per frame would update everyone twice |

### F5. `Private/Tests/UpdateBudgetTestTypes.h` / `.cpp`

| Line(s) | Why |
|---|---|
| A header in `Private/Tests/` | UHT reads only headers, so reflected test types need one. The module's `Private` folder is on its include path (`UEBuildModuleCPP.cs:471–473`), hence `#include "Tests/UpdateBudgetTestTypes.h"`. Like the engine's own test classes in `Engine/Private/Tests/` (for example `AutoRTFMTestCameraShake.h:8`), they compile in every configuration; they are tiny and never spawned outside the tests |
| `UCLASS(NotBlueprintable, HideDropdown)` | Hidden from class pickers and not usable as a Blueprint base, the same specifiers the engine uses on its test classes |
| `TFunction<void(UUpdateBudgetTestComponent&)> OnUpdate;` called from `OnBudgetedUpdate` | Each test decides what happens **inside** an update: destroy, spawn, unregister. A `TFunction` owns its callable (06 §22: a `TFunctionRef` must not be stored) |
| `AUpdateBudgetTestActor`: no tick, one `CreateDefaultSubobject<UUpdateBudgetTestComponent>` | The smallest actor that carries a client. Not a scene component, so no root component is needed |

### F6. `Private/Tests/UpdateBudgetLifetimeTests.cpp` and `UpdateBudgetTestFlags.h`

| Part | What it proves |
|---|---|
| `UpdateBudgetTestFlags.h`: `UpdateBudgetTests::Flags` | The test flags (E3) in one header that both test files include (simplify: the lifetime tests had a copy). A filter change now reaches every suite. It is a separate header because the scheduler tests must not pull in the UHT test types and `GameFramework/Actor.h` |
| `using FAction = TFunction<void(UUpdateBudgetTestComponent&, int32 Update)>;` and `Spawn(Log, Index == K ? Action : FAction())` | Each test defines its in-update action **once** and gives it only to the client that performs it (simplify: every client used to capture a "am I the one?" flag) |
| `FTestBudgetWorld` | The engine's `FTestWorldWrapper` (06 §28) plus the mode. **Member order is deliberate**: `FTestConsoleVariable Enabled` is declared first, so it is destroyed **last**, after the world (04 §18). Restoring the cvar earlier would change the mode under a living world once S7 adds the live switch |
| `Enabled.Set(…)` before `CreateTestWorld`/`BeginPlayInTestWorld` | The mode is read in `OnWorldBeginPlay`, inside `BeginPlayInTestWorld` (`AutomationCommon.cpp:185`) |
| `FUpdateLog` declared **before** the world in every test | The components' lambdas write into it by reference, so it must outlive the world |
| Counts kept in `FUpdateLog`, never read from a destroyed component | `UWorld::Tick` may collect garbage at its end (`LevelTick.cpp:1968–1971`), so a destroyed component may be freed after any test tick |
| `UpdatesOutOfPlay`: an update when `!HasBegunPlay() \|\| !IsRegistered()` | The definition of a **stale entry**: the registry updated something that had left play or left the world. Every test asserts it stays 0 |
| `Spawn` after `BeginPlayInTestWorld` | The actor begins play inside `SpawnActor` (`Actor.cpp:4509–4529`), so spawn order is registry order. With equal deadlines the policy runs clients in index order (S4 `DeadlineOrder`), which makes "before" and "after" in a frame exact |
| `ModeAtBeginPlay` | Native and budgeted worlds: 4 clients registered; driven-by-scheduler and native-tick state match the mode; **exactly 3 updates in 3 frames** in both modes |
| `DestroyDuringUpdate` | In its 2nd update, client 2 destroys client 0 (already run this frame), **itself**, and client 5 (not run yet). Client 5 is skipped (1 update), 3 clients remain, the survivors keep exactly one update per frame |
| `SpawnDuringUpdate` | Client 1 spawns one client and spawns-then-destroys another inside its update. The kept one is registered after the loop, driven by the scheduler with its native tick off, and first updated **next frame**; the cancelled one never registers (02 §12 item 7) |
| `LevelUnloadOrder` | The streaming-removal order: `UnregisterAllComponents` first (once between frames, once from inside an update, which is deferred), then `RouteEndPlay(RemovedFromWorld)` later. The client stops updating at the first step, and the later `EndPlay` changes nothing |
| `NoDoubleUpdate` | `ReregisterComponent()` keeps the client registered with its native tick off. `Activate(true)` really turns the native tick on (asserted), and the guard turns it off again. Both clients get exactly 4 updates in 4 frames |
| `WorldTeardown` | A frame whose updates spawn and destroy clients, then the world's `EndPlay` (0 clients, no component keeps a link), then `DestroyTestWorld(true)`: `Deinitialize`, a full garbage collection, and a weak pointer proving the subsystem is gone. The engine never ends a world inside a tick group (02 §8), so this is the worst real case of "end play mid-frame" |
| `Test.Wrapper.ForwardErrorMessages(this)` | The wrapper records its own failures (for example a missing game mode) without logging them; this turns them into test errors |
