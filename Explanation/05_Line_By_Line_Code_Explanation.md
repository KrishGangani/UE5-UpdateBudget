# 05 · Line By Line Code Explanation: UpdateBudget

**Rule:** every file that exists in `UpdateBudget\` and `Tools\` is explained here, line by line, and the explanation changes in the same commit as the code (Hard rule 13).

**How to read it.** Since Phase 6 this file is organised **by file, as the code is now**. During Phase 5 it grew one part per build step, so a file was explained in several places and later parts amended earlier ones; Phase 6 merged those into one pass. Where a line exists because of a step, a review or a measurement, the row says so in brackets: *(S6)*, *(S5 code review)*, *(S7 simplify)*. The history of each step stays in 09 and 03.

Section ids use one letter per part: **A** project and build files, **P** the plugin, **G** the game module, **T** the tests, **M** the measurement tools. They are chosen so that they can't be confused with the baselines **B0–B3**, the hypotheses **H1–H6**, the success criteria **SC1–SC7** or the build steps **S1–S8**.

| Section | File(s) | What it is |
|---|---|---|
| A1–A2 | `UpdateBudget.uproject`, `Source\*.Target.cs` | The project and its two build targets |
| A3–A4 | `Source\UpdateBudget\UpdateBudget.Build.cs`, `UpdateBudgetModule.cpp` | The game module's build rules and entry point |
| A5–A6 | `UpdateBudgetCore.uplugin`, `UpdateBudgetCore.Build.cs`, `UpdateBudgetCoreModule.cpp` | The plugin descriptor, build rules and entry point |
| A7 | `Config\*.ini` | Engine, game and input configuration |
| A8 | `Tools\*.bat` | Build, smoke run, package, automation tests |
| P1 | `UpdateBudgetScheduler.h` / `.cpp` | The scheduling policy: plain C++, unit-tested |
| P2 | `UpdateBudgetSignificance.h` | Significance, the view test, the category rule |
| P3 | `UpdateBudgetSettings.h` / `.cpp` | Project Settings |
| P4 | `UpdateBudgetTickFunction.h` / `.cpp` | The tick function that runs the scheduler in `TG_PrePhysics` |
| P5 | `BudgetedUpdateComponent.h` / `.cpp` | The client component |
| P6 (a–h) | `UpdateBudgetSubsystem.h` / `.cpp` | The registry, one budgeted frame, live switching, telemetry, the debug view |
| G1–G2 | `UBDemoBrainComponent`, `UBDemoAgent` | The workload |
| G3 | `UBBenchmarkSubsystem` | The benchmark director |
| G4 | `UBBaselineBands.h`, `UBBaselineDriverSubsystem` | The baselines B2 and B3 |
| T1–T7 | `Private\Tests\*` in both modules | The 28 automation tests |
| M1–M5 | `Tools\*.py` | Running batches and computing statistics |

**Old ids → new ids.** 09 and older commits refer to the step-by-step ids. They map as follows: A1–A8 → A1–A8; B1, B2, F3, H3, I1 → P5; B3, F4, G4, H1, H2, H4, I2 → P6; B4 → G1; B5 → G2; B6, D3 → G3; C1 → M2; C2, E5 → M3; D1 → G4; D2 → A1, A3; E1, E2, F2, G3 → P1; E3 → T3; E4 → A8; E6 → M4; F1 → P4; F5, I3 → T2 (and A6); F6, H5 → T4; G1 → P2; G2 → P3; G5 → M3, M5; G6 → T5; I4 → T6.

---

## Part A · Project, build and config files (Phase 4; later additions are marked with their step)

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
| `"SignificanceManager"` (S3) | Baseline B3 needs Epic's Significance Manager, an engine plugin that is off by default (A3, G4) |
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

		// SignificanceManager is private: only UBBaselineDriverSubsystem.cpp (baseline B3) uses it.
		PrivateDependencyModuleNames.AddRange(new string[] { "SignificanceManager" });
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
| `PrivateDependencyModuleNames … "SignificanceManager"` (S3) | Only a `.cpp` includes its header (`UBBaselineDriverSubsystem.cpp`, baseline B3), so modules that depend on ours don't inherit its include path. The plugin must also be enabled in the `.uproject` (A1), or the module can't load |
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

```csharp
public class UpdateBudgetCore : ModuleRules
{
	public UpdateBudgetCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// DeveloperSettings is public: the public header UpdateBudgetSettings.h derives from UDeveloperSettings.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });
		// Editor builds only: the S8 test builds a Blueprint in code (UpdateBudgetBlueprintTests.cpp, inside WITH_EDITOR).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph" });
		}
	}
}
```

| Line | What it does, and why |
|---|---|
| `Core`, `CoreUObject`, `Engine` (public) | The same pattern as A3. The plugin's public headers include engine types (`UActorComponent`, `UTickableWorldSubsystem`, `FTickFunction`), so any module that includes them needs these include paths |
| `"DeveloperSettings"` (public, S6) | `UpdateBudgetSettings.h`, a **public** header, derives from `UDeveloperSettings`, which lives in its own module. Added in S6 with the settings class (P3), not before |
| `if (Target.bBuildEditor) … "UnrealEd", "BlueprintGraph"` (private, S8) | The S8 test builds and compiles a Blueprint in code (T6), which needs the editor's Kismet utilities (`UnrealEd`) and graph nodes (`BlueprintGraph`). **Editor targets only**: a packaged game never links them, and the test sits inside `#if WITH_EDITOR`. *Planned change (owner decision, Phase 9):* a separate editor-only test module, so that even editor builds of the runtime module don't link editor modules |
| `IMPLEMENT_MODULE(FDefaultModuleImpl, UpdateBudgetCore);` (module `.cpp`) | The non-primary version of A4: an ordinary module entry point. `FDefaultModuleImpl` is the empty default implementation; the plugin needs no startup or shutdown logic |
| *History:* `LogUpdateBudget` | A log category added in S1 was **removed by the S1 simplify audit**, because nothing logged yet. It came back in S5, as a static category in the subsystem's `.cpp`, when the scheduler first logged |

The build log line `[Adaptive Build] Excluded from UpdateBudgetCore unity file: …` is normal. UBT compiles recently changed files on their own instead of inside the "unity" batch file, to speed up iteration.

### A7. `Config\DefaultEngine.ini`

| Section / key | Why |
|---|---|
| `GameMapsSettings: GameDefaultMap / EditorStartupMap = /Engine/Maps/Templates/Template_Default` | A shipped engine map with a floor and lights. No map has to be authored in the editor, so the project stays fully buildable from text files. The template's `OpenWorld` map was rejected: it is World Partition, and streaming would add measurement noise |
| `[/Script/Engine.Engine] bSmoothFrameRate=False, bUseFixedFrameRate=False` | Both are already the engine defaults (`BaseEngine.ini`). They are **restated** so nobody can turn on smoothing or a frame cap later without it showing in a diff. Either one would make frame time stop reflecting real work |
| `[SystemSettings] r.VSync=0` | Already the engine default. Restated for the same reason: VSync makes Present wait for the monitor |
| `RendererSettings`, `WindowsTargetSettings` | Copied from Epic's 5.8 template: DX12 with Shader Model 6, virtual shadow maps, no static lighting. These are standard modern defaults, not benchmark choices |

`DefaultGame.ini` sets the project name and description, plus (since S1) `+DirectoriesToAlwaysCook=(Path="/Engine/BasicShapes")`. The agent's cube mesh is referenced **only from C++** (`ConstructorHelpers`). No map references it, so without this line the cooker would leave it out and packaged agents would be invisible. `DefaultInput.ini` is Epic's template file, unchanged.

**Engine-written block (removed in S4):** on the first editor run, the engine's `AndroidFileServerEditor` plugin had appended an `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` section, with a random `SecurityToken`, to `DefaultEngine.ini`. It configures Epic's Android device file server and does nothing on Windows. Since S4 the plugin is disabled in the `.uproject` (A1) and the block is gone, so the token is no longer written or committed. The earlier commits still contain the old token; it guards nothing here, because no Android build of this project exists.

### A8. `Tools\*.bat`: build, smoke run, package, automation tests

| Script | Key lines | Why |
|---|---|---|
| `build.bat [Target] [Config]` | `call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project=… -WaitMutex -NoHotReloadFromIDE` | A reproducible command-line build. `-WaitMutex` waits if another build is already running instead of failing. `%~dp0` is the script's own folder, so the script works from any working directory. The usage line lists Development, Shipping and DebugGame: the launcher engine can't build Test (Phase 4) |
| `smoke_game.bat` | `UnrealEditor-Cmd.exe <uproject> -game -nullrhi -nosound -unattended -traceautostart=0 -ExecCmds="quit" -abslog=…` | Runs the uncooked game with no window and no GPU, loads the default map, then quits. `-traceautostart=0` stops an open Insights from tracing it (`01b` §4) |
| `package.bat [Config]` | `RunUAT.bat BuildCookRun … -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory=…` | Builds, cooks, stages, paks and archives a standalone game. **The default is Development**, because the launcher engine can't build Test (Phase 4 finding) |
| `run_tests.bat [Filter]` (S4) | `UnrealEditor-Cmd.exe "<project>" -ExecCmds="Automation RunTests %FILTER%;Quit"` | The automation commands are parsed in `AutomationCommandline.cpp` (`RunTests` at `:610`, `Quit` at `:726`). `Quit` waits until the test queue is empty, then exits non-zero if any test failed: it sets `GIsCriticalError`, logs `TEST COMPLETE. EXIT CODE: -1` and requests the exit with that status (`:493–504`). So the script's exit code is the verdict. The default filter `UpdateBudget` runs every test of the project, the game module's included, because all their names start with it |
| | `-unattended -nullrhi -nosound -nosplash -nopause` | No dialogs, no GPU, no audio: the tests need none of them, and they run unattended through the job queue |
| | `-ReportExportPath="…\Saved\Automation\Reports"` | Writes a JSON report (`AutomationControllerManager.cpp:233`) into the gitignored `Saved\` folder; the results are copied into 09. The older name `-ReportOutputPath=` still works but logs a deprecation warning (`:213–219`), which the first S4 run showed |
| Every script | one engine command, then `exit /b %ERRORLEVEL%` | The engine tool's exit code is the script's exit code, so the job queue sees real failures. A chain of steps would need a check after each one: under `cmd /c`, `call step.bat \|\| exit /b` can exit with 0 after a failure |

**Phase 4 results (2026-11-20):**
- The editor target builds with **0 errors and 0 warnings** (MSVC 14.44.35229).
- The smoke test loads `Template_Default` in 0.15 s, mounts and loads both modules, and exits with code 0. The log has 0 errors; its warnings are all engine-side `LogEditorDataStorageUI`, from running the editor binary in `-game`.
- The packaged build result is recorded in `09_Testing_And_Benchmarks.md`.

---

## Part P · The plugin, `UpdateBudgetCore` (the reusable system)

Six pieces, in dependency order: the **policy** (P1, plain C++), **significance** (P2, plain C++), the **settings** (P3), the **tick function** (P4), the **client component** (P5) and the **subsystem** (P6) that connects them every frame. Only P5 and P6 touch actors and worlds; P1 and P2 are pure functions on plain data, which is why most of the policy is tested without a world (Part T).

### P1. `UpdateBudgetScheduler.h` / `.cpp`: the scheduling policy (S4; `LastUpdateTime` in S5, `Significance` and `MandatoryMs` in S6)

The policy is given numbers (deadlines, a time, a budget) and a function that runs one client. It knows nothing about components, worlds or the engine tick, so every rule can be tested with fake clients and fake costs (02 §4.1).

**The header**

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "Containers/Array.h"`, `"Containers/ArrayView.h"`, `"Templates/Function.h"` | Only what the header uses (`TArray`, `TConstArrayView`, `TFunctionRef`). No `CoreMinimal.h` and no `UObject` headers: the policy has nothing to do with reflection, which keeps it cheap to include and easy to test |
| `#define UE_API UPDATEBUDGETCORE_API` … `#undef UE_API` | The 5.8 export pattern (`01b` §5): each exported function is marked `UE_API`, and the short name is undefined at the end so it can't leak into files that include this one. UBT defines `UPDATEBUDGETCORE_API` for every file that can see the module: `DLLEXPORT` inside the module's own DLL, `DLLIMPORT` in other binaries, and empty in a monolithic (packaged) build (`UEBuildModule.cs:223`, `:750–770`) |
| `struct FBudgetClientState` | **The hot data**: a dense array of these is all the policy reads, so the scheduling loop touches no UObject memory (H5). Plain struct, not a USTRUCT: nothing reflects or saves it |
| `double Deadline = 0.0;` | World time (s) by which the client must next be updated: last update + max staleness, set by the subsystem every frame (P6). `double`, because world time is `double` in 5.x, and seconds since start lose `float` precision after a few hours |
| `int32 CategoryIndex = 0;` | Which cost prediction the client uses (one per settings category). An index out of range means category 0 (`ClampCategory`) |
| `float Significance = 1.f;` (S6) | This frame's significance, written by the deadline loop. **The policy doesn't read it**: the SC3 count does. It sits in the 4 bytes of padding after `CategoryIndex`, so the struct stays at 24 bytes (8 + 4 + 4 + 8). *Changed from the plan* (02 §13): 02 said S6 would add `MaxStaleness`, but nothing reads the max staleness once the deadline is set, and SC3 needs "near", which significance gives directly |
| `double LastUpdateTime = -1.0;` (S5) | Mirrored by `RecordUpdate` in every mode, so the per-frame deadline loop never reads the component (H5). **Negative means never updated**: `Deadline = LastUpdateTime + MaxStaleness` is then below any world time, so a new client is mandatory in its first frame, like a native tick on its first frame. It is the **last** member, so the S4 tests' aggregate initialisation `{ Deadline, 0 }` still compiles (04 §16) |
| `struct FBudgetFrameStats { SpentMs; Runs; MandatoryRuns; MandatoryMs; Deferred; bOverBudget; }` | One frame's outcome, returned by value: six fields, no allocation. `MandatoryRuns`, `MandatoryMs` and `bOverBudget` are what SC1 needs. `Deferred` shows how much work waited |
| `double MandatoryMs` (S6) | The measured cost of the mandatory updates alone. When it is above the budget, that frame's overrun was **forced** by the staleness bound, which SC1 reports separately. Measured, not inferred (S6 simplify, altitude angle: the first version made the analysis infer it from "no optional update ran") |
| The class comment (steps 1–3) | The policy in three sentences, matching 02 §2: mandatory clients first and always; the rest earliest-deadline-first while the next predicted cost fits; each measured cost teaches its category's prediction. Ties broken by client index |
| `using FRunClient = TFunctionRef<double(int32 ClientIndex)>;` | **How the policy runs a client without knowing what a client is.** `TFunctionRef` is a non-owning reference to any callable: no heap allocation, no copy, valid only during the call (06 §22). The callable returns the measured cost in ms: the subsystem times the real update, tests return made-up costs. Its comment states the contract: it may change only fields the policy doesn't read (the subsystem mirrors `LastUpdateTime` while the loop runs) |
| `explicit FUpdateBudgetScheduler(int32 NumCategories = 1, double InitialCostMs = 0.005, double SmoothingAlpha = 0.2)` | `explicit` stops an `int` silently converting into a scheduler. **0.005 ms** is about one demo update on this machine (09 S2-f: about 5.2 µs), so the first frame's prediction is realistic. **0.2** is the design's smoothing weight (02 §7) |
| `Run(TConstArrayView<FBudgetClientState> Clients, double NextFrameTime, double BudgetMs, FRunClient RunClient)` | A **view**, not a copy: the subsystem's array is read in place. `NextFrameTime` = now + this frame's delta (02 §6): "would this client be late if it waited one frame?" needs the time of the next chance to run. Its comment states the one precondition: deadlines must be finite, because a NaN would break the sort's ordering |
| `GetPredictedCostMs(CategoryIndex)` | Read-only access for the tests |
| `static double SanitiseCostMs(double)` (private) | **Every** cost that enters a prediction passes here: the measured ones in `Run` and the starting one in the constructor (S4 simplify, altitude angle: the constructor was a second way in for NaN or infinity) |
| `ClampCategory(int32)` (private, inline) | `PredictedCostMs.IsValidIndex(Index) ? Index : 0`: an index the scheduler doesn't have falls back to category 0, so a settings change can never index out of range |
| `TArray<double> PredictedCostMs;` | One moving-average prediction per category. Categories describe kinds of client, and kinds of client cost different amounts |
| `TArray<int32> Order;` | The sort buffer is a **member**, reused every frame. After the first frame (or when N grows) `SetNumUninitialized` reuses the same memory, so the steady state allocates nothing |
| `double Alpha;` | The smoothing weight, clamped once in the constructor |

**The `.cpp`**

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `SanitiseCostMs`: `return FMath::IsFinite(CostMs) ? FMath::Max(0.0, CostMs) : 0.0;` | A negative cost (a clock oddity) counts as 0 rather than *giving back* budget. **A non-finite one counts as 0 too** (S4 code review): `FMath::Max(0.0, NaN)` is NaN, a NaN prediction would stay NaN for good, and every later `Spent + NaN > Budget` is false, which would silently switch the budget off |
| Constructor: `Alpha(FMath::Clamp(SmoothingAlpha, 0.0, 1.0))`; `PredictedCostMs.Init(SanitiseCostMs(InitialCostMs), FMath::Max(1, NumCategories))` | Bad arguments are made safe instead of producing negative or NaN predictions or an empty array. `Init(Value, Count)` fills the array with `Count` copies |
| `Order.SetNumUninitialized(Clients.Num(), EAllowShrinking::No);` then `Order[Index] = Index` | Sizes the buffer without zero-filling (every slot is written on the next line) and without giving memory back when N shrinks |
| `const FBudgetClientState* const State = Clients.GetData();` | **Raw pointer reads** (S4 simplify, efficiency angle): every index is in [0, N) by construction, so the view's per-access range check, which is compiled into Development builds, only added cost to the hottest loop of the frame. On a clang copy of UE's introsort it was about 15% of the sort time, roughly 45–75 µs per frame at N = 4,000; Phase 8 measures the MSVC build |
| `Order.Sort([State](int32 A, int32 B) { … return DeadlineA < DeadlineB \|\| (DeadlineA == DeadlineB && A < B); });` | **Earliest deadline first.** One O(N log N) sort per frame (v1, 02 §12 item 6). Each deadline is read once per comparison. **Ties are broken by index**, because `TArray::Sort` is not stable: without it, two clients with the same deadline could swap places between runs and break determinism. (UE5-TestTrust's mutation campaign found the `A < B` → `A <= B` mutant equivalent: indices are unique, and the introsort never compares an element with itself, 09 S7) |
| `while (NumMandatory < Order.Num() && State[Order[NumMandatory]].Deadline < NextFrameTime) ++NumMandatory;` | Because the order is sorted, **the mandatory clients are exactly a prefix**: one sort gives both sets. Counted **before** anything runs, because `RunClient` moves the deadline of the client it updates. The test is `<`, not `<=`: a client due exactly at the next frame can still run on time then (pinned by the `DeadlineAtNextFrameIsOptional` test, T3) |
| `if (Position >= NumMandatory && Stats.SpentMs + PredictedCostMs[Category] > BudgetMs) break;` | **The budget cut-off.** Don't start an update that is predicted not to fit. It is `break`, not `continue`: skipping one client to run a cheaper one with a *later* deadline would break deadline order. `>`, not `>=`: an update that fits exactly still runs (`ExactFitStillRuns`). Mandatory clients skip the test, so they always run (no starvation) |
| `const double CostMs = SanitiseCostMs(RunClient(ClientIndex));` | Runs the client and sanitises what it measured |
| `PredictedCostMs[Category] = FMath::Lerp(PredictedCostMs[Category], CostMs, Alpha);` | **The moving average**: move a fraction α of the way towards the newest measurement. `FMath::Lerp(A, B, α)` is exactly `A + α · (B − A)` (`Math/UnrealMathUtility.h:1123–1126`), the same as `(1 − α) · old + α · new` with one multiply (S4 simplify, reuse angle). With α = 0.2 a step change is about 89% absorbed after 10 updates (1 − 0.8¹⁰). Mandatory runs teach the prediction too |
| `Stats.SpentMs += CostMs; ++Stats.Runs;` and `if (Position < NumMandatory) Stats.MandatoryMs = Stats.SpentMs;` | Because the mandatory clients are the prefix, the last assignment happens at the prefix's last client, so `MandatoryMs` ends at their total. Three tests assert it (0.5, 0.03 and 0 ms) |
| `Stats.MandatoryRuns = NumMandatory;` | Set once after the loop: the loop only ever stops at an optional client, so every mandatory client has run. A per-iteration counter would always end equal to `NumMandatory` (S4 simplify) |
| `Stats.Deferred = Clients.Num() - Stats.Runs; Stats.bOverBudget = Stats.SpentMs > BudgetMs;` | Everything not run waits for a later frame. Over budget is a fact about the frame, whatever caused it; `>` so a frame exactly on budget isn't over (`ExactlyOnBudgetIsNotOverBudget`) |
| *Alternatives not taken* | A priority score (the first draft's formula) mixed two meanings into one number; deadlines give one meaning and a classic, defensible algorithm (EDF, 02 §13). A heap or partial sort would avoid sorting clients that can't fit; v1 measures the full sort first (11 lists the candidates) |

### P2. `UpdateBudgetSignificance.h`: significance, the view test and the category rule (S6; `Pick` in Phase 6)

Significance is maths on plain data, so it lives in a header of its own with no UObject and no world, and is unit-tested like the policy (T5). It was first written into the settings header; the S6 code review's view-cone findings were easier to test once it stood alone.

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "Camera/CameraTypes.h"` | `FMinimalViewInfo`, the camera view type, for `FUpdateBudgetView::FromCamera` |
| `USTRUCT() struct FUpdateBudgetCategory` with `UPROPERTY(EditAnywhere, …)` fields | A USTRUCT because the settings class stores an array of them as a `config` property, and only reflected types can be saved, loaded and edited in Project Settings. The fields have **default member initialisers**, which are the design defaults (02 §7). `ClampMin` keeps the editor from accepting negative values |
| `FName Name = TEXT("Default")` | Only for people: Project Settings shows it, and the begin-play warning names the category. Clients refer to a category by index |
| `float NearDistance = 1500.f; float FarDistance = 6000.f;` | 15 m and 60 m: within 15 m a client in view has significance 1; from 60 m, 0. The B2/B3 baselines read the **same two numbers** (G4) |
| `float NearMaxStaleness = 0.f; float FarMaxStaleness = 0.5f;` | The longest a client may go without an update at significance 1 (every frame) and at 0 (half a second) |
| `float OutOfViewMultiplier = 0.5f;` (`ClampMax = "1"`) | Out of view, significance is halved: staler, but never frozen (02 §7) |
| `GetSignificance(double DistanceSquared, bool bInView) const` | **1 at or inside the near distance; 0 at or beyond the far distance; linear in distance between**, then × `OutOfViewMultiplier` out of view. Squared distances avoid a square root in the two bands; `FMath::Sqrt` runs only between them. `GetRangePct(Near, Far, d)` is the engine's `(d − Near) / (Far − Near)` (06 §21). **Edge order is deliberate:** the near test comes first, then `DistanceSquared < Far²`, which can only be true when Far > Near. A category with Far ≤ Near therefore gets a step at the near distance and **never a division by zero** (`FarNotBeyondNear`, T5). Exactly 15 m is near, exactly 60 m is far, the same edges as B2's distance bands (`SameBandsAsUpdateBudget`, T7) |
| `const double Near = NearDistance; const double Far = FarDistance;` | Converted once, so the comparisons and `GetRangePct` run in `double` like the distances (S6 simplify: four casts became two) |
| `GetMaxStaleness(float Significance) const` → `FMath::Lerp(FarMaxStaleness, NearMaxStaleness, Significance)` | Significance 0 gives the far limit, 1 the near limit, linear between. Near and in view: 0 s, so the client is mandatory every frame. Near but out of view: significance 0.5 → 0.25 s |
| `IsInverted() const` | True when far clients would be at least as fresh as near ones, or when there is no band between near and far. The subsystem warns about it once per world (S6 code review: such settings were accepted silently) |
| `static const FUpdateBudgetCategory& Pick(const TArray<FUpdateBudgetCategory>& Categories, int32 Index)` → `IsValidIndex(Index) ? Categories[Index] : Categories[0]` (Phase 6) | **The one rule for "which category does this client use"**, shared by the subsystem (P6) and the B2/B3 baselines (G4), so the two sides of the comparison can never pick differently (owner decision, Phase 6). An index out of range means category 0. The list is never empty, because both readers get it from `UUpdateBudgetSettings::GetCategories` (P3). It takes the `TArray` itself, not a view (Phase 6 simplify, efficiency angle): converting to a `TConstArrayView` runs a `check` on the count, which would sit in the subsystem's per-client loop in Development builds; and an empty-list branch with a function-local default, the first version, moved into the settings (altitude angle), where it is written once |
| `struct FUpdateBudgetView` (plain, not reflected) | One frame's camera, in the form the per-client test needs |
| `static constexpr double AlwaysInViewRadius = 200.0;` | Everything within 2 m of the camera counts as in view. **Added after the S6 code review:** a client at the camera, such as a first-person pawn's own component, gave `Along = 0` and counted as out of view, so the player's own logic would have updated about 4 times a second. 2 m covers a pawn's root below a first-person camera; it never triggers in the benchmark, whose camera flies 4 m above the agents. `static constexpr` in a struct is an inline variable: one definition, no `.cpp` needed (04 §21) |
| `FromCamera(const FMinimalViewInfo&)` | `FOV` is the **horizontal** field of view in degrees (`CameraTypes.h:48–50`), clamped to [1°, 170°] against bad data. **The cone circumscribes the view rectangle** (S6 code review): the corner direction in camera space is (1, tan h, tan h / aspect), so tan²(half-diagonal) = tan² h × (1 + 1/aspect²). The first version used half the horizontal FOV, which left the screen's corners outside: at 16:9 and 90° the corners are 48.9° off axis against 45°. `AspectRatio` is width / height, default 4:3 (`CameraTypes.h:179`); a non-positive value falls back to 16:9. A narrower aspect only widens the cone, so the test errs towards "in view". `cos² = 1 / (1 + tan²)` turns the angle into the constant the test needs, once per frame |
| `IsInView(const FVector& ToClient, double DistanceSquared) const` | `Along > cos(half angle) · |v|`, the same idea as the brain's cone test (G1), without a square root: when both sides are positive, squaring keeps the order (04 §22). `Along > 0` first rejects everything behind the camera. The squared distance is passed in because the caller already has it |
| *Alternatives not taken* | The engine's `USignificanceManager` (baseline B3) computes significance on worker threads with per-object callbacks and keeps its own registry; UpdateBudget needs significance only to set a deadline, once per client per frame, inside a loop it already runs (02 §13) |

### P3. `UpdateBudgetSettings.h` / `.cpp`: Project Settings (S6)

| Line(s) | Why |
|---|---|
| `#include "Engine/DeveloperSettings.h"` | The base class lives in the `DeveloperSettings` module (`Runtime/DeveloperSettings/Public/Engine/DeveloperSettings.h`), a **public** dependency because this public header derives from it (A6) |
| `UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "Update Budget"))` | `config = Game`: the values live in the Game ini. `defaultconfig`: the editor saves them into `Config/DefaultGame.ini`, which is committed, not a per-user file. `DisplayName` is the Project Settings entry. `MinimalAPI` makes UHT export the generated part of the class (its `StaticClass` and the other generated members) with the module's API macro (`UhtHeaderCodeGeneratorHFile.cs:833`); hand-written functions need `UE_API` themselves, which the constructor has. That is all the game module's `GetDefault<UUpdateBudgetSettings>()` needs (G4): it reads a property, it calls no member function. The same specifiers as the engine's `UDataRegistrySettings` (`Plugins/Runtime/DataRegistry/.../DataRegistrySettings.h:16`). `UDeveloperSettings` subclasses appear in Project Settings automatically (`DeveloperSettings.h:21–23`; the section name defaults to the class name, `DeveloperSettings.cpp:68–76`) |
| `UPROPERTY(config, EditAnywhere, …, ClampMin = "0") float BudgetMs = 2.f;` | `config` marks it for loading from and saving to the ini. 2.0 ms is the design default (02 §7). The `ub.BudgetMs` cvar overrides it at any time (P6) |
| `UPROPERTY(config, EditAnywhere, …) TArray<FUpdateBudgetCategory> Categories;` | Index 0 is the default category. Any `Categories` entries in the ini **replace the whole array**: `LoadConfig` empties it before adding the ini's values, and keeps the C++ default when the ini has none (`CoreUObject/Private/UObject/Obj.cpp:3091–3095`) |
| Constructor: `Categories.Add(FUpdateBudgetCategory());` | One default category, so a project with no settings section works out of the box. No `[/Script/UpdateBudgetCore.UpdateBudgetSettings]` section is committed: the code defaults are the design |
| `UE_API TArray<FUpdateBudgetCategory> GetCategories() const` (Phase 6) | **The categories in effect: never empty.** An ini can empty the array (its entries replace the C++ default, see above); then one default category remains. The subsystem (P6c) and the baselines (G4) both read the list through it, so the rule is written once (Phase 6 simplify, altitude angle). Returned by value: it runs once per world, at begin play |
| *Read once per world* | The subsystem copies the settings in `OnWorldBeginPlay` (P6), and so do the baselines (G4), so a run can't change halfway |
| *Not in the settings:* the tick group | 02 §4.1 listed one. It is fixed at `TG_PrePhysics` instead, because the native baselines tick there (P5), and a configurable group would let the two drift apart (02 §13) |

### P4. `UpdateBudgetTickFunction.h` / `.cpp`: the scheduler's tick function (S5)

A tickable world subsystem's own `Tick` runs after `TG_PostPhysics` and can't choose a tick group (`01b` §2), so the subsystem owns this tick function and registers it on the persistent level, as Mass and Niagara do.

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "Engine/EngineBaseTypes.h"` | Declares `FTickFunction`, `ELevelTick`, `ETickingGroup` and `TStructOpsTypeTraits`. The base must be complete to derive from it |
| `USTRUCT() struct FUpdateBudgetTickFunction : public FTickFunction` + `GENERATED_BODY()` | `FTickFunction` is itself a USTRUCT (`EngineBaseTypes.h:182–183`), and the engine's own tick functions derive from it as USTRUCTs (`FActorTickFunction`, `:565–608`), as Niagara's does (`NiagaraWorldManager.h:47–69`). It is **not** a UPROPERTY of the subsystem: nothing needs to reflect or serialise it. It is a USTRUCT so it matches its base and the engine's pattern |
| `UE_API FUpdateBudgetTickFunction();` and `UE_API` on the three overrides | The 5.8 export pattern (P1): the members are exported one by one, like `ENGINE_API` on `FActorTickFunction`'s members |
| `UUpdateBudgetSubsystem* Subsystem = nullptr;` | A raw back-pointer to the owner, exactly like `FActorTickFunction::Target` (`EngineBaseTypes.h:570–575`). It needs no GC reference: the subsystem *contains* this struct, so the struct can't outlive it |
| `ExecuteTick(float DeltaTime, ELevelTick, ENamedThreads::Type, const FGraphEventRef&) override` | The one function the tick manager calls (`EngineBaseTypes.h:524`, pure virtual). `DeltaTime` is the world's delta for this frame: for a function with no interval, `CalculateDeltaTime` returns it unchanged (`TickTaskManager.cpp:2825–2846`) |
| `if (ensure(Subsystem)) Subsystem->RunFrame(DeltaTime);` | The subsystem sets the pointer before it registers the function, so a null here is a bug. `ensure` reports it once and skips the frame instead of crashing. `RunFrame` is private; this struct is the subsystem's `friend`, so nothing else can run a frame of updates |
| `DiagnosticMessage()` → `"UUpdateBudgetSubsystem::RunFrame"`; `DiagnosticContext()` → `"UpdateBudget"` | Pure virtual in the base (`:532`). The tick manager prints them in `dumpticks` and in cycle reports. `DiagnosticContext` returns a `static const FName`, built once instead of on every call |
| Constructor: `TickGroup = TG_PrePhysics; EndTickGroup = TG_PrePhysics;` | Both are already the defaults (`TickTaskManager.cpp:2361–2363`); they are **stated** because the fair comparison depends on them (the native baselines tick in `TG_PrePhysics`, P5). `EndTickGroup` = start group means "must finish within the group it started in", which a synchronous game-thread function always does |
| *Not set:* `bCanEverTick`, `bStartWithTickEnabled` | `RegisterTickFunction` reads neither (`TickTaskManager.cpp:2406–2427`): only actor and component setup do (`ActorComponent.cpp:1739–1750`). Setting them would suggest they matter. The subsystem enables or disables the function explicitly |
| `template<> struct TStructOpsTypeTraits<FUpdateBudgetTickFunction> … { enum { WithCopy = false }; };` | Tells the reflection system this struct can't be copied. `FTickFunction` deletes its copy assignment because the tick manager holds raw pointers to registered functions (`EngineBaseTypes.h:550–551`). **Without it** the generated struct operations would try to copy the struct and fail to compile |

### P5. `BudgetedUpdateComponent.h` / `.cpp`: the client (S1; lifecycle and mode in S5, category in S6, tick ownership in S7, Blueprint event in S8)

A client is a component with one gameplay update, which runs either from its own native tick (the baselines B0–B3) or from the scheduler (UB). The update itself never knows which: `RunUpdate` is the only entry point, and it owns the time contract.

**The header**

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#pragma once` | Include the header once per translation unit. *Alternative:* `#ifndef` guards, which do the same thing more verbosely. Unreal uses `#pragma once` everywhere |
| `#include "Components/ActorComponent.h"` | The base class must be complete to derive from it, so a forward declaration isn't enough |
| `#include "BudgetedUpdateComponent.generated.h"` | Code generated by UHT, the Unreal Header Tool, for this file's reflection. **Must be the last include**: UHT checks this and fails the build otherwise |
| `#define UE_API UPDATEBUDGETCORE_API` … `#undef UE_API` | The 5.8 export pattern (P1). **Without it** the game module gets "unresolved external symbol" link errors for the constructor, `RunUpdate`, `BeginPlay` and the other exported members |
| `class UUpdateBudgetSubsystem;` | A forward declaration. The header needs only the *name* (for a pointer and a `friend`), so it avoids an include cycle and keeps rebuilds small |
| Class comment | States the three contracts a user relies on: one update path for every mode, the time contract, and when registry membership starts and ends |
| `UCLASS(MinimalAPI, Abstract, Blueprintable, ClassGroup = (UpdateBudget))` | `UCLASS` registers the class with the reflection system, so it can be a `UPROPERTY` target, garbage-collected and spawned. `MinimalAPI` exports the generated part (P3); the hand-written members we need are exported one by one with `UE_API`, which keeps the DLL's export table small. `Abstract`: it can't be added to an actor directly, because the base has no gameplay. `ClassGroup` is the editor's "Add Component" category. **`Blueprintable` (S8)** sets the `IsBlueprintBase` metadata (`UhtDefaultSpecifiers.cs:122–127`). The editor offers "create a Blueprint class" only for a class with that metadata somewhere in its chain; `UActorComponent` itself is special-cased, its subclasses are not (`Kismet2.cpp:1080–1083`), and `UActorComponent` is `BlueprintType` but not `Blueprintable` (`ActorComponent.h:159`) |
| `class UBudgetedUpdateComponent : public UActorComponent` | A **component** rather than an interface: it must hold data (last update time, registry link) and attach to any actor, Blueprint actors included (02 §4). *Rejected:* an interface (every class would re-implement the bookkeeping) |
| `GENERATED_BODY()` | Expands to the code UHT generated: the reflection constructors, `StaticClass()`, the `Super` typedef and more. Every `UCLASS` needs it |
| `UE_API UBudgetedUpdateComponent();` | The constructor: it sets the tick defaults. It runs for the class default object (CDO) and for every instance |
| `UE_API void RunUpdate();` | **The one entry point for an update, used by every caller**: the native tick, the scheduler, and the tests. Nobody calls `OnBudgetedUpdate` directly, so the time accounting can't be skipped |
| `double GetLastUpdateTime() const` | `const`: reading can't change the object. `double`, because `UWorld::GetTimeSeconds()` returns `double` in UE5; `float` would lose milliseconds after a few hours |
| `IsRegisteredWithBudget()`, `IsDrivenByScheduler()`, `GetBudgetCategory()`, `HasBlueprintUpdate()` | Read-only facts for the tests, the debug view and the baselines (G4 reads the brain class's category). They expose state, not the link itself |
| `BeginPlay` / `EndPlay` / `TickComponent` `override` (public) | Engine lifecycle hooks (06 §5). `override` makes the compiler check that the signature really matches a base virtual; a typo would otherwise add a new function that is never called |
| `UE_API virtual void SetComponentTickEnabled(bool bEnabled) override;` (S7) | **Gameplay's switch for the native tick.** It is virtual (`ActorComponent.h:1002`), and `Activate`, `Deactivate`, replication and the async variant all call it (`ActorComponent.cpp:2819`, `2830`, `3087`, `1771`). The override keeps gameplay's choice in every mode (P5's `.cpp` table) |
| `virtual void OnBudgetedUpdate(float DeltaSeconds) {}` (protected) | The customisation point. `protected`: only the class and subclasses call it. An empty default instead of pure virtual, because a UObject class needs a constructible CDO. `float`, because gameplay maths is `float`; the time *bookkeeping* stays `double` |
| `UFUNCTION(BlueprintImplementableEvent, Category = "UpdateBudget", meta = (DisplayName = "Budgeted Update")) void ReceiveBudgetedUpdate(float DeltaSeconds);` (protected, S8) | An event with no C++ body: UHT generates a stub that finds the function and calls `ProcessEvent` (06 §37). `Receive…` is the engine's naming for such events (`ReceiveTick`, `ReceiveBeginPlay`). Protected, like `OnBudgetedUpdate`: only the class calls it. Its comment states the cost rule: it runs inside the timed update, so it spends the budget like C++ work (09 S8 measured it) |
| `UPROPERTY(EditAnywhere, Category = "UpdateBudget", meta = (ClampMin = "0")) int32 BudgetCategory = 0;` (S6) | Which settings category the client uses, editable per component and per Blueprint default. **Read when the client registers** (`AddClient` copies it into the hot state), so changing it later needs a re-register. An index the settings don't have means category 0 (`FUpdateBudgetCategory::Pick`, P2) |
| `OnRegister` / `OnUnregister` `override` (protected, S5) | Protected in the base (`ActorComponent.h:830`, `835`), and kept protected here: the engine calls them, gameplay shouldn't |
| `friend class UUpdateBudgetSubsystem;` | Only the subsystem may write the link fields and change the mode. A friend keeps them private to everyone else. *Rejected:* public setters, which would let any code corrupt the registry |
| `JoinBudget()` / `LeaveBudget()` (private, S5) | One place for each direction, used by four lifecycle hooks. `LeaveBudget` is safe to call twice |
| `SetDrivenByScheduler(bool)` (private, S5) | Switches between scheduler-driven updates and native ticking; only the subsystem calls it |
| `UPROPERTY(Transient) TObjectPtr<UUpdateBudgetSubsystem> Subsystem;` | `UPROPERTY` makes the reference visible to GC. `Transient`: never saved, it is a runtime link. `TObjectPtr` is UE5's UObject pointer wrapper (06 §8) |
| `int32 RegistryIndex = INDEX_NONE;` | The slot in the subsystem's arrays, for O(1) removal. `INDEX_NONE` (−1) means "not registered". Not a UPROPERTY: plain bookkeeping that nothing reflects |
| `double LastUpdateTime = -1.0;` | Negative means "never updated". That one state also defines "first update" for the time contract |
| `bool bDrivenByScheduler = false;` | The mode, set only by the subsystem |
| `bool bNativeTickEnabled = true; bool bNativeStartWithTickEnabled = true;` (S7) | Gameplay's tick choice and the start flag, kept while the scheduler drives the component, restored when it hands the component back |
| `bool bHasBlueprintUpdate = false;` (S8) | Cached in `OnRegister`: calling the event on a class that doesn't implement it would still cost a `ProcessEvent` (35 ns per call measured, 09 S8). The five `bool`s share one 8-byte slot after `LastUpdateTime`, so they add nothing to the component's size |

**The `.cpp`**

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include UE_INLINE_GENERATED_CPP_BY_NAME(BudgetedUpdateComponent)` | Pulls the UHT-generated `.gen.cpp` into this file instead of compiling it separately: fewer translation units, faster builds. 5.x convention |
| Constructor: `PrimaryComponentTick.bCanEverTick = true;` | Components default to **not** ticking (`ActorComponent.cpp:550`). Without this line the native baselines would never run. `bStartWithTickEnabled` is already `true` by default, so it isn't repeated (S1 simplify) |
| `PrimaryComponentTick.TickGroup = TG_PrePhysics;` | Components default to `TG_DuringPhysics` (`ActorComponent.cpp:548`). The scheduler runs in `TG_PrePhysics` (P4), so pinning both there keeps B0–B3 and UB at the same point in the frame. **If removed:** the native baselines would run during physics, a different frame position, and the comparison would be unfair |
| `BeginPlay`: `Super::BeginPlay(); JoinBudget();` | The engine's begin-play work (it sets `bHasBegunPlay` and fires the Blueprint event) comes first. The component's tick function is already registered at this point (`Actor.cpp:4822–4825` registers it right before `BeginPlay`), which matters for the subsystem's `Register` (P6) |
| `EndPlay`: `LeaveBudget(); Super::EndPlay(…)` | Leave the registry **while still fully valid**. GC only nulls references when it runs (`01b #8`), so the registry must not rely on it: without this, the scheduler could update a component that has ended play |
| `OnRegister`: `Super::OnRegister();` then `bHasBlueprintUpdate = GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UBudgetedUpdateComponent, ReceiveBudgetedUpdate));` (S8) | A native class always answers false (`Class.cpp:6525–6529`); a Blueprint class answers true when it, or a Blueprint parent, implements the event (`BlueprintGeneratedClass.cpp:1402–1406`). **Asked in `OnRegister`** (S8 code review): a first registration comes before `BeginPlay`, so the flag is right before any update can run, including one called directly. `GET_FUNCTION_NAME_CHECKED` (`AssertionMacros.h:520`) makes a misspelt or renamed function a compile error instead of a silent `false`. The engine's own `TickComponent` guards `ReceiveTick` with a coarser class-flag test (`ActorComponent.cpp:1886–1888`); this one also skips Blueprint subclasses that don't implement the event |
| `OnRegister`: `if (HasBegunPlay()) JoinBudget();` (S5) | **Re-registration while playing** (`ReregisterComponent`, or a re-attach) unregisters and re-registers the component without a second `BeginPlay`; the engine brings the native tick back in `HandleRegisterComponentWithWorld` (`Actor.cpp:6443–6451`). Joining here keeps the budget in step. A **first** registration happens before `BeginPlay`, so it doesn't join twice |
| `OnUnregister`: `LeaveBudget(); Super::OnUnregister();` (S5) | **Streaming removal** unregisters every actor's components (`Level.cpp:2184–2213`, through `UnregisterAllComponents`) and routes `EndPlay(RemovedFromWorld)` only afterwards (`World.cpp:4196–4208` before `4244–4251`). Both steps are time-sliced, so they can be frames apart, and the native tick goes at the first one (`ActorComponent.cpp:2147`). Without this override, budgeted mode would keep updating a component its level had already taken out of the world. Leaving **before** `Super`: leave while still fully valid |
| `JoinBudget`: `if (UUpdateBudgetSubsystem* Found = UWorld::GetSubsystem<UUpdateBudgetSubsystem>(GetWorld())) Found->Register(this);` | The static, null-safe overload (`World.h:4331`). It returns null in editor and preview worlds, where the subsystem doesn't exist (P6's `DoesSupportWorldType`); there the component just ticks natively. The subsystem sets **both** link fields (S1 altitude audit): one owner for "registered ⇔ `Subsystem` and `RegistryIndex` are set" |
| `LeaveBudget`: `if (UUpdateBudgetSubsystem* Current = Subsystem) Current->Unregister(this);` | Copies the `TObjectPtr` into a raw local once. In editor builds a `TObjectPtr` dereference adds handle tracking, so doing it once is cheaper (S1 efficiency audit). Unregister clears the link, so a second call does nothing |
| `SetDrivenByScheduler`: `if (bInDrivenByScheduler == bDrivenByScheduler) return;` | **The early return** keeps a second "to the scheduler" call from saving "off" as gameplay's choice (S7). The subsystem calls this for every client at registration and at every switch, so it must be idempotent |
| To the scheduler: `bNativeTickEnabled = IsComponentTickEnabled(); bNativeStartWithTickEnabled = PrimaryComponentTick.bStartWithTickEnabled; PrimaryComponentTick.bStartWithTickEnabled = false; Super::SetComponentTickEnabled(false);` | **Why both tick lines:** `SetComponentTickEnabled` moves the tick function between the tick manager's enabled and disabled sets (`TickTaskManager.cpp:2439–2461`); a disabled function costs nothing per frame, because only the enabled set is walked (`:1479–1485`). But whenever the tick function is registered again, the engine re-enables it if `bStartWithTickEnabled` is set (`ActorComponent.cpp:1747`). Without the start-flag line, a re-registered component would tick natively **and** be scheduled: two updates per frame. **The snapshot** covers a tick changed without the virtual (for example `PrimaryComponentTick.SetTickFunctionEnable` directly) before the takeover |
| Back to native: restore both, through `Super::SetComponentTickEnabled(bNativeTickEnabled)` | A live switch there and back changes nothing gameplay chose. The component calls `Super::` so that its own mode changes are not recorded as gameplay's choice (04 §23) |
| `SetComponentTickEnabled(bool bEnabled)` override: `bNativeTickEnabled = bEnabled; if (!bDrivenByScheduler) Super::SetComponentTickEnabled(bEnabled);` (S7) | **Records gameplay's choice in every mode**, and applies it only when the scheduler isn't driving the component. *Why* (the S7 code review, then its altitude review): a live switch to budgeted and back used to turn every client's native tick on, un-pausing anything gameplay had paused. A first fix took a snapshot at the takeover, but choices made *while* budgeted were still lost. Recording at the one function they all go through fixes both |
| `TickComponent`: `if (bDrivenByScheduler) { Super::SetComponentTickEnabled(false); return; }` **before** `Super` | **The safety net**, for code that enables the tick function directly (the `NoDoubleUpdate` test does exactly that). It turns the tick off again on its first native call and returns without updating, so the client is still updated once per frame, by the scheduler. Turning off your own tick function from inside it is allowed: "Disabling during a tick frame will stop it from executing unless it is already in progress" (`EngineBaseTypes.h:357–364`). It calls `Super::` for the same reason as above. Cost in the native modes: one `bool` test per tick |
| `Super::TickComponent(DeltaTime, TickType, ThisTickFunction);` | Keeps the engine's behaviour (`check(bRegistered)`; Event Tick and latent actions for Blueprint subclasses, `ActorComponent.cpp:1882–1900`). It costs about 1–2 ns and **exists only on the native path**: the one per-update infrastructure difference between the modes, stated for honesty. A Blueprint's Event Tick therefore runs only natively; the subsystem warns about it (P6) |
| `RunUpdate();` *(the engine's `DeltaTime` is not forwarded)* | **The time contract.** The engine's `DeltaTime` differs by mode (a frame delta, or the time since an interval tick) and includes per-actor time dilation; using it would give two clocks. The S1 code review found exactly that bug |
| `RunUpdate`: `if (!ensure(World)) return;` | `ensure` logs a callstack once and continues, where `check` would crash. A missing world is a caller bug worth reporting, not a reason to crash a benchmark |
| `const double PreviousUpdateTime = LastUpdateTime; LastUpdateTime = World->GetTimeSeconds();` | Stamp first, so the subsystem sees the new time. `GetTimeSeconds()` is world time: paused time excluded, global time dilation included |
| `if (UUpdateBudgetSubsystem* Current = Subsystem) Current->RecordUpdate(*this, PreviousUpdateTime);` | **The single per-update sync point** (S1 altitude audit), in every mode at the same cost, so updates and staleness are measured fairly (02 §9). It counts every update, the first included (the S1 review found the first wasn't counted), and mirrors the new time into the scheduler's hot state (P6) |
| `const float DeltaSeconds = static_cast<float>((PreviousUpdateTime < 0.0) ? 0.0 : LastUpdateTime - PreviousUpdateTime);` | 0 on the first update, then the exact time since the previous one, from one clock. The explicit cast makes the `double` → `float` narrowing visible |
| `OnBudgetedUpdate(DeltaSeconds); if (bHasBlueprintUpdate) ReceiveBudgetedUpdate(DeltaSeconds);` (S8) | Same time contract and the same `DeltaSeconds` for both, right after each other, inside the update the scheduler times. The guard means a native class pays one `bool` test per update, never a `ProcessEvent`. **Here, not in the base `OnBudgetedUpdate`** (S8 altitude review): a subclass that forgot `Super::` would silently lose the Blueprint event. *Rejected:* a `BlueprintNativeEvent`. In 5.8 its generated stub skips `ProcessEvent` when no Blueprint overrides the function, but it still looks the function up on every call (`UhtHeaderCodeGeneratorCppFile.cs:2427–2453`), against one cached `bool` here |

### P6. `UpdateBudgetSubsystem.h` / `.cpp`: the registry and the mechanism (S1; rewritten in S5; significance, budget and telemetry in S6; live switching and the debug view in S7; the Event Tick warning in S8)

One subsystem per game world owns the registry of clients, runs the policy once per frame in budgeted mode, and publishes the frame's numbers. The rows below follow the file: the header (P6a), then the `.cpp` in its own order (P6b–P6h).

#### P6a. The header

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "BudgetedUpdateComponent.h"` | Needed because `RecordUpdate` is defined **inline in this header** and calls `Client.GetLastUpdateTime()`. No cycle: the component header only forward-declares the subsystem |
| `#include "Subsystems/WorldSubsystem.h"`, `"UObject/TopLevelAssetPath.h"`, `"UpdateBudgetScheduler.h"`, `"UpdateBudgetSettings.h"`, `"UpdateBudgetTickFunction.h"` | The base class; `FTopLevelAssetPath` for the warned-classes set (S8); and the three plugin types the subsystem holds **by value** (scheduler, categories, tick function), so their full definitions are needed |
| `class IConsoleVariable;` | Forward declaration for the cvar-change handler's parameter |
| `struct FUpdateBudgetFrameReport { bBudgeted; Clients; Updates; MaxGapSeconds; BudgetMs; Scheduler; NearClients; NearMissed; }` (S7) | **One frame's numbers in one value** (about 80 bytes), as the CSV capture, the `ub.Debug` line and `OnFrameStats` see them. The last four are filled only by `RunFrame`, so they stay zero in native frames |
| The class comment | The mode, the settings, the registry's two-array design, why the subsystem is *tickable*, and one stated limitation: CSV stats use `ECsvCustomStatOp::Set`, so with several game worlds in one process (multi-client PIE) a capture shows the last world's values. Benchmarks run one standalone world |
| `UCLASS(MinimalAPI) class UUpdateBudgetSubsystem : public UTickableWorldSubsystem` | A world subsystem has the world's lifetime (`01b` §3). The *tickable* variant gives a `Tick` after `TG_PostPhysics` (`LevelTick.cpp:1821`), when every `TG_PrePhysics` update of the frame is finished, so the per-frame totals are complete (02 §13). `MinimalAPI` as in P3 |
| `UE_API void Register(UBudgetedUpdateComponent* Client);` / `Unregister(…)` | The component's `JoinBudget` / `LeaveBudget` call these (P5). They are exported (`UE_API`) so that code in other modules can call them too; the component itself lives in this module |
| `void RecordUpdate(const UBudgetedUpdateComponent& Client, double PreviousUpdateTime)` (inline) | **The single per-update sync point**, called by `RunUpdate` in every mode. `++Frame.Updates` counts every update; the gap is recorded only when there was a previous update; and the new time is mirrored into `States[Client.RegistryIndex].LastUpdateTime`, so the deadline loop never reads the component (H5). `INDEX_NONE` means the client is waiting in `PendingAdds` (for example it ran its own update in `BeginPlay`); `AddClient` copies the time when the add happens. **Inline** because it runs thousands of times per frame and is only a few instructions; an out-of-line call would add call overhead to every update (S1 efficiency audit) |
| `GetNumClients()`, `IsBudgeted()` | Read-only facts for the tests and the debug view |
| `DECLARE_MULTICAST_DELEGATE_OneParam(FOnFrameStats, const FUpdateBudgetFrameReport&); FOnFrameStats OnFrameStats;` (S7) | The design's per-frame event "for the debug HUD" (02 §4.1). A **native** multicast delegate: no reflection or Blueprint binding, and broadcasting with no listener walks an empty list (the S7 efficiency review estimated 10–30 ns per frame). A HUD or a tool subscribes with `AddUObject` or `AddLambda` (06 §36) |
| `OnWorldBeginPlay`, `Deinitialize`, `Tick`, `GetStatId` (public overrides); `DoesSupportWorldType` (protected) | The lifecycle (P6c, P6g, P6h). `DoesSupportWorldType` stays protected like the base's |
| `friend struct FUpdateBudgetTickFunction;` and `RunFrame` private | Only the tick function can run a frame. A second `RunFrame` call in one frame would update clients twice |
| `GetCategory(int32 Index) const` (inline) → `FUpdateBudgetCategory::Pick(Categories, Index)` | The category a client uses, by the rule the baselines share (P2). Called once per client per frame in the deadline loop, so `Pick` takes the array by reference (no view conversion) |
| The other private functions | One per job, each documented where it is defined below: `UpdateDeadlines`, `ResolveBudgetMs`, `CountNearClients`, `ShowDebugStats`, `DrawClientRecency`, `HandleEnabledChanged`, `HandlePostActorTick`, `SetBudgeted`, `RunClient`, `ApplyModeToClients`, `GetModeName`, `AddClient`, `RemoveClientAt`, `RemoveInvalidClients`, `HandlePostGarbageCollect`, `DetachClient` |
| `UPROPERTY(Transient) TArray<TObjectPtr<UBudgetedUpdateComponent>> Clients;` | Every registered client. A `UPROPERTY`, so GC sees the references. An entry can become null if GC collects a component that never reached `EndPlay`; every access tolerates that |
| `TArray<FBudgetClientState> States;` | The hot data, same index as `Clients` (P1). Not a UPROPERTY: plain numbers |
| `UPROPERTY(Transient) TArray<TObjectPtr<UBudgetedUpdateComponent>> PendingAdds;` | Clients registered while the scheduler was running; added after the loop. A UPROPERTY, so GC sees the waiting clients |
| `FUpdateBudgetScheduler Scheduler; FUpdateBudgetTickFunction TickFunction;` | Held by value: the tick function can't outlive the subsystem that contains it (P4) |
| `FDelegateHandle PostGarbageCollectHandle, EnabledChangedHandle, PostActorTickHandle;` | One handle per binding, so `Deinitialize` can remove each. `PostActorTickHandle` is bound only while a switch is pending (P6d) |
| `float LongestMaxStaleness` | The longest staleness any category allows: the SC2 bound, and red in the debug draw |
| `TArray<FUpdateBudgetCategory> Categories; double SettingsBudgetMs = 2.0;` | The settings, copied in `OnWorldBeginPlay` |
| `bool bBudgeted, bRunningScheduler, bHasInvalidClients` | The mode; "the scheduler is running, defer registry changes"; and "some entry needs removing after the loop" |
| `FUpdateBudgetFrameReport Frame;` (S7) | **The accumulator itself**: `RecordUpdate`, `RunFrame` and `CountNearClients` fill it, `Tick` publishes and resets it (P6h) |
| `TSet<FTopLevelAssetPath> EventTickClassesWarned;` (S8) | The client classes already warned about in this world (P6e). Names, not object references, so GC doesn't need to see it |

#### P6b. The top of the `.cpp`: logging, CSV, stats and cvars

| Line(s) | Why |
|---|---|
| `DEFINE_LOG_CATEGORY_STATIC(LogUpdateBudget, Log, All);` | The plugin's log category (A6's history). `STATIC` keeps it private to this file; nothing else logs to it |
| `CSV_DEFINE_CATEGORY(UpdateBudget, true);` | The CSV category of every `UpdateBudget/…` column. `true`: enabled by default, so a capture always contains it |
| `DECLARE_STATS_GROUP(TEXT("UpdateBudget"), STATGROUP_UpdateBudget, STATCAT_Advanced);` + `DECLARE_CYCLE_STAT` for `RunFrame` and `Deadlines` (S6) | The `stat UpdateBudget` group of 02 §4.1 (06 §32). The stats system exists in Development builds, not in Test or Shipping (`01b` §4) |
| `static TAutoConsoleVariable<int32> CVarUpdateBudgetEnabled(TEXT("ub.Enabled"), 0, …, ECVF_Default);` (S5) | The mode switch (02 §4.1). `int32` like the engine's own switches (`tick.AllowBatchedTicks`, `TickTaskManager.cpp:59–62`). **`ECVF_Default`, not `ECVF_Cheat`:** cheat cvars can't be set from ini files (`01b #11`), and a budgeted run sets this one through the ini system (`-ini:Engine:[ConsoleVariables]:ub.Enabled=1`, 06 §26). Default 0 keeps every baseline unchanged. Its help text says when a change takes effect, and that it waits while the game is paused (S7 code review) |
| `CVarUpdateBudgetMs` (`float`, default −1) and `CVarUpdateBudgetDebug` (`int32`, default 0), both `ECVF_Default` (S6) | −1 means "use the settings value", so the cvar overrides only when someone sets it. `ub.Debug 1` shows one on-screen line; `2` adds the per-client recency draw (S7) |
| `DoesSupportWorldType` → `Game \|\| PIE` | The engine default also includes `Editor` worlds (`WorldSubsystem.cpp:31`). The override keeps the subsystem, and therefore its tick function, out of the editor |

#### P6c. `OnWorldBeginPlay`: settings, mode and registration

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `bBudgeted = CVarUpdateBudgetEnabled.GetValueOnGameThread() != 0;` | The mode is read **once per world**, before any actor's `BeginPlay` (`World.cpp:6165–6174`), so every client registers into a known mode. Later changes go through the live switch (P6d) |
| `Categories = Settings->GetCategories();`, `SettingsBudgetMs = Settings->BudgetMs;`, `Scheduler = FUpdateBudgetScheduler(Categories.Num());` | **Settings are read once per world** (`GetDefault` returns the class default object, which holds the loaded config), so a run can't change halfway. `GetCategories` never returns an empty list (P3). The scheduler gets one cost prediction per category |
| The loop computing `LongestMaxStaleness`, with `UE_CLOG(Category.IsInverted(), …, Warning, …)` | The longest staleness any category allows is **the SC2 bound of this run**, written into the capture below so the analysis never assumes a value (S6 code review). An inverted category is reported once |
| `ApplyModeToClients();` **before** `RegisterTickFunction` | Normally nobody has registered yet (`OnWorldBeginPlay` runs before any actor's `BeginPlay`, `01b #7`); if someone has, the mode reaches them too. It also sets the tick function's enable state **before** registration: on an unregistered function `SetTickFunctionEnable` only stores the state (`TickTaskManager.cpp:2457–2460`), and registration then puts it straight into the right set, as Niagara does (`NiagaraWorldManager.cpp:462–463`) |
| `TickFunction.Subsystem = this; TickFunction.RegisterTickFunction(InWorld.PersistentLevel);` | Registered in **every** mode, so a later switch only has to enable it; a disabled tick function costs nothing per frame. **In `OnWorldBeginPlay`, never `Initialize`:** a registered function starts ticking at once (`01b #2`). On the **persistent level**, because a tick function lives in one level's tick list, and the persistent level lives as long as the world (Mass: `MassProcessingPhaseManager.cpp:485–486`) |
| `PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UUpdateBudgetSubsystem::HandlePostGarbageCollect);` | **Added after the S5 code review.** GC nulls the entry of a component collected without `EndPlay` (`01b #8`). Before this hook only the scheduler's loop ever found such entries, so in the native modes a dead slot stayed for the rest of the run and the CSV `Clients` count was too high. The delegate fires after every collection (`UObjectGlobals.h:3490`, broadcast at `GarbageCollection.cpp:5943`); the engine's own `UWorld` and `UNetDriver` bind to it the same way (`World.cpp:1765`, `NetDriver.cpp:839`) |
| `EnabledChangedHandle = CVarUpdateBudgetEnabled->OnChangedDelegate().AddUObject(this, &UUpdateBudgetSubsystem::HandleEnabledChanged);` (S7) | **The change callback.** Every console variable has a multicast delegate that fires when its value is set (`IConsoleManager.h:242`, `:694`; broadcast at `ConsoleManager.cpp:336`). `TAutoConsoleVariable`'s `operator->` gives the variable (`IConsoleManager.h:2120`). Each world's subsystem binds its own handler, so no global list of subsystems is needed; `AddUObject` won't call a subsystem that is gone, and `Deinitialize` removes the binding anyway |
| `UE_LOG(LogUpdateBudget, Display, "%s mode (ub.Enabled %d). Budget %.2f ms (…), %d categories. Update timer resolution: %.1f ns.", …)` | One line per world says how the run is configured, so a benchmark log proves its mode (09 S5). `GetSecondsPerCycle64()` × 10⁹ is the tick length of the timer `RunClient` uses (100 ns on this machine), so the precision of every measured cost is known |
| `CSV_METADATA(TEXT("UBMode"), GetModeName(bBudgeted)); CSV_METADATA(TEXT("UBMaxStaleness"), …)` | The capture records its own mode and its SC2 bound (`analyse_ub.py` reads both, M5) |

#### P6d. Live switching (S7)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `HandleEnabledChanged`: binds `HandlePostActorTick` to `FWorldDelegates::OnWorldPostActorTick`, once | **Never switch inside the callback.** It can fire anywhere, including inside a client's update (gameplay code setting the cvar). Switching there would enable native ticks during `TG_PrePhysics`, and a tick function enabled during a tick frame is queued for a later group of the same frame (`EngineBaseTypes.h:357–364`; the newly-spawned list at `TickTaskManager.cpp:1686–1690`): clients the scheduler had already updated would update natively too. The handler is bound **only while a switch is pending**, so in steady state, the baselines included, it costs nothing per frame |
| `HandlePostActorTick(UWorld* World, …)`: ignore other worlds; unbind; compare the cvar with the mode; `SetBudgeted` if they differ | `OnWorldPostActorTick` is broadcast after the tick manager's `EndFrame` (`LevelTick.cpp:1886` then `1906`; declared at `World.h:4526–4527`). `EndFrame` clears the newly-spawned state (`TickTaskManager.cpp:1652–1656`), so a tick function enabled now first runs **next frame**: every client was updated once this frame by the old mode, and is updated from the next frame by the new one. **Comparing the value**, not counting callbacks, makes repeated sets, or a set undone within one frame, harmless (tested, T4). The subsystem's own `Tick` would be too early: it runs before `TG_PostUpdateWork` and `EndFrame` (`LevelTick.cpp:1821` vs `1877–1886`). While the game is paused `bDoingActorTicks` is false (`LevelTick.cpp:1650–1656`), so this delegate doesn't fire (`:1899–1906`) and a switch waits until play resumes |
| `ApplyModeToClients()`: `SetDrivenByScheduler(bBudgeted)` on every non-null client, then `TickFunction.SetTickFunctionEnable(bBudgeted)` | **One helper for both places that set the mode** (S7 simplify): `OnWorldBeginPlay` and `SetBudgeted`. The component's own early return makes a repeated call free (P5) |
| `SetBudgeted(bool)`: `bBudgeted = …; ApplyModeToClients(); CSV_EVENT(UpdateBudget, "ModeSwitch …"); CSV_METADATA(TEXT("UBMode"), TEXT("Mixed"));` and a log line | **No resynchronisation is needed:** `RecordUpdate` mirrored every client's last update time into `States` in both modes, so the first budgeted frame computes correct deadlines. The CSV event marks the switch frame, so an analysis can split a capture there (`csv_stats.py --split-event ModeSwitch`, M2). **`UBMode` becomes `Mixed`** (S7 code review): a capture that spans a switch is neither mode, and must not be filed under the one the world began with |
| `GetModeName(bool)` (static inline) | One spelling of "Budgeted" and "Native" for the log, the CSV metadata and the switch event (S7 simplify) |

#### P6e. The registry

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `Register`: `check(Client)`; `if (Client->Subsystem) { ensure(Client->Subsystem == this); return; }` | Null is a programming error (`check` stops in Development builds and compiles out in Shipping). Registering twice, or while the add is still pending, does nothing; a link to a **different** subsystem would be a bug, which the `ensure` reports |
| `Client->Subsystem = this;` | Set at once, even if the add is deferred, so `IsRegisteredWithBudget()` and the early return above are right from this moment |
| **The Event Tick warning** (S8): `if (Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UBudgetedUpdateComponent, ReceiveTick)))` → `EventTickClassesWarned.Add(Class->GetClassPathName(), &bAlreadyWarned); UE_CLOG(!bAlreadyWarned, LogUpdateBudget, Warning, …)` | `ReceiveTick` is `UActorComponent`'s public Blueprint event (`ActorComponent.h:1370–1371`). A client whose Blueprint implements it does **more work in native mode than in budgeted mode**: the native tick runs Event Tick and then the update, the scheduler only the update. That breaks the rule that every configuration does identical gameplay work (02 §9), so the subsystem says so. **Once per class and world:** `TSet::Add` reports whether the key was already there (`SparseSet.h.inl:415`; `TSet` is the sparse set, `ContainerAllocationPolicies.h:1912`); `FTopLevelAssetPath` is hashable (`TopLevelAssetPath.h:162–165`). The set lives on the subsystem, so a new PIE session or test world warns again. **Why here, in every mode** (S8 simplify: three review angles agreed): the first version warned from the component at the switch to budgeted mode, through a per-component flag, a process-wide `static` and a shared log header. The difference belongs to the class in both modes, and `Register` runs once per registration in a world that has the scheduler (editor worlds have none, so they stay quiet). A native class answers false at once (`Class.cpp:6525–6529`) |
| `Client->SetDrivenByScheduler(bBudgeted);` **before** the add | Applied at once, even when the add itself is deferred: a component spawned inside an update has just had its native tick registered and enabled (`Actor.cpp:4824`), which would run it in a later tick group of this frame (`EngineBaseTypes.h:335–341`). Disabling it also removes it from that newly-spawned list (`TickTaskManager.cpp:1906–1909`). **In both modes** (S5 code review): a client that was scheduler-driven in another world and joins a native one gets its tick back. The component's early return makes the native case free |
| `if (bRunningScheduler) PendingAdds.Add(Client); else AddClient(Client);` | **Why defer:** the policy reads `States` through a raw pointer (P1). `TArray::Add` may reallocate, and the policy would then read freed memory |
| `Unregister`: `if (Index == INDEX_NONE) PendingAdds.RemoveSingleSwap(Client, EAllowShrinking::No);` | A client that registered and unregistered inside the same loop never reaches the arrays: the add and the removal cancel out |
| `else if (Clients.IsValidIndex(Index) && Clients[Index] == Client) { if (bRunningScheduler) { Clients[Index] = nullptr; bHasInvalidClients = true; } else RemoveClientAt(Index); }` | Only remove if the slot really holds this client, which protects against a stale index. **Deferred removal nulls the slot**: no element moves, so every index the policy holds stays valid, and `RunClient`'s `IsValid` check skips the slot. Between frames, the O(1) swap-remove runs at once |
| `DetachClient(Client);` at the end of `Unregister` | The link is cleared **in every case**, so a component never points at a registry it isn't in |
| `AddClient`: `Client->RegistryIndex = Clients.Add(Client);` then `States.AddDefaulted_GetRef()` with `LastUpdateTime = Client->GetLastUpdateTime(); CategoryIndex = Client->GetBudgetCategory();` | Both arrays grow together, so the index is shared; `TArray::Add` returns the new index. The hot state starts from the client's **own** last update time: a client re-registered after updates keeps its staleness, and one that waited in `PendingAdds` loses nothing. The category is copied here, which is why changing it later needs a re-register (P5). `AddDefaulted_GetRef` (`Array.h:3067`) constructs the element in place and returns it |
| `RemoveClientAt`: `Clients.RemoveAtSwap(Index, EAllowShrinking::No); States.RemoveAtSwap(Index, EAllowShrinking::No);` then fix the moved entry's `RegistryIndex` | **Swap-remove:** the last element moves into the hole, O(1), in both arrays with the same index, so they stay parallel. *Rejected:* `RemoveAt`, which shifts everything down, O(N), and would make every despawn cost proportional to the agent count; nothing depends on registry order. `EAllowShrinking::No` avoids reallocating while agents come and go. **The null check on the moved entry:** GC can null an entry whose component was collected without `EndPlay` (`01b #8`); the S1 review found that crash path |
| `RemoveInvalidClients()`: backwards loop, `if (!IsValid(Client)) { DetachClient(Client); RemoveClientAt(Index); }` | Removes the slots nulled during the loop **and** any client that became invalid without `EndPlay` (garbage, or nulled by GC). **Backwards**, because a swap moves the last element into `Index`, and the last element has already been checked. A garbage component's link is cleared, so a later `EndPlay` finds nothing to do |
| `HandlePostGarbageCollect()`: `if (bRunningScheduler) bHasInvalidClients = true; else RemoveInvalidClients();` | Collections run between frames (`LevelTick.cpp:1968–1971`), so normally the arrays can change at once. If an update forced a collection inside the loop, the entries are only marked, and the loop removes them when it ends: the arrays never change under the policy. One O(N) `IsValid` pass per collection, roughly once a minute, in every mode |
| `DetachClient` (static, accepts null) | **Null-tolerant** (S5 simplify): every caller that walks the arrays may meet an entry GC has already cleared |

#### P6f. One budgeted frame

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `ResolveBudgetMs()`: `ub.BudgetMs` if ≥ 0, else the settings value | **One rule in one place** (S6 simplify): the begin-play log and every frame use it. Read once per frame, so `ub.BudgetMs 1` from the console takes effect on the next frame |
| `UpdateDeadlines()`: `SCOPE_CYCLE_COUNTER(STAT_UpdateBudget_Deadlines); CSV_SCOPED_TIMING_STAT(UpdateBudget, Deadlines);` | The significance loop is the part that touches UObjects (each owner's location, H5), so it is timed on its own: CSV column `UpdateBudget/GameThread/Deadlines` |
| `GetWorld()->GetFirstPlayerController()` → `PlayerCameraManager.Get()` → `FUpdateBudgetView::FromCamera(Camera->GetCameraCacheView())` | **The same view the brains and the B2/B3 drivers use** (G1, G4): every consumer sees one camera. The cached view is last frame's, because cameras update after all tick groups (`LevelTick.cpp:1828–1847`); the brains sense the same one-frame-old view, so the comparison stays fair (02 §12 item 10) |
| No camera → `FUpdateBudgetView()`, and significance 1 for everyone | Without a local player (a test world, a server) nothing can be judged far or out of view, so nothing is allowed to go stale. The lifetime tests rely on it: their worlds have no player, so every client is due every frame there |
| Per client: `Owner->GetActorLocation() - View.Location`, then `Category.GetSignificance(DistanceSquared, View.IsInView(ToClient, DistanceSquared))` | One location read and a few multiplies per client. A null client or owner keeps significance 1, so it is due, and `RunClient`'s `IsValid` guard handles it |
| `State.Significance = Significance; State.Deadline = State.LastUpdateTime + Category.GetMaxStaleness(Significance);` | The design formula (02 §2). A never-updated client (`LastUpdateTime` −1) has a deadline below any world time, so its first update is this frame, as natively |
| `RunFrame`: `SCOPE_CYCLE_COUNTER(STAT_UpdateBudget_RunFrame); CSV_SCOPED_TIMING_STAT(UpdateBudget, RunFrame);` | The whole budgeted path is timed as `UpdateBudget/GameThread/RunFrame`: deadlines, sort, timer reads and updates. It covers more than `UpdateMs`, which is only the measured update costs, so the difference is the scheduler's own overhead (about 0.29 ms at N = 2,000, 09 S6) |
| `Frame.BudgetMs = ResolveBudgetMs();` | The budget in effect this frame, also published to the CSV |
| `bRunningScheduler = true; Frame.Scheduler = Scheduler.Run(States, Now + DeltaSeconds, Frame.BudgetMs, [this](int32 Index) { return RunClient(Index); }); bRunningScheduler = false;` | **Step 2 of 02 §6.** `States` converts to the policy's `TConstArrayView` without a copy. `Now + DeltaSeconds` estimates the next frame's start; with a fixed step it is exact. The lambda converts to the policy's `TFunctionRef` (06 §22). One edge (S5 code review): in a frame where world time doesn't advance, a client updated last frame has `Deadline == NextFrameTime`, which is not mandatory under the strict `<`; nothing is overdue in such a frame, and fixed-step and normal play never produce one |
| `if (bHasInvalidClients) RemoveInvalidClients();` | **Step 3**, right after the loop, when nothing reads the arrays any more. The pass runs only when something was invalid, so a normal frame doesn't pay an O(N) scan |
| `CountNearClients(Now);` **before** the pending adds | **SC3 from the outcome**, in one dense pass over `States`: a client with significance 1 is near, and missed if its `LastUpdateTime` isn't this frame's time. *Rewritten after the S6 reviews:* the first version counted near clients before the loop and updates inside it, and counted entries that could never run (null, or destroyed during the loop) as missed. Counting after the removal pass, from the result, needs no special case. Before the adds, because a client added now hasn't had its first frame |
| The `PendingAdds` loop: `AddClient` for each valid entry, `DetachClient` for an invalid one, then `PendingAdds.Reset()` | A waiting client that became garbage is **detached** rather than dropped silently (S5 code review), so it can't keep reporting itself as registered. `Reset` keeps the allocation for the next frame (`Array.h:2469`) |
| `RunClient`: `if (!IsValid(Client)) { bHasInvalidClients = true; return 0.0; }` | The **`IsValid` guard** (`01b #8`): null (unregistered during this loop, or nulled by GC) or garbage (`Object.h:1886`). It costs the budget nothing |
| `const uint64 StartCycles = FPlatformTime::Cycles64(); Client->RunUpdate(); return FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);` | The measured cost the budget is spent on, a Blueprint event's included (P5). `Cycles64()` is `QueryPerformanceCounter` on Windows (`WindowsPlatformTime.h:39–44`); `ToMilliseconds64` converts with the measured frequency (`GenericPlatformTime.h:201–205`). Integer cycles avoid `Seconds()`'s large offset (`WindowsPlatformTime.h:23–29`). **Two timer reads per update** are the budgeted path's own overhead; one read per update is a Phase 8 candidate (11) |

#### P6g. `Deinitialize`

| Line(s) | Why |
|---|---|
| `TickFunction.UnRegisterTickFunction();` and every delegate binding removed (GC, cvar, post-tick) | The tick function must not stay registered once the subsystem is going (`01b` §2). `~FTickFunction` would unregister it too (`TickTaskManager.cpp:2396–2399`), but only when the memory is freed, after GC. `Deinitialize` runs during world cleanup (`World.cpp:6486`) |
| One loop over `{ &Clients, &PendingAdds }` detaches every client, then the three arrays are emptied | Normally every client has already unregistered (`EndPlay` runs before `Deinitialize`, `01b #7`). This is the safety net, so no component keeps a strong reference to a dead subsystem |
| `Super::Deinitialize();` **last** | The tickable base clears its "initialised" flag there |

#### P6h. `Tick`: telemetry and the debug view

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `Frame.bBudgeted = bBudgeted; Frame.Clients = Clients.Num();` | The two fields only `Tick` knows. `Frame` is otherwise already filled: `RecordUpdate` counted updates and the largest gap, `RunFrame` set the budget, the scheduler stats and the near counts (S7 simplify: the report **is** the accumulator; the first S7 version copied six counters into a report every frame, so a new statistic needed four edits) |
| Three `CSV_CUSTOM_STAT(UpdateBudget, Clients / UpdatesRun / MaxGapMs, …, ECsvCustomStatOp::Set)` | One value per frame per column, in every mode. **Why not one CSV call per update?** The CSV profiler's own startup benchmark measured about 50 ns per custom stat on this machine; 4,000 calls per frame would add about 0.2 ms of pure instrumentation. The gap stays in seconds until here, so `RecordUpdate` does no extra multiply |
| `if (bBudgeted) { CSV_CUSTOM_STAT(UpdateBudget, UpdateMs / BudgetMs / MandatoryRuns / MandatoryMs / Deferred / OverBudget / NearClients / NearMissed, …) }` (S6) | The scheduler's numbers, **only in budgeted mode**: a native capture gets no columns of zeros, and the native modes pay nothing for them. `OverBudget` is 0 or 1, so its mean over a capture is the share of over-budget frames. *Changed from the plan* (02 §13): `NearMaxGapMs` became `NearClients` + `NearMissed`; a near client's gap at its first near frame covers the time it was far, so a near-gap maximum would mix two things |
| `OnFrameStats.Broadcast(Frame);` | Listeners get exactly the numbers the CSV gets |
| `ub.Debug` ≥ 1 → `ShowDebugStats()`; ≥ 2 → `DrawClientRecency()` | The string is built and the points drawn only when the debug view is on, so a benchmark never pays for them |
| `Frame = FUpdateBudgetFrameReport();` | One assignment resets everything, so no counter can be forgotten (S6's field-by-field reset had missed the budget value). A frame where `RunFrame` didn't run (the game paused) publishes zeros, not the previous frame's numbers |
| `ShowDebugStats()`: one line, `GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, Over ? FColor::Orange : FColor::Green, Line)` | **The cast is required:** the function has a `uint64` and an `int32` key overload (`Engine.h:2197`, `:2200`), and `GetUniqueID()` returns `uint32`, which converts equally well to both, so the first S6 build failed with C2668 (ambiguous call). **Keyed by the subsystem's unique ID**, so each frame's line replaces the last instead of stacking (`Engine.h:2196–2197`: `bNewerOnTop` matters only for key −1). Time 0 shows it for one frame. Orange when the frame went over budget |
| `DrawClientRecency()`: `#if ENABLE_DRAW_DEBUG` … `#endif` | Debug drawing exists only where the engine compiles it in (`DrawDebugHelpers.h:14`); elsewhere the calls are empty inline functions (`:186`). The guard also skips the loop |
| Colour: `FColor::MakeRedToGreenColorFromScalar(1 − (Now − LastUpdateTime) / LongestMaxStaleness)` | **One scale for both modes:** green = updated this frame, red = as stale as the settings allow. Natively every agent stays green (B0) or pulses with its interval (B2/B3); budgeted, far agents redden between updates. That is the picture the Phase 10 demo films. The engine's own green-yellow-red scale (`Color.h:640`, `Color.cpp:569–577`) clamps its input, so no clamp is needed (S7 simplify, reuse angle: it replaced an HSV interpolation). `Scale` has a floor of 1 ms, so a category set to "every frame everywhere" can't divide by zero |
| `DrawDebugPoint(World, OwnerLocation + (0, 0, 60), 8.f, Color)` | One point per client, 60 cm above the 50 cm cube so it isn't hidden inside it, for one frame (lifetime −1, `DrawDebugHelpers.h:24`). Only at `ub.Debug 2`, so never in a benchmark |
| `GetStatId`: `RETURN_QUICK_DECLARE_CYCLE_STAT(UUpdateBudgetSubsystem, STATGROUP_Tickables)` | Required by `FTickableGameObject` (pure virtual). It gives this tick a named entry in `stat` and Insights |

---

## Part G · The game module, `UpdateBudget` (the demo workload and the benchmark harness)

Everything here exists to **measure** the plugin: the workload agents (G1, G2), the benchmark director (G3) and the two "smarter native tick" baselines (G4). The module depends on the plugin, never the reverse (A3).

### G1. `UBDemoBrainComponent.h` / `.cpp`: the workload (S1; compute step calibrated in S2)

The class comment states the property the benchmark's correctness check (SC6) depends on: the patrol position is a closed-form function of the total patrol time, and the base class guarantees `DeltaSeconds` = world time since the previous update. So any update rate lands on the same circle position for the same last-update time, up to float rounding.

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `UENUM() enum class EUBBrainState : uint8 { Patrol, Alert };` | `enum class`: scoped and strongly typed, no implicit `int` conversion. `uint8` keeps it one byte. `UENUM` makes it reflectable for debugging and future `UPROPERTY`s |
| `UCLASS(ClassGroup = (UpdateBudget)) class UUBDemoBrainComponent : public UBudgetedUpdateComponent` | The concrete client the agents use: no `Abstract`. It inherits the time contract, both modes, and the category (P5) |
| `InitPatrol(Center, Phase)` | Called **between** `SpawnActorDeferred` and `FinishSpawning` (G3), so the patrol exists before `BeginPlay` and before the first update |
| `SetComputeIterations` / `GetComputeIterations` | The director sets the compute step from the command line (`-UBWork=`), and reports the value in the run metadata, so a later recalibration can't hide behind "default" |
| `GetPatrolPosition(double)`, `GetPatrolTime()`, `GetState()`, `GetStateChanges()` | Read by the director's end-of-run report: final positions for SC6 and decision churn. All `const` |
| `virtual void OnBudgetedUpdate(float) override` (protected) | The gameplay. Protected like the base's, so nothing calls it except `RunUpdate` |
| `GetPatrolAngle(double)` (private, inline) | One formula, used by `GetPatrolPosition` and `OnBudgetedUpdate`. It was copied into two functions before the S1 simplify audit |
| `UPROPERTY(EditAnywhere, Category = "Workload", meta = (ClampMin = "0")) float PatrolRadius = 300.f;` | `EditAnywhere`: tunable per instance and on the class default. `Category`: grouping in the Details panel. `ClampMin`: the editor refuses negative values. **300 cm** gives a 6 m wide circle: big enough to see motion from the camera, small enough for dense grids. **Not `BlueprintReadWrite`:** nothing in Blueprint needs it, and exposing it would invite changes mid-benchmark |
| `PatrolAngularSpeed = 0.5f` (rad/s) | 0.5 rad/s × 3 m = 1.5 m/s, a walking pace. One lap takes about 12.6 s, so a 60 s capture covers about 5 laps |
| `AlertRadius = 1500.f`, `AlertConeCos = 0.5f` (`ClampMin -1`, `ClampMax 1`) | 15 m awareness and a 60° half-angle cone: a realistic mix of Patrol and Alert as the camera passes. The cone is stored as a **cosine**, so the per-update test needs no `acos` |
| `ComputeIterations = 128` | A stand-in for heavier logic, **calibrated in S2** (09 S2-f, batch `2026-11-20_233941_calib`): at N = 1,000 it makes B0 game-thread bound (median 5.21 ms, against 2.46 ms with no compute, where the frame was render-bound), so a game-thread improvement shows up in frame time. About 20 ns per iteration per agent on this machine, about 5.2 µs per update |
| `PatrolCenter`, `PhaseRadians`, `PatrolTime`, `State`, `StateChanges`, `ComputeSink` | Plain members, not UPROPERTYs: runtime state that nothing reflects or saves |
| `OnBudgetedUpdate`: `PatrolTime += DeltaSeconds;` | Gameplay that integrates time, the kind of code that needs the accumulated-delta contract: an agent updated after 0.3 s walks 0.3 s worth |
| `Cos`/`Sin` once → `Position` and `Forward(-Sin, Cos, 0)` | Two trig calls per update instead of four. `Forward` is the derivative of the circle (the counter-clockwise tangent). Done before the S2 calibration, so the calibrated cost measures the final code |
| `GetWorld()->GetFirstPlayerController()` … `PlayerCameraManager.Get()` | "Where is the player?" is asked **per agent, as real perception code would**. The S1 efficiency audit priced it at about 10–20 ns per update, the same in every mode. It is kept on purpose: caching it once per frame would make the workload unrealistically cheap. `.Get()` turns the `TObjectPtr` into a raw pointer once. `GetCameraLocation()` returns a field of the camera's cached view (`PlayerCameraManager.cpp:774–782`), the view P6 and G4 use too |
| `DistanceSquared < Square(AlertRadius)` | Squared distances avoid a square root in the common "far away" case |
| `Dot(Forward, ToPlayer) > AlertConeCos * Sqrt(DistanceSquared)` | The same test as `Dot(Forward, Normalized(ToPlayer)) > cos`, without a division (S1 efficiency audit). The square root is computed only when the agent is near enough to matter (`&&` short-circuits) |
| `if (NewState != State) { State = NewState; ++StateChanges; }` | Counts transitions, reported as decision churn |
| `GetOwner()->SetActorLocationAndRotation(Position, FacingDirection.Rotation())` | **One** transform update per call, not separate location and rotation calls that would each update the component and its render state |
| The compute loop with `Sink` → `ComputeSink` | A fixed amount of floating-point work. It writes to a member, so the compiler can't prove the result unused and delete the loop. Without the store, an optimising build could remove the "work" entirely and the benchmark would measure nothing |

### G2. `UBDemoAgent.h` / `.cpp`: one benchmark agent (S1)

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `UCLASS() class AUBDemoAgent : public AActor` | A minimal actor: a visible mesh plus its brain. `A` prefix: an actor |
| `UUBDemoBrainComponent* GetBrain() const` | The director and the B2/B3 driver reach each agent's brain through it |
| `UPROPERTY(VisibleAnywhere, Category = "Agent") TObjectPtr<UStaticMeshComponent> Mesh;` and `… Brain;` | `UPROPERTY` so GC sees the components; `VisibleAnywhere` shows them in the Details panel without allowing the pointers to be replaced |
| `PrimaryActorTick.bCanEverTick = false;` | The actor itself does nothing per frame; all gameplay is in the brain. Its tick function is **never registered** (`Actor.cpp:1690`), so it adds no tick-manager cost |
| `CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))` | A default subobject: a component every instance gets, created in the constructor. The name must be unique within the class |
| `RootComponent = Mesh;` | The mesh defines the actor's transform; `SetActorLocationAndRotation` moves it |
| *(no `SetMobility`)* | Scene components default to `Movable` (`SceneComponent.cpp:124`), which a moving agent needs. The explicit call was redundant (S1 simplify) |
| `SetCollisionEnabled(NoCollision)`, `SetGenerateOverlapEvents(false)` | The workload measures **gameplay logic**, not physics. No collision means no physics bodies are created, and moving costs no physics updates |
| `SetCastShadow(false)`, `SetRelativeScale3D(FVector(0.5))` | Cuts GPU shadow cost for thousands of moving cubes; the benchmark targets the game thread. The engine cube is 1 m, so the agent is a 50 cm cube (the debug point sits 60 cm above it, P6h) |
| `static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));` then `if (CubeMesh.Succeeded())` | Finds the engine cube once: `static` means the lookup runs once for the class, not per instance. **Only valid inside a constructor.** The asset is cooked through `DirectoriesToAlwaysCook` (A7); if it were missing the agent would simply be invisible, which the check allows |
| `CreateDefaultSubobject<UUBDemoBrainComponent>(TEXT("Brain"))` | The workload component. It registers with the subsystem in its own `BeginPlay` (P5) |

### G3. `UBBenchmarkSubsystem.h` / `.cpp`: the benchmark director (S1; timeline, camera and report in S2)

The header comment is the run's contract: the timeline (warm-up, `BenchStart`, optional wave, `BenchEnd`, report, exit) and every `-UB…=` command-line value with its default. `run_bench.py` (M3) builds its command lines from that list.

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `class UUBBenchmarkSubsystem : public UTickableWorldSubsystem` | **Changed from the plan** (02 §13): the project has no hand-authored map, so nothing would place a director or spawner actor. A world subsystem has exactly the world's lifetime and replaces both. *Tickable* (S2), because it drives the camera and the run timeline every frame. That work is benchmark control, not measured gameplay, and it is identical in every configuration |
| `const TArray<TObjectPtr<AUBDemoAgent>>& GetAgents() const` and its comment (S3) | **The agent-list contract**, written where the data lives (S3 altitude audit): spawn order, **append-only** (the baseline driver adopts new agents by index, G4), and entries may be destroyed or nulled by GC, so every user checks `IsValid()` |
| `DoesSupportWorldType` → `Game \|\| PIE` | Never in editor worlds, like the plugin's subsystem (P6b) |
| `struct FRunSettings { NumAgents = 0; Seed = 1; ComputeIterations = -1; WarmupSeconds = 10.0; CaptureSeconds = 60.0; WaveAtSeconds = -1.0; RunTag = "run"; bExitWhenDone = false; }` (private) | The `-UB…=` values, read **once** into one place, with their defaults beside them. A plain struct, not a USTRUCT: nothing reflects it. `NumAgents = 0` means "no workload": a normal play session isn't affected |
| `enum class ERunPhase : uint8 { Idle, WarmUp, Capture, Done }` | The run timeline as a small state machine. `Idle` means no workload was requested, so the subsystem does nothing |
| `namespace UBBench { GridSpacing = 400.0; GridJitter = 100.0; AgentHeight = 25.0; CameraHeight = 400.0; CameraMinRadius = 1500.0; CameraPitchDegrees = -15.0; ExitDelayFrames = 3; }` | Named constants with their reasons in comments: 4 m cells keep 4,000 agents within a 250 m square (patrol circles overlap, harmless without collision); 1 m of jitter so the layout isn't regular; 25 cm puts the 50 cm cubes on the floor |
| `CSV_DEFINE_CATEGORY(UBBench, true);` | The category of the run's events (`BenchStart`, `Wave`, `BenchEnd`) |
| `ReadRunSettings()` (static) | Reads the command line once (S1 simplify). `FParse::Value` leaves a field untouched when its key is absent, so the defaults stand; `FParse::Param(…, TEXT("UBExit"))` is a flag with no value |
| `ValidateRunSettings()` | Logs an **Error** when the capture is ≤ 0 s, the warm-up is < 0 s, or `-UBWaveAt` falls outside the capture. A misconfigured run then fails the batch (`run_bench.py` counts log errors, M3) instead of silently measuring the wrong thing (S2 review) |
| `OnWorldBeginPlay`: read and validate; `SpawnAgents(…)` timed and logged with its start-position hash | Runs before any actor's `BeginPlay` (`World.cpp:6167` vs `6174`). The hash proves that two runs with one seed placed agents identically (09 S1) |
| `EffectiveComputeIterations` from the first valid agent's brain | The value actually in effect goes into the CSV metadata and the report: "−1 = default" would hide a recalibration of the default (S2 review) |
| `Camera = InWorld.SpawnActor<ACameraActor>(); Phase = WarmUp;` | A plain engine `ACameraActor`, so no new class is needed |
| `CSV_METADATA(TEXT("UBRunTag"), …)`, `UBAgents`, `UBSeed`, `UBWork`, `UBWaveAt` | Writes the run description **into the CSV file itself** (keys arrive lower-case, e.g. `ubruntag`), so a capture can never be separated from what produced it |
| `SpawnAgents(World, Count, Seed)`: `FRandomStream Random(Seed)`; a square grid of `ceil(√Count)` columns; `ArenaHalfExtent = Max(…)` | Same seed, same placement (06 §13). It appends, which the wave needs, and keeps the largest grid's extent so the camera path covers it. `Phase0` is named so it doesn't shadow the run phase |
| `World.SpawnActorDeferred<AUBDemoAgent>(…, ESpawnActorCollisionHandlingMethod::AlwaysSpawn)` → `InitPatrol` (and `SetComputeIterations` if given) → `FinishSpawning(FTransform(StartPosition))` | **The deferred spawn** (06 §11): the brain's patrol is set before `BeginPlay`, and the actor is finished at its true start position. `AlwaysSpawn`: no collision tests, nothing is rejected. A failed spawn is logged as an Error, so the batch fails rather than measuring fewer agents |
| `PositionHash = FCrc::TypeCrc32(FVector3f(StartPosition), PositionHash);` | A running CRC: chaining the seed equals one CRC over all positions, without buffering them. `FVector3f`, so the hash doesn't depend on double rounding noise below float precision. Not for hash tables (06 §13) |
| `UpdateCamera(Elapsed - WarmupSeconds)` | A circle at half the arena's half-width (at least 15 m), 4 m up, pitched down 15°, **one lap per capture window**. The angle is **0 at the planned BenchStart** (negative during warm-up), so the captured path never depends on the warm-up length; the S2 review found the earlier begin-play timing did. It is analytic, so every run sees exactly the same path, and the agents near the camera change continuously, which gives significance something real to react to |
| `if (PlayerController && PlayerController->GetViewTarget() != Camera) PlayerController->SetViewTarget(Camera);` every frame | The player controller may possess its default pawn after begin play, which resets the view target. Re-applying it every frame is robust and costs one pointer compare |
| `Tick` → `WarmUp`: at `Elapsed >= WarmupSeconds` → `CSV_EVENT(UBBench, TEXT("BenchStart"))` | The CSV profiler compares event **text** (without category) case-insensitively with `-csvStartOnEvent=` (`CsvProfiler.cpp:4769`). So this event **starts the capture**: warm-up frames are never in the file |
| `Capture`: the optional wave at `WaveAtSeconds` | `N` more agents with seed + 1, doubling the load, and a `Wave` event marks the frame: the "load doubles" stress case (01 §6.1) |
| `Capture`: at `CaptureSeconds` → `CSV_EVENT(BenchEnd)`, `Phase = Done` | `-csvStopOnEvent=BenchEnd` stops the capture **at the end of this frame** (the stop is queued and handled in `EndFrame`). The game owns the whole window, so the run script needs no frame count |
| `Done`: `WriteRunReport()` on the **first** Done frame | Written *after* the capture has stopped. The S2 review found that writing it on the `BenchEnd` frame put an N-dependent string build and disk write inside the last measured frame |
| `Done`: `FPlatformMisc::RequestExit(false, …)` after `ExitDelayFrames = 3`, once | The CSV profiler writes its file the frame after the capture ends (log: "Capture Ended. Writing CSV"); three frames let it finish before shutdown. `false` is a normal, non-forced exit: on Windows it posts a quit message for the main loop instead of terminating the process (`WindowsPlatformMisc.cpp:1493–1521`), so shutdown runs normally and the log is complete. The director never reports a failure through the exit code; `run_bench.py` judges a run by its log errors and its files instead (M3) |
| `WriteRunReport()` | One CSV line per agent: final position, patrol time, last update time, **age at report** (−1 if never updated), state changes. A `#` header line records the run settings, the `BenchStart` time, the report time and the **effective** compute iterations. It feeds SC6 (positions against the reference run) and the end-of-run staleness check, which catches agents that were *never* updated. `Report.Reserve(Agents.Num() * 96)` avoids repeated string growth for thousands of lines |
| `IsValid(Agent)` before use; an invalid agent writes an empty line | Agents can be destroyed during a run; the line count still equals the agent count, so files line up by index |
| `FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UBRuns"), RunTag + ".csv")` | In the packaged build this resolves to `Build\Windows\UpdateBudget\Saved\UBRuns\`, where `run_bench.py` collects it. A failed write logs an Error |
| `GetStatId`: `RETURN_QUICK_DECLARE_CYCLE_STAT(UUBBenchmarkSubsystem, STATGROUP_Tickables)` | Required by `FTickableGameObject`; gives the director's tick its own entry in `stat` and Insights |

### G4. `UBBaselineBands.h` and `UBBaselineDriverSubsystem.h` / `.cpp`: the baselines B2 and B3 (S3; bands from the settings in Phase 6)

The common hand-made fixes for "too many ticking actors", written the way a team would write them, so UpdateBudget is compared against good native practice, not only against doing nothing (01 §3). **B2** puts every agent into a distance band each frame and gives it that band's tick interval; **B3** does the same through Epic's Significance Manager, the engine's own tool for it.

**`UBBaselineBands.h`: the bands (Phase 6, owner decision)**

Until Phase 6 the bands were constants in the driver that matched UpdateBudget's defaults. The owner decided that B2 and B3 must read **the same numbers as UB from `UUpdateBudgetSettings`**, so a settings change can never make the comparison unfair. The struct's defaults are the S3 constants, which are also the settings' defaults, so the captured S3 baselines stay valid (test `DefaultsAreTheS3Bands`, T7).

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `#include "Math/NumericLimits.h"`, `"Math/UnrealMathUtility.h"`, `"UpdateBudgetSignificance.h"` | `TNumericLimits`, `FMath`, and the plugin's `FUpdateBudgetCategory`. The game module may include the plugin's public headers: it depends on the plugin publicly (A3) |
| `enum class EUBBaselineBand : uint8 { Near, Mid, Far }` | The three bands as a type, so both baselines speak the same language and a typo can't produce a fourth band. Outside the driver class (it was a private nested enum until Phase 6), so the tests can name it |
| `struct FUBBaselineBands { double NearDistance = 1500.0; double FarDistance = 6000.0; float NearInterval = 0.f; float MidInterval = 0.1f; float FarInterval = 0.5f; }` | Plain data, so the tests check it without a world. Distances in centimetres as `double`, because 5.x world positions are `double`. `TickInterval = 0` means "every frame"; 0.1 s is about every 6th frame at 60 FPS, 0.5 s about every 30th |
| `static FUBBaselineBands FromCategory(const FUpdateBudgetCategory& Category)` | **Edges** from the category's `NearDistance` / `FarDistance`. **The Near and Far intervals are its staleness limits** (`NearMaxStaleness` / `FarMaxStaleness`), so an agent at either end may go as long without an update as UpdateBudget allows there: a tick interval and a staleness limit mean the same thing, "at most this long between updates". **Mid** keeps the baselines' own hand-picked 0.1 s, a rate a team would choose, clamped between the two with `FMath::Clamp(Mid, Min(Near, Far), Max(Near, Far))` (`Clamp` is `Max(Min(X, Max), Min)`, `UnrealMathUtility.h:592–595`), so it can never be staler or fresher than both, even for an inverted category (Phase 6 code review: a plain `Clamp(Mid, Near, Far)` returned the Near value there). Why not UB's continuous staleness for Mid: three fixed bands **is** the baseline technique being measured. **Not shared:** the out-of-view multiplier, because B2 and B3 are distance-only techniques; the header comment says so |
| `BandForDistanceSquared(double) const`: `<= Square(NearDistance)` → Near; `< Square(FarDistance)` → Mid; else Far | **The one edge test** both baselines use (S3 altitude audit). Squared distances need no square root. Exactly 15 m is Near and exactly 60 m is Far, the same as UB's significance (P2); the test `SameBandsAsUpdateBudget` checks it at both edges for two categories. Before the S3 audit B2 compared squared distances and B3 compared distances, two tests that agreed only by rounding luck |
| `static EUBBaselineBand BandForSignificance(float)`: `>= 1` → Near; `> 0` → Mid; else Far | The inverse mapping, for B3: the Significance Manager hands back a number, and this turns it back into a band |
| `IntervalForBand(EUBBaselineBand) const` | A `switch` over the three intervals |
| `SignificanceForDistanceSquared(double) const`: Near → `1.f`, Far → `0.f`, Mid → `FMath::Clamp(static_cast<float>(1.0 - FMath::GetRangePct(Near, Far, FMath::Sqrt(DistanceSquared))), TNumericLimits<float>::Min(), 0.99999994f)` | A linear fall-off across the Mid band gives the manager a meaningful **order** (it sorts objects by significance). The square root is taken only in the Mid band. **The clamp:** a Mid value such as 1 − 10⁻⁹ is below 1 as a `double` but rounds to exactly `1.0f`, which would put the agent in the Near band; 0.99999994f is the largest `float` below 1, and `TNumericLimits<float>::Min()` the smallest positive one. So `BandForSignificance` returns Mid for every Mid distance: **B3's bands equal B2's by construction** |

**`UBBaselineDriverSubsystem.h` / `.cpp`: the driver**

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| `UCLASS() class UUBBaselineDriverSubsystem : public UTickableWorldSubsystem` (no `_API`) | The world's lifetime, and a `Tick` after `TG_PostPhysics`. Not exported: only this module uses it. **A subsystem, not an actor:** the project has no hand-authored map (02 §13), and a subsystem exists in every game world automatically. The class comment explains both modes and the cost accounting (H6) |
| `DoesSupportWorldType` → `Game \|\| PIE` | Never in editor worlds |
| `Initialize`: `Benchmark = Collection.InitializeDependency<UUBBenchmarkSubsystem>();` **before** `Super::Initialize` | Tickable world subsystems join the tick list inside `Initialize` (`WorldSubsystem.cpp:94–101`), so initialisation order is tick order (06 §19). The director therefore ticks first: it spawns the wave, and this driver adopts the new agents **in the same frame**, identically in every build. The returned pointer is kept (S3 efficiency audit), so there is no subsystem lookup per frame |
| `UPROPERTY(Transient) TObjectPtr<UUBBenchmarkSubsystem> Benchmark;` and `… TArray<TObjectPtr<UUBDemoBrainComponent>> Brains;` | `UPROPERTY` so GC sees the references; `Transient`: runtime state, never saved. `Brains` is B2's list; B3's brains are held by the manager |
| `FUBBaselineBands Bands;` (Phase 6) | Set in `OnWorldBeginPlay` by `ReadBands`, and only read afterwards, also from B3's worker threads (see the significance lambda) |
| `namespace UBBaseline { const FName SignificanceTag(TEXT("UBAgent")); }` | The Significance Manager groups objects by tag; `UnregisterAll(Tag)` uses it at world end |
| `OnWorldBeginPlay`: `FParse::Value(FCommandLine::Get(), TEXT("UBBaseline="), Requested); Requested.ToUpperInline();` | The mode is read **once**, at world start, and never changes during a run. `FString ==` already compares case-insensitively; the upper-case copy is for the log and the CSV metadata |
| B3: `if (USignificanceManager::Get(&InWorld))` | `Get` is a map lookup by world. The manager is created in `OnPreWorldInitialization`, after world subsystems initialise, so it can't be fetched in `Initialize` (06 §17). It exists by begin play |
| `UE_LOG(…, Error, …)` for a missing manager or an unknown value | A misconfigured run **fails the batch** (`run_bench.py` counts log errors) instead of silently running B0 under the name B3 |
| `static FUBBaselineBands ReadBands()`: `FromCategory(Pick(GetDefault<UUpdateBudgetSettings>()->GetCategories(), GetDefault<UUBDemoBrainComponent>()->GetBudgetCategory()))`; `Bands = ReadBands();` in `OnWorldBeginPlay` (Phase 6) | **The category the brains use**, picked by UpdateBudget's own rule (P2) from the list UB reads too (P3, P6c), so the two can't differ. `GetDefault<UUBDemoBrainComponent>()` is the brain class's default object: every spawned brain uses the class default category, and adoption reports one that doesn't (below). A public static function, so the test checks the driver's real path, this project's settings included (T7; Phase 6 code review). The settings' list is copied into a local first: `Pick` returns a reference into it |
| `CSV_METADATA(TEXT("UBBaseline"), *Requested);`, then `UBIntervalMid` and `UBIntervalFar` (Phase 6), and a `Display` log with all the bands | The capture records which baseline produced it **and the intervals it used**, so an analysis never has to assume them: `analyse_baselines.py` takes its H4 lags from them (M4; Phase 6 code review, the rule UB follows with `UBMaxStaleness`). Only the two keys something reads are written (Phase 6 simplify); the log line states all five values, so a run with changed settings proves what it compared against |
| `OnWorldEndPlay`: `UnregisterAll(SignificanceTag)`, `Brains.Empty()`, `Mode = None`, then `Super` | Runs after every actor's `EndPlay` (`World.cpp:6203–6210`), so the agents have normally unregistered themselves already. This is the safety net, run **while the manager still exists**: it is removed in `OnPostWorldCleanup`, before `Deinitialize`, which would be too late |
| `GetSignificanceManager()` → the manager in B3 mode, else `nullptr` | One rule, used at every call site (S3 simplify). **Looked up each time, not cached:** the manager is removed during world cleanup, so a cached raw pointer could dangle. One map find per call site per frame |
| `GetViewTransform`: first player controller → `PlayerCameraManager` → `GetCameraCacheView()` → `FTransform(ViewInfo.Rotation, ViewInfo.Location)` | The **same view the brains and UB use** (G1, P6f). `GetCameraLocation()` and `GetCameraRotation()` both just return fields of this cached view (`PlayerCameraManager.cpp:774–782`), so one call replaces two virtual calls (S3 reuse audit). Returns `false` before a camera exists, and that frame's re-tune is skipped |
| `SetIntervalIfChanged`: `if (Brain.GetComponentTickInterval() != Interval) { Brain.SetComponentTickIntervalAndCooldown(Interval); ++FrameIntervalChanges; }` | The setter that applies **at once** (06 §18): the plain setter would make a far agent that comes close wait out its old 0.5 s cooldown. Called **only on change**, because each call reschedules the tick function in the cooldown list (H6); calling it every frame for every agent would make the baseline a strawman. The count is published as `UBBaseline/IntervalChanges` |
| `UFUNCTION() void HandleAgentEndPlay(AActor*, EEndPlayReason::Type)` | Bound to each agent's `OnEndPlay` with `AddDynamic`, which binds by name through reflection, so it must be a `UFUNCTION` (06 §20). **B3:** `UnregisterObject(Brain)`, because the manager holds a raw pointer and would keep calling our callbacks on a destroyed object. **B2:** `Brains.RemoveSingleSwap(Brain, EAllowShrinking::No)`, which stops at the first match (each brain is listed once) and fills the hole with the last element instead of shifting the array |
| `AdoptNewAgents`: `const TArray<…>& Agents = Benchmark->GetAgents();` and `if (Agents.Num() == AdoptedAgents …) return;` | A reference, not a copy. On a normal frame the whole adoption costs one comparison |
| `!ensureMsgf(Agents.Num() > AdoptedAgents, …)` | Agents are adopted **by index**, which is correct only because the director's list is append-only (G3). If a future change compacted the list, adoption would silently skip agents and the baseline would be wrong; the `ensure` reports it instead (S3 altitude audit). An `ensure` logs a call stack once and continues |
| The significance lambda: `[this](FManagedObjectInfo* Info, const FTransform& View) -> float { … Bands.SignificanceForDistanceSquared(FVector::DistSquared(…)); }` | **Runs on worker threads** (`Update` uses `ParallelFor`). It reads transforms, at a point in the frame (after `TG_PostPhysics`) where nothing writes them, and `Bands`, which nothing writes after `OnWorldBeginPlay`. It captures `this` (Phase 6; it captured nothing while the bands were constants); the lifetime argument is the post lambda's, below |
| The post lambda: `[this](Info, OldSignificance, NewSignificance, bFinal)` with `EPostSignificanceType::Sequential` | `Sequential` runs on the **game thread** after the parallel pass, where changing tick intervals is allowed; `Concurrent` would run it on worker threads. `bFinal` is `true` only for the call made inside `UnregisterObject` (`SignificanceManager.cpp:325–327`), which is ignored. Capturing `this` is safe: every registration is removed in `HandleAgentEndPlay` or `OnWorldEndPlay`, both before this subsystem is destroyed |
| `if (!bFinal && NewBand != FUBBaselineBands::BandForSignificance(OldSignificance))` | Only a **band change** touches the component (S3 efficiency audit), as a competent Significance Manager user writes it, because the manager hands over both values. It holds from the very first call: the manager starts every object at significance 1 (`SignificanceManager.h:72`) and registration passes `Old = 1` (`SignificanceManager.cpp:236`), the Near band |
| B3, before `RegisterObject`: `SetIntervalIfChanged(*Brain, Bands.NearInterval);` (Phase 6) | Each brain **starts with the Near interval**, the band the manager assumes at registration, so the "only on a band change" rule stays correct for any settings. With the default settings it is a no-op: a new brain already ticks every frame (interval 0) |
| Both lambdas are defined **once**, before the loop | Each `RegisterObject` copies them into its own `TFunction`. Defining them inside the loop rebuilt the same objects per agent (S3 simplify) |
| `if (!SignificanceManager) Brains.Reserve(Agents.Num());` | B2 only: one allocation for the initial set, and one for the wave, instead of repeated growth |
| The loop from `AdoptedAgents`: `IsValid(Agent)` → `GetBrain()`; a category check; `OnEndPlay.AddDynamic(…)`; B3 `RegisterObject(Brain, Tag, …, Sequential, …)`, else `Brains.Add(Brain)` | Each agent is bound **once**, when it is adopted. B3's brains live in the manager, B2's in `Brains`, so neither mode pays for the other's bookkeeping. **The category check** (Phase 6 simplify, altitude angle): the bands come from the brain class's default category, while UB uses each brain's own, so a brain whose `GetBudgetCategory()` differs is logged as an **Error**, once per adoption, and the batch fails instead of comparing unlike things |
| `Retune()`: no view → return; B3 → `SignificanceManager->Update(MakeArrayView(&View, 1)); return;`; B2 → for each valid brain, distance² to the camera → `Bands.BandForDistanceSquared` → `Bands.IntervalForBand` → `SetIntervalIfChanged` | Early returns keep each mode's path flat (S3 simplify). `MakeArrayView(&View, 1)` passes the single viewpoint as an array view without building an array. Epic's design expects **the game** to call `Update` (`SignificanceManager.h:24–27`) |
| `Tick`: `if (Mode == EMode::None) return;` | In B0, B1 and UB the driver costs one comparison per frame |
| `{ CSV_SCOPED_TIMING_STAT(UBBaseline, Register); AdoptNewAgents(); }` | One-off registration (the initial set and the wave frame) is timed **separately** from the per-frame re-tuning, so a spawn frame doesn't pollute the steady-state cost. CSV column **`UBBaseline/GameThread/Register`**: the profiler inserts the thread name for timing stats (seen in the S3 smoke captures) |
| `FrameIntervalChanges = 0;` **before** the `Register` scope, then `{ CSV_SCOPED_TIMING_STAT(UBBaseline, Retune); Retune(); }` | The re-tuning is part of these approaches' real cost, so it is measured (H6). CSV column **`UBBaseline/GameThread/Retune`**. In B3 the scope includes the manager's parallel pass and the post callbacks. The count is reset before adoption (Phase 6 code review), so the interval a B3 brain gets when it is adopted counts in its frame, as B2's first re-tune does; with the default settings adoption changes nothing |
| `CSV_CUSTOM_STAT(UBBaseline, IntervalChanges, FrameIntervalChanges, ECsvCustomStatOp::Set);` | One value per frame: how many agents changed band. With `Retune` it separates "how often" from "how expensive" |
| `GetStatId` → `RETURN_QUICK_DECLARE_CYCLE_STAT(…, STATGROUP_Tickables)` | Required by `FTickableGameObject`; gives the tick a named entry in `stat` and Insights |
| **Not in this class:** the cost of the engine's cooldown list | An agent that has ever had a non-zero interval stays on the tick manager's cooling-down list and is rescheduled every frame (`TickTaskManager.cpp:1538`), even after returning to 0. That cost is inside `GameThreadTime`, so the comparison stays fair, but outside `Retune`. It is examined with Unreal Insights (H6) |

---

## Part T · The tests

### T1. How the tests are built and run, and `UpdateBudgetTestFlags.h`

Every test is an automation test, run headless in the editor by `Tools\run_tests.bat` through the job queue (A8). **28 tests** in five suites: `Scheduler` (11), `Lifetime` (8), `Significance` (6), `Blueprint` (1, editor only) and `Baselines` (2, in the game module). Results are in 09.

| Part | Why |
|---|---|
| `#if WITH_DEV_AUTOMATION_TESTS` … `#endif` | Tests compile into Development builds and disappear from Test and Shipping (`UEBuildTarget.cs:6316`), so they cost a shipped game nothing |
| `constexpr EAutomationTestFlags Flags = EditorContext \| ClientContext \| ProductFilter;` | **Where** a test may run (editor and game) and **which group** it belongs to (project tests). The macro refuses at compile time a test without exactly one filter flag (`AutomationTest.h:4127–4133`) |
| `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClass, "UpdateBudget.Suite.Name", Flags)` + `bool FClass::RunTest(const FString&)` | Declares a test class and registers it under a dotted name, which is what `Automation RunTests UpdateBudget.Scheduler` filters on. `TestEqual` / `TestTrue` record a failure and keep going, so one run reports every broken rule |
| `UpdateBudgetTestFlags.h`: `UpdateBudgetTests::Flags` | The test flags in one header that every plugin test file includes (S5 simplify: the lifetime tests had a copy), so a filter change reaches every suite. A header of its own because the scheduler tests must not pull in the UHT test types and `GameFramework/Actor.h` |
| `UpdateBudgetTestFlags.h`: `UpdateBudgetTests::EditorFlags` = `EditorContext \| ProductFilter` (S8) | For tests that need editor code, such as the Blueprint compiler (T6): no `ClientContext`. In the shared header (S8 code review) |
| The game module's tests define their own `Flags` (T7) | The flags header is private to the plugin module, and a module can't include another module's private headers; the values are the same |

### T2. `Private/Tests/UpdateBudgetTestTypes.h` / `.cpp`: the reflected test types

| Line(s) | Why |
|---|---|
| A header in `Private/Tests/` | UHT reads only headers, so reflected test types need one. The module's `Private` folder is on its include path (`UEBuildModuleCPP.cs:471–473`), hence `#include "Tests/UpdateBudgetTestTypes.h"`. Like the engine's own test classes in `Engine/Private/Tests/` (for example `AutoRTFMTestCameraShake.h:8`), they compile in every configuration and are hidden from editor pickers (`HideDropdown`); they are tiny and never spawned outside the tests |
| `UCLASS(NotBlueprintable, HideDropdown)` | Hidden from class pickers and not usable as a Blueprint base, the same specifiers the engine uses on its test classes. All but the S8 probe use them |
| `TFunction<void(UUpdateBudgetTestComponent&)> OnUpdate;` called from `OnBudgetedUpdate` | Each test decides what happens **inside** an update: destroy, spawn, unregister. A `TFunction` owns its callable (06 §22: a `TFunctionRef` must not be stored) |
| `AUpdateBudgetTestActor`: no tick, one `CreateDefaultSubobject<UUpdateBudgetTestComponent>` | The smallest actor that carries a client. Not a scene component, so no root component is needed |
| `UUpdateBudgetBlueprintProbe` (`UCLASS(Blueprintable, HideDropdown)`, S8) | The client the Blueprint test subclasses (T6). `CountCall()` is a `BlueprintCallable` that counts, so the test can prove the Blueprint ran. `CallEventUnguarded()` calls the event without the guard. `GetEventName()` exists because the event is protected: its name can only be taken (checked) inside a subclass. `Blueprintable` repeats the base's on purpose, next to the `NotBlueprintable` test classes |

### T3. `Private/Tests/UpdateBudgetSchedulerTests.cpp`: the policy (11 tests, S4; three from UE5-TestTrust in S7)

| Part | What it proves |
|---|---|
| `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClass, "UpdateBudget.Scheduler.Name", Flags)` + `bool FClass::RunTest(const FString&)` | Declares a test class and registers it under a dotted name, which is what `Automation RunTests UpdateBudget.Scheduler` filters on. `TestEqual` / `TestTrue` record a failure and keep going, so one run reports every broken rule |
| `NoClients` | Zero clients: nothing runs, nothing breaks, no over-budget flag |
| `DeadlineOrder` | Five clients with a tie: run order is `1, 3, 4, 2, 0`, earliest deadline first, the tie broken by index |
| `BudgetCutOff` | Ten 0.1 ms clients and a 0.45 ms budget: exactly the four earliest run, 0.4 ms spent, six deferred, not over budget |
| `MandatoryBeatsBudget` | Five clients all due before the next frame and a budget for one: all five run, and the overrun is reported |
| `ZeroBudget` | A budget of 0 still runs every mandatory client, most overdue first, and nothing else |
| `CostPrediction` | One step of the moving average is exact (1.0 → 0.8); a steady cost is learned (error far below 10⁻⁹ after 100 frames); a negative measurement counts as 0; **a NaN measurement leaves the prediction and the spent time finite, and the budget cut-off still works on the next frames** (`std::numeric_limits<double>::quiet_NaN()`, because `TNumericLimits` has no NaN in 5.8) |
| `CategoryFallback` | An unknown category reads and teaches category 0, and nothing crashes |
| `DeadlineAtNextFrameIsOptional`, `ExactFitStillRuns`, `ExactlyOnBudgetIsNotOverBudget` (added in S7, written by UE5-TestTrust) | Three boundaries no earlier test sat on, each found by a surviving mutant in UE5-TestTrust's mutation campaign on this file (09, S7). **A deadline exactly at the next frame time is optional** (the strict `<` of P1: it can still run on time next frame); with a zero budget it doesn't run. **An update that exactly fills the remaining budget still runs** (0.25 + 0.25 = 0.5 ms; the cut-off is `>`, not `>=`). **Spending exactly the budget is not over budget.** Their inputs are binary fractions (0.25, 0.5), so each boundary is hit exactly, without rounding. Each was reviewed against the intended behaviour before merging: all three test documented design rules, not implementation accidents |
| `NoStarvation` | **The guarantee itself, under real overload**, simulated: 200 clients (max staleness 0 s, 0.1 s and 0.5 s) for 600 fixed 60 Hz frames.<br>**The overload:** a 0.3 ms budget at 0.01 ms per update. The 20 near clients take 0.2 ms, leaving room for 10 optional updates per frame against a demand of about 16.7 (80/6 + 100/30). The code review found that the first version (0.5 ms) never actually overloaded, so the test now **asserts that mid/far clients were forced to run**.<br>**The checks:** no client ever waits longer than its max staleness, or one frame when that is shorter; every client is updated; near clients run every frame; and every frame that ran optional work stayed inside the budget.<br>**Why the bound holds:** a client becomes mandatory on the first frame where waiting would make it late. On the previous frame it was not yet due, so it always runs at or before its deadline |

### T4. `Private/Tests/UpdateBudgetLifetimeTests.cpp`: real components in a real world (8 tests, S5; two more in S7)

| Part | What it proves |
|---|---|
| `using FAction = TFunction<void(UUpdateBudgetTestComponent&, int32 Update)>;` and `Spawn(Log, Index == K ? Action : FAction())` | Each test defines its in-update action **once** and gives it only to the client that performs it (simplify: every client used to capture a "am I the one?" flag) |
| `using namespace UpdateBudgetLifetimeTests;` as the first line of each test, not at file scope (Phase 6) | **Unity builds compile several `.cpp` files as one**, so a file-scope using-directive leaks into the files after it. The Phase 6 build was the first to batch this file with the scheduler tests, and it failed: the scheduler test's local `FrameSeconds` hid the leaked `UpdateBudgetLifetimeTests::FrameSeconds`, which the compiler reports as error C4459. A using-directive inside each function can't leak |
| `FTestBudgetWorld` | The engine's `FTestWorldWrapper` (06 §28) plus the mode. **Member order is deliberate**: `FTestConsoleVariable Enabled` is declared first, so it is destroyed **last**, after the world (04 §18). Restoring the cvar earlier would change the mode under a living world, through the live switch (P6d) |
| `Enabled.Set(…)` before `CreateTestWorld`/`BeginPlayInTestWorld` | The mode is read in `OnWorldBeginPlay`, inside `BeginPlayInTestWorld` (`AutomationCommon.cpp:185`) |
| `FUpdateLog` declared **before** the world in every test | The components' lambdas write into it by reference, so it must outlive the world |
| Counts kept in `FUpdateLog`, never read from a destroyed component | `UWorld::Tick` may collect garbage at its end (`LevelTick.cpp:1968–1971`), so a destroyed component may be freed after any test tick |
| `UpdatesOutOfPlay`: an update when `!HasBegunPlay() \|\| !IsRegistered()` | The definition of a **stale entry**: the registry updated something that had left play or left the world. Every test asserts it stays 0 |
| `Spawn` after `BeginPlayInTestWorld` | The actor begins play inside `SpawnActor` (`Actor.cpp:4509–4529`), so spawn order is registry order. With equal deadlines the policy runs clients in index order (`DeadlineOrder`, T3), which makes "before" and "after" in a frame exact |
| `ModeAtBeginPlay` | Native and budgeted worlds: 4 clients registered; driven-by-scheduler and native-tick state match the mode; **exactly 3 updates in 3 frames** in both modes |
| `DestroyDuringUpdate` | In its 2nd update, client 2 destroys client 0 (already run this frame), **itself**, and client 5 (not run yet). Client 5 is skipped (1 update), 3 clients remain, the survivors keep exactly one update per frame |
| `SpawnDuringUpdate` | Client 1 spawns one client and spawns-then-destroys another inside its update. The kept one is registered after the loop, driven by the scheduler with its native tick off, and first updated **next frame**; the cancelled one never registers (02 §12 item 7) |
| `LevelUnloadOrder` | The streaming-removal order: `UnregisterAllComponents` first (once between frames, once from inside an update, which is deferred), then `RouteEndPlay(RemovedFromWorld)` later. The client stops updating at the first step, and the later `EndPlay` changes nothing |
| `NoDoubleUpdate` | Three ways other code can try to bring a scheduler-driven client's native tick back. `ReregisterComponent()` keeps the client registered with its native tick off. `Activate(true)` can't turn the tick on (since S7 it goes through the `SetComponentTickEnabled` override, P5). Enabling the tick function directly (`PrimaryComponentTick.SetTickFunctionEnable(true)`) does turn it on (asserted), and the `TickComponent` guard turns it off again. Both clients get exactly 4 updates in 4 frames |
| `WorldTeardown` | A frame whose updates spawn and destroy clients, then the world's `EndPlay` (0 clients, no component keeps a link), then `DestroyTestWorld(true)`: `Deinitialize`, a full garbage collection, and a weak pointer proving the subsystem is gone. The engine never ends a world inside a tick group (02 §8), so this is the worst real case of "end play mid-frame" |
| `LiveSwitch` (S7) | Budgeted → native → budgeted, 3 clients, 9 frames. **The first switch is requested from inside a client's update**, while the scheduler is running: the case the deferral exists for (code review). A change undone within one frame (`1` then `0`) switches nothing. A request isn't applied before its frame ends. After each switch every client is in the new mode (`CheckClientsAre`). `Log.Updates == { 9, 9, 9 }`: **exactly one update per client per frame through both switches**, none doubled or missed (03 S7's check) |
| `LiveSwitchKeepsTickState` (S7) | Native; gameplay pauses one client. Switch to budgeted; there, gameplay pauses the running client and resumes the paused one (the native ticks stay off). Switch back: the client paused while budgeted stays paused, and the one resumed while budgeted ticks again and updates once per frame |
| `Test.Wrapper.ForwardErrorMessages(this)` | The wrapper records its own failures (for example a missing game mode) without logging them; this turns them into test errors |
| The test cvar is process-wide | Running the lifetime tests in an editor with a PIE session open switches that session too (S7 code review); `run_tests.bat` runs them in their own process |

### T5. `Private/Tests/UpdateBudgetSignificanceTests.cpp`: significance and the view (6 tests, S6; `CategoryPick` in Phase 6)

| Test | What it proves |
|---|---|
| `Significance.Bands` | 1 at the camera and at exactly 15 m; 0.75 a quarter of the way; 0.5 half way (37.5 m); 0 at exactly 60 m and beyond |
| `Significance.OutOfView` | × 0.5 out of view: near 0.5, half way 0.25, far 0 |
| `Significance.MaxStaleness` | 0 s at 1, 0.5 s at 0, 0.25 s at 0.5, and 0.25 s for a near client out of view |
| `Significance.FarNotBeyondNear` | Far = Near and Far < Near give a step at the near distance, with no division by zero and no NaN |
| `Significance.ViewCone` | At 90° and 16:9: straight ahead in, behind out, near the left edge in, **near a screen corner (47° off axis) in**, beyond the corner (51°) out, 90° to the side out, at the camera and 1.5 m behind it in (the always-in-view radius). The corner case is the code review's finding, kept as a test |
| `Significance.CategoryPick` (Phase 6) | The shared category rule (P2): a valid index, and an index past the end and a negative one (both mean category 0). And the list it reads: a settings object whose array was emptied still gives one default category through `GetCategories` (P3) |

### T6. `Private/Tests/UpdateBudgetBlueprintTests.cpp`: `UpdateBudget.Blueprint.EventCost` (S8, editor only)

| Part | What it does, and why |
|---|---|
| `MakeUniqueObjectName(…)`, `FKismetEditorUtilities::CreateBlueprint(UUpdateBudgetBlueprintProbe::StaticClass(), GetTransientPackage(), Name, BPTYPE_Normal)` | A Blueprint subclass made in code (`KismetEditorUtilities.h:110`). `CreateBlueprint` makes it `RF_Standalone` (`Kismet2.cpp:453`), which the editor's garbage collector keeps (`GarbageCollection.h:28`), so a fixed name would collide on a second run in the same editor: the name is unique, as in the engine's own test (`ToolsetLibraryTest.cpp:33–43`) |
| `ON_SCOPE_EXIT { Blueprint->ClearFlags(RF_Standalone); };` | **Cleanup** (S8 code review): on every way out the flag goes, so the next collection takes the Blueprint and its generated class once the test world and its probes are gone. The engine's test does the same (`ToolsetLibraryTest.cpp:1698–1704`); `ON_SCOPE_EXIT` is `ScopeExit.h:73` |
| `AddDefaultEventNode(…, GetEventName(), UBudgetedUpdateComponent::StaticClass(), …)` | The event node, made the way `CreateBlueprint` makes a new Blueprint's default events (`Kismet2.cpp:563`, `671–745`). It is placed as a disabled "ghost" node (`:741`, `EdGraphNode.cpp:1132–1137`), like the greyed-out events a new Blueprint shows. Passing the class that **declares** the event means no "call parent" node is added: its parent has no such function (`Kismet2.cpp:714–739`, `EdGraphSchema_K2.cpp:883–897`) |
| `FGraphNodeCreator<UK2Node_CallFunction>` + `SetFromFunction(CountCall)` + `Finalize()`; `TryCreateConnection(event "then" pin, call exec pin)` | The call node, placed the way the editor does (`EdGraph.h:283–307`), and the wire. The K2 schema connects through `MakeLinkTo` (`EdGraphSchema_K2.cpp:2294–2298`, `EdGraphSchema.cpp:474–494`), which **turns both nodes into real ones** (`EdGraphPin.cpp:514`, `538–539`). The wire is what makes the event part of the compiled class: the smallest real graph, the event and one call |
| `FKismetEditorUtilities::CompileBlueprint(Blueprint)` and `Status != BS_Error` | Compiles the graph to bytecode (`KismetEditorUtilities.h:169`); the generated class is what gets instantiated |
| One native and one Blueprint probe on one actor in a native-mode test world; `RegisterComponent()` | The owner has begun play, so each component begins play at registration (`Actor.cpp:6443–6451`). `OnRegister` sets `bHasBlueprintUpdate`, and the test checks it on both. Native mode, and no ticking: only the test calls `RunUpdate` |
| Three named lambdas (`RunNative`, `RunScripted`, `RunUnguarded`), a warm-up, then 5 alternating rounds of 100,000 calls each; the median round per kind | Named once (S8 simplify), so the warm-up and the timed rounds can't drift apart. Alternating rounds spread drift over all three. The **Blueprint event cost** is the Blueprint probe's median minus the native one's: the stub, `ProcessEvent`, the VM running the graph and the native call back. Each lambda captures one pointer and is inlined through the template, so the loop adds almost nothing: two clock reads per 100,000 calls |
| `GInitRunaway();` at the start of every batch, before the clock starts | **Found by the first S8 run.** In editor and development builds the Blueprint VM counts branch steps (jumps and flow-stack pops, `ScriptCore.cpp:2598–2699`) since the last reset, and past `MaximumLoopIterationCount` (1,000,000: `BaseEngine.ini:317`, `ScriptCore.cpp:107`) it aborts the call and reports an infinite loop (`:1268–1294`). The world tick resets the count once per frame (`LevelTick.cpp:1563`). The test ran all 501,000 Blueprint calls in one frame, so one call was aborted and the count check failed (499,999). Each batch now stands for one frame, and resets the count as the world tick does; the engine resets it the same way outside a world tick, for example before a widget's tick (`UserWidget.cpp:2106`). The guard exists when `DO_BLUEPRINT_GUARD` is on: every build but Shipping and Test, and every editor build (`CoreMisc.h:338–344`) |
| `Scripted->Calls == Calls × Rounds` | Proves the Blueprint ran once per update. A check that the native probe never ran it was dropped in the code review: with no script it can't, so that check could never fail |
| One `UE_LOG(LogTemp, Display, "UpdateBudget S8: …")` | One output path (S8 simplify): a Display line logged during a test also becomes an event in the test report (`AutomationTest.cpp:230–254`), so an extra `AddInfo` printed the line twice |
| What the numbers are, and aren't | Measured in the **editor** (Development, headless), in a tight loop on one object with warm caches: they compare one call with another on this machine. `ProcessEvent` does a little more checking in an editor build (`ScriptCore.cpp:2073–2084`). They are not benchmark results, and a larger Blueprint graph costs more |

### T7. `Source/UpdateBudget/Private/Tests/UBBaselineBandTests.cpp`: the baselines use UB's bands (2 tests, Phase 6)

| Part | What it proves |
|---|---|
| `#if WITH_DEV_AUTOMATION_TESTS`, local `Flags` | The same rules as the plugin's tests (T1). In the game module because the bands are (G4) |
| `BandFromUpdateBudget(Category, Distance)` | **UB's own reading of a distance**: `FUpdateBudgetCategory::GetSignificance` in view, mapped to a band with `FUBBaselineBands::BandForSignificance` (Phase 6 simplify: the helper had a copy of that mapping). B2's check still compares an independent edge test with it |
| `Baselines.DefaultsAreTheS3Bands` | **The driver's own path**, `UUBBaselineDriverSubsystem::ReadBands()` (this project's settings, the brain class's category, UpdateBudget's rule), gives **exactly** 1500 / 6000 cm and 0 / 0.1 / 0.5 s, the constants the S3 baselines were captured with, compared with `==`. So the Phase 6 change leaves every captured batch valid, and a `Categories` entry in `DefaultGame.ini` that changed the bands would fail this test on purpose (Phase 6 code review: the first version tested the struct's defaults, not the path the driver takes) |
| `Baselines.SameBandsAsUpdateBudget` | For the default category **and a changed one** (10 m / 30 m, 0.05 s / 1 s), at nine distances including both edges and 1 cm either side of them: B2's distance band and B3's significance band both equal UB's. The Near and Far intervals equal the staleness limits, and Mid lies between them. A far limit below 0.1 s pulls Mid down with it (0.05 s), and an **inverted** category keeps Mid between its two limits. *Stated limit:* within a fraction of a millimetre past the near edge, UB's `float` significance can round to exactly 1, a Near reading, where the baselines' clamp keeps Mid (the code review measured it at about 3 × 10⁻⁴ cm with the defaults); it changes nothing measurable |

---

## Part M · The measurement tools (`Tools\*.py`)

### M1. How they fit together

`run_bench.py` (M3) runs a batch: it pins itself to the performance cores, records the environment, launches the packaged game once per run with the configuration's flags, and collects each run's CSV capture, per-agent report and log into `Benchmarks\Raw\`, with a manifest. `csv_stats.py` (M2) is the one place where statistics are computed, used by everything else. `analyse_baselines.py` (M4) answers the baseline hypotheses H1, H4 and H6; `analyse_ub.py` (M5) checks SC1–SC3 on UB runs. None of them ever modifies a raw file. They are standard-library Python, run with `python -I` (isolated mode: no user site-packages, no script-directory imports).

### M2. `csv_stats.py`: loading captures and computing statistics (S2)

| Part | What, why |
|---|---|
| `csv.field_size_limit(1 << 30)` | Unreal's metadata row has one very long field (the command line and every metadata pair). Python's default 128 KB limit raised an error on the first real capture |
| `load_capture` | A finalised capture ends with the **full header** and a `[key],value,…` metadata row. Both are dropped, and **the end header is used for column lookup**: with `HasHeaderRowAtEnd`, columns can appear mid-capture, so the first header is only a *prefix* (seen on a real capture: 282 vs 286 columns). Finalised means the last row starts with `[` and the first header is a prefix of the end header; otherwise every row is data and the result is flagged `finalised: False`. **Two bugs found and fixed in S2:** (1) metadata keys were emptied (stripping up to `]`), caught by `tag=?`; (2) the first finalised check compared the two headers for equality, which rejected valid captures |
| `frame_events` / `--split-event` | The `EVENTS` column holds `Category/Name##timestamp;…`. A wave run is split at the `Wave` frame into **pre**, **event frame** and **post** statistics, so steady state at the new load isn't mixed with the spawn hitch (S2 review) |
| `percentile` | Linear interpolation between closest ranks: position = p/100 × (n−1), the same as numpy's default. Documented in the file so any reader can reproduce a number |
| `summarize` | n, mean, min, median, p95, p99, max |
| `--selftest` | Checks the maths against hand-calculated values (1…10: median 5.5, p95 9.55, p99 9.91) before any benchmark trusts it |
| `--skip N` | Drops the first N frames. Only needed for captures that start at boot. Event-started captures use 0 |

### M3. `run_bench.py`: running a batch (S2; P-core pinning and the run rules in S3/S4; UB in S6)

| Part | What, why |
|---|---|
| `COMMON_ARGS` | The same flags for every run: 1280×720 windowed on the **real GPU** (`GameThreadTime` is 0 under `-nullrhi`, S1 finding), fixed step `-deterministic -FPS=60` (identical simulated frames in every configuration, `01b` B-1), `-novsync`, `-traceautostart=0`, `-UBExit`, and the event-driven capture window |
| `CONFIGS` | Config name → extra arguments. B0 = nothing; B1 = `-ExecCmds="tick.AllowBatchedTicks 1"`; B2 = `-UBBaseline=B2`; B3 = `-UBBaseline=B3` (S3, G4). **UB is added only when the code implementing it exists** (S6). A config is never listed before its code exists, so a batch can't silently measure B0 under another name |
| `environment_record()` | CPU, GPUs and drivers, RAM, OS build, **power plan and AC power** (thermal noise control on a laptop), engine version and changelist, build configuration, compiler, resolution, flags. Stored with every batch ([Benchmarks/README.md](../Benchmarks/README.md)) |
| Command built as **one string** | So `-ExecCmds="…"` reaches Unreal exactly as written. Python's list quoting would have wrapped the whole argument in quotes, which Unreal's parser doesn't expect |
| `GameUserSettings.ini` deleted before each run | It can override VSync and frame caps at startup (`01b` §4) |
| Interleaving | The config order **rotates each round** (B0,B1 / B1,B0 / …), so laptop thermal drift doesn't systematically favour one configuration |
| Names | `<batch>_<Config>[_W<work>]_N<agents>_r<run>`, with `<batch>` = `YYYY-MM-DD_HHMMSS[_label]`. **Raw files are never overwritten** (`unique_path` raises). The S2 review found that date-only names let a second batch on the same day silently replace the first |
| Priming run | One discarded run before the batch warms file, driver and PSO caches, so the first measured run isn't the only cold one (S2 review). The number of runs per combination is a multiple of the number of configs, so with the rotation each config leads equally often |
| Artefact collection | Only files written **after this run started** are taken: the CSV, the per-agent report, and the log, whose modification time is checked so a run that never started can't inherit the previous log (S2 review). Analysis errors are caught per run. **The manifest is rewritten after every run**, so a crash loses nothing |
| Timeout | `warmup + capture + 180 s`; a hung run is recorded (`exit_code: null`) instead of hanging the batch |
| Failure rule | A run fails if it has a non-zero exit, a timeout, no or partial capture, a missing log, log errors, or an analysis error. Failed runs are listed as **ATTENTION** and the script exits non-zero. Each run's **PSO events** are counted, so shader-compile noise is visible |
| `write_summary` | For each (config, work, N): the **median across runs** of each run's median, p95 and p99, with the **[min–max] across runs**, as `Benchmarks\README.md` defines |
| `performance_core_mask()` | Asks Windows which logical CPUs belong to the **performance cores**: `GetLogicalProcessorInformationEx(RelationProcessorCore)` returns one record per physical core, with an `EfficiencyClass` byte (offset 9) and a group affinity mask (offset 32). P-cores have the higher class. On this i7-14650HX the result is `0xFFFF`: 8 P-cores × 2 hardware threads = logical CPUs 0–15. It returns `None` on a non-hybrid CPU, so the script still runs anywhere |
| `KERNEL32 = ctypes.WinDLL("kernel32")` with its prototypes, set once behind `sys.platform == "win32"` | One place for every Windows call. `restype`/`argtypes` are declared, so 64-bit handles and masks aren't truncated by ctypes' default `int`. Off Windows, `KERNEL32` is `None` and the pinning is skipped |
| `pin_self(mask)`, called once in `main()` before anything is recorded or started | `SetProcessAffinityMask(GetCurrentProcess(), mask)`. **Every process started afterwards inherits the mask from its first instruction** (S3 simplify, altitude angle; verified on this machine: a child of a process pinned to `0xFFFF` reports `0xffff`). This replaced pinning each game after `Popen`, which left a start-up window and needed a per-run failure state. If the pin fails on a hybrid CPU, the batch refuses to start |
| `applied_mask()` | Reads back the mask Windows actually applied (`GetProcessAffinityMask`), so the environment record states what happened, not what was asked for |
| `EXE` = `Build\Windows\UpdateBudget\Binaries\Win64\UpdateBudget.exe` | The **real** game executable, launched directly. The packaged bootstrap one folder up only starts this one. Launching it directly makes the game a direct child, so its **exit code is real** and a hung or interrupted run is always killed (`taskkill /T` in a `finally` block). The first pinned version used `cmd /c start /wait /affinity`, which **always returns 0**. P03 found this while testing the same idea, and it would have hidden every failed run from `is_failure` |
| `"cpu_affinity"` in the environment record | The mask applied to the whole batch, read back after pinning. Every run of the batch inherits it |
| `MIN_RUNS = 4`; `--runs` default `math.ceil(MIN_RUNS / len(configs)) * len(configs)` | The smallest multiple of the number of configs that is at least 4: 4 for B0–B3, 4 for two configs, 5 with UB. With the rotation, every config sits in every order position equally often. A `--runs` that isn't a multiple prints a warning (S3 code review: the first version gave a one-config batch a single run). The rule is stated once, in the `--runs` help (built from `MIN_RUNS`) and `Benchmarks/README.md`; the module docstring points there |
| `SetThreadExecutionState(ES_CONTINUOUS \| ES_SYSTEM_REQUIRED \| ES_DISPLAY_REQUIRED)` | While the batch process lives, Windows keeps the display and system awake, as it does for a video player. A locked or occluded desktop could throttle a windowed game's presentation. **No power setting is changed**, and the request ends with the process |
| `run_bench.py` `CONFIGS["UB"] = "-ini:Engine:[ConsoleVariables]:ub.Enabled=1"` | The budgeted configuration, added now that its code exists (the rule above). Set through the ini system because the mode is read at begin play (06 §26); S5-d verified the route. The budget comes from the settings: 2.0 ms |
| `SUMMARY_COLUMNS` gains `UpdateMs`, `MandatoryMs`, `MandatoryRuns`, `OverBudget`, `NearMissed`, `GameThread/RunFrame`, `GameThread/Deadlines` | So every UB run's manifest holds their statistics. `csv_stats.summarize_rows` skips columns a capture doesn't have, so native runs are unaffected (`csv_stats.py:110–118`) |

### M4. `analyse_baselines.py`: the baseline hypotheses (S3)

| Part | What, why |
|---|---|
| Reads the batch manifest, then each run's raw CSV through `csv_stats.load_capture` | The same loader as the summaries; raw files are never modified. Unfinished captures are skipped. **Per-run `GameThreadTime` and `UpdatesRun` statistics are taken from the manifest**, which `run_bench.py` computed with the same `csv_stats` code, so the summary and the analysis can't drift apart (simplify, reuse angle; bit-identical on all 64 runs). The CSV is read only for what the manifest doesn't hold: the per-frame series and the driver columns, through `csv_stats.column_values` and `csv_stats.summarize` |
| Groups by `(config, work, agents)`, then re-keys by `(config, N)` | The same key as `run_bench.py`'s summary, so different workloads are never pooled (code review). A batch with several workloads is refused, after which the workload is the same everywhere and the tables loop over `(config, N)` directly. `table()` and `col()` build every header, separator and cell, so a column count can't get out of step with its separator |
| **H1** table: B0 − B1 per run, paired **by run index** | Runs of the same round ran next to each other, so pairing them cancels slow drift. Pairing is by the run number both configs have, not by list position: a skipped run would otherwise shift every later pair (code review) |
| **H4** table: autocorrelation of `UpdatesRun` at the Mid and Far band intervals in frames, and max / median | With the default intervals at the fixed 60 Hz step the lags are 6 (0.1 s) and 30 (0.5 s). Values near 1 mean the load repeats with that period: agents given the same interval at the same moment stay in phase. **The lags come from the B2/B3 captures' own metadata** (`ubintervalmid`, `ubintervalfar`; Phase 6 code review), and the fixed step from the batch's command line, so a batch with changed settings is analysed at its own intervals. Batches captured before Phase 6 carry no band metadata and used the defaults, so the script falls back to them; a batch whose B2/B3 runs used different bands is refused. The B2/B3 runs are read first, so their intervals fix the lags before any run is measured, and nothing is kept per run (Phase 6 simplify, efficiency angle: the first version kept every run's update series until the end). The fixed step is read once. Re-running it on batch `2026-11-21_015041_baseline_pinned` gave a file identical to the committed one |
| **H6** table: `UBBaseline/GameThread/Retune` median and p99, interval changes per frame | The baselines' own re-tune cost. The tick manager's cooling-down list cost is inside `GameThreadTime`, not in this column |
| Optional drift check (`split_round`) | Rounds 1..split against the later rounds: shows whether the machine itself changed during a batch |

### M5. `analyse_ub.py`: SC1–SC3 on UB runs (S6)

| Part | Why |
|---|---|
| `analyse_ub.py` (new): `check_run(record, frame_ms)` | Per measured UB run: **SC1** p95 of `UpdateMs` ≤ 1.10 × the run's `BudgetMs`, with over-budget frames split into forced (`MandatoryMs > BudgetMs`) and soft; **SC2** every frame's `MaxGapMs` and every agent's end-of-run age within the run's own bound (`ubmaxstaleness` metadata + one frame), and no agent never updated; **SC3** the share of frames with `NearMissed == 0`. Plus `GameThreadTime` and the two scheduler scopes for context |
| Statistics from the manifest (`record["stats"]`), per-frame series from the CSV | The same numbers `run_bench.py` computed, so the two never drift apart (simplify, reuse angle; the rule `analyse_baselines.py` follows). The CSV is read only for what needs single frames: the over-budget split, the gap check and `NearMissed` |
| `run_bench.is_failure(record)` and per-run `try` / `except` | A run that `run_bench.py` counts as failed, or that can't be analysed (no metadata, an empty agent report), is **listed with its reason** instead of stopping the check (code review). The script then exits 1. It reuses `run_bench`'s own failure rule rather than a copy that could drift (simplify) |
| Checked offline before use | On a synthetic capture with known answers in the scratch area (raw data is never touched): one forced and one soft overrun gave "2 (1 / 1)"; one over-bound gap and one over-bound age gave SC2 FAIL; one missed-near frame in 100 gave 99.00 %; a run with `exit_code: null` was listed as not checked, and the exit code was 1 |
