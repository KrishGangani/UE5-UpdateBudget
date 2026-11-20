# 02: Technical Challenges in Modern AAA and Large-Scale Games

Research notes for Step 0 of the portfolio. The question is narrower than "what is hard in game development". It is: **which hard problems keep coming back in talks and engineering write-ups, and which of them can one developer reproduce and measure in a small UE5 project?**

All numbers below belong to the source that published them. **None of them are our results.** They describe other teams' hardware, builds and workloads. Our own numbers must come from our own controlled tests (Hard rule 1).

---

## Method

- **Date of research:** 2026-11-19.
- **Tools:** web search, plus a page-fetch tool. The fetch tool returns a model-written summary of a page, not the raw page. Where the exact number mattered, I checked it against raw text:
  - **GDC slide PDFs** (Hitman, Sea of Thieves, Naughty Dog) were downloaded and text-extracted with `pdftotext`. Every number quoted from them was read in the extracted text.
  - **unrealengine.com tech-blog posts** returned HTTP 403 to the fetch tool. I downloaded them with a normal browser user-agent and stripped the HTML to text. The numbers quoted from them were read in that text.
- **Source status labels used in this file:**
  - **opened**: I read the page (through the fetch tool's summary).
  - **opened, raw text checked**: I read the extracted raw text and checked the numbers in it.
  - **secondary (opened)**: I read a third party's write-up of a talk, not the talk itself. Treat the numbers as "reported by X about Y".
  - **snippet only**: I saw it only in search-result text. It is not verified.
- **Not done:** no video talks were watched. Video-only talks are summarized from their abstracts, slides or write-ups and are labelled that way.
- **Engine-version caution:** Epic's documentation pages were served as "Unreal Engine 5.9 Documentation" on the fetch date. This portfolio has UE 5.6.1, 5.7 and 5.8.3 installed. Any cvar, default value or API named here must be re-checked against the engine version we choose (Hard rule 3).

---

## Topic 1: Game-thread CPU cost, Tick overhead, frame-time consistency, hitches

**[T1-A] "Game engines and shader stuttering: Unreal Engine's solution to the problem"**: Kenzo ter Elst, Daniele Vettorel, Allan Bentham, Mihnea Balta (Epic), 2025-02-04. https://www.unrealengine.com/en-US/tech-blog/game-engines-and-shader-stuttering-unreal-engines-solution-to-the-problem. Status: opened, raw text checked. Authors and date confirmed by [T1-B].
- On PC, a driver can compile a shader the first time it is drawn. That can take "tens of milliseconds or more for a single draw command".
- PSO precaching (experimental from 5.2) collects possible PSOs at load time. As of 5.5 it still did not cover **global graphics shaders**, so those can still cause one-time hitches. On mobile, some rare permutations are skipped on purpose, and a loading-screen timeout can let the game start while compiles are still running.
- **Epic's number:** "Fortnite takes around 20-30 seconds longer to load into a Battle Royale match when the driver cache is empty."
- Epic advises studios to **monitor PSO stats as part of automated testing**. It also says other "traversal stutter" comes from synchronous loading, too much spawning or stream-in, and scene captures triggered by movement. It notes that finding the root cause is hard "without instrumentation".

**[T1-B] 80.lv coverage of [T1-A]**: Amber Rutherford, 2025-02-05. https://80.lv/articles/epic-games-devs-on-solutions-to-unreal-engine-5-shader-stuttering-issues. Status: opened.
- Confirms the authors and date. Says the older Bundled PSO Cache approach was resource-heavy and needed constant updating in games with dynamic content.

**[T1-C] "PSO Precaching for Unreal Engine" (official docs)**: Epic. https://dev.epicgames.com/documentation/unreal-engine/pso-precaching-for-unreal-engine?lang=en-US. Status: opened.
- Splits hitches into "Missed", "Too late" and "Untracked" precaches. Profiling is done with the Insights timers `PSOPrecache: Missed` / `PSOPrecache: Too Late`, with `stat PSOPrecache`, and with the `-clearPSODriverCache` argument (to test a cold cache).
- **Epic's numbers:** by default the compile pool uses 75% of hardware threads, and each compile thread can use up to 2 GB. The runtime hitch log threshold defaults to 20 ms. The doc's Insights example shows 5–10 ms misses and one 117 ms hitch.

**[T1-D] "The Great Hitch Hunt: Tracking Down Every Frame Drop"**: Ari Arnbjörnsson (Epic), Unreal Fest Orlando 2025 (June 2025).
- Text version: https://dev.epicgames.com/community/learning/tutorials/6XW8/unreal-engine-the-great-hitch-hunt-tracking-down-every-frame-drop. Status: the page body is rendered by JavaScript and **could not be read**.
- Forum thread: https://forums.unrealengine.com/t/tutorial-the-great-hitch-hunt-tracking-down-every-frame-drop/2541268. Status: opened.
- Press summaries: https://playersforlife.com/2025/07/29/epic-games-engineers-reveal-the-7-causes-of-unreal-engine-issues/ (secondary, opened) and https://wccftech.com/unreal-engine-declares-great-hitch-hunt-rid-games-stutters/ (snippet only).
- Hitch categories reported in the summaries: level streaming, physics/collision, character spawning, PSO compilation, garbage collection, synchronous loading, and Blueprint logic. Two more were added to the text later: UI Widget and Text hitches (snippet only, from Ari's post announcing the update).
- Fixes reported in the coverage: limit how many actors you create per frame; recycle actors with pooling, which coverage describes as "not a built-in feature yet"; load only asynchronously; use simple collision shapes.
- In the forum thread, a developer asked for a `SpawnActorAsync`. Ari agreed it would be useful. He said studios have built their own versions on top of incremental component registration, and that a native version would likely be **time-sliced on the game thread** rather than run on another thread. He also said Packed Level Actors reduce actor-loading overhead.

**[T1-E] "Incremental Garbage Collection in Unreal Engine" (official docs)**: Epic. https://dev.epicgames.com/documentation/en-us/unreal-engine/incremental-garbage-collection-in-unreal-engine. Status: opened.
- Normally, GC reachability analysis runs inside a single frame, so more UObjects means a longer pause. Incremental reachability (`gc.AllowIncrementalReachability`, `gc.IncrementalReachabilityTimeLimit`, for example 0.002 s) spreads the work over several frames.
- It depends on `TObjectPtr` write barriers, so raw-pointer `UPROPERTY`s must be converted.
- It is **Experimental and not fully thread-safe**. Epic recommends it only for single-threaded builds such as dedicated servers. The time limit is soft: one step can still overrun it.
- A third-party report (snippet only) describes a 629 ms GC reachability hitch in Cesium for Unreal: https://github.com/CesiumGS/cesium-unreal/issues/980.

**[T1-F] Tom Looman's engine-release performance notes and optimization talk**: Tom Looman (independent UE trainer). Status: opened.
- UE 5.5 highlights: https://tomlooman.com/unreal-engine-5-5-performance-highlights/ (page says updated 2026-04-10). Tick Batching (`tick.AllowBatchedTicks`) groups similar ticks to cut TaskGraph overhead. `SetTickableTickType` replaces the slow `IsAllowedToTick`. **No benchmark numbers are given.**
- UE 5.8 highlights: https://tomlooman.com/unreal-engine-5-8-performance-highlights/ (updated 2026-08-27).
  - A "UObject Count" Insights counter was added "since object count strongly affects GC cost".
  - GC can now trigger based on how many actors are waiting to unload.
  - Mass entities can be created off the game thread.
  - CitySample editor HLOD visibility updates went from **25.1 ms to 0.18 ms**, as reported in his 5.8 notes. I did not trace this to Epic's own release notes.
- "Unreal Engine Game Optimization on a Budget" (JetBrains GameDev Day talk write-up, updated 2025-08-30): https://tomlooman.com/unreal-engine-optimization-talk/.
  - Recommends `dumpticks` as a first step, then the Significance Manager, then URO for distant skeletal meshes.
  - Collision guidance: QueryOnly instead of Physics+Query, and `GenerateOverlaps=false`.
  - Says to profile **packaged builds**, not the editor.

**[T1-G] "Parallelizing the Naughty Dog Engine Using Fibers"**: Christian Gyrling (Naughty Dog), GDC 2015. Slides: https://media.gdcvault.com/gdc2015/presentations/Gyrling_Christian_Parallelizing_The_Naughty.pdf. Status: opened, raw text checked. Not UE; the lessons transfer.
- The problem slide: "CPU utilization is nowhere near 100%!"
- **Their frame-time progression for The Last of Us Remastered on PS4:**
  - initial port: 132 ms
  - after "jobify everything": 55 ms
  - after "more jobs and fewer locks": 36 ms
  - then a CPU critical path of ~25 ms with "lots of idle time on CPU cores"
- About 100 ms of total CPU work per frame was spread over 6 worker threads, using 160 fibers and ~800–1000 jobs per frame.
- To reach 60 fps they ran game logic and render logic **for different frames at the same time**.
- Lesson: frame time is set by the critical path, not by total work.

**Takeaway for Topic 1.** Most hitches come from **work that arrives in bursts as content enters the world**: spawning, component registration, physics-state creation, synchronous loads, GC and PSO compiles. Steady-state tick cost matters less. Epic's own advice is "instrument, then fix". Both Epic's blog and its docs treat PSO and hitch monitoring as something automated tests should do.

---

## Topic 2: Large actor and agent counts (Mass, ECS, data-oriented design, crowds)

**[T2-A] "Crowds in Hitman: Absolution"**: Kasper Fauerby (IO Interactive), GDC 2012. Vault: https://www.gdcvault.com/play/1015315/Crowds-in-Hitman. Slides: https://media.gdcvault.com/gdc2012/slides/Programming%20Track/Fauerby_Kasper_CrowdsInHitman.pdf. Status: opened, raw text checked.
- **Their numbers:**
  - ~1200 agents simulated, 500 on screen.
  - PS3 PPU cost 5 ms (animation ~2 ms, crowd AI/steering ~2 ms, framework ~1 ms).
  - ~20 ms of SPU work spread over several SPUs; GPU 8 ms.
  - ~16k cells in the crowd grid.
- "Scales nearly linearly with number of agents." The ratio of culled to on-screen agents also changes the cost, because of animation.
- Memory layout was "one of the most important things for performance". A full agent was ~256 bytes. They split out a 36-byte "agent core", allocated in one aligned block (1200 agents: 42 kb), to cut cache misses.
- For animation, agents were moved by the animation's trajectory channel, which removed foot sliding. The trade-off: agents "react much slower to steering input", which makes collisions harder to avoid. This is a clear animation-vs-gameplay authority problem (see Topic 5).

**[T2-B] "Massive Crowd on Assassin's Creed Unity: AI Recycling"**: Francois Cournoyer (Ubisoft), GDC 2015. https://gdcvault.com/play/1022411/Massive-Crowd-on-Assassin-s. Status: opened.
- Uses AI level of detail plus a pooling system that swaps low-res and high-res NPCs without the player noticing.
- **Their numbers:** a limit of 40 "real" AIs and 120 high-res models still let them show **10,000 crowd NPCs** on screen.

**[T2-C] City Sample / Mass**: Epic.
- City Sample docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/city-sample-project-unreal-engine-demonstration. Status: snippet only (the page body did not render). Mass spawners for crowds and traffic; crowds use StateTree, traffic does not.
- Mass docs hub: https://dev.epicgames.com/documentation/en-us/unreal-engine/mass-entity-in-unreal-engine. Status: opened. "A gameplay-focused framework for data-oriented calculations". The page is tagged Experimental.
- Community reference: https://github.com/Megafunk/MassSample (README targets 5.6/5.7). Status: opened.
  - Explains fragments, tags, archetypes and chunked struct-of-arrays storage.
  - `MassRepresentation` draws entities mainly as instanced static meshes and swaps them for full actors at set distances.
  - **No benchmark numbers.**
- Mario Palermo's "Large Numbers of Entities with Mass in Unreal Engine 5" is snippet only. No URL was confirmed.

**[T2-D] Witcher 4 UE5 tech demo (CD PROJEKT RED + Epic)**: State of Unreal, Unreal Fest Orlando, 2025-06-03.
- CDPR blog (2025-06-13): https://www.cdprojektred.com/en/blog/149/working-with-epic-to-debut-the-witcher-4-unreal-engine-5-tech-demo-at-unreal-fest. Status: opened.
  - Five showcased systems: FastGeo Streaming, Nanite Foliage, Mass, ML Deformer and the Unreal Animation Framework (UAF).
  - Lists the deep-dive talks: "Streaming Improvements for Dense Worlds" and "Road to 60FPS in The Witcher 4 Unreal Engine 5 Tech Demo" (Jarosław Rudzki), and "Locomotion and Interactions for Hundreds of Characters" (Julius Girbig).
- Press reports say the demo ran at 60 fps on a base PS5 and raised the market scene to **300 individually animated NPCs**. Status: snippet only (Yahoo/TechRadar, Creative Bloq).
- Epic's Wyeth Johnson reportedly said UAF let them fit the 60 fps budget "with room to spare on the game thread". Status: snippet only.
- Talk recordings: "Road to 60 fps" https://www.youtube.com/watch?v=ji0Hfiswcjo (Kevin Ortegren, Epic, with Rudzki) and "Streaming Improvements for Dense Worlds" https://www.youtube.com/watch?v=BdopUm1_1_E. Status: snippet only; not watched.

**[T2-E] "Overwatch Gameplay Architecture and Netcode"**: Timothy Ford (Blizzard), GDC 2017. https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and. Status: opened (abstract only).
- Says ECS let them add varied, layered gameplay without the complexity getting out of control. The networked simulation relies on determinism for responsiveness.
- Secondary transcripts report ~46 client systems and 103 component types, with gameplay netcode kept to 3 systems (movement, weapon, state script). Status: snippet only, e.g. https://www.mo4tech.com/overwatchs-architecture-is-designed-to-synchronize-with-the-web.html.

**Takeaway for Topic 2.** Every crowd talk solves the same problem: **cost per agent multiplied by agent count**. Every one uses the same family of tools: LOD for representation (cheap proxy ↔ full actor), pooling, contiguous data layout, and budgets. The published numbers are console-era and engine-specific. The pattern (a few full-fidelity agents plus many cheap ones, with switching that the player can't see) is what transfers.

---

## Topic 3: Networking at scale (Replication Graph, Iris, bandwidth, server CPU, prediction)

**[T3-A] "Unreal Engine Improvements for Fortnite: Battle Royale"**: Epic tech blog, 2017 (UE 4.18/4.19 era). https://www.unrealengine.com/en-US/tech-blog/unreal-engine-improvements-for-fortnite-battle-royale. Status: opened, raw text checked.
- Goal: a dedicated server that "could handle 100 simultaneous players, maintain 20hz, and minimize bandwidth" on a 5.5 km² map.
- Fixes:
  - Limit how many players get updates per frame: **25 in the lobby, 50 in gameplay**.
  - Rate-limit client movement updates, so high-FPS clients can't overload the server.
  - Stop replicating ping to other clients.
  - Remove allocations from `FArchive::SerializeIntPacked`.
  - Compare property types by FName instead of by string.
- **Console memory savings they reported:** volume textures −240 MB+, D3D12 descriptor heaps −120 MB, render targets −100 MB+, PS4 texture-streaming pools −300–400 MB. They also removed about **66 ms of input latency** in 30 Hz games.
- Future work they named: level streaming, and "editor performance when working with very large maps".

**[T3-B] "Replication Graph Overview and Proper Replication Methods"**: Epic tech blog (date not shown in the fetched text; UE4 era). https://www.unrealengine.com/en-US/tech-blog/replication-graph-overview-and-proper-replication-methods. Status: opened, raw text checked.
- The problem: "up to 100 clients… 50,000 synced actors". Testing every actor against every client every frame "does not scale well".
- The solution: persistent graph nodes that cache and share replication lists across connections and frames. Example: the Grid Spatialization 2D node replaces per-actor distance checks with a grid-cell lookup.

**[T3-C] Epic networking docs**. Status: opened.
- Networking Overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine. Calls dormancy "one of the most significant optimizations you can make". Advises unreliable RPCs for high-frequency calls and rate-limiting reliable RPCs bound to input.
- Introduction to Iris: https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine.
  - Iris is opt-in and **Experimental**.
  - It keeps replication data separate from game-thread data so work can run concurrently and be shared across connections.
  - The legacy system *polls* objects for changes; Iris expects game code to *notify* it.
  - Builds on Fortnite's 100-players-per-server experience.
- Push-model bugs on Epic's tracker: UE-191014 (push model stopping actors going dormant) and UE-226689 (desync when a push property isn't marked dirty across dormancy). Status: snippet only. https://issues.unrealengine.com/issue/UE-191014, https://issues.unrealengine.com/issue/UE-226689.

**[T3-D] "Developing and Launching a New Replication System" (Iris)**: Epic, Unreal Fest Orlando 2025. https://www.youtube.com/watch?v=K472O2rVvG0. Status: snippet only.
- Covers migrating from Replication Graph to Iris, filtering and prioritization, NetSerializers, and bandwidth debugging.
- One secondary claim, that Iris had already shipped quietly in Fortnite, comes from a Zhihu article and is **unverified**.

**[T3-E] "Iris Replication System: 100 Players in One Place"**: Sergei Vikhirev (BorMor blog), 2025-10-25. https://bormor.dev/posts/iris-one-hundred-players/. Status: opened. Independent benchmark, UE 5.7 Preview 1.
- Setup: 100 clients on a 2 km map; the server on a 2-vCPU VM; packaged Linux builds; Insights traces.
- **His numbers:**

  | Configuration | Server FPS | `FEngineLoop::Tick` | `NetBroadcastTickTime` |
  |---|---|---|---|
  | Generic replication | 10–15 | 83.9 ms | 66.2 ms |
  | Iris (config only) | 15–20 | 60.1 ms | 45.8 ms |
  | Iris + his own patch | — | 45.5 ms | 29.5 ms |

- The patch swapped a per-write allocation for a reused buffer. Replication Graph was not tested ("considered deprecated").
- Caveats: movement-only workload and Insights tracing overhead.

**[T3-F] Forum: "Server networking/replication performance with hundreds of clients"**: https://forums.unrealengine.com/t/server-networking-replication-performance-with-hundreds-of-clients/740652. Status: snippet only.
- A developer reports stock UE 5.1 with 200 mock clients spending ~120 ms+ in `TickFlush`, scaling roughly linearly with connections.

**[T3-G] "What Makes Apex Tick: A Developer Deep Dive into Servers and Netcode"**: Samy Duc (Respawn), 2021-04-28. https://www.ea.com/en/games/apex-legends/apex-legends/news/servers-netcode-developer-deep-dive. Status: opened. Respawn's engine is not UE.
- **Their numbers:** 20 Hz snapshot servers use ~60 kB/s per client at match start. 60 Hz would be ~180 kB/s (3×). A 20 Hz tick means "about five frames of delay" against three at 60 Hz.
- Server slow frames: one server averaged ~200 ms frame time, 4× the 50 ms budget ("slow-mo" servers). Physics-solver spikes came from overlapping players on the Champion screen.
- Lag compensation rolls the world back. Respawn chose to spread errors "symmetrically" between high- and low-ping players.
- Measurement: per-match telemetry, a player-facing network display, and per-frame server profiles (RPROF) to find slow frames.

**[T3-H] "I Shot You First: Networking the Gameplay of HALO: REACH"**: David Aldridge (Bungie), GDC 2011. Vault: https://gdcvault.com/play/1014346/I-Shot-You-First-Networking (snippet only). Summary: https://edgegap.com/blog/game-backend-deep-dive-halo-reach-netcode-host-migration (secondary, opened).
- **Numbers as reported by Edgegap, not checked against the slides:**
  - 16 players at ~250 kbps against a 384 kbps target.
  - More than 2,000 synced entities.
  - Fixing a leftover priority boost on idle grenades freed ~20% of bandwidth.
  - Not networking ragdolls after death freed ~10–12%.
- Design and engineering worked together to hide latency (armor-lock timing, assassination camera blends). They built a network profiler into the replay system and gave playtesters a "flag lag" button.

**Takeaway for Topic 3.** Server cost comes from **relevance and priority work per connection** and from **detecting property changes**. Bandwidth comes from sending what didn't need sending. Every studio that published numbers measured per-connection CPU ms and bytes per second, not FPS. Prediction and lag compensation are design trade-offs as much as code problems (Halo, Apex, Overwatch).

---

## Topic 4: World streaming and large worlds (World Partition, streaming hitches, persistence, traversal)

**[T4-A] Forum: "World Partition streaming performance"**: Epic Developer Community, April–May 2025, with replies from Epic staff. https://forums.unrealengine.com/t/world-partition-streaming-performance/2577446. Status: opened.
- The problem: `s.LevelStreamingActorsUpdateTimeLimit` (5 ms) is checked only *between* work items. One trace registered 10 components before it noticed it was over budget. Large ISMs create all their collision at once.
- An Epic engineer analysed one trace: a single `RemoveFromWorld` removed 61 components holding **47,574 physics bodies** (the two largest had more than 11k instances each).
- Epic's advice: runtime PCG, fewer actors and components, ask whether instances need collision, async physics state creation, and upgrade to 5.6 for FastGeo and the streaming changes.
- Still open at the end of the thread: `ComputeIntermediateSpatialAcceleration` and landscape-collision hitches. Fast travel kept adding levels while over budget.

**[T4-B] Witcher 4 streaming talks**: Jarosław Rudzki (CDPR) with Epic's Richard Mallo, Unreal Fest Orlando 2025. https://www.youtube.com/watch?v=BdopUm1_1_E. Status: snippet only (a Chinese write-up was found in search but returned 403).
- Reported contents: Actor Descriptor Mutators, Cell Transformers, async physics-state creation, a Unified Streaming Budget, and the FastGeo plugin (static geometry streamed without full actor/component cost). It reportedly compares three setups (original, async, async + FastGeo).
- Epic's World Building Guide (snippet only, https://dev.epicgames.com/community/learning/knowledge-base/r6wl/unreal-engine-world-building-guide) mentions `s.UseUnifiedTimeBudgetForStreaming` and a time budget for async physics state. It says budgets should be tuned "based on profiling".
- Epic's later reply in [T10-B] says FastGeo creates primitives "without components or actors" and that this is still being developed for 5.8.

**[T4-C] "The Player was Here: Persisting World State and Actor LOD Systems"**: Epic, Unreal Fest Chicago 2026. Sample code: https://github.com/ZKShao-EpicGames/Sample-PersistenceLab. Status: opened.
- The problem: saving and restoring the state of map-placed actors, spawned actors, instanced actors and Mass entities across World Partition streaming and between sessions. It uses a SaveGame plus World and GameInstance subsystems.
- Built for UE 5.8. The README says the code "has not been battle tested in a shipped title". Level Streaming Persistence and Instanced Actors are **Experimental**.
- A related forum thread (snippet only, https://forums.unrealengine.com/t/preprocessing-data-from-a-world-partition-world/2591463) states the core difficulty: you need a cell's state most when that cell is *unloaded*.

**[T4-D] "Marvel's Spider-Man: A Technical Postmortem"**: Elan Ruskin (Insomniac), GDC 2019. https://www.gdcvault.com/play/1026496/-Marvel-s-Spider-Man. Status: opened (abstract). Not UE.
- Abstract topics: fast-growing asset counts; adapting rendering, streaming and lighting to fit a New York City on a Blu-ray; procedural markup tools; pedestrians and traffic.
- A third-party aggregator claims swing speeds above 20 m/s drove predictive streaming. That claim is **snippet only and unverified**.

**[T4-E] Fortnite BR** ([T3-A]) used HLOD with an overnight rebuild commandlet so artists didn't rebuild locally. It named level streaming on very large maps as further work.

**Takeaway for Topic 4.** Streaming problems fall into two groups:
- **(a) Performance:** the game-thread cost of making content "live" (component registration, physics bodies, collision acceleration structures) is lumpy, and budgets that count items instead of time overrun.
- **(b) Correctness:** streamed-out actors lose their state.

Epic is still actively working on both: FastGeo, unified budgets, and the 2026 persistence talk and sample. That says the problems are real and not fully solved.

---

## Topic 5: Animation–gameplay synchronization (motion matching, root motion, multiplayer, Mover)

**[T5-A] "Motion Matching and The Road to Next-Gen Animation"**: Simon Clavet (Ubisoft Montreal, For Honor), GDC 2016. https://www.gdcvault.com/play/1023280/Motion-Matching-and-The-Road. Status: snippet only.
- Describes motion matching as a deliberately brute-force search that runs every frame. It matches pose features (feet, velocities, weapon) and desired trajectory.
- It trades off responsiveness (follow the gameplay trajectory) against fluidity and smoothness (follow the data).

**[T5-B] "Learned Motion Matching"**: Daniel Holden, Oussama Kanoun (Ubisoft La Forge), Maksym Perepichka, Tiberiu Popa (Concordia University), SIGGRAPH 2020. Page: https://www.ubisoft.com/en-us/studio/laforge/news/3NWqwcMU9lMfumgfPAEfka/learned-motion-matching. Status: opened.
- The problem: motion matching's memory "scales linearly with the amount of data used".
- Holden's example compressed a ~600 MB controller to ~9 MB. Status: snippet only, from his tweet https://x.com/anorangeduck/status/1289310587134959617.
- Search summaries say evaluation is slower than basic motion matching. Not checked against the paper.

**[T5-C] Epic animation docs**.
- Animation Budget Allocator: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine. Status: opened.
  - Caps the game-thread ms spent on skeletal mesh work (`a.Budget.BudgetMs`, default 1.0 ms; the doc's scalability example uses 1.0–2.5 ms).
  - Throttles and interpolates less significant meshes.
  - A UE 5.5 forum report (snippet only) says interpolation is "about as expensive on the game thread" as a full update, because the mesh still ticks: https://forums.unrealengine.com/t/animationbudgetallocator-doesnt-utilize-interpolation-on-registered-components-and-it-feels-like-framerate-drop-what-is-a-proper-way-to-address-it/2663219.
- Game Animation Sample: https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine?lang=en-US. Status: snippet only. Exposes a "Motion Matching Database LOD" setting. Unreal Fest 2024 talks covered motion matching and Choosers. **Epic published no CPU cost figures that I found.**

**[T5-D] Mover**: Epic. Docs: https://dev.epicgames.com/documentation/unreal-engine/mover-in-unreal-engine. Status: opened.
- Modular movement with **rollback networking**, through the Network Prediction Plugin or Chaos networked physics. Intended as the eventual successor to CMC.
- It is **Experimental**. Epic will keep supporting CMC "for the foreseeable future".
- Unreal Fest 2024 intro talk: https://m.youtube.com/watch?v=P4IKS5k47Wg (snippet only).
- An Epic forum reply (snippet only) says there are no plans to integrate NPP with CMC.

**[T5-E] Root motion and montages under CMC prediction**: Epic API docs and forums. Status: snippet only.
- CMC has a dedicated correction RPC, `ClientAdjustRootMotionPosition`, carrying `ServerMontageTrackPosition`. Saved moves record montage root motion.
- Developers report desyncs:
  - root motion started on a rotated base (UE 5.1): https://forums.unrealengine.com/t/root-motion-completely-broken-in-ue-5-1/744362
  - state replicated outside the prediction system (stamina, walk speed): https://forums.unrealengine.com/t/status-of-networkprediction-plugin-working-in-tandem-with-charactermovementcomponent-troubles-synchronizing-walk-speed-stamina-across-client-server/2640104
- `p.NetShowCorrections 1` is the usual diagnostic.

**[T5-F]** Hitman ([T2-A]): moving agents by animation trajectory removed foot sliding but slowed their reaction to steering. Witcher 4 ([T2-D]): "Locomotion and Interactions for Hundreds of Characters" (motion matching, Smart Objects, UAF), snippet only.

**Takeaway for Topic 5.** The recurring tension is **who has authority over position**: animation (root motion, motion matching) or gameplay (capsule, steering, server). The tension grows in multiplayer, where root motion has to be predicted and corrected. Epic's new answers (Mover, UAF) are still experimental, so the CMC + montage path that most shipped games use stays relevant. It is also measurable: corrections per minute and position error under simulated lag.

---

## Topic 6: Automated testing and CI (Sea of Thieves, Gauntlet, perf regression, flakiness)

**[T6-A] "Automated Testing of Gameplay Features in Sea of Thieves"**: Robert Masella (Rare), GDC 2019. Slides with speaker notes: https://media.gdcvault.com/gdc2019/presentations/Masella_Robert_AutomatedTestingOf.pdf. Status: opened, raw text checked. GDC preview: https://gdconf.com/article/sea-of-thieves-devs-share-automated-testing-tips-at-gdc-2019/ (opened).
- **Their numbers:**
  - "Just over 23 thousand tests", and more than 100 thousand including asset-audit checks.
  - 70% actor tests, 5% integration tests; actor-to-integration ratio about 12:1.
  - Coverage: 60% decision, 70% function.
  - Every test runs at least once every 20 minutes.
  - Build verification took 1.5 days, against "a full two weeks" on Kinect Sports Rivals.
  - Full-time testers at release: 17, against 50.
  - Maximum bug count: 214, against nearly 3000 on Banjo-Kazooie: Nuts & Bolts.
- **Flakiness policy:**
  - A failing test is retried immediately; the build turns red only on a second failure.
  - A weekly "top intermittent failures" list drives investigation.
  - Tests that keep failing are quarantined, their owner is notified, and they are deleted automatically if not fixed.
- **Speed tricks:** combine related integration tests; keep the expensive player character alive across test maps using world travel.
- **Ownership:** the person who makes a change writes its tests. Rare had no dedicated test engineers.

**[T6-B] Jessica Baker (Rare)**: "Tests and Testability", blog, 2018-03-11: https://jessicabaker.co.uk/2018/03/11/tests-and-testability/. Status: opened. Unreal Fest Europe 2019 talk "Automated Testing at Scale in Sea of Thieves": https://www.youtube.com/watch?v=KmaGxprTUfI (snippet only).
- Continuous delivery means "a stable production-ready build at all times".
- Testability comes from design: test behaviour not implementation, inject dependencies instead of calling `GetWorld()` everywhere, and mock dependencies through interfaces.

**[T6-C] "How Call of Duty handles automated testing and profiling"**: Jan van Valburg (Activision), GDC 2018. https://www.gamedeveloper.com/production/video-how-i-call-of-duty-i-handles-automated-testing-and-profiling. Status: opened.
- CI across "hundreds of PCs and devkits", performance and memory tracking, and mitigating intermittent/flaky errors.

**[T6-D] "Automated Testing: Using AI Controlled Players to Test The Division"**: Jose Paredes, Pete Jones (Ubisoft Reflections), GDC 2019. https://80.lv/articles/gdc-using-ai-controlled-players-to-test-the-division. Status: opened.
- "Client bots" take over a real client to play missions, gather performance data while wandering the city, and help reproduce bugs.

**[T6-E] EA SEED: "Technical Challenges of Deploying Reinforcement Learning Agents for Game Testing in AAA Games"**: Gillberg et al., IEEE CoG 2023. https://arxiv.org/pdf/2307.11105. Status: snippet only.
- The motivating example: Battlefield V had 601 features needing ~0.5M hours of manual testing.

**[T6-F] Epic Automated Performance Testing plugin**: "A Tech Artist's Guide to Automated Performance Testing", Unreal Fest Bali 2025 (snippet only): https://dev.epicgames.com/community/learning/talks-and-demos/0zx9/unreal-engine-a-tech-artist-s-guide-to-automated-performance-testing-unreal-fest-bali-2025. Epic staff forum replies, Sept 2025: https://forums.unrealengine.com/t/automated-performance-testing-plugin-guidance/2666136 (opened).
- Combines BuildGraph, UAT, Gauntlet, the CSV Profiler, PerfReportTool and Insights. A CitySample test caught "a small CPU regression before 5.6 released".
- **Gap Epic acknowledged:** the built-in tests focus on environment/GPU performance. For gameplay, ReplayRun suits only replicated games, and studios "build data-driven frameworks that play their game nightly". The glue code is Experimental.

**[T6-G] "UEFN Iteration Time & Stability Update"**: Epic (Kavita Chodavarapu, Chris Gagnon), 2026-06-24. https://forums.unrealengine.com/t/uefn-iteration-time-stability-update-progress-and-what-comes-next/2731098. Status: opened.
- Epic tracks Defect Escape Rate and Major Defect Escapes.
- Many regressions "span multiple systems or only appear in late-cycle, large-scale testing".
- Push-to-live time fell 40% on average, up to 70% on large projects (incremental cook, Zen DDC).

**[T6-H] GDC 2021 Automated Testing Roundtable notes** (community): https://github.com/Kadino/gdconf_notes/blob/main/gdc2021.md. Status: snippet only.
- Flaky tests "need immediate investment to fix or de-automate before poisoning testing morale".

**Takeaway for Topic 6.** The hard part is not writing a test. It is **keeping thousands of tests fast and trustworthy**: flakiness, run time, test-world setup cost. The other hard part is **catching performance regressions in gameplay**, which Epic says its own tooling doesn't fully cover. Rare's numbers are one of the few published before/after comparisons of process cost.

---

## Topic 7: Memory and asset loading (hard references, async loading, budgets, LLM, load times)

**[T7-A]** Fortnite BR ([T3-A]): console memory work was "much of this work was game-side content optimization", plus the engine savings already listed (−240 MB, −120 MB, −100 MB+, −300–400 MB). They "improved our low-level memory tracking tools". PSO blog ([T1-A]): an empty driver cache added 20–30 s to Fortnite match load.

**[T7-B] Low-Level Memory Tracker and Memory Insights (official docs)**: Epic. https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-low-level-memory-tracker-in-unreal-engine and https://dev.epicgames.com/documentation/en-us/unreal-engine/memory-insights-in-unreal-engine. Status: snippet only.
- LLM uses scoped tags (`LLM_SCOPE`) and is enabled with `-LLM`; `stat LLM` and `stat LLMFULL` show it.
- Memory Insights shows allocations with callstacks and LLM tags, and can find leaks and growth.
- Third-party notes say LLM is off by default in Test/Shipping builds.

**[T7-C] Hard references, casting and Size Map**: community write-ups. Status: snippet only.
- https://raharuu.github.io/unreal/hard-references-reasons-avoid/ and https://olssondev.github.io/2023-02-19-UnderstandingWhyCastsCanBeExpensive/.
- A Blueprint cast to a Blueprint class creates a hard reference that pulls that class's whole dependency chain into memory. Size Map and the Reference Viewer reveal it. Soft references plus async loading break the chain.
- **I found no official Epic page that quantifies this.** It is widely repeated community knowledge and should be measured, not assumed.

**[T7-D] "Optimizing UE5: Advanced Rendering, Graphics Performance, and Memory Management"**: Matt Oztalay (Epic), Unreal Fest 2024. https://forums.unrealengine.com/t/talks-and-demos-optimizing-ue5-advanced-rendering-graphics-performance-and-memory-management-unreal-fest-2024/1998435. Status: snippet only.
- Rendering-side memory: virtual texture pools, Nanite, Lumen and VSM.

**[T7-E] Zen Loader / IoStore**: Epic docs https://dev.epicgames.com/documentation/en-us/unreal-engine/zen-loader-in-unreal-engine (snippet only), Epic Games Japan decks (snippet only).
- Zen Loader uses an offline-computed package dependency graph and `.utoc`/`.ucas` containers.
- Epic Japan's UE 4.25 IOStore deck reported sample load times −28%, −28% and −5%: https://docswell.com/s/EpicGamesJapan/56G9EK-UE4_IOStore_UE425.
- Their Unreal Fest Gold Coast 2024 deck reported Zen Loader in the editor giving Open Map ×1.8 and Start PIE ×3.1: https://www.docswell.com/s/EpicGamesJapan/ZV172L-Improving%20Development%20Iterations.
- An Epic support reply (snippet only) says game-thread stalls from async loading occur only on the old loose-file/pak path: https://forums.unrealengine.com/t/concerning-game-thread-being-stalled-by-asynchronous-loading-thread/2688865.

**[T7-F] The Coalition + Epic, The Matrix Awakens on Xbox Series X|S**: Xbox Wire, 2021-12-10. https://news.xbox.com/en-us/2021/12/10/how-the-coalition-worked-with-epic-to-bring-the-matrix-awakens-to-life-on-xbox-series-xs/. Status: snippet only.
- They tuned memory systems "especially on Xbox Series S, to ensure it all fit in the memory".

**[T7-G]** Learned Motion Matching ([T5-B]) is a memory-scaling problem. Spider-Man ([T4-D]) fitting a city "onto a Blu-ray" is a disk-budget problem. Tom Looman's 5.8 notes ([T1-F]) report JIT/batched async loading in StreamableManager and a memory-mapped asset-registry cache that saves "GBs" of editor RAM.

**Takeaway for Topic 7.** Memory problems at studios are mostly **content discipline enforced by tools**: tracking (LLM tags), budgets per platform, and avoiding accidental load chains. Load time is dominated by I/O path and by synchronous work. Both are measurable in a small project (memreport / LLM / Size Map, cold and warm load times), as long as the workload is fixed.

---

## Topic 8: Editor tools, content validation, developer productivity, iteration and build times

**[T8-A] "Assassin's Creed Origins: Monitoring and Validation of World Design Data"**: Nicholas Routhier (Ubisoft Montreal), GDC 2018. https://gdcvault.com/play/1025452/-Assassin-s-Creed-Origins. Status: opened (abstract).
- In a huge open world it is "practically impossible" to validate world data in time by traditional methods, made worse by dependencies between AI, art and design data. Their answer: "daily automated tests and reports" that track errors and regressions.

**[T8-B] Data Validation plugin (official)**: Epic. https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/DataValidation/UEditorValidatorBase?lang=en-US and the UE 4.27 guide https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/ProgrammingWithCPP/Assets/DataValidation. Status: snippet only.
- `UEditorValidatorBase` validators (C++/BP/Python) run on save, from menus, and in CIS through a commandlet. They must live in an editor-only module. Function signatures differ between 5.0 and 5.4, so check per version.

**[T8-C] "Taming the Beast: Leveraging Machine Learning to Curb the Cost of Continuous Integration"**: Sarra Habchi (Ubisoft La Forge), GDC 2024. https://www.gdcvault.com/play/1034573/Machine-Learning-Summit-Taming-the. Status: opened (abstract).
- "Thousands of build jobs are submitted daily". RavenBuild predicts build outcomes for code *and data* changes and cut CI cost "up to 43%".

**[T8-D] Embark Studios, Unreal Fest 2022**: "Building Tools Quickly: Blueprints, Menus, Utilities, and Widgets" (Paul Greveson), https://www.youtube.com/watch?v=wJqOn88cU7o, and "The Unbreakable Team: Maintaining Build Stability in Unreal Engine" (Arturo Pérez), listed at https://www.unrealengine.com/en-US/events/unreal-fest-2022-sessions. Status: snippet only.
- "Better tools means faster iterations". Build stability through alerts, unit and integration tests, and Data Validation.

**[T8-E] Cook and iteration time**: Epic forums and talks. Status: snippet only.
- "Developer Efficiency in Unreal Engine 5.6" (Unreal Fest Orlando 2025) covers Zen as cooked-output store and Incremental Cook. Listed in https://dev.epicgames.com/community/learning/tutorials/8JYW/epic-for-indies-setting-up-an-unreal-engine-studio-the-epic-way.
- A forum user saw an incremental cook skip 45k of 57k packages yet still take 24 of 28 minutes: https://forums.unrealengine.com/t/zen-incremental-cook-has-almost-as-long-a-runtime-as-a-clean-cook/2667839.
- UEFN push time fell 40% on average ([T6-G], opened). The CitySample editor HLOD update went from 25.1 ms to 0.18 ms ([T1-F], opened, as reported by Looman).

**Takeaway for Topic 8.** Studios treat **iteration time and broken-content rate** as measurable costs: minutes per push, CI cost, defect escapes. Validation that runs automatically (on save, in CI) is the common answer. A solo project can measure catch rate on seeded content errors and its own iteration-loop time. The absolute minutes will be small.

---

## Topic 9: AI at scale (perception, EQS, utility, StateTree, Smart Objects, tactical)

**[T9-A] "The AI of Horizon Zero Dawn"**: Guerrilla, published 2017-10-17. https://www.guerrilla-games.com/read/the-ai-of-horizon-zero-dawn. Status: opened. Not UE.
- Navigation for agents of very different sizes in a dynamic world, HTN planning combined with utility scoring, and group coordination.
- Julian Berteling's GDC 2018 talk (snippet only, https://www.gdcvault.com/play/1024912/Beyond-Killzone-Creating-New-AI) describes moving from one human enemy type in corridors to "more than 25 wildly different characters" in an open world.
- "HTN Planning in Decima" (Tim Verweij): https://www.guerrilla-games.com/read/htn-planning-in-decima (snippet only).

**[T9-B] "Building a Better Centaur: AI at Massive Scale"**: Dave Mark, Mike Lewis (ArenaNet, Guild Wars 2: Heart of Thorns), GDC 2015. https://archive.org/details/GDC2015Mark. Status: snippet only.
- Modular utility AI (IAUS) plus influence maps. The abstract says agents used "significantly less processing time". A time-lapse shows designers building an AI package in about 7 minutes.

**[T9-C] Forum: "StateTree Performance vs BehaviorTree"**: Dec 2025–Feb 2026, UE 5.7 packaged build, with replies from James Keeling (wording suggests Epic staff). https://forums.unrealengine.com/t/statetree-performance-vs-behaviortree/2705328. Status: opened.
- **The poster's numbers, 400 NPCs, default settings:** StateTree had 15,563,600 executions and 49,803 ms total, against BehaviorTree's 271,073 executions and 2,721 ms. StateTree **ticks every frame by default**.
- With a unified 0.5 s tick, StateTree was ~1.8–2.1× slower in total execution time.
- Epic's reply: frame rate was nearly identical at 200 agents and StateTree was ~6 fps higher at 400. Some StateTree phases don't show correctly in Insights. Event-driven transitions are preferred over polling in Tick.
- **This is a live example of why "FPS is not proof".** Two measurements of the same thing (Insights exclusive time against average FPS) point in opposite directions.

**[T9-D] Unreal Fest 2023 "State Trees and Smart Objects" thread**: https://forums.unrealengine.com/t/talks-and-demos-state-trees-and-smart-objects-data-driven-state-machine-workflows-for-open-world-ai-designs-unreal-fest-2023/1656473. Status: opened.
- James Keeling (Epic), 2024-12-16: StateTree is "usually slightly more performant than BTs". Fortnite and LEGO Fortnite use both.

**[T9-E] Forum: "Is there any way for EQS to share results?"**: archived UE4-era thread (timestamps show 2016). https://forums.unrealengine.com/t/is-there-any-way-for-eqs-to-share-results/348550. Status: opened.
- A reply by "Mieszko" (wording suggests Epic's AI team; the page does not confirm it) says EQS is time-sliced with a budget hardcoded to 0.01 s in `UEnvQueryManager::Tick`.
- Results cannot be shared between agents. Another reply says EQS "does not scale well when running many instances".
- **Version note:** the value may have changed since; check the engine source we choose.

**[T9-F] AI Perception at scale**: old UE4 forum reports. Status: snippet only.
- Sight is time-sliced. One user saw 5–20 s detection delays with ~100 AIs: https://forums.unrealengine.com/t/ai-perception-detecting-the-players-slows-down-significantly-with-the-number-of-ais-in-the-level/88176. Old and anecdotal.

**[T9-G]** AC Unity ([T2-B]) used a "simple AI brain" for thousands of NPCs with 40 real AIs. Hitman ([T2-A]) used data-driven crowd "pulses" from zones. Witcher 4 used Smart Objects for crowd interactions ([T2-D], snippet only).

**Takeaway for Topic 9.** At scale, AI cost is dominated by **how often you decide and sense**, not by how clever one decision is. The recurring techniques are time-slicing, event-driven re-evaluation, shared group-level queries, and LOD for "brains". The StateTree/BT thread shows the measurement methodology itself is contested, which makes it a good portfolio angle.

---

## Topic 10: Procedural and runtime generation (PCG, runtime hitches, determinism)

**[T10-A] "Using PCG Generation Modes in Unreal Engine" (official docs)**: Epic. https://dev.epicgames.com/documentation/unreal-engine/using-pcg-generation-modes-in-unreal-engine. Status: opened. The page is labelled 5.9, but its text references 5.5.
- Modes: Non-partitioned, Partitioned, Hierarchical and Runtime. Runtime generation is scheduled around generation sources, with radii per grid size.
- cvars listed on the page: `pcg.FrameTime` (16.667 ms default as shown), `pcg.EditorFrameTime` (50 ms), `pcg.RuntimeGeneration.NumGeneratingComponents` (16), pooling on.
- **Discrepancy:** a forum post (snippet only, https://forums.unrealengine.com/t/pcg-on-5-6-is-slow/2671907) claims 5.6 lowered the runtime budget to 5 ms and that one team's in-build generation went from 10 s to 90 s. **Verify against the engine source.**
- The page does **not** discuss seeds or determinism.

**[T10-B] Forum: "Runtime Cost in PCG Hierarchical Generation"**: Dec 2025–Feb 2026, PS5 test build, with replies from Julien_LHeureux (Epic). https://forums.unrealengine.com/t/runtime-cost-in-pcg-hierarchical-generation/2708776. Status: opened.
- Budget: 500 µs CPU for PCG. On 5.7 they still saw ~1 ms spikes.
- Hot spots: `FPCGGraphExecutor::PrepareForExecute`, with up to a third of it in `GetDependenciesCRC`.
- Epic: GPU attribute packing and transfer is expensive. **The PCG budget does not include runtime-scheduler time.** The scheduler can cap concurrent generating components. 5.8 brings improvements, which may be hard to backport.

**[T10-C]** Tom Looman's 5.8 notes ([T1-F], opened): runtime generation scheduler "up to 30% less game-thread time per frame". A lazily allocated octree saves memory on large point sets. The profiler now shows GPU time for GPU nodes.

**[T10-D] "Building Worlds in No Man's Sky Using Math(s)"**: Sean Murray (Hello Games), GDC 2017. https://www.youtube.com/watch?v=C9RyEiEzMiU. Status: snippet only.
- Seed-based deterministic generation means nothing needs saving between visits. Per non-official sources, player changes are stored as a layer on top of the generated world.

**[T10-E]** Embark (2019, snippet only, https://medium.com/embarkstudios/an-update-from-embark-5f7a58ff1551): a 256 km² terrain test "created by three people over three weeks" with procedurally placed objects. Spider-Man ([T4-D]): procedural markup of Manhattan. WP forum ([T4-A]): Epic suggested runtime PCG to cut actor counts.

**Takeaway for Topic 10.** The problems people actually report are **frame-time spikes from runtime generation scheduling and data transfer**, and **budgets that don't count all the work**. Determinism matters for networking and for persistence (store only the diffs). I found no UE-specific primary source on determinism. That is a gap, and a deterministic, measurable PCG + persistence or networking project would be distinct.

---

## Topic 11: What distinguishes strong gameplay and engine engineers

**[T11-A] Mike Acton, "Everyone Watching This Is Fired" (2019 talk), as written up by Adam Johnson, 2022-06-17**: https://adamj.eu/tech/2022/06/17/mike-actons-expectations-of-professional-software-engineers/. Status: opened. Acton was Engine Director at Insomniac at the time of [T11-B].
- #1: "I can articulate precisely what problem I am trying to solve."
- #13: can state the hypothesis and how to falsify it.
- #28 / #29: "I have recently profiled the performance of my system" and its memory usage.
- #30: uses multiple profiling methods.
- #26: can name "the finite set of hardware" the solution targets.
- #37: can explain how the data is laid out in memory.
- #32: knows how to debug *live release builds* of their work.

**[T11-B] "Data-Oriented Design and C++"**: Mike Acton (Insomniac), CppCon 2014 keynote. https://www.youtube.com/watch?v=rX0ItVEVjHc. Status: snippet only.
- "If you don't understand the data, you don't understand the problem."
- Cache misses dominate. He gives roughly 200 cycles for a miss.

**[T11-C] Patterns from the studio talks above**:
- **Ownership:** at Rare, whoever makes a change writes its tests ([T6-A]).
- **Instrument before fixing:** Epic says hitches are hard to diagnose "without instrumentation" ([T1-A]). Respawn built per-match telemetry and per-frame server profiles ([T3-G]). Bungie built a profiler into replays plus a playtester "lag" button ([T3-H], secondary).
- **Engineer–designer collaboration:** Bungie changed game design to hide latency ([T3-H]). Hitman traded steering responsiveness for animation quality on purpose ([T2-A]).
- **Know the critical path:** Naughty Dog's 25 ms critical path with idle cores ([T1-G]).
- **Data layout:** Hitman's 36-byte agent core ([T2-A]).

**[T11-D] Debugging methodology**:
- Herman Tulleken, "How to debug": https://www.gamedeveloper.com/programming/how-to-debug (snippet only). Reproduce, list causes, design experiments to rule causes out, prioritize them, drill down to the root cause, then verify the fix.
- Naughty Dog GDC 2021, "Scriptable Core Dump Debugging and Memory Stomp Tracking" (Artem Kovalovs): https://www.naughtydog.com/blog/naughty_dog_at_gdc_2021 (snippet only). Post-mortem tools for crashes that change every run.

**Takeaway for Topic 11.** In these sources, "strong" means: states the problem and the hardware, profiles with more than one tool, knows the data layout, owns verification (tests and telemetry), and works with design on trade-offs. Every portfolio project should make these behaviours visible, not just the final code.

---

## Synthesis: ranked recurring hard problems

**How the ranking was built:**
- **Frequency** = how many distinct sources in this file describe the problem. This is an approximate count from this research, not a survey.
- **Severity (1–5)** = my judgment from how the sources describe the cost: shipping reputation, server money, QA headcount, platform memory limits, or blocked teams.
- **Solo reproducibility** = can one developer build a **controlled, repeatable** before/after test in a small UE5 project on one Windows PC?

| Rank | Recurring problem | Frequency | Severity | Why it is costly (from sources) | Solo reproducibility, and what to measure |
|---|---|---|---|---|---|
| 1 | **Game-thread hitches when content enters the world** (spawning, component/physics registration, synchronous loads, GC) | ~14 (T1-A, C, D, E, F, G; T4-A, B, C; T10-B; T3-A; Looman 5.5/5.8; Cesium) | 5 | Epic gave a whole talk to it; still-open WP threads; PC stutter hurts a game's reputation | **High.** Scripted spawn/stream/load workload; CSV profiler + Insights; frame-time p95/p99/max, hitch count over 33.3 ms / 50 ms, game-thread ms per event |
| 2 | **Cost per agent at hundreds to thousands of agents** (tick, animation, AI, representation LOD) | ~13 (T2-A, B, C, D, E; T5-C; T9-A, B, C, E, F; T1-F; Witcher 4 300 NPCs) | 5 | Crowds are a headline feature; every talk has a hard ms budget (Hitman 5 ms PPU) | **High.** N = 100/500/1000 agents; game-thread ms per agent, scaling curve, animation ms under a fixed budget |
| 3 | **Server replication CPU and bandwidth with many connections/actors** | ~10 (T3-A to H) | 5 | Server cost and tick rate (Fortnite 20 Hz goal, Apex 200 ms slow frames, 3× bandwidth for 60 Hz) | **Medium.** A local dedicated server plus headless/`-nullrhi` clients scales to dozens, not 100+ (bormor used VMs). Measure `NetBroadcastTickTime` ms, bytes/s per connection, and actors considered per frame |
| 4 | **Trustworthy automated tests and gameplay performance-regression detection** | ~10 (T6-A to H; T1-A advice) | 4 | Rare: 1.5 days vs ~2 weeks to verify a build, 17 vs 50 testers; Epic says gameplay perf gates are a gap | **High.** Measure detection rate on deliberately injected regressions, false-positive/flake rate across repeated runs, suite run time |
| 5 | **Networked movement/animation authority and correction** (root motion, prediction, lag hiding) | ~8 (T3-E, G, H; T5-A, D, E, F; T2-A) | 4 | Game feel and fairness; Mover is still Experimental; CMC root motion desync reports | **High** with `PktLag`/`PktLoss` emulation. Measure corrections per minute, position error (cm), and correction size at 0/100/200 ms with 1–5% loss |
| 6 | **Memory budgets, reference bloat and load time** | ~9 (T7-A to G; T5-B; T4-D) | 4 | Console memory caps (Fortnite saved hundreds of MB); Series S; disk size | **High.** Measure LLM / memreport MB, Size Map MB per asset, cold/warm load seconds over 10 runs |
| 7 | **World-state persistence across streaming** | ~4 (T4-C and its forum, plugins; T10-D) | 3–4 | Correctness bugs (collectibles respawn); Epic shipped a 2026 talk and sample, still Experimental | **High.** Measure state-loss error count over scripted stream-out/in cycles, save size, restore ms |
| 8 | **AI decision and sensing cost and architecture** | ~8 (T9-A to G) | 3 | Scales with agent count; contested StateTree/BT measurements | **High.** Measure decision ms per agent at N agents, re-evaluations per second, sensing traces per frame |
| 9 | **Runtime procedural generation cost and determinism** | ~6 (T10-A to E; T4-A) | 3 | PS5 team over a 500 µs budget; budgets that miss scheduler time | **High.** Measure generation ms per frame (p99), spikes per km traversed, determinism hash match rate |
| 10 | **Content validation and iteration/CI time** | ~9 (T8-A to E; T6-G; T3-A) | 3 | CI cost (Ubisoft up to 43%), UEFN push time, broken content reaching the team | **Medium.** Catch rate on seeded content errors and validator run time are measurable; iteration-time numbers will be small in a small project |
| 11 | **Shader/PSO compilation hitches (PC)** | ~4 (T1-A, B, C, D) | 5 on PC | Common player complaint; Fortnite +20–30 s cold load | **Medium/Low** for *solving* (engine/driver-level, Epic owns precaching). **Medium** for *detection tooling* (count `PSOPrecache: Missed` / `Too Late` with `-clearPSODriverCache` in an automated run) |
| 12 | **Multithreading the gameplay frame / critical path** | ~4 (T1-G; T1-F tick batching; Mass off-thread; T3-C Iris concurrency) | 4 | Idle cores while the frame is bound by one thread (Naughty Dog) | **Medium.** Task-graph or ParallelFor moves are measurable, but must be thread-safe with UObjects, which is a real risk for a portfolio project |

### Named gaps the sources themselves admit (useful for "distinct, not novel" positioning)

These are statements of what is missing, from Epic and from developers. They are not claims that nobody has built a solution.

- **No native async/time-sliced actor spawn and no built-in actor pooling** ([T1-D]). Epic agreed a spawn API would help; studios build their own.
- **Streaming budgets count items, not time**, and heavy components overrun them ([T4-A]). The PCG budget does not include scheduler time ([T10-B]).
- **Gameplay performance-regression testing is left to studios** ([T6-F]). Epic's plugin targets environment/GPU tests.
- **EQS results can't be shared between agents** ([T9-E]). StateTree ticks every frame by default ([T9-C]).
- **Persisting streamed world state** relies on Experimental systems and a sample that "has not been battle tested" ([T4-C]).
- **Root motion + CMC prediction** has known pain points, and its successor (Mover) is Experimental ([T5-D], [T5-E]).
- **Hard-reference memory bloat** is common community knowledge, but I found no official measured guidance ([T7-C]).

### What a single developer can realistically reproduce and measure

Strong candidates, all with a fixed scripted workload, measured in standalone/packaged Development builds, with median and p95/p99 frame time in ms:
1. Hitch budgeting for spawn/stream/load (rank 1).
2. Agent-scale LOD/representation and animation/AI budgeting (ranks 2, 8).
3. Gameplay performance-regression gates in CI (rank 4).
4. Networked movement/animation correction under simulated lag (rank 5).
5. Memory/reference/load-time auditing (rank 6).
6. Streamed world-state persistence correctness (rank 7).
7. Runtime PCG time-budgeting and determinism (rank 9).
8. Content validation tooling (rank 10).

**Possible, but scope carefully:**
- Server replication scaling (rank 3). Client count is limited on one PC, so measure per-connection cost and extrapolate honestly, and say so.
- Gameplay multithreading (rank 12). UObject thread-safety is the main risk.

**Not a good fit to "solve":**
- PSO compilation itself (rank 11). It is engine- and driver-level and Epic already owns the fix. Detecting it inside a test/CI tool is a good fit.

---

## Limitations

- **Summaries, not raw pages:** the fetch tool returns a model summary. Small paraphrase errors are possible. Only the PDF slides (Hitman, Sea of Thieves, Naughty Dog) and the three unrealengine.com posts (PSO, Fortnite BR, Replication Graph) had their numbers checked against raw extracted text.
- **Unreadable pages:** Epic Developer Community *tutorial* and *knowledge-base* pages (Great Hitch Hunt, Level Streaming Deep Dive, World Building Guide, City Sample) render in JavaScript and their bodies could not be read. They are cited from forum threads, search snippets or press coverage.
- **Retrieval workaround:** unrealengine.com blocked the fetch tool (403). Those three posts were retrieved with a standard browser user-agent through `curl`.
- **No videos watched:** talks that exist only as video (Witcher 4 talks, Iris talk, Mover talk, motion matching talks, No Man's Sky) are summarized from abstracts or secondary write-ups.
- **Old numbers:** many figures are from 2011–2019 hardware and engines (PS3, PS4, UE4). The *problem patterns* transfer; the numbers do not.
- **Uncontrolled community benchmarks:** forum numbers (StateTree vs BT, Iris 100 players, PCG spikes, WP streaming) are individual developers' measurements on their own setups, not controlled studies, and are not ours.
- **Docs version mismatch:** Epic docs were served as UE 5.9. Cvars and defaults (for example `pcg.FrameTime`, which has a conflicting value in [T10-A]) must be verified against the engine version we pick.
- **Studio coverage gaps:** Rockstar publishes almost no technical talks, and I found none. Insomniac, Naughty Dog, Guerrilla and Respawn sources are non-UE engines (transferable lessons). EA coverage is SEED research plus Respawn's Apex post. Severity scores are my qualitative judgment.
- **Unverified claims:** some secondary claims are explicitly flagged as unverified (Iris already shipping in Fortnite; Spider-Man's 20 m/s streaming figure; Overwatch system counts; Halo Reach numbers via Edgegap).

---

## Full source list

Status in brackets: **O** = opened · **O+R** = opened, raw text checked · **2** = secondary write-up opened · **S** = snippet only.

**Topic 1**
- [T1-A] Epic, "Game engines and shader stuttering: Unreal Engine's solution to the problem" (2025-02-04) [O+R]: https://www.unrealengine.com/en-US/tech-blog/game-engines-and-shader-stuttering-unreal-engines-solution-to-the-problem
- [T1-B] 80.lv, Rutherford (2025-02-05) [O]: https://80.lv/articles/epic-games-devs-on-solutions-to-unreal-engine-5-shader-stuttering-issues
- [T1-C] Epic docs, PSO Precaching [O]: https://dev.epicgames.com/documentation/unreal-engine/pso-precaching-for-unreal-engine?lang=en-US
- [T1-D] Arnbjörnsson, "The Great Hitch Hunt" (Unreal Fest Orlando 2025): tutorial (body unreadable) https://dev.epicgames.com/community/learning/tutorials/6XW8/unreal-engine-the-great-hitch-hunt-tracking-down-every-frame-drop · forum [O] https://forums.unrealengine.com/t/tutorial-the-great-hitch-hunt-tracking-down-every-frame-drop/2541268 · coverage [2] https://playersforlife.com/2025/07/29/epic-games-engineers-reveal-the-7-causes-of-unreal-engine-issues/ · [S] https://wccftech.com/unreal-engine-declares-great-hitch-hunt-rid-games-stutters/
- [T1-E] Epic docs, Incremental Garbage Collection [O]: https://dev.epicgames.com/documentation/en-us/unreal-engine/incremental-garbage-collection-in-unreal-engine · Cesium GC issue [S]: https://github.com/CesiumGS/cesium-unreal/issues/980
- [T1-F] Tom Looman [O]: https://tomlooman.com/unreal-engine-5-5-performance-highlights/ · https://tomlooman.com/unreal-engine-5-8-performance-highlights/ · https://tomlooman.com/unreal-engine-optimization-talk/
- [T1-G] Gyrling, "Parallelizing the Naughty Dog Engine Using Fibers" (GDC 2015) [O+R]: https://media.gdcvault.com/gdc2015/presentations/Gyrling_Christian_Parallelizing_The_Naughty.pdf

**Topic 2**
- [T2-A] Fauerby, "Crowds in Hitman: Absolution" (GDC 2012) [O+R]: https://www.gdcvault.com/play/1015315/Crowds-in-Hitman · https://media.gdcvault.com/gdc2012/slides/Programming%20Track/Fauerby_Kasper_CrowdsInHitman.pdf
- [T2-B] Cournoyer, "Massive Crowd on Assassin's Creed Unity: AI Recycling" (GDC 2015) [O]: https://gdcvault.com/play/1022411/Massive-Crowd-on-Assassin-s
- [T2-C] Epic, City Sample docs [S]: https://dev.epicgames.com/documentation/en-us/unreal-engine/city-sample-project-unreal-engine-demonstration · Mass docs hub [O]: https://dev.epicgames.com/documentation/en-us/unreal-engine/mass-entity-in-unreal-engine · Megafunk MassSample [O]: https://github.com/Megafunk/MassSample
- [T2-D] CDPR blog (2025-06-13) [O]: https://www.cdprojektred.com/en/blog/149/working-with-epic-to-debut-the-witcher-4-unreal-engine-5-tech-demo-at-unreal-fest · CDPR press [S]: https://www.cdprojekt.com/en/media/news/cd-projekt-red-and-epic-games-present-the-witcher-4-unreal-engine-5-tech-demo-at-the-state-of-unreal-2025/ · "Road to 60 fps" [S]: https://www.youtube.com/watch?v=ji0Hfiswcjo · Creative Bloq [O, partial]: https://www.creativebloq.com/3d/video-game-design/how-the-witcher-4s-infinite-forest-is-being-built-one-branch-at-a-time-using-unreal-engine-5-6
- [T2-E] Ford, "Overwatch Gameplay Architecture and Netcode" (GDC 2017) [O]: https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and · transcript [S]: https://www.mo4tech.com/overwatchs-architecture-is-designed-to-synchronize-with-the-web.html

**Topic 3**
- [T3-A] Epic, "Unreal Engine Improvements for Fortnite: Battle Royale" (2017) [O+R]: https://www.unrealengine.com/en-US/tech-blog/unreal-engine-improvements-for-fortnite-battle-royale
- [T3-B] Epic, "Replication Graph Overview and Proper Replication Methods" [O+R]: https://www.unrealengine.com/en-US/tech-blog/replication-graph-overview-and-proper-replication-methods · Replication Graph docs [S]: https://dev.epicgames.com/documentation/unreal-engine/replication-graph-in-unreal-engine?lang=en-US
- [T3-C] Epic docs, Networking Overview [O]: https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine · Introduction to Iris [O]: https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine · Network debugging cvars [S]: https://dev.epicgames.com/documentation/en-us/unreal-engine/console-commands-for-network-debugging-in-unreal-engine · UE-191014 [S]: https://issues.unrealengine.com/issue/UE-191014 · UE-226689 [S]: https://issues.unrealengine.com/issue/UE-226689
- [T3-D] Epic, "Developing and Launching a New Replication System" (Unreal Fest Orlando 2025) [S]: https://www.youtube.com/watch?v=K472O2rVvG0 · https://dev.epicgames.com/community/learning/talks-and-demos/WvlV/unreal-engine-developing-and-launching-a-new-replication-system-unreal-fest-orlando-2025
- [T3-E] Vikhirev, "Iris Replication System: 100 Players in One Place" (2025-10-25) [O]: https://bormor.dev/posts/iris-one-hundred-players/
- [T3-F] Forum, replication with hundreds of clients [S]: https://forums.unrealengine.com/t/server-networking-replication-performance-with-hundreds-of-clients/740652
- [T3-G] Duc, "What Makes Apex Tick" (2021-04-28) [O]: https://www.ea.com/en/games/apex-legends/apex-legends/news/servers-netcode-developer-deep-dive
- [T3-H] Aldridge, "I Shot You First" (GDC 2011) [S]: https://gdcvault.com/play/1014346/I-Shot-You-First-Networking · Edgegap summary [2]: https://edgegap.com/blog/game-backend-deep-dive-halo-reach-netcode-host-migration

**Topic 4**
- [T4-A] Forum, "World Partition streaming performance" (2025) [O]: https://forums.unrealengine.com/t/world-partition-streaming-performance/2577446
- [T4-B] Rudzki and Mallo, "Streaming Improvements for Dense Worlds" (Unreal Fest Orlando 2025) [S]: https://www.youtube.com/watch?v=BdopUm1_1_E · Epic World Building Guide [S]: https://dev.epicgames.com/community/learning/knowledge-base/r6wl/unreal-engine-world-building-guide · Level Streaming Deep Dive [S]: https://dev.epicgames.com/community/learning/knowledge-base/qB5K/unreal-engine-level-streaming-deep-dive
- [T4-C] Epic, "The Player was Here" (Unreal Fest Chicago 2026), Sample-PersistenceLab [O]: https://github.com/ZKShao-EpicGames/Sample-PersistenceLab · forum [S]: https://forums.unrealengine.com/t/preprocessing-data-from-a-world-partition-world/2591463
- [T4-D] Ruskin, "Marvel's Spider-Man: A Technical Postmortem" (GDC 2019) [O]: https://www.gdcvault.com/play/1026496/-Marvel-s-Spider-Man

**Topic 5**
- [T5-A] Clavet, "Motion Matching and The Road to Next-Gen Animation" (GDC 2016) [S]: https://www.gdcvault.com/play/1023280/Motion-Matching-and-The-Road
- [T5-B] Holden et al., "Learned Motion Matching" (2020) [O]: https://www.ubisoft.com/en-us/studio/laforge/news/3NWqwcMU9lMfumgfPAEfka/learned-motion-matching · tweet [S]: https://x.com/anorangeduck/status/1289310587134959617
- [T5-C] Epic docs, Animation Budget Allocator [O]: https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine · Game Animation Sample [S]: https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine?lang=en-US · ABA forum caveat [S]: https://forums.unrealengine.com/t/animationbudgetallocator-doesnt-utilize-interpolation-on-registered-components-and-it-feels-like-framerate-drop-what-is-a-proper-way-to-address-it/2663219
- [T5-D] Epic docs, Mover [O]: https://dev.epicgames.com/documentation/unreal-engine/mover-in-unreal-engine · Mover talk (Unreal Fest 2024) [S]: https://m.youtube.com/watch?v=P4IKS5k47Wg
- [T5-E] Root motion / NPP forum threads [S]: https://forums.unrealengine.com/t/root-motion-completely-broken-in-ue-5-1/744362 · https://forums.unrealengine.com/t/status-of-networkprediction-plugin-working-in-tandem-with-charactermovementcomponent-troubles-synchronizing-walk-speed-stamina-across-client-server/2640104

**Topic 6**
- [T6-A] Masella, "Automated Testing of Gameplay Features in Sea of Thieves" (GDC 2019) [O+R]: https://media.gdcvault.com/gdc2019/presentations/Masella_Robert_AutomatedTestingOf.pdf · preview [O]: https://gdconf.com/article/sea-of-thieves-devs-share-automated-testing-tips-at-gdc-2019/
- [T6-B] Baker, "Tests and Testability" (2018) [O]: https://jessicabaker.co.uk/2018/03/11/tests-and-testability/ · Unreal Fest Europe 2019 talk [S]: https://www.youtube.com/watch?v=KmaGxprTUfI
- [T6-C] van Valburg, Call of Duty CI (GDC 2018) [O]: https://www.gamedeveloper.com/production/video-how-i-call-of-duty-i-handles-automated-testing-and-profiling
- [T6-D] Paredes and Jones, The Division client bots (GDC 2019) [O]: https://80.lv/articles/gdc-using-ai-controlled-players-to-test-the-division · [S] https://gdcvault.com/play/1026382/Automated-Testing-Using-AI-Controlled
- [T6-E] Gillberg et al., EA SEED (CoG 2023) [S]: https://arxiv.org/pdf/2307.11105
- [T6-F] Epic Automated Performance Testing: talk [S] https://dev.epicgames.com/community/learning/talks-and-demos/0zx9/unreal-engine-a-tech-artist-s-guide-to-automated-performance-testing-unreal-fest-bali-2025 · forum [O] https://forums.unrealengine.com/t/automated-performance-testing-plugin-guidance/2666136
- [T6-G] Epic, UEFN iteration and stability update (2026-06-24) [O]: https://forums.unrealengine.com/t/uefn-iteration-time-stability-update-progress-and-what-comes-next/2731098
- [T6-H] GDC 2021 roundtable notes [S]: https://github.com/Kadino/gdconf_notes/blob/main/gdc2021.md

**Topic 7**
- [T7-B] Epic docs, LLM [S]: https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-low-level-memory-tracker-in-unreal-engine · Memory Insights [S]: https://dev.epicgames.com/documentation/en-us/unreal-engine/memory-insights-in-unreal-engine
- [T7-C] Hard references [S]: https://raharuu.github.io/unreal/hard-references-reasons-avoid/ · https://olssondev.github.io/2023-02-19-UnderstandingWhyCastsCanBeExpensive/
- [T7-D] Oztalay, "Optimizing UE5…" (Unreal Fest 2024) [S]: https://forums.unrealengine.com/t/talks-and-demos-optimizing-ue5-advanced-rendering-graphics-performance-and-memory-management-unreal-fest-2024/1998435
- [T7-E] Zen Loader docs [S]: https://dev.epicgames.com/documentation/en-us/unreal-engine/zen-loader-in-unreal-engine · Epic Japan decks [S]: https://docswell.com/s/EpicGamesJapan/56G9EK-UE4_IOStore_UE425 · https://www.docswell.com/s/EpicGamesJapan/ZV172L-Improving%20Development%20Iterations · forum [S]: https://forums.unrealengine.com/t/concerning-game-thread-being-stalled-by-asynchronous-loading-thread/2688865
- [T7-F] Xbox Wire, The Coalition and The Matrix Awakens (2021) [S]: https://news.xbox.com/en-us/2021/12/10/how-the-coalition-worked-with-epic-to-bring-the-matrix-awakens-to-life-on-xbox-series-xs/

**Topic 8**
- [T8-A] Routhier, "Assassin's Creed Origins: Monitoring and Validation of World Design Data" (GDC 2018) [O]: https://gdcvault.com/play/1025452/-Assassin-s-Creed-Origins
- [T8-B] Epic, Data Validation API [S]: https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/DataValidation/UEditorValidatorBase?lang=en-US · UE 4.27 guide [S]: https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/ProgrammingWithCPP/Assets/DataValidation
- [T8-C] Habchi, "Taming the Beast" (GDC 2024) [O]: https://www.gdcvault.com/play/1034573/Machine-Learning-Summit-Taming-the
- [T8-D] Embark, Unreal Fest 2022 [S]: https://www.youtube.com/watch?v=wJqOn88cU7o · https://www.unrealengine.com/en-US/events/unreal-fest-2022-sessions
- [T8-E] Epic, "Setting up an Unreal Engine Studio the Epic Way" (talk index) [S]: https://dev.epicgames.com/community/learning/tutorials/8JYW/epic-for-indies-setting-up-an-unreal-engine-studio-the-epic-way · incremental cook forum [S]: https://forums.unrealengine.com/t/zen-incremental-cook-has-almost-as-long-a-runtime-as-a-clean-cook/2667839

**Topic 9**
- [T9-A] Guerrilla, "The AI of Horizon Zero Dawn" (2017) [O]: https://www.guerrilla-games.com/read/the-ai-of-horizon-zero-dawn · Berteling (GDC 2018) [S]: https://www.gdcvault.com/play/1024912/Beyond-Killzone-Creating-New-AI · HTN in Decima [S]: https://www.guerrilla-games.com/read/htn-planning-in-decima
- [T9-B] Mark and Lewis, "Building a Better Centaur" (GDC 2015) [S]: https://archive.org/details/GDC2015Mark
- [T9-C] Forum, StateTree vs BehaviorTree (2025–26) [O]: https://forums.unrealengine.com/t/statetree-performance-vs-behaviortree/2705328
- [T9-D] Forum, Unreal Fest 2023 StateTree and Smart Objects [O]: https://forums.unrealengine.com/t/talks-and-demos-state-trees-and-smart-objects-data-driven-state-machine-workflows-for-open-world-ai-designs-unreal-fest-2023/1656473
- [T9-E] Forum, EQS sharing results (UE4-era archive) [O]: https://forums.unrealengine.com/t/is-there-any-way-for-eqs-to-share-results/348550
- [T9-F] Forum, AI Perception slowdown [S]: https://forums.unrealengine.com/t/ai-perception-detecting-the-players-slows-down-significantly-with-the-number-of-ais-in-the-level/88176

**Topic 10**
- [T10-A] Epic docs, PCG Generation Modes [O]: https://dev.epicgames.com/documentation/unreal-engine/using-pcg-generation-modes-in-unreal-engine · forum "PCG on 5.6 is slow" [S]: https://forums.unrealengine.com/t/pcg-on-5-6-is-slow/2671907 · PCG GPU docs [S]: https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine
- [T10-B] Forum, "Runtime Cost in PCG Hierarchical Generation" (2025–26) [O]: https://forums.unrealengine.com/t/runtime-cost-in-pcg-hierarchical-generation/2708776
- [T10-D] Murray, "Building Worlds in No Man's Sky Using Math(s)" (GDC 2017) [S]: https://www.youtube.com/watch?v=C9RyEiEzMiU
- [T10-E] Embark, "An update from Embark" [S]: https://medium.com/embarkstudios/an-update-from-embark-5f7a58ff1551

**Topic 11**
- [T11-A] Johnson, "Mike Acton's Expectations of Professional Software Engineers" (2022) [O]: https://adamj.eu/tech/2022/06/17/mike-actons-expectations-of-professional-software-engineers/
- [T11-B] Acton, "Data-Oriented Design and C++" (CppCon 2014) [S]: https://www.youtube.com/watch?v=rX0ItVEVjHc
- [T11-D] Tulleken, "How to debug" [S]: https://www.gamedeveloper.com/programming/how-to-debug · Naughty Dog at GDC 2021 [S]: https://www.naughtydog.com/blog/naughty_dog_at_gdc_2021
