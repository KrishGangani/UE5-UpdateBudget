# 07 · Common Mistakes: UpdateBudget

**Rule:** for every module, the mistakes that are easy to make here, **how to spot them, how to prevent them and how to debug them**. Mistakes this project **actually made or caught** are marked ⚠, with who or what caught them (a code review, a simplify audit, a test run, a benchmark batch, another project of the portfolio, or the compiler): those are real interview stories.

Since Phase 6 the file is organised by module, as 05 is; each row names the step it comes from where that matters. Section references (05 P5, 06 §29) point to the line-by-line explanation and the API notes.

---

## Component and registry (`BudgetedUpdateComponent`, the subsystem's registry)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught: **two clocks.** Native ticks forward the engine's `DeltaTime` (frame delta or interval delta, dilated per actor), while staleness uses world time | Behaviour differs by mode for the same world time; SC6 fails "randomly" | One owner for time: `RunUpdate()` measures `Now − LastUpdateTime` from `UWorld::GetTimeSeconds()` for every caller | Log `DeltaSeconds` and `Now − Last` side by side for one agent in two modes |
| ⚠ caught: **not counting the first update** (an update was only counted when a gap existed) | `UpdatesRun = 0` on the spawn frame; waves undercounted | Count every update. Only the *gap* is conditional. Pass facts (`PreviousUpdateTime`), not sentinel encodings | The CSV column on the first capture frames must equal N |
| ⚠ caught: **null entry after swap-remove.** GC nulls a `UPROPERTY` pointer to a collected component; the swap moves that null into a live slot, then it is dereferenced | An access violation in `Unregister`, often only in long sessions | One `RemoveClientAt` helper that null-checks the moved entry. Every access tolerates null | Run `obj gc` with agents being destroyed; Insights or a debugger on `RemoveClientAt` |
| ⚠ caught: **no cleanup on `Deinitialize`.** Components keep a strong reference to a dead subsystem | Stale-pointer crashes on world teardown or seamless travel | `Deinitialize` detaches every remaining client, then calls `Super` | Break on `Deinitialize` with agents alive; check the clients' `Subsystem` is null afterwards |
| Relying on GC to "clean up" registry entries | Entries point at garbage until the next GC run (`01b #8`) | Unregister explicitly in `EndPlay`; check `IsValid` where used | `IsValid()` false while the pointer is non-null |
| Forgetting `bCanEverTick = true` | The native baseline never updates (`UpdatesRun = 0`) | Set it in the constructor (default false) | `dumpticks` lists no component tick |
| Leaving the default `TG_DuringPhysics` | Baselines and scheduler run at different frame positions, an unfair comparison | Pin `TG_PrePhysics` explicitly | `dumpticks grouped` |
| ⚠ found in the source: **adding to an array while a loop reads it through a raw pointer** (a component spawned inside an update registers at once) | A crash or garbage deadlines only on frames where something spawns, and only when the array happens to reallocate | Defer: while the scheduler runs, adds wait in `PendingAdds` and removals null their slot; both are applied after the loop (02 §8) | Test `SpawnDuringUpdate`; a breakpoint on `Register` with `bRunningScheduler` true |
| ⚠ found in the source: **leaving the registry only in `EndPlay`** | In budgeted mode, agents of a level being streamed out keep moving after their level has unregistered them, while natively they stop. Only visible with streaming | Leave in `OnUnregister` too: streaming removal unregisters components first and routes `EndPlay` later, possibly frames later (`World.cpp:4196–4251`) | Test `LevelUnloadOrder`: `UnregisterAllComponents`, tick, then `RouteEndPlay(RemovedFromWorld)` |
| ⚠ found in the source: **disabling the native tick but not `bStartWithTickEnabled`** | Re-registering a component (`ReregisterComponent`, re-attach) makes it tick natively **and** be scheduled: `UpdatesRun` is above N | The mode sets both; `OnRegister` re-joins a re-registered component (`ActorComponent.cpp:1747`, `Actor.cpp:6443–6451`) | Test `NoDoubleUpdate`; count updates per client per frame |
| ⚠ found in the source: **`Activate()` re-enables the tick** | The same double update after any `Activate()` call, for example from gameplay code or a Blueprint | A guard at the top of `TickComponent` switches the native tick off again without updating (`ActorComponent.cpp:2819`) | Test `NoDoubleUpdate`; `IsComponentTickEnabled()` after the call |
| Ending a world "mid-frame" and guarding for it | Code complexity for a case that can't happen | Know the engine: PIE end and travel are queued and run between frames (02 §8); test the real worst case instead (`WorldTeardown`) | — |
| ⚠ caught (review, then the altitude review): **a mode switch that overwrites gameplay's choices** | A component gameplay had paused updates natively again after a switch there and back | Record gameplay's choice where every path goes through (`SetComponentTickEnabled`); have the component's own mode logic call `Super::` | Pause a client, switch there and back in a test (`LiveSwitchKeepsTickState`), and check that it is still paused |

---

## The scheduling policy (`UpdateBudgetScheduler`)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught (review): **a NaN entering a moving average** | After one bad measurement, every frame runs every client and is never "over budget" (every comparison with NaN is false) | Treat non-finite measurements as 0 (`FMath::IsFinite`), and test it with `quiet_NaN()` | Log the prediction once per frame: once it is NaN, it stays NaN |
| Sorting with a non-stable sort and no tie-break | Two clients with equal deadlines swap order between runs; results differ between identical runs | Break ties by index in the predicate | Run the same input twice and compare the run order: a difference means an unbroken tie |
| Treating the budget as a cap | `UpdateMs` sits at the budget on every frame even when few updates were due | The design is work-conserving: after the mandatory clients it keeps running clients in deadline order while the budget lasts (02 §2). A smaller budget is the way to spend less (02 §12) | Plot `UpdateMs` against `MandatoryMs`: the gap filled by optional work is by design |

---

## Significance and settings (`UpdateBudgetSignificance`, `UpdateBudgetSettings`)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught: **a view cone of half the horizontal FOV** | On a wide screen, agents in the corners of the picture count as out of view, so they get half significance and visibly update less often | Use the cone that circumscribes the view rectangle (half-diagonal angle from FOV and aspect ratio); test a point near a corner (`Significance.ViewCone`) | Test a point near a screen corner (`ViewCone`, 05 T5); when in doubt, draw the cone with debug lines |
| ⚠ caught: **a client at the camera counts as out of view** | A first-person pawn's own logic updates a few times a second: its position relative to the camera is zero or perpendicular, so "in front" fails | Everything within a small radius of the camera is in view (`AlwaysInViewRadius`) | Log the significance of a client at the camera: in view, it must be 1 |
| Expecting `+Categories=` lines in `DefaultGame.ini` to add to the C++ default category | The default category disappears | Any ini entries replace the array (`Obj.cpp:3091–3095`); list every category you want | The subsystem's begin-play log line counts the categories: one fewer than expected |
| Reading this frame's camera during `TG_PrePhysics` | Significance lags a camera cut by one frame | Know that cameras update after all tick groups (`LevelTick.cpp:1828–1847`); the brains see the same one-frame-old view, so the comparison stays fair | Cut the camera and log the first frame's significance: it reflects the old view |

---

## The subsystem's frame: telemetry, live switching and the debug view

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Recording CSV stats once **per update** | Instrumentation shows up as cost (about 50 ns × N per frame) | Aggregate in plain fields; publish once per frame | Compare the CSV profiler's startup benchmark line with N |
| ⚠ found in the source: **setting the mode with `-ExecCmds`** | A "UB" run that measures B0: the mode is read at begin play, and `-ExecCmds` run later as deferred commands (`UnrealEngine.cpp:2552, 2772–2810`) | `-ini:Engine:[ConsoleVariables]:ub.Enabled=1`, and the subsystem logs the mode once per world | Grep the run log for `LogUpdateBudget: Display: Budgeted mode` |
| ⚠ caught: **a "missed" counter that counts what could never run** | The SC3 share drops when agents are destroyed during the loop, although no live near agent was skipped | Count from the outcome, after invalid entries are removed (`CountNearClients`) | Destroy clients during an update in a test and check that `NearMissed` stays 0 |
| ⚠ caught by the compiler: **an integer that converts equally well to two overloads** (`AddOnScreenDebugMessage` takes a `uint64` or an `int32` key; `GetUniqueID()` is `uint32`) | Error C2668, ambiguous call to an overloaded function | Cast to the exact parameter type (`static_cast<uint64>`) | The compiler names both candidate overloads; cast to one of them |
| ⚠ caught (review): **switching tick functions inside the change callback** | A cvar set from inside an update enables native ticks mid-frame; clients the scheduler already ran update again in a later tick group | Mark the switch pending; apply it after the tick manager's `EndFrame` (`OnWorldPostActorTick`), and test the in-update case (`LiveSwitch`) | Set the cvar from inside an update in a test (`LiveSwitch`) and count the updates per client per frame |
| ⚠ caught (review): **a capture labelled with the mode it began in** | A capture that spans a switch is filed as native or budgeted | Mark it `Mixed` at the switch, and mark the frame with a CSV event | Read `ubmode` in the capture's metadata, and look for a `ModeSwitch` event |
| ⚠ caught (simplify): **counters copied field by field into a report** | A new statistic needs four edits; a reset that misses one field (it happened: the budget value) | Make the report the accumulator and reset it with one assignment | Check that a frame without `RunFrame` (paused) publishes zeros, not the previous frame's numbers |

---

## The Blueprint hook

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught (review): **making a client `Blueprintable` without thinking about the events it inherits** | A Blueprint client with Event Tick does more work natively than budgeted, so the comparison is unfair; under the scheduler its Event Tick logic silently never runs | Schedule only the dedicated event, and warn once per class and world when a Blueprint implements Event Tick (05 P6e) | Search the log for the Event Tick warning (`LogUpdateBudget`); compare a Blueprint client's updates in a native and a budgeted run |
| ⚠ caught (review): **caching a class fact in `BeginPlay` when it is needed earlier** | An update called before `BeginPlay` (directly, as tests do) skips the Blueprint event | Cache it in `OnRegister`, which a first registration runs before `BeginPlay` | Call `RunUpdate` before `BeginPlay` in a test, and check that the Blueprint ran |
| ⚠ caught (simplify): **a diagnostic hung on an event when the problem is a fact** (the Event Tick warning was first tied to the switch into budgeted mode) | The warning appears in one mode although the difference exists in both, and it needs a per-component flag and a process-wide `static` | Put it where the fact is known, once: registration with the world's subsystem | Look for the warning in both modes: a diagnostic that depends on the mode, for a fact that doesn't, is misplaced |

---

## Game module: workload and director

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Spawning with `SpawnActor`, then setting the patrol | `BeginPlay` or the first tick sees an uninitialised patrol (all agents at the origin) | `SpawnActorDeferred` → `InitPatrol` → `FinishSpawning` | Log the first update's position |
| Using `FMath::Rand` for placement | Hash differs run to run | Seeded `FRandomStream` | The S1 hash check: same seed, same hash |
| An asset referenced only from C++ isn't cooked | Invisible agents in packaged builds; a "Failed to load" warning in the packaged log | `DirectoriesToAlwaysCook` | Grep the packaged log for the asset path |
| A workload the optimiser can delete | Suspiciously flat cost vs `ComputeIterations` | Store results to a member (`ComputeSink`) | Sweep `-UBWork=` and confirm cost scales |
| Caching "where is the player" once per frame for speed | The baseline gets unrealistically cheap, and the result no longer transfers to real perception code | Keep per-agent queries; it is part of the workload's realism | Document the choice (efficiency audit, item 5) |
| ⚠ caught: **harness work inside a measured frame** (the report was written on the BenchEnd frame, which is still captured) | The max and p99 of the last frame grow with N | Do benchmark bookkeeping on frames outside the capture window | Compare the last captured frame with the rest: a spike there that grows with N is harness work |
| ⚠ caught: **the measured camera path depended on warm-up length** | Batches with different warm-ups differ slightly | Time the path from the *planned* BenchStart | Compare the camera's position at `BenchStart` across runs with different warm-ups: it must be the same |

---

## Game module: the baselines B2 and B3

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught: **a destroyed agent stays registered with the Significance Manager** | The manager holds a raw pointer, so the next `Update` calls the significance function on a dead object: a crash some time after a despawn | Bind the agent's `OnEndPlay` and call `UnregisterObject` there; `UnregisterAll(Tag)` in `OnWorldEndPlay` as the safety net | A crash inside the manager's `Update`, with the significance function on a destroyed object in the call stack. Reproduce it by destroying agents during a B3 run |
| ⚠ caught: **unregistering in `Deinitialize`** | `USignificanceManager::Get` already returns null there: the manager is removed in `OnPostWorldCleanup`, so nothing is unregistered | Unregister in `OnWorldEndPlay`, which runs while the manager still exists (06 §17) | Log the result of `USignificanceManager::Get` in `Deinitialize`: it is already null there |
| ⚠ caught: **one-off registration timed inside the per-frame re-tune stat** | `Retune`'s p99 jumps on the wave frame only | Two `CSV_SCOPED_TIMING_STAT` scopes: `Register` and `Retune` | Look at `UBBaseline/GameThread/Retune` on the wave frame: a spike only there is registration work |
| ⚠ caught: **tick order between two subsystems left to chance** | Wave agents are adopted a frame late in some builds and on time in others | `Collection.InitializeDependency<T>()` **before** `Super::Initialize` (06 §19) | Log the frame in which each wave agent is adopted: it must be the spawn frame, in every build |
| ⚠ caught: **dereferencing a brain that no longer exists** | A crash when an agent is destroyed before it's adopted | `IsValid(Agent)` before `GetBrain()`; skip null brains | A crash in `AdoptNewAgents`; the agent list holds an invalid entry at that index |
| ⚠ caught: **two "equivalent" band tests that disagree at an edge** | B2 and B3 put an agent at exactly 15 m in different bands. A significance of 1 − 1e-9 rounds to exactly `1.0f` | One shared band test; clamp the Mid-band significance into (0, 1) with 0.99999994f (the largest float below 1) and `TNumericLimits<float>::Min()` | Test both functions at the edges and 1 cm either side, as `SameBandsAsUpdateBudget` does (05 T7) |
| ⚠ caught (clean-up): **a baseline that does avoidable work** | B3 read every component's interval every frame although the manager hands over the old and new significance | Compare the old and new **bands** first. A baseline must be what a competent team writes, not a strawman | Count the setter calls per frame (`UBBaseline/IntervalChanges`): in a steady scene it should stay near 0 |
| ⚠ caught (altitude): **an unwritten contract between two classes** | Adoption by index works only while the agent list is append-only; nothing said so | Write the contract on `GetAgents()`; `ensureMsgf` where it is relied on | An `ensureMsgf` where the contract is relied on reports the first violation, with a call stack |
| Changing tick intervals in a `Concurrent` post-significance function | Intermittent tick-manager asserts or corruption: worker threads touching game-thread state | `EPostSignificanceType::Sequential` | `check(IsInGameThread())` in the post function: a failure names the caller at once |
| Using `SetComponentTickInterval` and expecting an immediate effect | A far agent that comes near keeps updating slowly for up to its old interval | `SetComponentTickIntervalAndCooldown` (06 §18), only when the band changes | Log an agent's interval and its update times when its band changes: with the plain setter, the first update after the change comes late |
| ⚠ found with the owner (Phase 6): **baselines with their own copies of the system's settings** | A settings change moves UB but not B2/B3, and the comparison silently becomes unfair | Read the same numbers from the same settings, by the same rule (`FUpdateBudgetCategory::Pick`), and test that the bands agree (05 G4, T7) | The baselines' begin-play log line states their bands and intervals; compare it with UB's categories |
| ⚠ caught (Phase 6 code review): **an analysis whose lags assume the default settings** (H4 looked for repetition at 6 and 30 frames, the default 0.1 s and 0.5 s) | With changed intervals, the periodicity test is judged at the wrong lags, and nothing in the capture says the bands weren't the defaults | Let the game write the bands it used into the capture, and the analysis read them, as UB does with `UBMaxStaleness` (05 G4, M4) | Read the `ubinterval…` metadata of a B2/B3 capture and compare it with the lags in the report's H4 header |

---

## Tests

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught (review): **a test that never reaches the case it claims to test** | Work out demand against capacity: at the first budget, optional demand (16.7 per frame) fit the free capacity (30), so the "overload" never happened | Make the precondition real, then **assert it happened** (`FramesWithForcedRuns > 0`) | Count the frames in which the precondition held (`FramesWithForcedRuns`) and assert that it is above 0 |
| ⚠ found in the source: **ticking a test world twice in one frame number** | The second tick runs no tick functions at all, so the test sees no updates and blames the code under test | Advance `GFrameCounter` after each tick, or use the engine's `FTestWorldWrapper`, which does (`AutomationCommon.cpp:232`) | Count `ExecuteTick` calls per `World->Tick` |
| ⚠ found in the source: **a hand-made test world whose actors never begin play** | `OnWorldBeginPlay` runs (so the subsystem looks fine), but no component registers | Use `FTestWorldWrapper::BeginPlayInTestWorld`, which spawns a game mode so `StartPlay` dispatches `BeginPlay` (`World.cpp:6165–6179`) | `HasActorBegunPlay()` on a spawned actor |
| ⚠ found in the source: **reading a destroyed component after a world tick in a test** | A test that passes or crashes depending on when GC runs | Keep counts outside the components (`FUpdateLog`); `UWorld::Tick` may collect garbage at its end (`LevelTick.cpp:1968–1971`) | Run the test with `gc.ForceCollectGarbageEveryFrame 1` |
| Restoring a test cvar before the world that reads it is destroyed | Once the mode switches live (S7), the test world changes mode during teardown | Declare the `FTestConsoleVariable` before the world wrapper, so C++ destroys it after (04 §18) | — |
| ⚠ found by UE5-TestTrust (mutation testing): **boundaries no test sits on** | A comparison can flip between `<` and `<=` (or `>` and `>=`) and every test still passes: 3 of 39 mutants of the scheduler survived for that reason | For every comparison that encodes a rule, add one test **exactly on the boundary**, with inputs that are exact in binary (0.25, 0.5) so rounding can't move it | Run a mutation tool (UE5-TestTrust) on the file: each surviving boundary mutant names a missing test |
| ⚠ caught (review): **a test that leaves a standalone object behind** | Each run in one editor session adds a Blueprint and a generated class that are never collected | Clear `RF_Standalone` in an `ON_SCOPE_EXIT`, so every way out cleans up (05 T6) | Run the test twice in one editor session: with a fixed name, the second run collides |
| ⚠ caught (review): **a check that can never fail** ("the native probe never ran the event": with no script it can't) | Ask which bug would make the check fail; here there is none | Keep a check only if a real mistake could break it | Break the code on purpose and see whether the check fails |
| ⚠ caught (simplify): **two output paths for one line** (`AddInfo` and `UE_LOG`) | The result appears twice in the test report | During a test, a Display log line already becomes a report event (`AutomationTest.cpp:230–254`) | Read the test report: the same line appears twice |
| ⚠ caught (the first S8 test run): **running hundreds of thousands of Blueprint calls in one frame** | `Runaway loop detected (over 1,000,000 iterations)` and "Infinite loop detected" in the log, and one call missing (499,999 of 500,000) | In editor and development builds the VM's step count is reset once per frame; reset it per batch with `GInitRunaway()` in a test, as a frame would (05 T6) | Search the log for `Runaway loop detected`, and compare the call count with the expected one |
| Forgetting to wire a node added with `AddDefaultEventNode` | The event stays a disabled ghost node; `IsFunctionImplementedInScript` answers false | Connect one of its pins: `MakeLinkTo` turns ghost nodes into real ones (06 §38) | After compiling, check `IsFunctionImplementedInScript` (here `HasBlueprintUpdate()`), and the node's enabled state in the graph |

---

## Measurement pipeline and analysis (`Tools\*.py`, the job queue)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Expecting `GameThreadTime` under `-nullrhi` | The column stays 0 (S1 finding) | Headline runs use the real RHI at a fixed small resolution | Check the column before trusting a run |
| Looking for editor `-game` CSVs in the project's `Saved\` | No file where expected | Editor `-game` writes to `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Profiling\CSV`; the log line "Writing CSV to file" gives the path | Search the log for `Writing CSV to file`; the path follows it |
| ⚠ caught: **raw files overwritten by a later batch** (names had only the date) | An old summary's numbers no longer match its raw file | Batch-timestamped names, and refuse to overwrite | Compare each summary's batch name with its raw files' timestamps: an overwritten file is newer than its summary |
| ⚠ caught: **one bad capture aborts the whole batch** | No manifest after a crash; good runs unaccounted for | Catch analysis errors per run; write the manifest after every run | The manifest lists every run with its status; a batch that leaves no manifest after a crash is the bug itself |
| ⚠ caught: **mixing two load levels in one statistic** (a wave run summarised over the whole capture) | The "median" lands between the two loads; p99 is dominated by the spawn spike | Split at the event: pre, event frame, post | Plot the frame times: two plateaus with a spike between them are two load levels |
| ⚠ caught: **first run always cold** | The first configuration in the order is systematically different | A discarded priming run, and an even run count with rotation | Compare each configuration's first run with its later runs: a cold first run is slower |
| ⚠ caught: **a run that never started inherits the previous run's log** | Clean error counts for a run that didn't run | Only accept files modified after this run started | Compare each log's modification time with the run's start time in the manifest |
| ⚠ found while testing: **using Unreal's first header row** | `HasHeaderRowAtEnd=1`: columns added mid-capture exist only in the end header | Use the end header; check the first header is a prefix of it | Count the columns of the first and the last header row (`csv_stats.load_capture` checks that the first is a prefix of the last) |
| Assuming a PSO event means a hitch | Panic over a cost that isn't there | Look at those frames' times. Here they were *below* the median (asynchronous precaching), so they are counted and monitored, not removed | Look up the frame times of the frames with PSO events in the CSV: a hitch shows as a spike there, asynchronous precaching doesn't |
| ⚠ found while testing: **looking for `UBBaseline/Retune` in the CSV** | The column "doesn't exist" | Timing-stat columns include the thread: `UBBaseline/GameThread/Retune`. List the header once before writing analysis code | Print the capture's last header row once and search it for the stat |
| ⚠ found in the S3 batch: **trusting the OS to put the game thread on a fast core** (hybrid CPU: 8 P-cores + 8 E-cores) | Runs sit at **two stable levels about 1.6× apart**; one run **steps** between them mid-capture. Thermal drift looks different: a slow ramp | Pin the benchmark process to the performance cores once (`performance_core_mask()`, `pin_self()`): every game it starts inherits the mask. Record the applied mask, then re-capture. Verified: B0 at N = 4,000 went from 16.5–28.3 ms to 16.2–18.0 ms | Plot each run's median: two clusters about 1.6× apart, or a step inside one run, mean a core change. Record the applied affinity mask with every batch |
| ⚠ caught by UE5-TestTrust: **`cmd /c start /wait` swallows the exit code** | A program that exits with 3 "succeeds" | Launch the executable directly as a child and pin it with `SetProcessAffinityMask`. If `start` is needed, `cmd /v:on /s /c "start "" /wait … & exit !errorlevel!"` | Run a program that exits with a known non-zero code through the same launcher, and check what it reports |
| ⚠ caught (review): **pairing runs by list position** | A skipped run shifts every later pair; differences mix rounds | Pair by the key both sides share (the run index) | Check that both sides of every pair share one run index in the manifest |
| ⚠ caught by UE5-SpawnPacer: **passing a command-line flag the engine doesn't parse** (`-ExitAfterSeconds`) | Nothing happens, so nothing tells you | Grep `Engine/Source` for the flag's text before relying on it; it's now removed from `smoke_game.bat` | Grep `Engine/Source` for the flag's text: if nothing parses it, it does nothing |
| ⚠ found in the S4 test log: **a deprecated command-line argument** (`-ReportOutputPath=`) | The run works, but the log carries `Warning: Argument -ReportOutputPath= is now -ReportExportPath=` | Read every warning in a tool run, not only the exit code; use the name the engine parses today (`AutomationControllerManager.cpp:233`) | Grep every tool log for `Warning`: a deprecation is only a warning |
| ⚠ caught (review): **a default that yields one run** | A one-config batch reports a "median across runs" of n = 1 with no spread | A minimum run count (4), rounded up to a multiple of the config count | Print the run count per configuration from the manifest: n = 1 has no spread |
| ⚠ caught: **an analysis script that stops at the first bad run** | One timed-out run gives a traceback and no report for the good runs | Reuse the harness's failure rule, catch per-run errors and list them in the report | Run the script on a batch with a known failed run: it must list it and still report the others |
| ⚠ caught: **an analysis that assumes the setting it checks** | SC2 passes or fails against a hard-coded 0.5 s, whatever the project's categories say | The game writes its own bound into the capture (`UBMaxStaleness` metadata) and the analysis reads it | Change the setting and re-run: the bound in the report must follow it |

---

## Build, repository and modules

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught by the Phase 6 build: **a file-scope `using namespace` in a `.cpp`** | A build fails in a file nobody changed (here error C4459, "declaration hides global declaration", in the scheduler tests), only once UBT's unity batching puts the two files together | Put using-directives inside functions, or qualify the names: a unity build compiles several `.cpp` files as one translation unit | The error's note names the other declaration and its file; UBT's adaptive-build line lists which files were left out of the unity file |
| Making the plugin a *private* dependency while a public header derives from it | Works until another module includes the header, then "cannot open include file" | Public dependency if a **public header** needs it | UBT error points at the include |
| Planning on the **Test** configuration with a launcher engine | `Targets cannot be built in the Test configuration with this engine distribution` | Headline = packaged Development (Phase 4 finding) | The UBT error names the configuration at once: nothing to debug, only to plan around |
| Trusting the MSVC folder name as the compiler version | You think you're on a banned toolchain (`14.44.35207`) when the real compiler is `14.44.35229` | Read the UBT log line "Using Visual Studio 14.44.35229 toolchain" | Read the UBT log's toolchain line, not the folder name |
| ⚠ found in UE5-SpawnPacer's audit: **committing an engine-written security token** (`AndroidFileServer` `SecurityToken` in `DefaultEngine.ini`) | Read every engine-written block in committed config before a public push | Disable plugins the project doesn't need (A1) and delete what they wrote | Before a push, grep the committed config for `SecurityToken` and read every block the engine wrote |
| ⚠ found while checking a commit: **a backslash sequence turned into a control character by a scripted edit** (`Tools\run_tests.bat` became `Tools` + carriage return + `un_tests.bat`) | Git treats the file as binary (`git ls-files --eol` shows `-text`); GitHub shows no diff | Scan for lone carriage returns before every commit; write paths with forward slashes or escape the backslash in the script | `git ls-files --eol` shows `-text` for the file; a scan for lone carriage returns finds the byte |
