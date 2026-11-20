# 07 · Common Mistakes: UpdateBudget

**Rule:** for every module, the mistakes that are easy to make here, **how to spot them, how to prevent them and how to debug them**. Mistakes the S1 code review **actually caught in this project** are marked ⚠ caught, which makes them real interview stories.

---

## UpdateBudgetCore: component and registry

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught: **two clocks.** Native ticks forward the engine's `DeltaTime` (frame delta or interval delta, dilated per actor), while staleness uses world time | Behaviour differs by mode for the same world time; SC6 fails "randomly" | One owner for time: `RunUpdate()` measures `Now − LastUpdateTime` from `UWorld::GetTimeSeconds()` for every caller | Log `DeltaSeconds` and `Now − Last` side by side for one agent in two modes |
| ⚠ caught: **not counting the first update** (an update was only counted when a gap existed) | `UpdatesRun = 0` on the spawn frame; waves undercounted | Count every update. Only the *gap* is conditional. Pass facts (`PreviousUpdateTime`), not sentinel encodings | The CSV column on the first capture frames must equal N |
| ⚠ caught: **null entry after swap-remove.** GC nulls a `UPROPERTY` pointer to a collected component; the swap moves that null into a live slot, then it is dereferenced | An access violation in `Unregister`, often only in long sessions | One `RemoveClientAt` helper that null-checks the moved entry. Every access tolerates null | Run `obj gc` with agents being destroyed; Insights or a debugger on `RemoveClientAt` |
| ⚠ caught: **no cleanup on `Deinitialize`.** Components keep a strong reference to a dead subsystem | Stale-pointer crashes on world teardown or seamless travel | `Deinitialize` detaches every remaining client, then calls `Super` | Break on `Deinitialize` with agents alive; check the clients' `Subsystem` is null afterwards |
| Relying on GC to "clean up" registry entries | Entries point at garbage until the next GC run (`01b #8`) | Unregister explicitly in `EndPlay`; check `IsValid` where used | `IsValid()` false while the pointer is non-null |
| Forgetting `bCanEverTick = true` | The native baseline never updates (`UpdatesRun = 0`) | Set it in the constructor (default false) | `dumpticks` lists no component tick |
| Leaving the default `TG_DuringPhysics` | Baselines and scheduler run at different frame positions, an unfair comparison | Pin `TG_PrePhysics` explicitly | `dumpticks grouped` |
| Recording CSV stats once **per update** | Instrumentation shows up as cost (about 50 ns × N per frame) | Aggregate in plain fields; publish once per frame | Compare the CSV profiler's startup benchmark line with N |
| Making the plugin a *private* dependency while a public header derives from it | Works until another module includes the header, then "cannot open include file" | Public dependency if a **public header** needs it | UBT error points at the include |

## Game module: workload and director

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Spawning with `SpawnActor`, then setting the patrol | `BeginPlay` or the first tick sees an uninitialised patrol (all agents at the origin) | `SpawnActorDeferred` → `InitPatrol` → `FinishSpawning` | Log the first update's position |
| Using `FMath::Rand` for placement | Hash differs run to run | Seeded `FRandomStream` | The S1 hash check: same seed, same hash |
| An asset referenced only from C++ isn't cooked | Invisible agents in packaged builds; a "Failed to load" warning in the packaged log | `DirectoriesToAlwaysCook` | Grep the packaged log for the asset path |
| A workload the optimiser can delete | Suspiciously flat cost vs `ComputeIterations` | Store results to a member (`ComputeSink`) | Sweep `-UBWork=` and confirm cost scales |
| Caching "where is the player" once per frame for speed | The baseline gets unrealistically cheap, and the result no longer transfers to real perception code | Keep per-agent queries; it is part of the workload's realism | Document the choice (efficiency audit, item 5) |
| Expecting `GameThreadTime` under `-nullrhi` | The column stays 0 (S1 finding) | Headline runs use the real RHI at a fixed small resolution | Check the column before trusting a run |

## Build and benchmark pipeline

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| Planning on the **Test** configuration with a launcher engine | `Targets cannot be built in the Test configuration with this engine distribution` | Headline = packaged Development (Phase 4 finding) |
| Trusting the MSVC folder name as the compiler version | You think you're on a banned toolchain (`14.44.35207`) when the real compiler is `14.44.35229` | Read the UBT log line "Using Visual Studio 14.44.35229 toolchain" |
| Looking for editor `-game` CSVs in the project's `Saved\` | No file where expected | Editor `-game` writes to `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Profiling\CSV`; the log line "Writing CSV to file" gives the path |

## Benchmark harness (S2): mistakes the S2 code review caught

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| ⚠ caught: **harness work inside a measured frame** (the report was written on the BenchEnd frame, which is still captured) | The max and p99 of the last frame grow with N | Do benchmark bookkeeping on frames outside the capture window |
| ⚠ caught: **raw files overwritten by a later batch** (names had only the date) | An old summary's numbers no longer match its raw file | Batch-timestamped names, and refuse to overwrite |
| ⚠ caught: **one bad capture aborts the whole batch** | No manifest after a crash; good runs unaccounted for | Catch analysis errors per run; write the manifest after every run |
| ⚠ caught: **mixing two load levels in one statistic** (a wave run summarised over the whole capture) | The "median" lands between the two loads; p99 is dominated by the spawn spike | Split at the event: pre, event frame, post |
| ⚠ caught: **first run always cold** | The first configuration in the order is systematically different | A discarded priming run, and an even run count with rotation |
| ⚠ caught: **a run that never started inherits the previous run's log** | Clean error counts for a run that didn't run | Only accept files modified after this run started |
| ⚠ caught: **the measured camera path depended on warm-up length** | Batches with different warm-ups differ slightly | Time the path from the *planned* BenchStart |
| ⚠ found while testing: **using Unreal's first header row** | `HasHeaderRowAtEnd=1`: columns added mid-capture exist only in the end header | Use the end header; check the first header is a prefix of it |
| Assuming a PSO event means a hitch | Panic over a cost that isn't there | Look at those frames' times. Here they were *below* the median (asynchronous precaching), so they are counted and monitored, not removed |

## Baselines (S3): mistakes the S3 code review, the clean-up and the smoke runs caught

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| ⚠ caught: **a destroyed agent stays registered with the Significance Manager** | The manager holds a raw pointer, so the next `Update` calls the significance function on a dead object: a crash some time after a despawn | Bind the agent's `OnEndPlay` and call `UnregisterObject` there; `UnregisterAll(Tag)` in `OnWorldEndPlay` as the safety net |
| ⚠ caught: **unregistering in `Deinitialize`** | `USignificanceManager::Get` already returns null there: the manager is removed in `OnPostWorldCleanup`, so nothing is unregistered | Unregister in `OnWorldEndPlay`, which runs while the manager still exists (06 §17) |
| ⚠ caught: **one-off registration timed inside the per-frame re-tune stat** | `Retune`'s p99 jumps on the wave frame only | Two `CSV_SCOPED_TIMING_STAT` scopes: `Register` and `Retune` |
| ⚠ caught: **tick order between two subsystems left to chance** | Wave agents are adopted a frame late in some builds and on time in others | `Collection.InitializeDependency<T>()` **before** `Super::Initialize` (06 §19) |
| ⚠ caught: **dereferencing a brain that no longer exists** | A crash when an agent is destroyed before it's adopted | `IsValid(Agent)` before `GetBrain()`; skip null brains |
| ⚠ caught: **two "equivalent" band tests that disagree at an edge** | B2 and B3 put an agent at exactly 15 m in different bands. A significance of 1 − 1e-9 rounds to exactly `1.0f` | One shared band test; clamp the Mid-band significance into (0, 1) with `BelowOne` and `TNumericLimits<float>::Min()` |
| ⚠ caught (clean-up): **a baseline that does avoidable work** | B3 read every component's interval every frame although the manager hands over the old and new significance | Compare the old and new **bands** first. A baseline must be what a competent team writes, not a strawman |
| ⚠ caught (altitude): **an unwritten contract between two classes** | Adoption by index works only while the agent list is append-only; nothing said so | Write the contract on `GetAgents()`; `ensureMsgf` where it is relied on |
| Changing tick intervals in a `Concurrent` post-significance function | Intermittent tick-manager asserts or corruption: worker threads touching game-thread state | `EPostSignificanceType::Sequential` |
| Using `SetComponentTickInterval` and expecting an immediate effect | A far agent that comes near keeps updating slowly for up to its old interval | `SetComponentTickIntervalAndCooldown` (06 §18), only when the band changes |
| ⚠ found while testing: **looking for `UBBaseline/Retune` in the CSV** | The column "doesn't exist" | Timing-stat columns include the thread: `UBBaseline/GameThread/Retune`. List the header once before writing analysis code |

## Measurement and the scheduling policy (S3 batch, S4): mistakes found by data, by another project and by review

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| ⚠ found in the S3 batch: **trusting the OS to put the game thread on a fast core** (hybrid CPU: 8 P-cores + 8 E-cores) | Runs sit at **two stable levels about 1.6× apart**; one run **steps** between them mid-capture. Thermal drift looks different: a slow ramp | Pin the benchmark process to the performance cores once (`performance_core_mask()`, `pin_self()`): every game it starts inherits the mask. Record the applied mask, then re-capture. Verified: B0 at N = 4,000 went from 16.5–28.3 ms to 16.2–18.0 ms |
| ⚠ caught by P03 (UE5-TestTrust): **`cmd /c start /wait` swallows the exit code** | A program that exits with 3 "succeeds" | Launch the executable directly as a child and pin it with `SetProcessAffinityMask`. If `start` is needed, `cmd /v:on /s /c "start "" /wait … & exit !errorlevel!"` |
| ⚠ caught (review): **a NaN entering a moving average** | After one bad measurement, every frame runs every client and is never "over budget" (every comparison with NaN is false) | Treat non-finite measurements as 0 (`FMath::IsFinite`), and test it with `quiet_NaN()` |
| ⚠ caught (review): **a test that never reaches the case it claims to test** | Work out demand against capacity: at the first budget, optional demand (16.7 per frame) fit the free capacity (30), so the "overload" never happened | Make the precondition real, then **assert it happened** (`FramesWithForcedRuns > 0`) |
| Sorting with a non-stable sort and no tie-break | Two clients with equal deadlines swap order between runs; results differ between identical runs | Break ties by index in the predicate |
| ⚠ caught (review): **pairing runs by list position** | A skipped run shifts every later pair; differences mix rounds | Pair by the key both sides share (the run index) |
| ⚠ caught by P02 (UE5-SpawnPacer): **passing a command-line flag the engine doesn't parse** (`-ExitAfterSeconds`) | Nothing happens, so nothing tells you | Grep `Engine/Source` for the flag's text before relying on it; it's now removed from `smoke_game.bat` |
| ⚠ found in the S4 test log: **a deprecated command-line argument** (`-ReportOutputPath=`) | The run works, but the log carries `Warning: Argument -ReportOutputPath= is now -ReportExportPath=` | Read every warning in a tool run, not only the exit code; use the name the engine parses today (`AutomationControllerManager.cpp:233`) |
| ⚠ found in the P02 audit: **committing an engine-written security token** (`AndroidFileServer` `SecurityToken` in `DefaultEngine.ini`) | Read every engine-written block in committed config before a public push | Disable plugins the project doesn't need (A1) and delete what they wrote |
| ⚠ caught (review): **a default that yields one run** | A one-config batch reports a "median across runs" of n = 1 with no spread | A minimum run count (4), rounded up to a multiple of the config count |

## Registry and budgeted path (S5): mistakes found in the 5.8.4 source before they could happen

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ found in the source: **adding to an array while a loop reads it through a raw pointer** (a component spawned inside an update registers at once) | A crash or garbage deadlines only on frames where something spawns, and only when the array happens to reallocate | Defer: while the scheduler runs, adds wait in `PendingAdds` and removals null their slot; both are applied after the loop (02 §8) | Test `SpawnDuringUpdate`; a breakpoint on `Register` with `bRunningScheduler` true |
| ⚠ found in the source: **leaving the registry only in `EndPlay`** | In budgeted mode, agents of a level being streamed out keep moving after their level has unregistered them, while natively they stop. Only visible with streaming | Leave in `OnUnregister` too: streaming removal unregisters components first and routes `EndPlay` later, possibly frames later (`World.cpp:4196–4251`) | Test `LevelUnloadOrder`: `UnregisterAllComponents`, tick, then `RouteEndPlay(RemovedFromWorld)` |
| ⚠ found in the source: **disabling the native tick but not `bStartWithTickEnabled`** | Re-registering a component (`ReregisterComponent`, re-attach) makes it tick natively **and** be scheduled: `UpdatesRun` is above N | The mode sets both; `OnRegister` re-joins a re-registered component (`ActorComponent.cpp:1747`, `Actor.cpp:6443–6451`) | Test `NoDoubleUpdate`; count updates per client per frame |
| ⚠ found in the source: **`Activate()` re-enables the tick** | The same double update after any `Activate()` call, for example from gameplay code or a Blueprint | A guard at the top of `TickComponent` switches the native tick off again without updating (`ActorComponent.cpp:2819`) | Test `NoDoubleUpdate`; `IsComponentTickEnabled()` after the call |
| ⚠ found in the source: **setting the mode with `-ExecCmds`** | A "UB" run that measures B0: the mode is read at begin play, and `-ExecCmds` run later as deferred commands (`UnrealEngine.cpp:2552, 2772–2810`) | `-ini:Engine:[ConsoleVariables]:ub.Enabled=1`, and the subsystem logs the mode once per world | Grep the run log for `LogUpdateBudget: Display: Budgeted mode` |
| ⚠ found in the source: **ticking a test world twice in one frame number** | The second tick runs no tick functions at all, so the test sees no updates and blames the code under test | Advance `GFrameCounter` after each tick, or use the engine's `FTestWorldWrapper`, which does (`AutomationCommon.cpp:232`) | Count `ExecuteTick` calls per `World->Tick` |
| ⚠ found in the source: **a hand-made test world whose actors never begin play** | `OnWorldBeginPlay` runs (so the subsystem looks fine), but no component registers | Use `FTestWorldWrapper::BeginPlayInTestWorld`, which spawns a game mode so `StartPlay` dispatches `BeginPlay` (`World.cpp:6165–6179`) | `HasActorBegunPlay()` on a spawned actor |
| ⚠ found in the source: **reading a destroyed component after a world tick in a test** | A test that passes or crashes depending on when GC runs | Keep counts outside the components (`FUpdateLog`); `UWorld::Tick` may collect garbage at its end (`LevelTick.cpp:1968–1971`) | Run the test with `gc.ForceCollectGarbageEveryFrame 1` |
| Ending a world "mid-frame" and guarding for it | Code complexity for a case that can't happen | Know the engine: PIE end and travel are queued and run between frames (02 §8); test the real worst case instead (`WorldTeardown`) | — |
| Restoring a test cvar before the world that reads it is destroyed | Once the mode switches live (S7), the test world changes mode during teardown | Declare the `FTestConsoleVariable` before the world wrapper, so C++ destroys it after (04 §18) | — |

## Significance and telemetry (S6): mistakes the S6 code review caught

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| ⚠ caught: **a view cone of half the horizontal FOV** | On a wide screen, agents in the corners of the picture count as out of view, so they get half significance and visibly update less often | Use the cone that circumscribes the view rectangle (half-diagonal angle from FOV and aspect ratio); test a point near a corner (`Significance.ViewCone`) |
| ⚠ caught: **a client at the camera counts as out of view** | A first-person pawn's own logic updates a few times a second: its position relative to the camera is zero or perpendicular, so "in front" fails | Everything within a small radius of the camera is in view (`AlwaysInViewRadius`) |
| ⚠ caught: **a "missed" counter that counts what could never run** | The SC3 share drops when agents are destroyed during the loop, although no live near agent was skipped | Count from the outcome, after invalid entries are removed (`CountNearClients`) |
| ⚠ caught: **an analysis script that stops at the first bad run** | One timed-out run gives a traceback and no report for the good runs | Reuse the harness's failure rule, catch per-run errors and list them in the report |
| ⚠ caught: **an analysis that assumes the setting it checks** | SC2 passes or fails against a hard-coded 0.5 s, whatever the project's categories say | The game writes its own bound into the capture (`UBMaxStaleness` metadata) and the analysis reads it |
| Expecting `+Categories=` lines in `DefaultGame.ini` to add to the C++ default category | The default category disappears | Any ini entries replace the array (`Obj.cpp:3091–3095`); list every category you want |
| Reading this frame's camera during `TG_PrePhysics` | Significance lags a camera cut by one frame | Know that cameras update after all tick groups (`LevelTick.cpp:1828–1847`); the brains see the same one-frame-old view, so the comparison stays fair |
| Treating the budget as a cap | `UpdateMs` sits at the budget on every frame even when few updates were due | The design is work-conserving: after the mandatory clients it keeps running clients in deadline order while the budget lasts (02 §2). A smaller budget is the way to spend less (02 §12) |
| ⚠ caught by the compiler: **an integer that converts equally well to two overloads** (`AddOnScreenDebugMessage` takes a `uint64` or an `int32` key; `GetUniqueID()` is `uint32`) | Error C2668, ambiguous call to an overloaded function | Cast to the exact parameter type (`static_cast<uint64>`) |

## Live switching (S7) and the scheduler tests from UE5-TestTrust: mistakes caught by review, by testing and by mutation testing

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| ⚠ caught (review): **switching tick functions inside the change callback** | A cvar set from inside an update enables native ticks mid-frame; clients the scheduler already ran update again in a later tick group | Mark the switch pending; apply it after the tick manager's `EndFrame` (`OnWorldPostActorTick`), and test the in-update case (`LiveSwitch`) |
| ⚠ caught (review, then the altitude review): **a mode switch that overwrites gameplay's choices** | A component gameplay had paused updates natively again after a switch there and back | Record gameplay's choice where every path goes through (`SetComponentTickEnabled`); have the component's own mode logic call `Super::` |
| ⚠ caught (review): **a capture labelled with the mode it began in** | A capture that spans a switch is filed as native or budgeted | Mark it `Mixed` at the switch, and mark the frame with a CSV event |
| ⚠ caught (simplify): **counters copied field by field into a report** | A new statistic needs four edits; a reset that misses one field (it happened: the budget value) | Make the report the accumulator and reset it with one assignment |
| ⚠ found by UE5-TestTrust (mutation testing): **boundaries no test sits on** | A comparison can flip between `<` and `<=` (or `>` and `>=`) and every test still passes: 3 of 39 mutants of the scheduler survived for that reason | For every comparison that encodes a rule, add one test **exactly on the boundary**, with inputs that are exact in binary (0.25, 0.5) so rounding can't move it |
| ⚠ found while checking a commit: **a backslash sequence turned into a control character by a scripted edit** (`Tools\run_tests.bat` became `Tools` + carriage return + `un_tests.bat`) | Git treats the file as binary (`git ls-files --eol` shows `-text`); GitHub shows no diff | Scan for lone carriage returns before every commit; write paths with forward slashes or escape the backslash in the script |
