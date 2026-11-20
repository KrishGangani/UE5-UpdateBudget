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
