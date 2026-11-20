# 01: Job Market Research. What Studios Ask of Gameplay and UE C++ Programmers

**Research date:** 2026-11-19
**Purpose:** Step 0 input for the 10-project roadmap. The goal is to find out which skills studios actually ask for, so that every portfolio project shows a skill that is in demand.

---

## 1. Method

**Sources, in order of preference**

1. **Official applicant-tracking systems (ATS), read through their public job APIs.** Several careers pages (epicgames.com, Microsoft Careers, EA's gr8people, showbizjobs) blocked automated fetches with HTTP 403, or rendered only with JavaScript. The full, verbatim posting text was still available from the same ATS:
   - Greenhouse public API (`boards-api.greenhouse.io/v1/boards/<board>/jobs/<id>`): Epic Games, Riot Games, Insomniac, Crystal Dynamics, 2K (Cloud Chamber, 31st Union), Hangar 13.
   - SmartRecruiters public API (`api.smartrecruiters.com/v1/companies/<co>/postings/<id>`): CD PROJEKT RED, Ubisoft, People Can Fly.
   - Workday public JSON (`warnerbros.wd5.myworkdayjobs.com/wday/cxs/...`): Rocksteady, WB Games Montréal.
   - EA's own careers site (`jobs.ea.com`, server-rendered HTML): Respawn, Motive, EA Vancouver, EA Madrid.
   - Teamtailor careers page: Embark Studios.
2. **Aggregator mirrors** (gamejobs.co, freehire.me, builtinla.com / builtin.com, jobright.ai, getro / anitab job boards). These were used for postings that are closed, or whose official page would not open. A summarising fetch tool read these pages. Quotes from them are close to verbatim but may be lightly trimmed. Each one is labelled **(aggregator)**.
3. **Search-result snippets only.** Some postings could not be opened at all. They appear in §3 (snippet-only), labelled **(search snippet only)**, and are **not** counted in the tally.

**How the tally was built.** I read each full-text posting and tagged it by hand against 38 skill categories (the tagging matrix is in §9). A tag means the skill is named as required, as a responsibility, or as a nice-to-have. The tally therefore measures **how often studios mention a skill**, not how often they make it a hard requirement. Postings that repeat the same text in several locations count once (for example, Epic's Lead Gameplay Programmer is listed 4 times, Epic's Senior AI Programmer 3 times, 2K's Gameplay Engineer twice).

**Scope.** I read the full text of **57 distinct postings** from **16 publisher or studio groups** (about 22 individual studios):

| Level | Count | Definition used |
|---|---|---|
| Intern | 4 | Internship or co-op |
| Mid | 11 | Untitled, "Gameplay Programmer/Engineer", 2–5 years |
| Senior | 32 | "Senior" / "Advanced", typically 5+ years |
| Lead / Staff / Expert | 10 | People lead, tech lead, Staff, Expert |

Engine context: **32 postings are for Unreal Engine projects** and 25 are for proprietary or unnamed engines (RAGE, Decima, Anvil, Snowdrop, Frostbite, Respawn's engine, Insomniac's engine, Naughty Dog's engine, League of Legends).

Status key: **Open** means listed on the ATS on 2026-11-19. **Closed** means the page says closed, removed or expired. **Possibly closed** means an aggregator copy with no confirmation.

---

## 2. Postings by studio (full text read)

### 2.1 Epic Games (Unreal Engine). Greenhouse API

| # | Role (level) | Date / status | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|---|
| E1 | [Lead Gameplay Programmer, Rocket League](https://epicgames.com/careers/jobs/6006101004?gh_jid=6006101004) (Lead). Same text at [Montréal](https://epicgames.com/careers/jobs/6021022004?gh_jid=6021022004), [Porto Alegre](https://epicgames.com/careers/jobs/6206043004?gh_jid=6206043004) and Remote | First published 2026-06-10; **open** | Gameplay physics (simulation, collision, vehicle); network replication; shipped live-service/AAA on UE; team leadership; strong C++ | none listed | "Experience with network replication systems (client-server prediction, state synchronization, lag compensation)"; "Experience using AI coding assistants …" |
| E2 | [Senior AI Programmer, UE NPC team](https://epicgames.com/careers/jobs/6242337004?gh_jid=6242337004) (Senior). Also [Montréal](https://epicgames.com/careers/jobs/6261103004?gh_jid=6261103004) | First published 2026-11-12; **open** | Shipped AAA AI systems ("behavior trees, perception, pathfinding", Montréal text); UE system design; strong C++ | LLM / generative-model integration; AI coding assistants; live service | "Optimize performance, memory usage, and scalability of NPC systems across UE" |
| E3 | [Senior Tools Programmer, UI & AI Workflows](https://epicgames.com/careers/jobs/6226621004?gh_jid=6226621004) (Senior) | First published 2026-10-19; **open** | Tools programming; UI frameworks and WYSIWYG editors; excellent C++ in a large codebase; some agentic-AI exposure | UE; Slate/UMG; shipped title | "Profile and resolve performance and memory issues in UI systems" |
| E4 | [Engine Programmer Intern, Fortnite Online](https://epicgames.com/careers/jobs/6202659004?gh_jid=6202659004) (Intern) | First published 2026-09-21; **open** | OOP and C++; networking, REST, resilient back-end use; CS degree | UE/UEFN; portfolio with code; use of LLMs for code and tooling | "If it's an online game feature, we touch it." |
| E5 | Senior Gameplay Programmer, Cary NC (Senior) **(aggregator, paraphrased)**: [jobright](https://jobright.ai/jobs/info/6a9b20159c24314c35f97de4) | About Oct 2026; **closed** | UE mandatory; exceptional C++; GAS, EQS, Enhanced Input, animation; online multiplayer; multi-platform | none listed | (search snippet) "Gameplay Ability Systems, EQS, Enhanced Input, Animation" |

### 2.2 CD PROJEKT RED (Cyberpunk 2 on Unreal Engine 5). SmartRecruiters API

| # | Role (level) | Date / status | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|---|
| C1 | [Senior Gameplay Programmer (Camera)](https://jobs.smartrecruiters.com/CDPROJEKTRED/744000162381314-senior-gameplay-programmer-camera-) (Senior) | Released 2026-11-18; window closes 2027-05-01; **open** | 5+ yrs; camera features; strong C++ and UE; engine architecture and algorithms | Shipped UE game; current consoles | "build new camera mechanics and features to generate compelling gameplay and cinematic moments" |
| C2 | [Senior Gameplay Programmer (vehicles)](https://jobs.smartrecruiters.com/CDPROJEKTRED/744000161840809-senior-gameplay-programmer) (Senior) | Released 2026-11-16; closes 2027-02-01; **open** | 5 yrs; vehicle pathfinding and AI; vehicle physics and drive models; 3D math; systems-level profiling | ECS or Unreal Mass; 3D navigation; AAA shipped | "vehicle pathfinding and AI, managing vehicle physics and drive models" |
| C3 | [Senior Gameplay Programmer, AI](https://jobs.smartrecruiters.com/CDPROJEKTRED/744000160657389-senior-gameplay-programmer-ai) (Senior) | Released 2026-11-09; closes 2027-05-01; **open** | 5+ yrs; pathfinding, navigation, avoidance; multithreaded systems; systems-level profiling; 3D math; consoles | Mass AI in UE5; ECS/Mass; 3D nav | "core navigation, pathfinding, and avoidance systems, with a focus on scalability and open world performance" |
| C4 | [Expert Gameplay Programmer, AI](https://jobs.smartrecruiters.com/CDPROJEKTRED/744000160657129-expert-gameplay-programmer-ai) (Expert) | Released 2026-11-09; closes 2027-05-01; **open** | 7+ yrs; spawning and systemic open world; procedural content generation; multithreading; profiling; monitoring performance and memory; mentoring and code reviews | Mass AI in UE5; ECS/Mass; AAA shipped | "Develop debug tools, and designer tools needed for non-deterministic systemic gameplay." |
| C5 | [Lead Quest Programmer](https://jobs.smartrecruiters.com/CDPROJEKTRED/744000157835480-lead-quest-programmer-) (Lead) | Released 2026-10-26; closes 2026-12-15; **open** | 7 yrs AAA; leadership; quest/narrative tools, node-based editors; shared-tech collaboration; C++ | UE architecture; multiplayer sync of quest state; UE editor scripting | "Ensure quest tools run efficiently without sacrificing runtime performance" |

### 2.3 Ubisoft (Anvil, Snowdrop, Unreal, and unnamed engines). SmartRecruiters API, plus two aggregator pages

| # | Role (level), studio, engine | Date / status | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|---|
| U1 | [Senior Gameplay Programmer](https://jobs.smartrecruiters.com/Ubisoft2/744000162345149-senior-gameplay-programmer) (Senior), Barcelona, cross-platform mobile/PC live game | Released 2026-11-18; **open** | C++; shipping on iOS/Android/macOS/Windows; client-server; profiling (memory, CPU, crashes); data structures and 3D math; TDDs; code review | Rendering; netcode; AI; CI/CD and automated testing; anti-cheat | "Participate in crash triage and live-service bug fixing" |
| U2 | [Gameplay Programmer](https://jobs.smartrecruiters.com/Ubisoft2/744000162340980-gameplay-programmer) (Mid), Barcelona | Released 2026-11-18; **open** | 2+ yrs; C++; data structures, algorithms, 3D math; client-server | Netcode; AI; profiling across platforms | "Assess feature feasibility by evaluating its cost and benefit" |
| U3 | [Gameplay Programmer](https://jobs.smartrecruiters.com/Ubisoft2/744000161749429-gameplay-programmer-) (Mid/junior), Sofia, AAA | Released 2026-11-16; **open** | C++ and OOP; memory management; templates and patterns; linear algebra; testing own features | Engine architecture; Perforce; scripting | "Basic understanding of templates, design patterns, and anti-patterns" |
| U4 | [AI Programmer, R6 Siege bots](https://jobs.smartrecruiters.com/Ubisoft2/744000151407298-ai-programmer-r6-siege-) (Mid), Montréal | Released 2026-09-23; **open** | 5 yrs C++; console; AI behaviours; tools for debugging and validating AI | none listed | "Use gameplay data, test results, and player feedback to iterate on AI features" |
| U5 | [Online Game Programmer, Rainbow Six](https://jobs.smartrecruiters.com/Ubisoft2/744000159397829-online-game-programmer-rainbow-six-) (Mid), Saguenay | Released 2026-11-03; **open** | C++ online systems; shipped or live game; debugging client and server | Ubisoft Online Services | "Optimize code and logic, identify bottlenecks, and suggest workflow improvements." |
| U6 | [Animation Programmer](https://jobs.smartrecruiters.com/Ubisoft2/744000149602139-animation-programmer) (Mid), Montréal, **UE multiplayer team** | Released 2026-09-15; **open** | C++ and C#; 3D math; state machines, ragdoll, IK, facial; animation tools | none listed | "Write clear and structured code while considering performance, memory usage, and compatibility." |
| U7 | [Gameplay Technical Lead, For Honor](https://jobs.smartrecruiters.com/Ubisoft2/744000155522049-gameplay-technical-lead-for-honor-) (Lead), Anvil | Released 2026-10-14; **open** | About 7 yrs plus 2 yrs lead; C++; technical vision and roadmap; code review; mentoring | Anvil; C#; live game | "you don’t just patch problems, you solve them so that they never happen again" |
| U8 | [Senior Engine Programmer, Snowdrop](https://jobs.smartrecruiters.com/Ubisoft2/744000162505597-senior-engine-programmer-snowdrop-) (Senior), Malmö | Released 2026-11-19; **open** | C++; memory, performance, load times, multithreading, debugging; review and mentoring | Snowdrop | "turning project-specific solutions into reusable systems that can benefit multiple teams" |
| U9 | [Senior C++ Software Engineer, Simulation & Tools](https://jobs.smartrecruiters.com/Ubisoft2/744000157133139-senior-c-software-engineer-simulation-tools) (Senior), Bucharest, *Motion* middleware | Released 2026-10-22; **open** | C++; algorithms and OOP; 3D math and computational geometry; API design; documentation | Game physics; ML for cloth/hair; C#/WPF; consoles | "keeping performance, maintenance, and compatibility requirements in mind" |
| U10 | [C++ Game Developer](https://jobs.smartrecruiters.com/Ubisoft2/744000161237793-c-game-developer) (Mid), Kyiv | Released 2026-11-12; **open** | Advanced C++; CPU and memory optimisation; multithreading; analysing complex code | Multi-platform; game dev | "Write solid code with focus on performances and memory usage" |
| U11 | Senior Gameplay Programmer, *Programmeur·euse Jouabilité Sénior* (Senior), Montréal, **UE5** **(aggregator, translated)**: [gamejobs.co](https://gamejobs.co/Programmeur-euse-Jouabilit%C3%A9-S%C3%A9nior-at-Ubisoft) | About Jul 2026; **possibly closed** | 7+ yrs C++; 3Cs; UE4/5; C++ and Blueprints; rendering/threading/networking awareness; debugging and profiling; mentoring | Multiplayer; FPS; console/mobile; **GAS and CMC** | (paraphrase) develop features "in UE5 with C++ and Blueprints" |
| U12 | Senior AI Programmer (Senior), Montréal, **UE** **(aggregator)**: [gamejobs.co](https://gamejobs.co/Senior-AI-Programmer-at-Ubisoft-4647) | About Jul 2026; **possibly closed** | About 5 yrs AI; C++; consoles; own AI end to end; server-authoritative AI | Shipped game | "CPU, AI budgets, behavioral LODs, agent counts, network impact"; "challenge over‐engineered solutions" |

### 2.4 Electronic Arts (Respawn, Motive, EA Vancouver, EA Madrid). jobs.ea.com, all **open** on 2026-11-19 (posting dates not shown)

| # | Role (level), studio, engine | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|
| EA1 | [Gameplay Engineer, Apex Legends](https://jobs.ea.com/en_US/careers/JobDetail/Gameplay-Engineer-Apex-Legends/216048) (Mid/Senior), **Respawn**, proprietary | 5+ yrs gameplay; shipped multiplayer; live service; large codebase; advanced C++/C | none listed | "Experience working with a large codebase, managing and improving performance and budget." |
| EA2 | [Senior Software Engineer, Generalist, Apex](https://jobs.ea.com/en_US/careers/JobDetail/Senior-Software-Engineer-Generalist-Apex-Legends/216015) (Senior), **Respawn** | C/C++; Python; profiling and optimising; debugging production software; telemetry and crash data; automated content validation | Feature flags and staged rollouts; engine/game-script boundary; asset pipelines | "quantify game performance, prevent regressions, and improve launch stability" |
| EA3 | [Lead Animation Engineer, Apex](https://jobs.ea.com/en_US/careers/JobDetail/Lead-Animation-Engineer-Apex-Legends/213629) (Lead), **Respawn** | Deep C++; blending, layering, procedural animation, IK; performance-sensitive systems; leadership | Motion matching; retargeting; animation compression; physics integration | "Comfortable developing foundational technology in a proprietary engine" |
| EA4 | [Tools Developer, Apex](https://jobs.ea.com/en_US/careers/JobDetail/Tools-Developer-Apex-Legends/216105) (Mid/Senior), **Respawn** | C++ and 3D math; 5+ yrs editors, systems and engines; content pipelines, data validation, baking | none listed (rest of the list was not captured) | "Use telemetry and user feedback to prioritize development work and measure the impact" |
| EA5 | [Senior Gameplay Software Engineer, NHL](https://jobs.ea.com/en_US/careers/JobDetail/Senior-Gameplay-Software-Engineer-NHL/215810) (Senior), EA Vancouver | 5+ yrs; C++; 3D math, quaternions, physics; multithreading; memory; profiling tools; animation state machines; spatial AI; AI tools | Current consoles; cloth and procedural animation; CI | "taking ownership of memory management, multi-threading, and frame-rate optimization" |
| EA6 | [Senior Software Engineer, Low-Level Systems](https://jobs.ea.com/en_US/careers/JobDetail/Senior-Software-Engineer-Low-Level-Systems-C/215973) (Senior), EA Madrid, **Unreal Engine**, American Football | 10+ yrs C++; 5+ yrs UE architecture, memory and multithreading; client/server debugging and profiling in UE | none listed | "define and manage per-system frame budgets, establish performance metrics" |
| EA7 | [Gameplay Software Engineer, FC](https://jobs.ea.com/en_US/careers/JobDetail/Gameplay-Software-Engineer-FC/215783) (Mid), proprietary engine | C++ and OOD; AI, character control or multiplayer; multithreading and synchronisation; debugging tools | none listed | "Understand performance implication of Gameplay systems & algorithms and how to improve." |
| EA8 | [Senior AI Software Developer, Iron Man](https://jobs.ea.com/en_US/careers/JobDetail/Senior-AI-Software-Developer/216047) (Senior), **Motive**, **UE5** | 10+ yrs C/C++; shipped AAA AI; ownership | UE4/5 strongly preferred; leadership | "defining the architecture of foundational AI technology and associated performance targets" |
| EA9 | [Senior Core Software Engineer, Battlefield](https://jobs.ea.com/en_US/careers/JobDetail/Software-Developer-Core-Systems-Battlefield/216643) (Senior), **Motive**, Frostbite | C++; CPU and memory optimisation; low-level code; hard bugs | Engines and content pipelines | "optimizing for CPU performance and memory usage" |

### 2.5 Riot Games (VALORANT and new projects on Unreal; League on a proprietary engine). Greenhouse API, all **open**

| # | Role (level) | First published | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|---|
| R1 | [Staff SWE, Engine Performance, Unreal Ecosystem](https://www.riotgames.com/en/work-with-us/job/8275281?gh_jid=8275281) (Staff) | 2026-10-14 | 5+ yrs; C++; debugging, profiling and optimising complex projects; Unreal Insights/PIX; large codebases; multi-platform shipping | UE5; online games; UE rendering | "Experience with profiling tools like Unreal Insights or Pix"; "improving cook and build times, editor load times" |
| R2 | [Staff SWE, Tools, Unreal Ecosystem](https://www.riotgames.com/en/work-with-us/job/8384686?gh_jid=8384686) (Staff) | 2026-11-18 | 5+ yrs; C++ and C#; profiling; design patterns; large codebases | UE; online; mobile; console | "buy/build decisions for major system components" |
| R3 | [Staff SWE, Unreal Ecosystem (Sydney)](https://www.riotgames.com/en/work-with-us/job/7981543?gh_jid=7981543) (Staff) | 2026-06-23 | 6+ yrs; C++; data structures, algorithms, concurrency; shared libraries, plugins and tools for many teams; code review; automated testing; documentation | UE C++ plugins, editor tooling, build systems; CI/CD | "using profiling and debugging to identify root causes" |
| R4 | [Staff SWE, Gameplay, unpublished R&D (Shanghai)](https://www.riotgames.com/en/work-with-us/job/8279634?gh_jid=8279634) (Staff) | 2026-10-15 | 6+ yrs; 4+ yrs UE; C++; game and software design patterns; gameplay architecture | Persistence; leadership; agile | "Continuously iterate on solutions to ensure they are robust, scalable, and performant." |
| R5 | [Staff SWE, Gameplay, unpublished R&D (Guangzhou)](https://www.riotgames.com/en/work-with-us/job/7922898?gh_jid=7922898) (Staff), *in Chinese, translated by me* | 2026-05-14 | 6–8+ yrs; expert C++; UE Gameplay Framework; C++ vs Blueprint/script trade-offs; performance; AI-agent workflows | Tech-lead experience; deeper UE modules | "能在 C++ 与 Blueprint / Script 间做工程化取舍" (*make engineering trade-offs between C++ and Blueprint/Script*) |
| R6 | [Senior SWE, Gameplay (Bots), League](https://www.riotgames.com/en/work-with-us/job/8316790?gh_jid=8316790) (Senior) | 2026-10-27 | 5+ yrs; 3+ yrs C++; gameplay or bots engineering; ML inference interest; Mandarin | Unreal/Unity; backend; data-driven decisions | "traditional game AI, machine learning tech, and proprietary engine technologies" |
| R7 | [Senior SWE, VALORANT Personalization](https://www.riotgames.com/en/work-with-us/job/8287413?gh_jid=8287413) (Senior) | 2026-10-15 | 4+ yrs; 3+ yrs C++ and/or Unreal; cross-team delivery | UE gameplay systems; high-load live services | "connecting Unreal Engine client experiences with the live services behind them" |

### 2.6 Warner Bros. Games (Unreal Engine 5). Workday JSON, both **open** (start date 2026-10-16)

| # | Role (level), studio | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|
| WB1 | [Senior Gameplay Programmer](https://warnerbros.wd5.myworkdayjobs.com/global/job/London-Rocksteady-Studios-Limited/Senior-Gameplay-Programmer_R000107418) (Senior), **Rocksteady** | C++ gameplay (AI, navigation, combat); a modern platform; shipped commercial title; game-design skill shown in a portfolio; UE5 | Familiarity with Rocksteady titles | "coordinating combat-related and navigation related NPC behaviours" |
| WB2 | [Advanced Gameplay Programmer](https://warnerbros.wd5.myworkdayjobs.com/global/job/Montreal-888-boul-De-Maisonneuve-Est/Programmeureuse-avance--jouabilit---Advanced-Gameplay-Programmer_R000109227-1) (Senior/Advanced), **WB Games Montréal** | 8+ yrs C++; 2+ AAA titles; gameplay and AI architecture; profiling with CPU/memory budgets; 3D math; **UE5 gameplay framework, State Trees, EQS, navigation**; TDDs | none listed | "profiling, CPU/memory budgets, time-slicing or behavioral LOD where relevant" |

### 2.7 2K / Take-Two (Unreal Engine 5). Greenhouse API, all **open**

| # | Role (level), studio | First published | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|---|
| K1 | [Gameplay Engineer](https://job-boards.greenhouse.io/2k/jobs/7655940003) (Mid), **Cloud Chamber** (BioShock). Same text [Montréal](https://job-boards.greenhouse.io/2k/jobs/7655939003) | 2026-03-10 | 3+ yrs; C/C++ and OO; 3D math; 3Cs; testing; documentation | Current consoles; current UE | "Commitment to code quality, documentation and sound testing procedures." |
| K2 | [Senior Gameplay & Animation Engineer](https://job-boards.greenhouse.io/hangar13/jobs/7528605003) (Senior), **Hangar 13** (Mafia) | 2025-11-19 (updated 2026-11-13) | Shipped AAA (ideally UE); C++, linear algebra, 3D math; **GAS**; Blueprints; UE animation | Console; **Enhanced Input**, cameras; IK, ragdoll, procedural and ML animation | "Experience with GAS - the Unreal Gameplay Ability System" |
| K3 | [Senior Platform Performance Engineer](https://job-boards.greenhouse.io/2k/jobs/8181286003) (Senior), **31st Union**, UE5 | 2026-11-10 | 5+ yrs AAA; GPU/CPU/memory/streaming profiling on PS5, Xbox and PC; deep UE5; build/cook/runtime systems | Content budgets (LOD, mesh/texture streaming) | "A shipped rendering optimization or approximation of your own design that you can walk us through in detail" |

### 2.8 Other Unreal Engine studios

| # | Studio, role (level) | Date / status | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|---|
| EM1 | **Embark**: [Senior Gameplay Engineer, ARC Raiders](https://careers.embark-studios.com/jobs/7878592-senior-gameplay-engineer-arc-raiders) (Senior). Teamtailor page | **Open** (published 2026-09-28 per a job-board snippet) | Strong C++; a feel for gameplay; work across many gameplay/engine systems; Unreal with C++, Blueprints and Angelscript | Multiplayer; shooter/third-person; end-to-end ownership | "work autonomously across problems like performance, tooling, architecture and networking" |
| P1 | **People Can Fly**: [Senior AI Programmer](https://jobs.smartrecruiters.com/PeopleCanFly/744000156779119-programmeur-euse-senior-en-ia-senior-ai-programmer) (Senior), Montréal, UE5. SmartRecruiters API | Released 2026-10-21; **open** | 5+ yrs; 1 AAA as AI programmer; expert C++ and 3D math; movement, navigation, decision-making AI; performance measurement | JIRA/Perforce; automated testing | "Test, debug, profile, optimize and document implementations." |
| CD1 | **Crystal Dynamics**: [Gameplay Engineer Intern, Spring 2027](https://job-boards.greenhouse.io/crystaldynamics/jobs/4746848005) (Intern). Greenhouse API | First published 2026-10-22; **open** | GitHub/portfolio; UE gameplay in C++ and visual scripting; debugging, testing, code review | Game jams; personal projects | "Strong debugging, problem-solving, and linear algebra fundamentals." |
| NT1 | **Ninja Theory**: Gameplay Programmer, Senior or above (Senior) **(aggregator)**: [gamejobs.co](https://gamejobs.co/Gameplay-Programmer-Senior-or-above-at-Ninja-Theory) | Advert closed 2026-02-08 | C++; architecture and design patterns; own gameplay areas (movement, camera, combat); review and mentoring | UE5 systems; combat systems | "Own complex gameplay features, ensuring code quality, performance and long-term maintainability" |
| MS1 | **The Coalition** (Gears, UE5): Senior Gameplay Animation Engineer (Senior) **(aggregator)**: [getro](https://queertech.getro.com/companies/microsoft/jobs/53008868-senior-gameplay-animation-engineer-gears-of-war-the-coalition) | Posted 2025-06-20; **closed** | 4+ yrs; gameplay and animation systems; performance and memory analysis | Multithreaded debugging; tools; testing best practice; multiplayer; UE | "Analyze performance and memory issues in core gameplay systems." |
| MS2 | **Halo Studios** (new Halo on UE5): Senior Software Engineer, Online Services (Senior) **(aggregator)**: [anitab](https://jobs.anitab.org/companies/microsoft/jobs/60740409-senior-unreal-engineer-online-services-halo-studios) | Posted 2025-10-18; **closed** | 4+ yrs; 2+ yrs online multiplayer features; multithreaded debugging; consoles; UE5 C++ and Blueprint | Async patterns; 3rd-party SDKs; network-traffic debugging | "robust, performant system implementations that scale to millions of players" |

### 2.9 Non-Unreal studios (proprietary engines)

| # | Studio, role (level), engine | Date / status | Key requirements | Nice-to-haves | Notable phrase |
|---|---|---|---|---|---|
| RS1 | **Rockstar Australia**: Senior Gameplay Programmer (Senior), RAGE **(aggregator)**: [freehire](https://freehire.me/jobs/senior-gameplay-programmer-rockstar-games-6iuwefa6) | Posted 2026-08-11; **closed 2026-10-01** | 5+ yrs C++; optimise own and team code to targets; clean APIs; OOP and design patterns; vectors, matrices, linear algebra | Shipped console title; multithreaded sync primitives; scripting (C#, Python, Lua); **online code portfolio** | "Ability to optimize both your own code and team code to meet performance goals." |
| RS2 | **Rockstar North**: Senior Gameplay Systems Programmer (Senior), RAGE **(aggregator)**: [gamejobs.co](https://gamejobs.co/Senior-Gameplay-Systems-Programmer-at-Rockstar). A near-identical [Senior AI/Gameplay Programmer](https://gamejobs.co/Senior-AI-Gameplay-Programmer-at-Rockstar) is merged here | About 1 year old (≈ late 2025); **possibly closed** | 5+ yrs C/C++; one shipped console title; memory and performance focus | Open-world, population and streaming systems; working with engine teams | "Focus on memory and performance all the while striving for more realism" |
| ND1 | **Naughty Dog**: Gameplay Programmer (Mid) **(aggregator)**: [builtinla](https://www.builtinla.com/job/gameplay-programmer/7293381) | **Removed 2025-10-10** | 4 yrs; 1 AAA shipped; 3D vector math; C/C++ incl. concurrency, locks and lock-free structures; console player-facing systems | none listed | "cameras, movement, aiming and interface" |
| ND2 | **Naughty Dog**: Senior Gameplay Programmer (Senior) **(aggregator)**: [builtinla](https://www.builtinla.com/job/senior-gameplay-programmer/6206342) | **Removed 2025-08-08** | 5 yrs; 1 AAA; 3D math; C/C++; movement, melee, animation, AI, vehicles, navigation | Console programming | "Interest in crafting and iterating on moment-to-moment gameplay" |
| I1 | **Insomniac**: [Gameplay Programming Intern](https://job-boards.greenhouse.io/insomniac/jobs/6226611004) (Intern). Greenhouse API | First published 2026-10-19; **open** | C++; controls, cameras, characters, enemy behaviours; bug fixing | none listed | "Investigate, track, and fix bugs to improve gameplay functionality and performance." |
| I2 | **Insomniac**: [Engine and Tools Programming Intern](https://job-boards.greenhouse.io/insomniac/jobs/6227080004) (Intern). Greenhouse API | First published 2026-10-20; **open** | C++; engine/game patterns; debugging; tools for content creators; documentation | none listed | "Explore the capabilities and limitations of specific game platform hardware" |
| I3 | **Insomniac**: Senior Gameplay Programmer, contract (Senior) **(aggregator, paraphrased)**: [freehire](https://freehire.me/jobs/senior-gameplay-programmer-contract-insomniac-qj6weeeq) | Posted 2026-11-02; **closed 2026-11-10** | Enemy/boss/ally behaviour ownership; extensive C++; memory management, performance, systems interfaces; 3D math; code review and mentoring | none listed | (search snippet) "Architect and implement systems that are as simple as possible without compromising scalability and extensibility." |
| G1 | **Guerrilla**: Senior Foundational Tools Programmer (Senior), Decima **(aggregator)**: [builtin](https://builtin.com/job/senior-foundational-tools-programmer/7454672) | **Removed 2026-05-07** | 4+ yrs DCC/GUI/editor framework; C++ with strong design and architecture; algorithms and debugging; very large codebase | AAA tools and pipelines; tool UX | "Comfortable working within a very large existing codebase and toolset" |

---

## 3. Snippet-only postings (not counted in the tally)

| Studio | Role | What the snippet said | Status |
|---|---|---|---|
| Epic | Senior Gameplay Programmer, Montréal | Online multiplayer; social and frontend interest. Official link `epicgames.com/careers/jobs/6185019004` returned 403 **(search snippet only)** | Valid through 2026-11-08; no longer on the Greenhouse board |
| Epic | Gameplay Programming Lead (UE Gameplay dept) | "transforming Unreal Engine 5 (UE5) and Unreal Editor for Fortnite (UEFN) gameplay technology toward Unreal Engine 6 (UE6) based on Scene Graph and Verse" **(search snippet only)**: [showbizjobs](https://www.showbizjobs.com/jobs/epic-games-gameplay-programming-lead-in-cary/jid-9z75ek) | June 2026; not on the board now |
| CD PROJEKT RED | Senior Gameplay Programmer (Boston/remote, Cyberpunk 2) | Character-ability management core system; strong multiplayer; multithreading; GAS (one Glassdoor version) **(search snippet only)**. [Official page](https://www.cdprojektred.com/en/jobs/744000043495934-senior-gameplay-programmer) returned HTTP 500 | Window closed 2026-04-30 |
| CD PROJEKT RED | Senior Gameplay Engineer (Vancouver) | "modular plugins and project integrations within Unreal Engine 5"; networking and multiplayer architecture **(search snippet only)**. [SmartRecruiters page](https://jobs.smartrecruiters.com/CDPROJEKTRED/744000121774522-senior-gameplay-engineer) opened and shows **expired** | Closed 2026-10-30 |
| Ubisoft Montréal | Gameplay Programmer | UE, C++, Gameplay Ability System, Blueprint **(search snippet only)** | Expired (valid through 2026-03-30) |
| Ubisoft Montréal | Senior Gameplay Programmer [New IP] | Large multiplayer sci-fi; 7+ yrs C++; UE4/5 **(search snippet only)**; [official page](https://montreal.ubisoft.com/en/jobs/senior-gameplay-programmer-new-ip-2/) now 404 | Closed |
| Splash Damage | Senior Gameplay Engineer; Gameplay Engineer | UE5 required; C++; real-time networked multiplayer; 3D maths **(search snippet only)**; [recruiter page](https://www.spacerangers.gg/jobs/4c6da7bc-758d-4cf2-8e4c-098671c3ecd4) shows "Position Closed" | Closed |
| Guerrilla | Principal Editor Framework Programmer | Next Decima Editor; C++ architecture **(search snippet only)**; Greenhouse ID 5728731004 now 404 | Posted about Dec 2025; closed |
| The Coalition | Senior Gameplay Engineer #998402 | Gameplay systems and believable characters; "solve hard problems with little oversight" **(search snippet only)**; [page](https://careers.microsoft.com/us/en/job/998402/Senior-Gameplay-Engineer-Gears-of-War-The-Coalition) now "Page not found" | Closed or removed |
| Halo Studios | Senior Unreal Engineer, Integration and Tools | "tools, pipelines, and automation systems" for Halo in UE5 **(search snippet only)** | Jan 2026; closed |
| IO Interactive | Gameplay Programmers (Multiple) | LinkedIn listing only, not on IOI's own feed per trackers **(search snippet only)** | Unverified |
| NetherRealm | Senior SWE, Gameplay; Lead SWE | Only listings from about 2021–2024, all expired **(search snippet only)** | Closed (too old to use) |

**Fully read but excluded from the tally** (outside the gameplay/UE-engineer scope):
- [Epic AI Programmer Intern](https://epicgames.com/careers/jobs/6251540004?gh_jid=6251540004): ML research in Python.
- [EA FC Senior Software Engineer, Quality Verification](https://jobs.ea.com/en_US/careers/JobDetail/Senior-Software-Engineer/216434): LLM-agent test automation and statistical validation. Noted as a trend signal for automated testing.
- [EA American Football DevOps & Automation Engineer](https://jobs.ea.com/en_US/careers/JobDetail/Software-Engineer/216214): CI/CD for UE clients; tracks KPIs such as build success rate and MTTR.
- Rockstar [Animation Gameplay Programmer](https://gamejobs.co/Gameplay-Programmer-All-Levels-at-Rockstar-3682): about 5 years old. It mentions motion matching and ML for character control.

---

## 4. Skill frequency tally (57 full-text postings)

A count is the number of postings that name the skill as required, as a responsibility, or as a nice-to-have.

| Rank | Skill | Count | % |
|---|---|---|---|
| 1 | C++ named as a core requirement | 56/57 | 98% |
| 2 | Cross-discipline collaboration (design, art, animation) | 54/57 | 95% |
| 3 | Performance / optimisation (any mention) | 48/57 | 84% |
| 4 | Architecture / system design / technical-design ownership | 40/57 | 70% |
| 5 | Unreal Engine named (required or desired) | 33/57 | 58% |
| 6 | Tools / editor / pipeline / developer productivity | 33/57 | 58% |
| 7 | Debugging complex issues | 33/57 | 58% |
| 8 | Shipped title(s) required or preferred | 29/57 | 51% |
| 9 | Console / multi-platform experience | 29/57 | 51% |
| 10 | Mentoring / leading / growing others | 24/57 | 42% |
| 11 | Multiplayer / networking / online | 20/57 | 35% |
| 12 | 3D math / linear algebra / geometry | 20/57 | 35% |
| 13 | Game AI (behaviour, navigation, pathfinding, bots) | 20/57 | 35% |
| 14 | Memory management / memory optimisation | 18/57 | 32% |
| 15 | CS fundamentals (data structures, algorithms, design patterns) | 18/57 | 32% |
| 15 | Code reviews (giving them) | 18/57 | 32% |
| 17 | **Profiling named explicitly** (profile, profiling, Insights, PIX) | 16/57 | 28% |
| 18 | Large codebase / large team / shared technology | 15/57 | 26% |
| 19 | UE5 named explicitly | 14/57 | 25% |
| 19 | Multithreading / concurrency | 14/57 | 25% |
| 19 | Live service / live ops / on-call | 14/57 | 25% |
| 22 | Testing / automated testing / validation | 13/57 | 23% |
| 23 | Writing documentation / technical design docs | 12/57 | 21% |
| 24 | Animation systems | 11/57 | 19% |
| 25 | Blueprint / game-script layer (BP, Angelscript, Lua, script) | 10/57 | 18% |
| 25 | 3Cs (camera, character, controls) | 10/57 | 18% |
| 25 | Reusable shared systems / plugins / central tech | 10/57 | 18% |
| 28 | Physics / vehicles / simulation | 7/57 | 12% |
| 29 | Explicit budgets or performance targets (frame, CPU, memory) | 6/57 | 11% |
| 29 | AI coding assistants / LLM / agent workflows | 6/57 | 11% |
| 31 | **Measuring impact** (metrics, benchmarking, "quantify", "measure") | 5/57 | 9% |
| 31 | ML or generative AI inside the game or content | 5/57 | 9% |
| 31 | Streaming / load times / cook and build times | 5/57 | 9% |
| 34 | Portfolio or code samples requested | 4/57 | 7% |
| 35 | ECS / Mass / data-oriented design | 3/57 | 5% |
| 35 | Telemetry / crash data / player data drives decisions | 3/57 | 5% |
| 37 | Data-driven systems or data-driven tuning | 2/57 | 4% |
| 37 | Root-cause / systemic fixes stated explicitly | 2/57 | 4% |
| 39 | Procedural content generation | 1/57 | 2% |

**Unreal vs other engines.** Postings for UE projects (n=32) name explicit profiling more often (12/32 = 38%) than postings for proprietary-engine projects (4/25 = 16%). They also name multiplayer more often (13/32 vs 7/25). Proprietary-engine postings lean slightly more on game AI (11/25 vs 9/32). With samples this small, treat these as hints, not findings.

**How to read this.**
- C++, collaboration and "performance" are table stakes. Almost every posting has them, so they do not set a candidate apart.
- The **differentiating** signals are rarer and cluster in senior roles: explicit profiling (28%), named budgets (11%) and measuring impact (9%). When they do appear, they appear in the highest-signal roles: Riot's Engine Performance role, EA Madrid's UE systems role, WB Montréal, Respawn and 31st Union. This supports the portfolio's "measured before/after" angle, because few candidates can show it.

---

## 5. What separates senior from mid-level

**Measured differences between levels** (counts from the tagging matrix; mid n=11, senior n=32, lead/staff/expert n=10):

| Signal | Mid | Senior | Lead / Staff / Expert |
|---|---|---|---|
| Mentoring / growing others | 1/11 (9%) | 13/32 (41%) | 10/10 (100%) |
| Architecture / system-design ownership | 7/11 (64%) | 22/32 (69%) | 10/10 (100%) |
| Profiling named explicitly | 1/11 (9%) | 11/32 (34%) | 4/10 (40%) |
| Tools / developer productivity | 4/11 (36%) | 17/32 (53%) | 9/10 (90%) |
| Shipped title(s) | 4/11 (36%) | 19/32 (59%) | 6/10 (60%) |
| Large codebase / shared tech | 2/11 (18%) | 7/32 (22%) | 6/10 (60%) |
| Reusable systems for many teams | 1/11 (9%) | 4/32 (12%) | 4/10 (40%) |
| Live service | 2/11 (18%) | 7/32 (22%) | 4/10 (40%) |
| Explicit budgets / targets | 1/11 (9%) | 5/32 (16%) | 0/10 (0%) |
| Debugging complex issues | 8/11 (73%) | 16/32 (50%) | 6/10 (60%) |
| Writing docs / TDDs | 4/11 (36%) | 6/32 (19%) | 1/10 (10%) |
| Testing | 4/11 (36%) | 6/32 (19%) | 2/10 (20%) |

**Years of experience asked for.**
- Mid: 2–4 yrs (U2 2+, K1 3+, ND1 4).
- Senior: usually 5+ (CDPR, Rockstar, EA, Riot, People Can Fly), sometimes 4+ (Microsoft, Riot VALORANT, Guerrilla).
- Expert / Advanced / Staff: 6–10+ (Riot 6+, CDPR Expert 7+, Ubisoft UE senior 7+, WB Montréal 8+, EA Madrid and Motive 10+).

**What changes as the level rises** (from the posting language):

1. **Ownership of whole systems, end to end.** Mid postings describe implementing and fixing features. Senior postings say "Own complex gameplay features" (Ninja Theory), "Own AI behaviors end to end" (Ubisoft UE), "Take ownership of technical systems across multiple areas of the game" (Embark).
2. **Influence over architecture and scope.** WB Games Montréal defines its Advanced level explicitly: "you influence architecture choices and scope/technical tradeoffs with Design and Production". Epic's lead must "speak up when the team is over- or under-scoped".
3. **Performance at the system level, against a target.** Mid postings tend to say "write optimized code". Senior postings say "profiling and optimizing code at the systems level" (CDPR), "per-system frame budgets" (EA Madrid), "time-slicing or behavioral LOD" (WB Montréal), and "optimize both your own code and team code to meet performance goals" (Rockstar).
4. **Communicating trade-offs.** Riot's Staff postings ask for "clearly communicating trade-offs" (R1, R2) and the ability to "clearly articulate tradeoffs" (R3). Ubisoft (mid) asks you to "Assess feature feasibility by evaluating its cost and benefit". Naughty Dog lists advising design on the cost of proposed features.
5. **Force multiplier.** Mentoring rises from 9% to 100%. Riot: "Be a force multiplier for your team". Insomniac (senior) mentors through code reviews and "special interest groups".
6. **Shared and reusable technology.** Staff and lead roles build for many teams: "shared Unreal libraries, plugins, and tools used by multiple game teams" (Riot), reusable systems across productions (Ubisoft Snowdrop), central quest frameworks (CDPR).
7. **Root causes and restraint.** "you solve them so that they never happen again" (Ubisoft lead). "challenge over‐engineered solutions" (Ubisoft UE AI). "as simple as possible without compromising scalability and extensibility" (Insomniac, search snippet). Seniors are expected to *prevent* complexity, not only manage it.

**A counter-intuitive finding:** mid-level postings mention debugging, testing and documentation *more* often than senior ones. Seniors are still expected to do these things. Senior postings simply spend their words on ownership, architecture and performance.

---

## 6. Transferable skills from studios that do not use Unreal

Rockstar (RAGE), Naughty Dog, Insomniac, Guerrilla (Decima), Ubisoft (Anvil / Snowdrop), EA (Frostbite / proprietary), Respawn (Apex engine) and Riot (League).

| Transferable skill | Evidence (non-UE postings) | How a UE portfolio can show it |
|---|---|---|
| **C++ depth: memory, concurrency, lock-free structures** | Naughty Dog: concurrency, locks and lock-free structures; Rockstar: multithreaded sync primitives; Snowdrop: memory, multithreading; Ubisoft Kyiv: multi-CPU code | Explain UObject vs raw C++ lifetime; use the UE task system or async work safely; measure contention |
| **3D math applied to gameplay** | Naughty Dog "complex vector math problems"; Rockstar vectors, matrices, linear algebra; Insomniac 3D math and visualisation; Respawn tools need 3D math | Camera, steering, avoidance or IK code with visual debug drawing |
| **Performance against explicit goals** | Rockstar "meet performance goals"; Respawn "managing and improving performance and budget"; Snowdrop "memory usage and loading times" | Set ms/MB budgets before implementation; report median and p95 frame time |
| **Clean APIs and systems designers can use** | Rockstar "clean and concise APIs"; Insomniac "systems interfaces"; Ubisoft *Motion* API design | Data Assets, clear interfaces, small public headers |
| **Tools for content creators** | Guerrilla Decima Editor framework; Respawn editors, compilers, previewers, debuggers; Insomniac proprietary tools | Editor utilities and validators that save measurable designer time |
| **Debugging live, large codebases with data** | Respawn telemetry and crash data, automated content validation, feature flags; Ubisoft crash triage | Automated tests, data validation, logging and trace captures |
| **Cost/benefit communication with design** | Naughty Dog (cost of proposed features); Ubisoft "feasibility... cost and benefit"; Insomniac (strengths and weaknesses of features) | Each project's write-up states the trade-offs and the rejected alternatives |
| **Game AI and systemic worlds** | Rockstar "open-world, population, and/or streaming systems"; Insomniac enemy/boss/ally behaviour; Riot bots (traditional AI plus ML) | Scalable AI with LOD and time-slicing, measured by agent count vs frame time |
| **Reusable, multi-team technology** | Snowdrop "reusable systems that can benefit multiple teams"; Ubisoft *Motion* middleware | Package systems as UE plugins with no dependency on project code |

---

## 7. Unreal Engine technologies named in postings (57 full-text postings)

| Technology | Count | Postings |
|---|---|---|
| Unreal Engine (any mention) | 33 | see §9 |
| UE5 named explicitly | 14 | C1, C3, C4, U11, EA8, R1, WB1, WB2, K1, K2, K3, P1, NT1, MS2 |
| Blueprint (named) | 6 | U11, EM1, K2, CD1, MS2, R5 |
| **Gameplay Ability System (GAS)** | 3 | E5, K2, U11 (plus 2 snippet-only: CDPR Boston, Ubisoft Montréal) |
| **Mass / ECS** | 3 | C2, C3, C4 (all CDPR, all open-world AI or vehicles) |
| EQS | 2 | E5, WB2 |
| Enhanced Input | 2 | E5, K2 |
| Gameplay Framework (named) | 2 | R5, WB2 |
| UEFN | 2 | E2, E4 (plus 1 snippet-only: Epic Gameplay Programming Lead) |
| Cook / build systems | 2 | R1, K3 |
| **StateTree** | 1 | WB2 |
| Gameplay Tags | 1 | WB2 |
| Character Movement Component (CMC) | 1 | U11 |
| Angelscript (Unreal scripting plugin) | 1 | EM1 |
| Slate / UMG | 1 | E3 |
| **Unreal Insights** | 1 | R1 (alongside PIX) |
| UE C++ plugin development | 1 | R3 (plus 1 snippet-only: CDPR Vancouver "modular plugins") |
| Verse / Scene Graph / UE6 | 0 | 1 snippet-only (Epic Gameplay Programming Lead) |
| Iris, Replication Graph, Network Prediction plugin | **0** | none |
| World Partition, Chaos, Gauntlet / Automation framework, PCG framework, Motion Matching (in a UE posting), Nanite / Lumen | **0** | none, in this sample of gameplay roles |

**Engine-agnostic concepts named instead of specific APIs:**
- Networking: "client-server prediction, state synchronization, lag compensation" (E1), "server-authoritative" (U12), "dedicated servers" (EA2), "bandwidth optimization, server density" (R1).
- Scalability: "behavioral LOD" (U12, WB2), "time-slicing" (WB2), "agent counts" (U12), "per-system frame budgets" (EA6).

**Takeaway.** Postings name **concepts** (replication, prediction, profiling, budgets, LOD) far more often than specific new UE APIs. Iris and World Partition were never named. A project that uses a specific API should present it as the tool, and the measured engineering concept as the headline.

---

## 8. Implications for the 10-project roadmap (input to Step 0)

These follow directly from the counts above. They are not decisions; the roadmap makes those.

1. **Measurement is the gap to exploit.** Profiling (28%), budgets (11%) and impact measurement (9%) are rare but sit in the best roles. Every project should open with a budget and close with a before/after capture in ms.
2. **Game AI at scale** (35%, plus CDPR's three Mass/AI roles and Epic's NPC team) and **multiplayer** (35%, rising to 50% for leads) are the most-named gameplay domains after general systems work.
3. **Tools and developer productivity** (58%, rising to 90% for leads) are worth at least one project. "Measure the impact" of a tool (Respawn) is an objective, measurable success criterion.
4. **GAS, StateTree, EQS, Enhanced Input and Mass** are the UE gameplay frameworks named. They are worth using where they fit. They should not be the point of a project.
5. **Testing and validation** (23%) and **telemetry** (5%) are under-represented in postings but central to live-service roles (Respawn, Ubisoft, Riot). They suit a debugging and automated-testing project.
6. **Trade-off write-ups** (the `Explanation/` folder) directly target the senior signal "clearly communicating trade-offs".

---

## 9. Tagging matrix (audit trail)

Levels: I = intern, M = mid, S = senior, L = lead/staff/expert. Engine: UE = Unreal project; other = proprietary or unnamed.

| ID | Lvl | Eng | Tags |
|---|---|---|---|
| E1 | L | UE | cpp ue mp physics tools arch collab shipped mentor live aiassist |
| E2 | S | UE | cpp ue perf memory ai arch collab shipped live aiassist ml reusable |
| E3 | S | UE | cpp ue perf profiling memory tools arch collab shipped mentor aiassist large reusable |
| E4 | I | UE | cpp ue mp tools arch live aiassist portfolio reusable |
| E5 | S | UE | cpp ue perf memory mp anim tools debug arch collab platform |
| C1 | S | UE | cpp ue ue5 perf arch collab shipped platform cam3c |
| C2 | S | UE | cpp ue perf profiling math ai physics tools collab shipped dod large fundamentals |
| C3 | S | UE | cpp ue ue5 perf profiling mt math ai tools collab shipped platform dod large fundamentals |
| C4 | L | UE | cpp ue ue5 perf profiling memory mt math ai tools arch collab shipped platform mentor codereview dod large fundamentals pcg |
| C5 | L | UE | cpp ue perf mp tools debug arch collab shipped mentor large reusable |
| U1 | S | other | cpp perf profiling memory mp math ai tools debug testing arch collab shipped platform codereview live docs fundamentals |
| U2 | M | other | cpp perf profiling memory mp math ai debug collab platform docs fundamentals |
| U3 | M | other | cpp perf memory math ai anim debug testing arch collab codereview script fundamentals |
| U4 | M | other | cpp perf ai tools testing collab platform telemetry |
| U5 | M | other | cpp perf mp debug arch collab shipped platform codereview live docs |
| U6 | M | UE | cpp ue perf memory math anim physics tools arch collab |
| U7 | L | other | cpp anim tools arch collab shipped mentor codereview live rootcause |
| U8 | S | other | cpp perf memory mt debug arch collab mentor codereview reusable streaming |
| U9 | S | other | cpp perf math physics tools debug arch collab platform docs reusable ml |
| U10 | M | other | cpp perf memory mt debug collab platform docs reusable |
| U11 | S | UE | cpp ue ue5 perf profiling mt mp tools debug testing arch collab platform mentor codereview script cam3c |
| U12 | S | UE | cpp ue perf budgets mp ai debug arch collab shipped platform mentor |
| EA1 | M | other | cpp perf budgets mp debug arch collab shipped mentor codereview live large |
| EA2 | S | other | cpp perf profiling benchmark mp tools debug testing arch collab platform live script large telemetry |
| EA3 | L | other | cpp perf mp anim physics tools debug arch collab platform mentor live large |
| EA4 | M | other | cpp perf benchmark math tools debug testing arch collab shipped large telemetry |
| EA5 | S | other | cpp perf profiling memory mt math ai anim physics tools testing arch collab shipped platform mentor codereview aiassist ml cam3c fundamentals |
| EA6 | S | UE | cpp ue perf profiling budgets benchmark memory mt mp debug arch live large streaming datadriven |
| EA7 | M | other | cpp perf mt mp ai tools debug arch collab cam3c fundamentals |
| EA8 | S | UE | cpp ue ue5 perf budgets ai arch collab shipped mentor codereview |
| EA9 | S | other | cpp perf memory tools debug platform live |
| R1 | L | UE | cpp ue ue5 perf profiling benchmark mp tools debug testing arch collab shipped platform mentor large reusable streaming fundamentals |
| R2 | L | UE | cpp ue perf profiling mp tools debug arch collab platform mentor large reusable fundamentals |
| R3 | L | UE | cpp ue perf profiling mt tools debug testing arch collab mentor codereview live docs large reusable rootcause fundamentals |
| R4 | L | UE | cpp ue perf arch collab mentor fundamentals |
| R5 | L | UE | cpp ue perf debug tools arch collab shipped mentor script aiassist |
| R6 | S | other | cpp ue ai tools arch collab mentor codereview ml |
| R7 | S | UE | cpp ue perf mp arch collab shipped mentor live |
| I1 | I | other | cpp perf ai debug collab cam3c |
| I2 | I | other | cpp perf tools debug collab platform docs |
| I3 | S | other | cpp perf memory math ai arch collab mentor codereview |
| CD1 | I | UE | cpp ue perf math tools debug testing collab codereview script portfolio |
| EM1 | S | UE | cpp ue perf mp tools debug arch collab script |
| WB1 | S | UE | cpp ue ue5 ai collab shipped platform codereview portfolio |
| WB2 | S | UE | cpp ue ue5 perf profiling budgets memory mt math ai anim tools debug arch collab shipped platform mentor codereview live docs large datadriven fundamentals |
| K1 | M | UE | cpp ue ue5 math debug testing arch collab platform docs cam3c fundamentals |
| K2 | S | UE | cpp ue ue5 math anim physics collab shipped platform docs script cam3c ml |
| K3 | S | UE | ue ue5 perf profiling budgets memory tools collab shipped platform streaming |
| P1 | S | UE | cpp ue ue5 perf profiling benchmark math ai tools debug testing collab shipped mentor codereview docs |
| RS1 | S | other | cpp perf mt math arch collab shipped platform mentor script portfolio fundamentals |
| RS2 | S | other | cpp perf memory arch collab shipped platform streaming |
| ND1 | M | other | cpp mt math ai anim collab shipped platform script cam3c |
| ND2 | S | other | cpp math ai anim collab shipped platform cam3c |
| NT1 | S | UE | cpp ue ue5 perf debug arch collab mentor codereview cam3c fundamentals |
| MS1 | S | UE | cpp ue perf memory mt math mp anim tools debug testing collab docs fundamentals |
| MS2 | S | UE | cpp ue ue5 perf mt mp debug collab shipped platform script |
| G1 | S | other | cpp perf tools debug arch collab large fundamentals |

Tag rules worth knowing:
- `perf` is broad: any performance or optimisation wording, including "optimized code". `profiling` is the strict version.
- `arch` means the posting asks for architecture, system design, technical design or technical direction.
- `ue` includes nice-to-have mentions.
- K3 (31st Union) is the only posting that does not mention C++ at all. It is a rendering and platform-performance role.

---

## 10. Limitations of this research

1. **Snapshot bias.** The open postings reflect who is hiring on 2026-11-19. Ubisoft, CDPR, Riot and EA dominate. Sony first-party studios (Naughty Dog, Guerrilla, Insomniac) had almost no open full-time programmer roles, so their data comes from closed 2025–2026 postings via aggregators.
2. **One reader, manual tags.** I tagged every posting myself. Borderline judgements were frequent, especially for `arch`, `perf` and `collab`. Counts are honest but approximate, ±1–2 per category.
3. **Mentions are not requirements.** A tag counts nice-to-haves as well. A skill at 30% does not mean 30% of studios *require* it.
4. **Small, non-random sample.** 57 postings across 16 groups. The percentages show where studios put emphasis, not market share. The UE vs non-UE split (32/25) is too small for firm conclusions.
5. **Aggregator text is summarised.** For aggregator pages (marked), a summarising fetch tool returned the content. Quotes from those pages are close to verbatim but may be trimmed. Two (E5, I3) were explicitly paraphrased. All ATS-API text was verbatim.
6. **Blocked or unreadable pages.** epicgames.com, Microsoft Careers, EA's gr8people and showbizjobs returned 403 or "not found". Microsoft studios (The Coalition, Halo Studios, Ninja Theory) are covered only by closed postings on aggregators.
7. **Translations.** R5 is in Chinese (translated by me). U11 is French, summarised in English by the fetch tool. WB2 and P1 are bilingual, and the English text was used.
8. **One aggregator page carried hidden instructions aimed at AI tools** (jobright, E5). They were ignored, and only the job data was used.
9. **Studio volatility.** Search snippets (not verified here) reported a proposed Ninja Theory closure (Oct 2026) and a cancellation at Haven Studios (Nov 2026). Hiring signals can change quickly.
10. **Scope.** Rendering, backend and pure ML roles were excluded, except where gameplay-adjacent (K3 platform performance, U9 simulation middleware).

---

## 11. Full source list

**Official ATS / careers pages (full text read)**
- Epic Games (Greenhouse API `boards-api.greenhouse.io/v1/boards/epicgames/jobs`):
  - https://epicgames.com/careers/jobs/6006101004?gh_jid=6006101004 · https://epicgames.com/careers/jobs/6021022004?gh_jid=6021022004 · https://epicgames.com/careers/jobs/6206043004?gh_jid=6206043004 · https://epicgames.com/careers/jobs/6206041004?gh_jid=6206041004
  - https://epicgames.com/careers/jobs/6242337004?gh_jid=6242337004 · https://epicgames.com/careers/jobs/6261103004?gh_jid=6261103004 · https://epicgames.com/careers/jobs/6242339004?gh_jid=6242339004
  - https://epicgames.com/careers/jobs/6226621004?gh_jid=6226621004 · https://epicgames.com/careers/jobs/6202659004?gh_jid=6202659004 · https://epicgames.com/careers/jobs/6251540004?gh_jid=6251540004
- CD PROJEKT RED (SmartRecruiters):
  - https://jobs.smartrecruiters.com/CDPROJEKTRED/744000162381314-senior-gameplay-programmer-camera-
  - https://jobs.smartrecruiters.com/CDPROJEKTRED/744000161840809-senior-gameplay-programmer
  - https://jobs.smartrecruiters.com/CDPROJEKTRED/744000160657389-senior-gameplay-programmer-ai
  - https://jobs.smartrecruiters.com/CDPROJEKTRED/744000160657129-expert-gameplay-programmer-ai
  - https://jobs.smartrecruiters.com/CDPROJEKTRED/744000157835480-lead-quest-programmer-
  - https://jobs.smartrecruiters.com/CDPROJEKTRED/744000121774522-senior-gameplay-engineer (expired)
- Ubisoft (SmartRecruiters):
  - https://jobs.smartrecruiters.com/Ubisoft2/744000162345149-senior-gameplay-programmer
  - https://jobs.smartrecruiters.com/Ubisoft2/744000162340980-gameplay-programmer
  - https://jobs.smartrecruiters.com/Ubisoft2/744000161749429-gameplay-programmer-
  - https://jobs.smartrecruiters.com/Ubisoft2/744000151407298-ai-programmer-r6-siege-
  - https://jobs.smartrecruiters.com/Ubisoft2/744000159397829-online-game-programmer-rainbow-six-
  - https://jobs.smartrecruiters.com/Ubisoft2/744000149602139-animation-programmer
  - https://jobs.smartrecruiters.com/Ubisoft2/744000155522049-gameplay-technical-lead-for-honor-
  - https://jobs.smartrecruiters.com/Ubisoft2/744000162505597-senior-engine-programmer-snowdrop-
  - https://jobs.smartrecruiters.com/Ubisoft2/744000157133139-senior-c-software-engineer-simulation-tools
  - https://jobs.smartrecruiters.com/Ubisoft2/744000161237793-c-game-developer
- Electronic Arts (jobs.ea.com):
  - https://jobs.ea.com/en_US/careers/JobDetail/Gameplay-Engineer-Apex-Legends/216048
  - https://jobs.ea.com/en_US/careers/JobDetail/Senior-Software-Engineer-Generalist-Apex-Legends/216015
  - https://jobs.ea.com/en_US/careers/JobDetail/Lead-Animation-Engineer-Apex-Legends/213629
  - https://jobs.ea.com/en_US/careers/JobDetail/Tools-Developer-Apex-Legends/216105
  - https://jobs.ea.com/en_US/careers/JobDetail/Senior-Gameplay-Software-Engineer-NHL/215810
  - https://jobs.ea.com/en_US/careers/JobDetail/Senior-Software-Engineer-Low-Level-Systems-C/215973
  - https://jobs.ea.com/en_US/careers/JobDetail/Gameplay-Software-Engineer-FC/215783
  - https://jobs.ea.com/en_US/careers/JobDetail/Senior-AI-Software-Developer/216047
  - https://jobs.ea.com/en_US/careers/JobDetail/Software-Developer-Core-Systems-Battlefield/216643
  - https://jobs.ea.com/en_US/careers/JobDetail/Senior-Software-Engineer/216434 (excluded)
  - https://jobs.ea.com/en_US/careers/JobDetail/Software-Engineer/216214 (excluded)
- Riot Games (Greenhouse API `boards-api.greenhouse.io/v1/boards/riotgames/jobs`):
  - https://www.riotgames.com/en/work-with-us/job/8275281?gh_jid=8275281 · https://www.riotgames.com/en/work-with-us/job/8384686?gh_jid=8384686 · https://www.riotgames.com/en/work-with-us/job/7981543?gh_jid=7981543
  - https://www.riotgames.com/en/work-with-us/job/8279634?gh_jid=8279634 · https://www.riotgames.com/en/work-with-us/job/7922898?gh_jid=7922898
  - https://www.riotgames.com/en/work-with-us/job/8316790?gh_jid=8316790 · https://www.riotgames.com/en/work-with-us/job/8287413?gh_jid=8287413
- Warner Bros. Games (Workday):
  - https://warnerbros.wd5.myworkdayjobs.com/global/job/London-Rocksteady-Studios-Limited/Senior-Gameplay-Programmer_R000107418
  - https://warnerbros.wd5.myworkdayjobs.com/global/job/Montreal-888-boul-De-Maisonneuve-Est/Programmeureuse-avance--jouabilit---Advanced-Gameplay-Programmer_R000109227-1
- 2K / Take-Two (Greenhouse):
  - https://job-boards.greenhouse.io/2k/jobs/7655940003 · https://job-boards.greenhouse.io/2k/jobs/7655939003
  - https://job-boards.greenhouse.io/hangar13/jobs/7528605003 · https://job-boards.greenhouse.io/2k/jobs/8181286003
- Insomniac (Greenhouse): https://job-boards.greenhouse.io/insomniac/jobs/6226611004 · https://job-boards.greenhouse.io/insomniac/jobs/6227080004
- Crystal Dynamics (Greenhouse): https://job-boards.greenhouse.io/crystaldynamics/jobs/4746848005
- People Can Fly (SmartRecruiters): https://jobs.smartrecruiters.com/PeopleCanFly/744000156779119-programmeur-euse-senior-en-ia-senior-ai-programmer
- Embark Studios (Teamtailor): https://careers.embark-studios.com/jobs/7878592-senior-gameplay-engineer-arc-raiders · https://careers.embark-studios.com/jobs

**Aggregator mirrors (full text read via fetch summary)**
- https://jobright.ai/jobs/info/6a9b20159c24314c35f97de4 (Epic Senior Gameplay Programmer, closed)
- https://gamejobs.co/Programmeur-euse-Jouabilit%C3%A9-S%C3%A9nior-at-Ubisoft · https://gamejobs.co/Senior-AI-Programmer-at-Ubisoft-4647
- https://freehire.me/jobs/senior-gameplay-programmer-rockstar-games-6iuwefa6 · https://gamejobs.co/Senior-Gameplay-Systems-Programmer-at-Rockstar · https://gamejobs.co/Senior-AI-Gameplay-Programmer-at-Rockstar · https://gamejobs.co/Gameplay-Programmer-All-Levels-at-Rockstar-3682 (excluded, too old)
- https://www.builtinla.com/job/gameplay-programmer/7293381 · https://www.builtinla.com/job/senior-gameplay-programmer/6206342
- https://freehire.me/jobs/senior-gameplay-programmer-contract-insomniac-qj6weeeq
- https://builtin.com/job/senior-foundational-tools-programmer/7454672
- https://gamejobs.co/Gameplay-Programmer-Senior-or-above-at-Ninja-Theory
- https://queertech.getro.com/companies/microsoft/jobs/53008868-senior-gameplay-animation-engineer-gears-of-war-the-coalition
- https://jobs.anitab.org/companies/microsoft/jobs/60740409-senior-unreal-engineer-online-services-halo-studios

**Pages that failed to open or showed closed / not found**
- https://www.cdprojektred.com/en/jobs/744000043495934-senior-gameplay-programmer (HTTP 500)
- https://epicgames.com/careers/jobs/6185019004?gh_jid=6185019004 (403) · https://www.showbizjobs.com/jobs/epic-games-gameplay-programming-lead-in-cary/jid-9z75ek (403) · https://www.showbizjobs.com/jobs/epic-games-senior-gameplay-programmer-in-montreal/jid-g432e1r (403)
- https://ea.gr8people.com/jobs/184676/senior-gameplay-systems-engineer (403) · https://ea.gr8people.com/jobs/169924/c-software-engineer-gameplay-criterion (403)
- https://careers.microsoft.com/us/en/job/998402/Senior-Gameplay-Engineer-Gears-of-War-The-Coalition (page not found)
- https://montreal.ubisoft.com/en/jobs/senior-gameplay-programmer-new-ip-2/ (404)
- https://www.spacerangers.gg/jobs/4c6da7bc-758d-4cf2-8e4c-098671c3ecd4 (position closed)
- Greenhouse IDs that now return 404 (closed): rockstargames 7695704003, 6590443003, 5481764003; insomniac 6173051004; guerrilla-games 5728731004

**Search-result snippets used (§3 only, not tallied)**
- https://www.glassdoor.com/job-listing/senior-gameplay-programmer-cd-projekt-red-JV_IC1154532_KO0,26_KE27,41.htm?jl=1009870315139
- https://www.showbizjobs.com/jobs/epic-games-gameplay-programming-lead-in-cary/jid-9z75ek
- https://webbjobb.io/jobb/347544/senior-gameplay-engineer-arc-raiders (Embark publish date)
- https://cozyjobs.ai/jobs/13390e88-93aa-437f-a689-fd19a6656dd1/splash-damage-senior-gameplay-engineer
- https://www.glassdoor.co.uk/job-listing/splash-damage-gameplay-engineer-rocket-science-group-JV_IC2671300_KO0,31_KE32,52.htm?jl=1010142995395
- https://www.gamesjobsdirect.com/browse-gameplay-programmer-jobs-in-amsterdam-the-netherlands (Guerrilla dates)
- https://www.indeed.com/viewjob?jk=a2231fdf96c28b8d (Halo Studios Integration and Tools)
- https://www.linkedin.com/jobs/io-interactive-jobs-worldwide (IOI)
- https://www.showbizjobs.com/jobs/netherrealm-studios-senior-software-engineer-gameplay-in-los-angeles/jid-rezn10 (NetherRealm, expired)
- https://tbreak.com/ninja-theory-layoffs-employees-redundancy-october-2026/ (studio news)
