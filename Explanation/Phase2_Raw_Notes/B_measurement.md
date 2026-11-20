> **Raw Phase 2 research notes (B_measurement), UE 5.8.4, 2026-11-20.** Written by a research pass and kept as an evidence trail. Every claim cites a file:line or URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md) (marked ✔). Treat anything here that is not restated there as unverified.**

# P01 Phase 2, Part B: Measurement infrastructure in UE 5.8.4 (source-verified)

Engine: `C:\Program Files\Epic Games\UE_5.8\Engine`. `Build/Build.version` gives `"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 4, "Changelist": 58902052`.
All paths are relative to `Engine/`. **[inferred]** marks conclusions drawn from code flow rather than a single explicit line.

---

## B1. Frame timing modes

### B1.1 The command-line switches

| Switch | Where it's parsed | Effect |
|---|---|---|
| `-BENCHMARK` | `Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:2451`: `FApp::SetBenchmarking(FParse::Param(FCommandLine::Get(), TEXT("BENCHMARK")));` (inside `#if !UE_BUILD_SHIPPING`; Shipping forces `false` at :2453) | Turns on fixed time step (see below). It also forces a fixed RNG seed (:2462), forces `FApp::HasFocus()` to true (`Source/Runtime/Core/Private/Misc/App.cpp:347-349`), disables audio (`Source/Runtime/Core/Public/Misc/App.h:437`: `bApplicationTypeDoesNotRenderAudio = FApp::IsBenchmarking() || ...`), detaches the ini files so they aren't saved (`LaunchEngineLoop.cpp:4324-4328`), and enables `-BENCHMARKSECONDS=` (:4717-4722) and the frame-count exit (:5692). |
| `-UseFixedTimeStep` | `LaunchEngineLoop.cpp:2460`: `FApp::SetUseFixedTimeStep(bDeterministic \|\| FParse::Param(FCommandLine::Get(), TEXT("UseFixedTimeStep")));` | Fixed time step, without benchmark's other side effects. |
| `-deterministic` | `LaunchEngineLoop.cpp:2457-2462`: `// "-Deterministic" is a shortcut for "-UseFixedTimeStep -FixedSeed"` | Fixed time step plus fixed seed (`FApp::bUseFixedSeed`, so `FMath::RandInit(0)` at :2470-2477). It does **not** set benchmarking. |
| `-FPS=<n>` | `LaunchEngineLoop.cpp:4725-4731`: `FParse::Value(FCommandLine::Get(),TEXT("FPS="),FixedFPS); if( FixedFPS > 0 ) { FApp::SetFixedDeltaTime(1 / FixedFPS); }` (inside `#if (!UE_BUILD_SHIPPING \|\| ENABLE_PGO_PROFILE)`, :4708) | Only changes `FixedDeltaTime`. It has no effect unless fixed step is active. |
| (default) | `Source/Runtime/Core/Public/Misc/AppTime.h:74`: `double FixedDeltaTime = 1 / 30.0;` | **Surprise:** `-BENCHMARK` alone gives a **30 Hz** simulation step. |
| `-SECONDS=` / `-BENCHMARKSECONDS=` | `LaunchEngineLoop.cpp:4709`, `:4719`; frames computed at :4736 `MaxFrameCounter = FMath::TruncToInt(MaxTickTime / FApp::GetFixedDeltaTime());`; exit at :5692-5695 | Auto-exit after N seconds (`-SECONDS=` counts summed DeltaTime, :6075 `TotalTickTime += FApp::GetDeltaTime();`). |
| `-novsync` / `-vsync` | `Source/Runtime/Engine/Private/UnrealEngine.cpp:2555-2561`: `GEngine->DeferredCommands.Add(TEXT("r.vsync 0"));` | Queues `r.vsync` as a deferred console command (non-Shipping). |
| `-ExecCmds="a, b"` | `UnrealEngine.cpp:2552`; split on commas at `Source/Runtime/Engine/Private/ParseExecCommands.cpp:28` | Deferred console commands. |

Compile gate: `WITH_FIXED_TIME_STEP_SUPPORT` comes from `TargetRules.bWithFixedTimeStepSupport`, default `true` (`Source/Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs:1605`; emitted at `Configuration/UEBuildTarget.cs:6450-6456`).

**5.8 change:** time now lives in a pipelined `FAppTime` struct (`Source/Runtime/Core/Public/Misc/AppTime.h:22`). Its comment says the value "is pipelined through the TaskGraph", and `FAppTime::Get()` ensures when called from a thread with no time context (:98-104). `Get_UnsafeNoCheck` is `UE_DEPRECATED(5.8, ...)` (:107). So `FApp::GetDeltaTime()` must not be read from arbitrary worker threads. **[inferred]**

### B1.2 `UEngine::UpdateTimeAndHandleMaxTickRate` (`Source/Runtime/Engine/Private/UnrealEngine.cpp:2980-3218`)

Selecting the mode (:3004-3008):
```cpp
#if WITH_FIXED_TIME_STEP_SUPPORT
	const bool bUseFixedTimeStep = FApp::IsBenchmarking() || FApp::UseFixedTimeStep();
```
A `CustomTimeStep` (`UEngineCustomTimeStep`), if set, runs first and can skip everything (:2987-2996).

**Fixed-step branch** (:3015-3023). No wait and no max-tick-rate code:
```cpp
	const float FrameRate = FApp::GetFixedDeltaTime();
	FApp::SetDeltaTime(FrameRate);
	FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());
```
- DeltaTime equals FixedDeltaTime every frame, whatever the real time. It passes through a `float`, so `-FPS=60` gives `(double)(float)(1.0f/60)` ≈ 0.016666668.
- **The engine runs as fast as possible.** There is no sleep, and `t.MaxFPS`, smoothing, `bUseFixedFrameRate` and `MaxDeltaTime` are all inside the `else` branch, so they are ignored. **[inferred]** Present-level VSync (`r.VSync`) still throttles, because it acts in the RHI, not here.

**Real-time branch** (:3024-3197):
- `CurrentRealTime = FPlatformTime::Seconds()` (:3027). DeltaRealTime is the difference from the previous frame (:3046).
- `UpdateRunningAverageDeltaTime(DeltaRealTime)` (:3055) feeds the smoothing average.
- `GivenMaxTickRate = GetMaxTickRate(DeltaRealTime)` (:3058). Then (:3059):
  `const float MaxTickRate = FABTest::StaticIsActive() ? 0.0f : (bUseFixedFrameRate ? FixedFrameRate : GivenMaxTickRate);`
- `WaitTime = FMath::Max( 1.f / MaxTickRate - DeltaRealTime, 0.f )` when `MaxTickRate > 0` (:3062-3065).
- **It does wait.** It sleeps `WaitTime - 2 ms` when the wait is over 5 ms, then spins with `FPlatformProcess::SleepNoStats(0)` until `WaitEndTime` (:3100-3124). The wait is wrapped in `UE::Stats::FThreadIdleStats::FScopeIdle` (:3084), so it is **excluded from GameThreadTime**.
- With `bUseFixedFrameRate`, logical time is forced to `LastRealTime + 1/FixedFrameRate` (:3129-3134, :3141-3146). That is "fixed frame rate": it waits to the rate **and** lies about DeltaTime when the frame overruns ("we start falling behind real time (and that's ok)", :3143).
- `MaxDeltaTime` clamp: only in standalone and only if `UGameEngine::MaxDeltaTime > 0` (:3169-3195). `Config/BaseEngine.ini:1581` sets `MaxDeltaTime=0` under `[/Script/Engine.GameEngine]`, so it is off by default.
- After both branches: `t.OverrideFPS` (non-Shipping) can still force `DeltaTime = 1/OverrideFPS` (:3201-3211). The cvar is defined at :520-524.

**`UEngine::GetMaxTickRate`** (`UnrealEngine.cpp:12193-12240`):
- If `IsAllowedFramerateSmoothing()` holds, `MaxTickRate = 1/RunningAverageDeltaTime`, clamped to `SmoothedFrameRateRange` (:12197-12211).
- **`t.MaxFPS` overrides smoothing:** `else if (CVarMaxFPS.GetValueOnAnyThread() > 0) { MaxTickRate = CVarMaxFPS.GetValueOnAnyThread(); }` (:12234-12237).
- `IsAllowedFramerateSmoothing()` = `FPlatformProperties::AllowsFramerateSmoothing() && bSmoothFrameRate && !bForceDisableFrameRateSmoothing && !IsRunningDedicatedServer()` (:12161). Windows is `true` (`Source/Runtime/Core/Public/GenericPlatform/GenericPlatformProperties.h:215-217`).
- `t.MaxFPS` is defined at :12136-12138: `TEXT("t.MaxFPS"),0.f, TEXT("Caps FPS to the given value.  Set to <= 0 to be uncapped.")`.
- `UGameEngine::GetMaxTickRate` (`Source/Runtime/Engine/Private/GameEngine.cpp:1719-1765`) adds the net-server tick-rate clamp for dedicated and listen servers, and lets a non-zero `Super` value win (:1758-1762).

**Defaults:**
- `Config/BaseEngine.ini:351-352` (`[/Script/Engine.Engine]`, header at :130): `bSmoothFrameRate=false`, `SmoothedFrameRateRange=(...22...62)`.
- `UEngine::UEngine` (defined in `GameEngine.cpp:878`) sets `bUseFixedFrameRate = false; FixedFrameRate = 30.f;` (:910-911).
- The properties are declared in `Source/Runtime/Engine/Classes/Engine/Engine.h:1669-1682`.

**VSync:**
- `r.VSync` default is 0 (`Source/Runtime/Core/Private/HAL/ConsoleManager.cpp:4329-4334`).
- The game viewport passes `bLockToVsync = CVar->GetValueOnGameThread() != 0`, plus `|= bCinematicMode` (`Source/Runtime/Engine/Private/UnrealClient.cpp:1822-1828`). **Cinematic mode forces VSync.**
- D3D12 uses `int32 SyncInterval = PresentArgs.bLockToVsync ? RHIGetSyncInterval() : 0;` (`Source/Runtime/D3D12RHI/Private/D3D12Viewport.cpp:647`). In windowed mode with tearing support it adds `DXGI_PRESENT_ALLOW_TEARING` when the interval is 0 (`D3D12RHI/Private/Windows/WindowsD3D12Viewport.cpp:381-384`).
- `rhi.SyncInterval` default is 1 (`Source/Runtime/RHI/Private/RHIUtilities.cpp:91-102`). It only matters when VSync is on.

**`UGameUserSettings` can override at startup:**
- `UGameEngine` init calls `LoadSettings(); ApplyNonResolutionSettings();` (`GameEngine.cpp:1249-1250`).
- That sets `r.VSync` from `bUseVSync`, unless `[SystemSettings] r.Vsync` exists in Engine ini (`Source/Runtime/Engine/Private/GameUserSettings.cpp:521-530`).
- It also calls `SetFrameRateLimitCVar(FrameRateLimit)`, which becomes `GEngine->SetMaxFPS` and so `t.MaxFPS` (:533-537, :413-416). The default `FrameRateLimit = 0.0f` (:297).
- A stale `Saved/Config/Windows/GameUserSettings.ini` can therefore cap or VSync a benchmark run. **[inferred]**

**How they interact** **[inferred from the code above]**:
- Fixed step (`-BENCHMARK`/`-UseFixedTimeStep`/`-deterministic`) wins over everything in this function.
- Otherwise `bUseFixedFrameRate` replaces `GetMaxTickRate()`, so `t.MaxFPS` and smoothing are ignored.
- Otherwise `t.MaxFPS > 0` replaces the smoothing value.
- Otherwise the smoothing range applies, if `bSmoothFrameRate` is set.
- `r.VSync` is independent and blocks at Present on the RHI side.

### B1.3 Recipes

**(a) Uncapped real-time run** (no cap, no VSync, no smoothing):
```
MyGame.exe <Map> -novsync -ExecCmds="t.MaxFPS 0,r.VSync 0" -windowed -ResX=1920 -ResY=1080 -nosound -traceautostart=0 <csv args, see B2>
```
Plus config:
- `DefaultEngine.ini [/Script/Engine.Engine]`: `bSmoothFrameRate=False`, `bUseFixedFrameRate=False`. Both are already the defaults; keep them explicit.
- `DefaultEngine.ini [SystemSettings]`: `r.VSync=0`. This stops GameUserSettings from setting VSync (`GameUserSettings.cpp:521-524`).
- Delete or reset `Saved/Config/Windows/GameUserSettings.ini` (`FrameRateLimit`).
- Do **not** pass `-BENCHMARK`, `-UseFixedTimeStep` or `-deterministic`.
- `-traceautostart=0`: see the surprise in B4.

Verify with the CSV metadata: `VSyncEnabled` (`CsvProfiler.cpp:4083-4087`) and `TargetFramerate` (:4078).

**(b) Fixed-step deterministic run** (DeltaTime = 1/60 every frame):
```
MyGame.exe <Map> -deterministic -FPS=60 -novsync -ExecCmds="r.VSync 0" -nosound <csv args>
```
- `-deterministic` turns on fixed step and a fixed seed. `-FPS=60` sets `FixedDeltaTime = 1/60`.
- Alternative: `-BENCHMARK -FPS=60`. That also adds audio off, focus forced, ini detach and the `-BENCHMARKSECONDS=` exit.
- VSync must be off, or real time is capped at the refresh rate (the simulation stays fixed).
- The engine does not sleep in this mode. The CSV `FrameTime` and `GameThreadTime` still measure **real** wall time (they use `FPlatformTime::Seconds()`/`Cycles()`, see B2/B5), so frame-cost measurement stays valid while simulation work is identical per frame. **[inferred]**

---

## B2. CSV profiler

### Compile gates
`Source/Runtime/Core/Public/ProfilingDebugging/CsvProfilerConfig.h`:
- :9-11 `#define CSV_PROFILER_ENABLE_IN_SHIPPING 0`
- :15-17 `#define CSV_PROFILER_ALLOW_DEBUG_FEATURES (!UE_BUILD_SHIPPING)`. This gates all the `-csv*` switches.
- :30-32 `#define CSV_PROFILER (WITH_ENGINE && (!UE_BUILD_SHIPPING || CSV_PROFILER_ENABLE_IN_SHIPPING))`
- :24-26 `CSV_PROFILER_MINIMAL 0`. :35 `CSV_PROFILER_STATS (CSV_PROFILER && !CSV_PROFILER_MINIMAL)`.

**Result: compiled in Development and Test. Not in Shipping** unless the Target.cs defines `CSV_PROFILER_ENABLE_IN_SHIPPING=1`. Even then the command-line switches stay off unless `CSV_PROFILER_ALLOW_DEBUG_FEATURES` is set too.

### Command-line arguments (`Source/Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp`)
| Arg | Line | Meaning |
|---|---|---|
| `-csvCaptureFrames=N` | 5049 (PreInit), 5177-5185 | Starts a **boot capture** of N frames at init: `BeginCapture(NumCsvFrames, FString(), FString(), ECsvProfilerFlags::BootCapture);`. Includes loading frames, and boot captures use the legacy frame time (:4163, :4371-4372). |
| `-csvRepeat=N` | 5186 | Repeat count for a capture of N frames (handled at :4293-4305). |
| `-csvCategories=a,b` | 5132-5139 | Enables categories by name (`EnableCategoryByString`). |
| `-csvAllCategoriesDisabled` | 5121-5129 | All categories start disabled (memzero). |
| `-csvMetadata=k=v,k2=v2` | 5143-5156 | Adds metadata pairs (`SetMetadata(*Key, *Value)`). |
| `-csvGpuStats` | 5098-5104 | Sets `r.GPUCsvStatsEnabled 1` (per-pass GPU stats, category `GPU`, `Source/Runtime/RHI/Private/GPUProfiler.cpp:118-121, 1059-1060`). |
| `-csvStartOnEvent=Name` | 5081 | Starts a capture when a `CSV_EVENT` with that exact text fires (:4769-4772; text compared case-insensitively; category not required). |
| `-csvStopOnEvent=Name` | 5090 | Ends the capture on that event (:4760-4763). |
| `-csvCaptureOnEventFrameCount=N` | 5078 | Frame count used by start-on-event (`GetNumFrameToCaptureOnEvent`, :5347-5350; default -1, meaning unlimited). |
| `-csvExecCmds=F:cmd,rN:cmd` | 5204-5247 | Runs console commands at capture frame F, or every N frames with the `r` prefix. Only runs while capturing (:1008-1011). |
| `-csvEventExecCmds=Event::cmd` | 5249-5286 | Runs commands when an event fires. |
| `-csvNoProcessingThread` | 5158 (and 4320) | Processes on the game thread. |
| `-csvNamedEvents` / `-csvNamedEventsTiming` | 5163-5170 | Emits named events for exclusive and timing stats. |
| `-csvStatCounts` | 5172-5175 | Sets `csv.statCounts 1`. |
| `-csvCompression=0\|1` | 5189-5198 | Sets `csv.CompressionMode`; 1 writes `.csv.gz` (:3970). |
| `-csvTest`, `-csvTestMT`, `-csvTestCategoryOnly`, `-csvTestTasks` | 5106-5119, 5465 | Self-tests. |
| `-csvABTest=`, `-csvABTestStatFrameOffset=`, `-csvABTestSwitchDuration=`, `-csvABTestFastCVarSet` | 669-693 | A/B cvar toggling. |
| `-csvContinuousWritesDisabled` | 541 | |
| `-csvProfilerHighPriority` | 3275 | Processing thread priority. |
| `-csvBench` | 4111 | Runs the CSV overhead micro-benchmark before capture. |
| `-ExitAfterCsvProfiling` | 4309 | **Quits when the capture ends** (`RequestExit`). |
| `-csv.TargetFrameRateOverride` | 4051 | Overrides the `TargetFramerate` metadata. |

**`-csvExitOnCompletion` does not exist.** The real options are `-ExitAfterCsvProfiling` or the console `csvprofile exitoncompletion` (:1070-1073).

### Console commands
- `CsvProfile` (:1117-1121), handled at :1050-1087:
  - `csvprofile start` begins an unlimited capture.
  - `csvprofile stop` ends it.
  - `csvprofile frames=N` captures N frames.
  - `csvprofile repeat=N`.
  - `csvprofile startfile=<name>` sets the filename for the next start.
  - `csvprofile exitoncompletion`.
  - Comparisons use `FString ==`, which is case-insensitive.
- `CsvCategory <cat> [enable|disable]`, toggles if the second argument is omitted (:1123-1171).
- `csv.DeleteDefaultDirectory` (:1185-1189).
- Useful cvars: `csv.UseLegacyFrameTime` (:150-156), `csv.TargetFrameRateOverride` (:143-148), `csv.CompressionMode` (:185), `csv.statCounts` (:195), `csv.ForceExit` (:118), `csv.BlockOnCaptureEnd` (:94), `csv.trackWaitsGT/RT/AllThreads` (:331-333).
- Ini: `[CsvProfiler] EnabledCategories / DisabledCategories / bEventTimestamps` in Engine ini (:918-919, :1112).
- C++ API: `FCsvProfiler::Get()->BeginCapture(int InNumFramesToCapture=-1, const FString& InDestinationFolder, const FString& InFilename, ECsvProfilerFlags)` and `TSharedFuture<FString> EndCapture(FGraphEventRef)` (`Source/Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h:519-529`).

### Output
- Folder: `FPaths::ProfilingDir() + TEXT("CSV/")` (:4739-4741). `ProfilingDir()` is `ProjectSavedDir() + "Profiling/"` (`Source/Runtime/Core/Private/Misc/Paths.cpp:548-551`). For a packaged game that is `<Archive>/Windows/<Project>/Saved/Profiling/CSV/`. **[inferred]**
- Filename: `FString::Printf(TEXT("Profile(%s)"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")))`, plus `.csv` or `.csv.gz` (:3970-3976).
- Layout: a header row, one row per frame, then a final `EVENTS,<stat names...>` header row (:3145-3151, `HasHeaderRowAtEnd=1`), then a metadata row of `[Key],Value` pairs (:1946-1950) with `Commandline` last (:3156-3172). A custom parser must drop the last two rows.
- Column naming:
  - Categorised stats get a `<Category>/` prefix (:2453-2457).
  - Timing stats (non-custom) get a `<ThreadName>/` prefix (:2446-2450).
  - Global custom stats have no prefix.
- Metadata written at start includes `CsvID`, `TargetFramerate`, `StartTimestamp`, `NamedEvents`, `HasGtStalls`, `VSyncEnabled` and `ProgramSizeMB` (:4039, :4078-4092).

### Default per-frame stats (exact 5.8.4 column names)
| Column | Producer | Source of value |
|---|---|---|
| `FrameTime` | `CsvProfiler.cpp:4388` `CSV_CUSTOM_STAT_MINIMAL_GLOBAL(FrameTime, FrameTimeMs, ECsvCustomStatOp::Set);` in `UpdateFrameTime()` | `FPlatformTime::Seconds()` delta measured right after `UpdateTimeAndHandleMaxTickRate` (called at `LaunchEngineLoop.cpp:5701-5704`). Includes any frame-cap wait. The legacy path (EndFrame-to-EndFrame) is at :4163-4169 and is used for boot captures or when `csv.UseLegacyFrameTime=1`. |
| `GameThreadTime` | `LaunchEngineLoop.cpp:1519` | `GGameThreadTime` (cycles, excluding idle and waits) |
| `RenderThreadTime` | `LaunchEngineLoop.cpp:1518` | `GRenderThreadTime` |
| `GPUTime` | `LaunchEngineLoop.cpp:1520` | `RHIGetGPUFrameCycles()` |
| `GameThreadTime_CriticalPath`, `RenderThreadTime_CriticalPath` | `LaunchEngineLoop.cpp:1521-1522` | critical-path variants |
| `RHIThreadTime` | `LaunchEngineLoop.cpp:1523-1526` | only `if (IsRunningRHIInSeparateThread())` |
| `InputLatencyTime` | `LaunchEngineLoop.cpp:1527-1530` | only if > 0 |
| `MemoryFreeMB`, `PhysicalUsedMB`, `VirtualUsedMB`, `ExtendedUsedMB`, `SystemMaxMB` | `CsvProfiler.cpp:4188-4192` (also `LaunchEngineLoop.cpp:1537`) | platform memory stats |
| `MaxFrameTime` | `LaunchEngineLoop.cpp:1546` | `1000/t.MaxFPS` (30 if uncapped) |
| `CPUUsage_Process`, `CPUUsage_Idle` | `LaunchEngineLoop.cpp:1468-1469` (non-Shipping) | |
| `GPUUsage/Clock`, `/Usage`, `/Memory`, `/ExternalUsage`, `/ExternalMemory` | `LaunchEngineLoop.cpp:1552-1556` | when `GRHISupportsGPUUsage` |
| `RenderThreadIdle/Total`, `/CriticalPath`, `/SwapBuffer`, `/NonCriticalPath`, `/GPUQuery` | `Source/Runtime/SlateRHIRenderer/Private/SlateRHIRenderer.cpp:1345-1349` | |

The thread-time stats are written in `UpdateCoreCsvStats_EndFrame()` (`LaunchEngineLoop.cpp:1513`). It is called at :6144 inside `#if WITH_ENGINE && CSV_PROFILER`.

### `ECsvCustomStatOp` (`CsvProfiler.h:219-225`)
`Set, Min, Max, Accumulate`.

### Macros (`CsvProfiler.h`)
| Macro | Line | Expands to | Needs category enabled? |
|---|---|---|---|
| `CSV_DEFINE_CATEGORY(Name, bDefault)` | 50 | `FCsvCategory _GCsvCategory_##CategoryName(TEXT(#CategoryName),bDefaultValue)` | n/a. `bDefault` is the initial enabled state; ini or `-csvCategories` can override (:889-911, :964). |
| `CSV_DEFINE_CATEGORY_MODULE(API, Name, bDefault)` | 51 | Exported variant | n/a |
| `CSV_DECLARE_CATEGORY_EXTERN(Name)` | 52 | `extern FCsvCategory _GCsvCategory_##CategoryName` | n/a |
| `CSV_SCOPED_TIMING_STAT(Cat, Stat)` | 120-122 | `FScopedCsvStat` plus a trace inline stat | **Yes.** `BeginStat`/`EndStat` check `GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex]` (CsvProfiler.cpp:4554, :4603). |
| `CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Stat)` | 129-131 | `FScopedCsvStatExclusive`. Exclusive time, no category argument. | Uses the built-in `Exclusive` category (`CsvProfiler.cpp:72`, enabled by default; checked at :4620). Output column `Exclusive/<Thread>/<Stat>`. |
| `CSV_CUSTOM_STAT(Cat, Stat, Value, Op)` | 155-160 | `FCsvProfiler::RecordCustomStat(#StatName, CSV_CATEGORY_INDEX(Category), Value, Op)` | **Yes** (:4965, :4986). |
| `CSV_CUSTOM_STAT_GLOBAL(Stat, Value, Op)` | 161-166 | category index 0 (`GLOBAL`, always enabled, :69) | Global only |
| `CSV_EVENT(Cat, Fmt, ...)` | 56-58 | `RecordEventf(...)` **and** `TRACE_BOOKMARK(...)` | **Yes** to record (:4746). The start-on-event trigger ignores the category (:4747, :4766-4772). |
| `CSV_METADATA(Key, Value)` | 85 | `FCsvProfiler::SetMetadata(Key, Value)` | **No.** No category, and it is persistent (:4862-4865). |
| `CSV_CUSTOM_STAT_MINIMAL[_GLOBAL]` | 90-91 | still emitted when `CSV_PROFILER_MINIMAL=1` | |

All of these compile to nothing when `CSV_PROFILER` is 0 (Shipping) (:93-112, :173-191). The timing and custom-stat macros also compile out when `CSV_PROFILER_MINIMAL` is set (:115).

---

## B3. CSV tools (`Binaries/DotNET/CsvTools/`)

Executables:
- `CSVCollate.exe`
- `CSVFilter.exe`
- `CSVSplit.exe`
- `CSVToSVG.exe`
- `CsvConvert.exe`
- `PerfreportTool.exe`
- `RegressionsReport.exe`
- `csvinfo.exe`

**There is no `CSVStats.exe`.** `CsvStats.dll` is the shared library. Source is not shipped (`Source/Programs/CSVTools/` contains only `CSVTools.tps`).

**Runtime gotcha:** the tools target .NET **10.0**. The system has 6/8/9 only, so they fail with "You must install or update .NET". They run with the engine's bundled runtime:
`DOTNET_ROOT="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64"` (contains `shared/Microsoft.NETCore.App/10.0.7`).

Help output, captured with the bundled runtime:
- **`csvinfo` 1.07**: `<csv> [-showaverages] [-showmin] [-showmax] [-showTotals] [-showAllStats] [-showEvents] [-statFilters a,b] [-toJson file]`. Tested on a synthetic CSV:
  - `-showaverages -showmin -showmax` printed `FrameTime (25)  (Min: 10)  (Max: 40)`.
  - `-showAllStats -statFilters FrameTime` printed `FrameTime (All: [10,20,30,40])`.
  - So it gives **avg, min and max, plus every raw value**. No percentiles.
- **`PerfreportTool` v4.274.1**: `-csv <file> -o <dir> [-reportType Default60fps|Default30fps|...] [-readStatMinMax] [-beginEvent/-endEvent] [-minx/-maxx] [-summaryTableOutputFormats html|csv|json] [-writeSummaryCsv] [-collateAverageMethod mean|median] ...`
  - Summary classes found in the DLL: `FPSChartSummary`, `HitchSummary`, `HistogramSummary`, `PeakSummary`, `BoundedStatValuesSummary`, `BucketSummary`, `EventSummary`, `StatBudgetsSummary`, `TimingRegionSummary`, among others.
  - BoundedStatValues formulas (DLL strings): `average, unweighted_average, maximum, minimum, percentoverthreshold, percentunderthreshold, sumwhenotheroverthreshold, sumwhenotherunderthreshold, streamingstressmetric, ratio`. **No percentile formula.**
  - `ReportTypes.xml` ships `Default60fps`/`Default30fps` (auto-detected by `targetframerate` metadata), using `fpschart` and `hitches` summaries over `Frametime, GameThreadtime, RenderThreadtime, RHIThreadTime, GPUtime`.
- **`CSVToSVG` 3.70**: graphs, with `-percentile`, `-percentile90` and `-percentile99`. Internally these are `percentile`, `percentileTop90`, `percentileTop99` and `DrawPercentileGraph`. They render a **percentile curve in the SVG**, not a printed number. **[inferred]**
- `CSVCollate` (`-avg`, `-startEvent`, outlier filter), `CSVFilter` (`-stats`), `CSVSplit` (`-splitStat`, `-delay`), `CsvConvert` (csv/bin), and `RegressionsReport` (needs a thresholds JSON).

**Can they compute p95/p99 as numbers? No.**
- Per-stat average, min and max: `csvinfo -showaverages -showmin -showmax`, or PerfReportTool with `-readStatMinMax`.
- Percentiles: export raw per-frame values (`csvinfo <csv> -showAllStats -statFilters FrameTime,GameThreadTime,GPUTime -toJson out.json`), or read the CSV directly and dropping the last two rows, then compute median/p95/p99 in our own script.
- CSVToSVG's percentile graph is useful as a visual check.

---

## B4. Unreal Insights / Trace

**Compile gates:**
- `Source/Runtime/TraceLog/Public/Trace/Config.h:12-18`: `UE_TRACE_ENABLED 1` when `!UE_BUILD_SHIPPING && !IS_PROGRAM` on Windows/Unix/Apple/Android. Shipping is 0. Full tracing in Shipping is `static_assert`ed off (:65-68).
- `CPUPROFILERTRACE_ENABLED` = `UE_TRACE_ENABLED && !UE_BUILD_SHIPPING` (`Source/Runtime/Core/Public/ProfilingDebugging/CpuProfilerTrace.h:19-25`).
- `MISCTRACE_ENABLED` is the same (`MiscTrace.h:13-19`).
- **Result: Development and Test include tracing. Shipping does not.**
- Command-line args, console commands and autostart all follow `UE_TRACE_ENABLED` (`Source/Runtime/Core/Private/ProfilingDebugging/TraceAuxiliary.cpp:17-33`).

**Channels.** The name is the variable name with the `Channel` suffix removed, matched case-insensitively (`Source/Runtime/TraceLog/Private/Trace/Channel.cpp:73-81`, :64).
| `-trace=` name | Definition | Meaning |
|---|---|---|
| `cpu` | `Core/Private/ProfilingDebugging/CpuProfilerTrace.cpp:24` | "CPU profiler scoping events and timing data. Captures hierarchical timing information for CPU-bound operations on all threads." |
| `frame` | `Core/Private/ProfilingDebugging/MiscTrace.cpp:9` (`UE_TRACE_MINIMAL_CHANNEL`) | "Traces the frame durations for different frame types." |
| `bookmark` | `MiscTrace.cpp:10` | "Low frequency markers signifying important transitions..." |
| `stats` | `Core/Private/Stats/StatsTrace.cpp:14` | "Named stat declarations and batched stat operations ... declared with DECLARE_STATS_GROUP." Needs `STATS`, so effectively Development only. **[inferred]** |
| `counters` | `Core/Private/ProfilingDebugging/CountersTrace.cpp:9-11` | Counters API values over time. "It also enables the CSV Profiler Trace." |
| `default` | preset `TraceAuxiliary.cpp:138` | `TEXT("cpu,gpu,frame,log,bookmark,screenshot,region")` |

Other presets: `Memory` = `memtag,memalloc,callstack,module`; `Memory_Light` = `memtag,memalloc` (:139-140). Config presets can be added under `[Trace.ChannelPresets]` (:405, :2698).

**Destination switches** (`TraceAuxiliary.cpp:1946-2024`):
- `-trace=<list>` (:1952). Bare `-trace` uses the default preset (:1955-1957).
- `-tracehost=<ip>` streams to a trace server (:1977-1981). `-tracehost` alone means localhost (:1982-1986).
- `-tracefile=<path>` writes a file (:1987-1999). Bare `-tracefile` uses the default name (:2000-2004).
- A destination with no channels uses `default` (:2007-2010).
- Relative paths go to `ProfilingDir()`. The default name is `%Y%m%d_%H%M%S_<ticks>.utrace`, and `.utrace` is forced (:995-1039).
- `-tracefiletrunc` and `-tracefiletimestamps` (:1971-1972). `-traceautostart=0|1` (:2378).
- Console: `Trace.File [Path] [Channels]`, `Trace.Send`, `Trace.Stop`, `Trace.Pause`, `Trace.Resume`, `Trace.Status`, `Trace.Enable`, `Trace.Disable`, `Trace.Bookmark [Name]`, `Trace.RegionBegin`, `Trace.RegionEnd`, `Trace.SnapshotFile`, `Trace.SnapshotSend` (:1827-1935). `Trace.Start` is deprecated in favour of `Trace.File` (:1837-1838).

**`-statnamedevents`** (`LaunchEngineLoop.cpp:1759-1762`): `++GCycleStatsShouldEmitNamedEvents;`. Stat scopes (`SCOPE_CYCLE_COUNTER`) then emit CPU trace events: `... Condition && GCycleStatsShouldEmitNamedEvents` (`Core/Public/Stats/Stats.h:562`, :586). It adds detail **and overhead**. Also available as `-verbosenamedevents` (:1764-1767) and the console `stats.NamedEvents on|off` (`Core/Private/Misc/CoreMisc.cpp:547`).

**Cost of `TRACE_CPUPROFILER_EVENT_SCOPE` with `cpu` off:**
- It expands to a function-static `uint32` spec id plus an `FEventScope` object (`CpuProfilerTrace.h:453-454` → :399-401 → :358-359).
- The constructor is `bEnabled(bInCondition && CpuChannel)` (:200-202, :212-214).
- `CpuChannel` converts to bool through `FChannel::IsEnabled()`, which is `AtomicLoadRelaxed(&Enabled) >= 0` (`TraceLog/Public/Trace/Detail/Channel.inl:16-25`).
- So when the channel is off, the cost is **one relaxed atomic load plus a branch** at scope entry and a branch on `bEnabled` at exit (:236-241). There is no spec registration and no event output. **[inferred]**
- In Shipping the macro is empty (:532).

**`TRACE_BOOKMARK(Fmt, ...)`** (`Core/Public/ProfilingDebugging/MiscTrace.h:102-115`):
```cpp
if (UE_TRACE_CHANNELEXPR_IS_ENABLED(BookmarkChannel)) { static bool __BookmarkPoint; if (!...) { ... OutputBookmarkSpec(...) } FMiscTrace::OutputBookmark(CallstackTrace_GetCurrentId(), &..., ##__VA_ARGS__); }
```
The format must be a `const TCHAR` array literal (static_asserts at :108-109). It shows as a timeline marker in Insights. `CSV_EVENT` also emits it (CsvProfiler.h:58). It is empty when `MISCTRACE_ENABLED` is 0 (:180).

**Insights binaries:** `Binaries/Win64/UnrealInsights.exe` (224,696 bytes) and `Binaries/Win64/UnrealTraceServer.exe` are **present**.

**Surprise:** `FTraceAuxiliary::TryAutoConnect()` (`TraceAuxiliary.cpp:2740-2757`, called from `LaunchEngineLoop.cpp:1847`) checks for the named event `Local\UnrealInsightsAutoConnect`. If Unreal Insights is open in browser mode, every Development/Test game **automatically starts tracing the `default` channels to 127.0.0.1**. That would contaminate CSV benchmark runs. Close Insights, or pass `-traceautostart=0`. Note that `-traceautostart=0` also stops `-trace=` from starting at boot (:2017-2021), so use it only for non-trace runs.

---

## B5. In-game frame and thread times

Globals (`Source/Runtime/RenderCore/Public/RenderTimer.h:8-29`, defined in `RenderCore/Private/RenderingThread.cpp:152-158, 1070-1076`). All are `uint32` **cycles**:
- `GGameThreadTime`: "How many cycles the gamethread used (excluding idle time)". Set in `FViewport::Draw` (`Source/Runtime/Engine/Private/UnrealClient.cpp:1789`), at :1838-1844:
  `uint32 ThreadTime = CurrentTime - Lastimestamp; GGameThreadTime = (ThreadTime > GameThread.Waits) ? (ThreadTime - GameThread.Waits) : ThreadTime;`
  Here `CurrentTime = FPlatformTime::Cycles()`, and the Waits come from `UE::Stats::FThreadIdleStats`. The interval runs from Draw to Draw, so it is only updated when the viewport has non-zero size (:1820). **[inferred]**
- `GGameThreadTimeCriticalPath` (:1848-1849), `GGameThreadWaitTime` (:1845).
- `GRenderThreadTime` and `GRenderThreadTimeCriticalPath`: set in `FSlateRHIRenderer::PresentWindow_RenderThread` (`Source/Runtime/SlateRHIRenderer/Private/SlateRHIRenderer.cpp:1286`), at :1359-1364. `r.RenderThreadTimeIncludesDependentWaits` swaps in the critical-path value (:1367-1371).
- `GRHIThreadTime`: an RHI-thread lambda, `AtomicStore((int32*)&GRHIThreadTime, ...)` (SlateRHIRenderer.cpp:1380-1395).
- GPU: `RHIGetGPUFrameCycles(GPUIndex)` returns `GGPUFrameTime` (`Source/Runtime/RHI/Private/GPUProfiler.cpp:2281-2284`).
- Convert with `FPlatformTime::ToMilliseconds(uint32)` (`Core/Public/GenericPlatform/GenericPlatformTime.h:183`).
- Note: GT, RT and GPU values in the same frame row refer to different pipelined frames. GT is the latest Draw interval; RT and GPU lag one or more frames. **[inferred]**

**`FStatUnitData`** (`Source/Runtime/Engine/Public/UnrealClient.h:299-345`):
- Fields: `FrameTime, GameThreadTime, RenderThreadTime, GPUFrameTime[], RHITTime, InputLatencyTime`, plus `Raw*` versions and 200-sample history arrays under `#if !UE_BUILD_SHIPPING`.
- Values are filled **only inside `FStatUnitData::DrawStat`** (`UnrealClient.cpp:364-439`): `RawGameThreadTime = FPlatformTime::ToMilliseconds(GGameThreadTime); GameThreadTime = 0.9 * GameThreadTime + 0.1 * RawGameThreadTime;` (:385-386).
- So the non-raw fields are an **exponential moving average**, and they are only updated while `stat unit` is being drawn.
- In benchmark mode, DrawStat uses real `FPlatformTime::Seconds()` for FrameTime (:364-374).
- Accessor: `UGameViewportClient::GetStatUnitData()` (`Classes/Engine/GameViewportClient.h:716`), allocated at `GameViewportClient.cpp:361`.
- **Recommendation: read the raw globals per frame, not `FStatUnitData`.**

**Availability in Test:** there are no preprocessor guards around the definitions or writers (checked: none in `UnrealClient.cpp:1789-1870`, `RenderingThread.cpp:1040-1080` or `SlateRHIRenderer.cpp:1280-1400`). The globals are `RENDERCORE_API`, so a game module that depends on `RenderCore` and includes `RenderTimer.h` can read them in **Development, Test (and Shipping)**. **[inferred]** `FStatUnitData` exists in Test too (only its history arrays are non-Shipping).

**Windows timers** (`Core/Public/Windows/WindowsPlatformTime.h:23-44`):
- `Seconds()`: `QueryPerformanceCounter(&Cycles); return (double)Cycles.QuadPart * GetSecondsPerCycle() + GetSecondsTimeOffset();`. The offset is `16777216.0` (:46-49).
- `Cycles()`: `(uint32)Cycles.QuadPart`. This is QPC truncated to 32 bits; unsigned deltas stay correct over intervals shorter than the wrap period. **[inferred]**
- `Cycles64()`: the raw `uint64` QPC tick count.
- Frequency: `InitTiming()` uses `QueryPerformanceFrequency`, so `SecondsPerCycle = 1.0 / (double)Frequency.QuadPart;` (`Core/Private/Windows/WindowsPlatformTime.cpp:27-32`).

---

## B6. Console commands (5.8.4)

- **`stat unit`**: registered as `STAT_Unit` (`UnrealEngine.cpp:2632`), unconditionally, so it works in Test. It is drawn by `FStatUnitData::DrawStat`. Rows: `Frame`, `Game`, `Draw`, `RHIT`, `GPU Time` (+ GPU Clock/Usage/Mem), `Input`, `Mem`/`VMem`/`VRam`, `DynRes`. Columns: `Avg`, optional `Critical`/`Max` (UnrealClient.cpp:613-621). Values are the 0.9/0.1 EMA unless `stat raw` is set (:455). Related: `stat unitcriticalpath`, `stat unitmax`, `stat raw` (UnrealEngine.cpp:2643, 2644, 2647, non-Shipping). Row titles are at UnrealClient.cpp:626 `Frame`, :702 `GPU Time`, :802 `Input`, :871 `DynRes`.
- **`stat unitgraph`**: `STAT_UnitGraph`, "Displays a frame time stats graphed over time." (UnrealEngine.cpp:2645, inside `#if !UE_BUILD_SHIPPING`).
- **`stat fps`**: `STAT_FPS` (:2630). It prints `"%5.2f FPS"` of `GAverageFPS` and `"%5.2f ms"` of `GAverageMS` (:19348-19362). Those come from `CalculateFPSTimings()`, an EMA with a 0.1 factor (:805-830).
- **`stat game`**: not an engine stat. It is the stats-system group `STATGROUP_Game` (`Core/Public/Stats/GlobalStats.inl:25`; contains e.g. `STAT_TickTime`, `Engine/Public/EngineStats.h:54`). It is handled by `UE::Stats::DirectStatsCommand` (`Core/Private/Stats/StatsCommand.cpp:2273`), whose work is inside `#if STATS` (:2286). `STATS` is on in Development but `(FORCE_USE_STATS && !ENABLE_STATNAMEDEVENTS)` in Test (`Core/Public/Misc/Build.h:286` vs :314). **So `stat game` works in Development only.** **[inferred]**
- **`dumpticks`**: `UEngine::Exec` handles `DUMPTICKS` (UnrealEngine.cpp:5731-5733) under `UE_ALLOW_EXEC_COMMANDS`, which is 1 except Shipping (`Core/Public/Misc/Exec.h:11-17`). Args (:6148-6168): `GROUPED`, `ENABLED`, `DISABLED`.
  - Default output: one line per tick function, `"%s, %s, ActualStartTickGroup: %s, Prerequesities: %d"` (`Engine/Private/TickTaskManager.cpp:1703`), then `"Total registered tick functions: %d, enabled: %d, disabled: %d."` (:2317).
  - `GROUPED` prints `"<context>, <count>"` sorted by count, then `"Total enabled tick functions: %d."` (:2296-2302).
  - Also registered as a console command for help text (`TickTaskManager.cpp:2225`).
- **`t.MaxFPS`**: confirmed (`UnrealEngine.cpp:12136-12138`).
- 5.8 note: `stat startfile`/`stopfile` are now behind `UE_ENABLE_STATS_FILE_DEPRECATED_IN_5_8`, default 0 (`Core/Public/Stats/StatsFile.h:8-10`; `StatsCommand.cpp:2295`). The legacy `.ue4stats` capture path is effectively gone; use Insights. **[inferred]**

---

## B7. Packaging from the launcher build

- `Build/BatchFiles/RunUAT.bat` exists. Because `Build/InstalledBuild.txt` exists, it runs the **precompiled** `Binaries/DotNET/AutomationTool/AutomationTool.dll` (RunUAT.bat:62-63, :19).
- AutomationTool source is present: `Source/Programs/AutomationTool/Scripts/BuildCookRun.Automation.cs` and `AutomationUtils/ProjectParams.cs`.

Flag verification (`ProjectParams.cs` unless noted):
| Flag | Evidence |
|---|---|
| `-project=` | BuildCookRun.Automation.cs:18 `[Help("project=Path", @"Project path (required) ...")]` |
| `-platform=Win64` | :39 `[Help("targetplatform=PlatformName", "... (also -Platform)")]`, :726 |
| `-clientconfig=Development` | :1140 `Command.ParseParamValue("clientconfig")` (falls back to `config`/`configuration`, :1142-1146) |
| `-build` | :735. On an installed engine it is skipped only for Blueprint-only projects (`Scripts/BuildProjectCommand.Automation.cs:68`). It also builds the project's **Editor** target (needed by the cook) unless `-skipbuildeditor`/`-nocompileeditor` (:90; ProjectParams.cs:743). |
| `-cook` | :745 |
| `-stage` | :905 |
| `-pak` | :812 (`-iostore` :814) |
| `-archive` | :939 |
| `-archivedirectory=` | :940, :1744. The platform folder `Windows` is appended unless the path already contains it (`AutomationUtils/DeploymentContext.cs:513-520`). |
| `-target=` | :892 `Command.ParseParamValues("target")`. Required only if the project has more than one Game target and no `DefaultGameTarget` in `[/Script/BuildSettings.BuildSettings]` (:2595; "Specify which one to use with the -Target= option", :2571). |
| `-unattended` | :1053 |
| `-NoP4`, `-UTF8Output` | `AutomationUtils/Automation.cs:465-470` (global) |
| `-prereqs`, `-nodebuginfo`, `-distribution`, `-compressed` | :945, :964, :943, :856 |

Do **not** pass `-client`. In BuildCookRun it means a Client-type target in a client/server setup (:20, :2318). A standalone game uses the Game target with `-clientconfig`.

**Command:**
```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="<repo>\UpdateBudget\UpdateBudget.uproject" ^
  -noP4 -utf8output -unattended ^
  -platform=Win64 -clientconfig=Development ^
  -build -cook -stage -pak -archive ^
  -archivedirectory="<repo>\Build"
```
- Add `-target=UpdateBudget` only if several Game targets exist.
- Use `-clientconfig=Test` for a Test build. CSV and Trace still work there, but `stat game`/STATS do not.
- Output lands in `...\Build\Windows\`. **[inferred]**

---

## Surprises and gotchas (summary)
1. `-csvExitOnCompletion` doesn't exist. Use `-ExitAfterCsvProfiling` or `csvprofile exitoncompletion`.
2. `-csvCaptureFrames=` is a **boot** capture: it includes loading and uses legacy frame time. For steady-state benchmarks:
   - Fire `CSV_EVENT(Cat, TEXT("BenchStart"))` from the harness after warm-up.
   - Launch with `-csvStartOnEvent=BenchStart -csvCaptureOnEventFrameCount=N -ExitAfterCsvProfiling`.
   - Or call `FCsvProfiler::Get()->BeginCapture(N)` directly.
3. `-BENCHMARK` alone gives a 30 Hz fixed step (`FixedDeltaTime = 1/30`). It also mutes audio and fixes the seed.
4. In fixed-step mode the engine never sleeps, and `t.MaxFPS`/smoothing/`bUseFixedFrameRate` are bypassed. Only VSync can throttle.
5. `UGameUserSettings` (`Saved/Config/.../GameUserSettings.ini`) can silently set `r.VSync` and `t.MaxFPS` at startup.
6. An open Unreal Insights auto-attaches tracing to every Development/Test run.
7. The CSV tools need .NET 10 and the system lacks it. Use the engine-bundled runtime through `DOTNET_ROOT`. There is no `CSVStats.exe`, and no tool prints p95/p99.
8. `FStatUnitData` values are EMAs, and only update while `stat unit` is visible.
9. `stat game` (STATS) is Development-only. `stat startfile` is deprecated in 5.8.
10. `bCinematicMode` forces VSync on the game viewport.
