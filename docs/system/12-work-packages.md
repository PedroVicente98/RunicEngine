# 12 — Work packages

This file lists every Engine and Platform work package through the current roadmap, with what makes it done, what must be merged first, and the slice that first needs it.

Source: ARCH § Modules, work packages and the implementation contract (Work packages), revision 502. Tables are
copied from ARCH word for word.

## How to read the tables

- **ID** is the module prefix plus the build order inside the module: CMB-3 is the third Combat package.
- **Done when** is the acceptance behavior, written as automated tests in the package's contract session.
- **Needs** lists the packages that must be merged first. A package starts only when all of them are merged. Needs
  orders merges; it does not grant a link (links are limited by [03](03-dependency-rules.md)).
- **Slice** is the roadmap slice that first needs the package ([08](08-implementation-roadmap.md)).
- Packages are **Stage A** unless the table says otherwise ([02 § Maturity stages](02-module-contract.md#maturity-stages)).
- Expansion packages (slice 44+) get their full definition when those slices are planned.
- How a package is built: [10-ai-development-workflow.md](10-ai-development-workflow.md).

**Prefixes.** Engine: BUILD build skeleton, CORE Core, APP Application, EVT Events, CNT Content, ASSET Assets, ECS,
WORLD World, PHYS Physics, SIM Simulation, GPL Gameplay, CMB Combat, FLOW GameplayFlow, QST Quest, PRC Perception, NAV
Navigation, AI, ENV Environment, WATER Water, NET Network, DSIM DistributedSimulation, CLI Client, RND Renderer, ANIM
Animation, UI, AUD Audio, EDT Editor, COOK Cooker. Platform: PCT Contracts, PTR Transport, PSDK Go SDK, PBND Server
bindings, PCS Core services, PBB Building blocks, PINF Infrastructure. Game: GAME executables, GSH Shared rules, GSRV
Server module, GCL Client module, GCNT content, GSVC Go game services, GNAV naval rules, GWTH weather rules.

## Engine: foundation

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| BUILD-1 | Build skeleton in all three repositories: `runic_add_module` helper, presets, CI, the chosen test framework, automated headless and forbidden-dependency checks, submodule pins (Game pins Engine and Platform; Platform pins Engine) with a local option for sibling checkouts, an AVX2 (x86-64-v3) CPU baseline | Every repository configures and builds; the server target links no graphics library; a forbidden link fails CI | — | 0 |
| CORE-1 | Plain ID types (GlobalEntityId, GlobalPlayerId, SimulationId, WorldId, TickId, AuthorityEpoch, and value types other modules give meaning to, such as AssetId, WaterBodyId, FieldId and WidgetId), Result, asserts, logging hooks, time types, GLM math | Unit tests cover IDs, Result and time conversions | BUILD-1 | 1 |
| CORE-2 | Worker pool, metrics registry (per-tick timings and counters) and debug-stream registry where modules publish traces | Jobs run on the pool; metrics are recorded per tick domain | CORE-1 | 3 |
| CORE-3 | RenderData foundation: the double-buffered RenderFrame and DebugGeometry (lines, triangles, text, cached debug meshes) | A frame is written on one thread and read on another with no lock; debug vertices need no conversion before upload | CORE-1 | 5 |
| CORE-4 | RenderData scene kinds: mesh instances, skinned instances with pose buffers, world anchors | Every kind round-trips through a frame packet in tests | CORE-3 | 22 |
| CORE-5 | RenderData world-state kinds: lights, environment frame, volumes | Every kind round-trips through a frame packet in tests | CORE-3 | 25a |
| CORE-6 | RenderData water and field kinds: water surface state, field grids | Versions change only when their source changes | CORE-3 | 42a |
| CORE-7 | RenderData effect kinds: particles, decals, trails | Defined when the production pipeline is planned | CORE-3 | 44+ |
| APP-1 | Entry point, Application, ApplicationHost, Layer, LayerStack with queued changes at safe points, application events, loop policies | Layer order, events top-down until handled, updates bottom-up, safe-point push and pop, no allocation per event | CORE-1 | 1 |
| APP-2 | Window class on GLFW, compiled only in client and tools builds; keyboard, mouse and gamepad as application events | A window opens and closes; the headless build compiles without GLFW | APP-1 | 1 |
| EVT-1 | Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs (SimulationId plus sequence) | Two consumers read the same fact; IDs stay stable on replay | CORE-1 | 4 |
| CNT-1 | ContentId with `engine:` and `game:` namespaces, registries, lookup | Duplicate and unknown IDs are reported with their source | CORE-1 | 9 |
| CNT-2 | Versioned schemas, validation, loading, live-reload hooks | A changed definition reloads without a restart | CNT-1 | 20 |
| ASSET-1 | AssetId, handles, cache, loading of native cooked mesh and collision formats | A cooked test mesh loads on client and server | CNT-1 | 9 |
| ASSET-2 | Random-access packages and manifests with Zstandard decompression, client and server package selection, asynchronous streaming | Cells load from a package without stalling a tick or a frame | ASSET-1, WORLD-3 | 21 |

ASSET-2 has an open item: [11 § OQ-3](11-open-questions.md#oq-3).

## Engine: simulation core

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| ECS-1 | Flecs world wrapper, component registration, query helpers, conventions | Entities are created, queried and destroyed; Flecs types appear in public headers only through the approved world-registration exception | CORE-1 | 4 |
| WORLD-1 | 64-bit double coordinates (real Y, 1 cm minimum unit), WorldId, region-local origins, Region, Sector and sparse 3D Cell keys | Conversions round-trip exactly far from the origin; a cell key round-trips at any altitude or depth | ECS-1 | 4 |
| WORLD-2 | Sparse 3D spatial index (XZ columns holding only occupied Y cells), exact XYZ candidate queries (radius, box, ellipsoid), membership updates; authored volumes (caves, rule areas, indivisible volumes) with point-in-volume queries | Query results match brute force in 3D and draw in the debug view; a point inside a cave volume is reported inside it | WORLD-1 | 11 |
| WORLD-3 | Activation: per-player asymmetric ellipsoid activation volumes, their union, prepare, commit and release hysteresis, load and unload requests per cell | Walking back and forth on an edge never thrashes loads; a cell far above or below the player stays unloaded until approached | WORLD-2 | 21 |
| PHYS-1 | PhysicsWorld, bodies, shapes and collision layers on Jolt v5.6 in double precision, kept private | Bodies collide without jitter far from the origin | WORLD-1 | 7 |
| PHYS-2 | CharacterVirtual integration and ground movement | A capsule walks, climbs steps and stops at walls | PHYS-1 | 7 |
| PHYS-3 | Debug-geometry extraction: Jolt's debug interface implemented inside Physics, written into Core DebugGeometry buffers | Bodies and shapes come out as plain lines and triangles; Physics links no Renderer | PHYS-1, CORE-3 | 8 |
| PHYS-4 | Jolt job system on the Core worker pool: a JobSystem adapter inside Physics, so Jolt never owns threads | A physics step on the pool matches a single-thread step; no thread is created outside the pool | PHYS-1, CORE-2 | 10 |
| PHYS-5 | Gameplay queries: ray, shape cast, overlap, per-worker query queues | Results are identical for a fixed scene with any worker count | PHYS-1, SIM-3 | 15 |
| SIM-1 | SimulationRuntime: 60 Hz master clock, 30/20/10 Hz domains, TickContext, the Simulation layer, domain metrics | Domain tick counts stay exact over a long run, with no drift | APP-1, CORE-2 | 3 |
| SIM-2 | Phases, commands and command queue, deferred mutation, event flush at barriers | Commands apply in phase order; facts are visible to the next phase | SIM-1, ECS-1, EVT-1 | 5 |
| SIM-3 | Phases on the worker pool, per-worker buffers, deterministic merge; the physics step runs through PHYS-4 | The same scene with one worker and with many gives identical results | SIM-2, PHYS-4 | 10 |
| SIM-4 | Client prediction subset: selected systems run on the client with RunicGame::Shared | The same inputs give the same result on client and server | SIM-3 | 14 |
| SIM-5 | Service port: an outbox of durable operations with stable operation IDs and an inbox of accepted results drained at safe ticks; the Engine interface the Platform services bridge implements, shipped as the header-only Runic::SimulationPort target (Core types only) | A fake port that delivers a result twice is applied once; no I/O callback touches Flecs | SIM-2, EVT-1 | 27 |
| SIM-6 | Replication registry: modules register replicated components and world-state values (schema, quantization profile, relevance); Network encodes only what is registered | A test component and a world-state value replicate without Network depending on the modules that own them | SIM-2 | 13 |

## Engine: GameplayAPI group

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| GPL-1 | Movement-mode registry and modes (walk, sprint, dash hook, glide, mount; Water registers swimming) with movement parameters from data; gliding reads wind once ENV-2 provides it | Modes switch without teleporting; parameters come from Game data | PHYS-2 | 14 |
| GPL-2 | Attributes, modifiers, resources and tags; death and kill facts when a vital resource reaches zero (Combat fills in the source) | Modifier order is deterministic; resources clamp | SIM-2, CNT-1 | 15 |
| GPL-3 | Restrictions: source-owned restriction sets, acquire and release by source, command-validation hooks | Blocked commands are rejected on the server; releasing a source clears everything it held | GPL-2 | 18 |
| GPL-4 | Item and equipment runtime that mirrors ledger containers | Equipping changes modifiers; the mirror matches the ledger after a reload | GPL-2 | 28 |
| GPL-5 | Interaction framework: interactables, reservation, interaction facts | Two players cannot reserve the same target | GPL-2, EVT-1 | 29 |
| GPL-6 | Factions and spawn primitives | A spawner keeps its population; faction checks drive hostility | GPL-2 | 31 |
| GPL-7 | Flight and climbing movement modes: flight inside flight-zone volumes with an altitude ceiling above which only gliding works, glide and flight as separate mount capabilities, flying vehicles, climbing mounts; restrictions can forbid flight and gliding | Above the ceiling flight stops and gliding takes over; outside a flight zone a flying mount only glides; a restriction blocks both; a mount climbs a marked wall | GPL-1, GPL-3, WORLD-2, ENV-2 | 44+ |
| GPL-8 | Routine spawning: when a cell activates, routine entities (escorts, caravans, ships, mob routines) spawn where their routine puts them; stored exceptions override the routine and fade back into it | A caravan appears at the right point of its route when a player arrives; a delayed escort rejoins its routine over time | GPL-6, ENV-6, WORLD-3 | 43a |
| CMB-1 | Ability toolkit: timelines and hit timing per skill (ServerTime, CappedRewind) | Timeline phases fire on exact ticks | GPL-2, CNT-1 | 16 |
| CMB-2 | Targeting modes (free aim, FriendlyLock, light aim-assist hooks), combat volumes and hit detection over Physics queries | A melee arc and a projectile each hit a dummy exactly once; FriendlyLock never selects a hostile | CMB-1, PHYS-5 | 16 |
| CMB-3 | Statuses and effects, cooldowns, resource costs | Durations, stacks and cooldowns expire on the right tick | CMB-1 | 17 |
| CMB-4 | Rule pipeline and behavior registries (fixed hooks for Game code); value variants selected by a per-hit context the Game defines | A test rule and a test behavior run in order; a value with a base and a contextual variant resolves by the hit context; the Engine holds no game rule | CMB-3 | 17 |
| CMB-5 | Damage pipeline, interception hooks, invulnerability windows | Interception follows hook order; invulnerability blocks hits | CMB-4, GPL-3 | 18 |
| CMB-6 | Threat table and crowd control (target cap, immunity windows) | A CC chain and its immunity follow the test data | CMB-5 | 19 |
| CMB-7 | Input queue: one queued action per character inside the game-set window, started on the first legal tick; game-named priority inputs skip it when the cancel window allows | An early input fires on the first legal tick; a stun in between cancels it | CMB-1, SIM-4 | 19 |
| CMB-8 | Foreign combat: volumes crossing an authority border become requests to the owner | The target is damaged exactly once, by its owner | CMB-5, DSIM-4 | 37 |
| FLOW-1 | Sequence runtime: trigger, condition and action registries, scheduler, timers on simulation time | A sequence waits, branches and acts only through commands and Gameplay APIs | GPL-5 | 29 |
| FLOW-2 | Termination path, restriction ownership, watchdog, presentation cues | Complete, abort, timeout, disconnect and shutdown all release everything the sequence held | FLOW-1, GPL-3 | 29 |
| FLOW-3 | Handoff policies and sequence state in the transfer snapshot | Transferable keeps its timers; PinAuthority blocks transfer; AbortOnTransfer cleans up | FLOW-2, DSIM-3 | 37 |
| QST-1 | Quest runtime and quest-creation API: definitions, objectives, triggers from facts, instances | A quest defined only in data advances from kill and interaction facts | FLOW-1 | 30 |
| QST-2 | Durable bridge: outgoing quest facts with stable IDs through the service port; accepted results applied at safe ticks | A replayed fact never doubles progress; no service callback touches Flecs | QST-1, SIM-5 | 30 |
| QST-3 | Quest instances registered as a transfer-snapshot contributor | A quest in progress crosses a simulation border with its objectives and timers intact | QST-1, DSIM-3 | 37 |

## Engine: world systems

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| PRC-1 | Light: emitters, ambient profiles, skylight curve driven by the Environment clock, luminosity at a point | Luminosity matches emitter falloff; client and server agree | WORLD-2, GPL-2, ENV-1 | 25 |
| PRC-2 | PvP visibility tiers (Full, Silhouette, Hidden) with reveal on contact | Tiers change at the configured distances and light levels | PRC-1 | 25 |
| PRC-3 | Perception profiles: warn and detect radii from light and concealment, with a minimum | Radii shrink with darkness and stealth but never below the minimum | PRC-1 | 33 |
| NAV-1 | Detour runtime, nav tiles streamed by cell, path queries behind MoveGoal | Paths cross cell borders; tiles load and unload with cells | WORLD-3, COOK-6 | 31 |
| NAV-2 | Flight navigation: 3D paths through free space over the sparse cells for flying actors, respecting volumes and altitude ceilings | A flying NPC reaches a target behind an obstacle without entering a forbidden volume | NAV-1, WORLD-2 | 44+ |
| AI-1 | AI contracts and runtime shell: components, intents, working memory, AIDefinition, BehaviorTree.CPP adapter, brain pool, wake and sleep | A test tree ticks only while its NPC is awake | GPL-2, SIM-3 | 32 |
| AI-2 | Command bridge and data assets: intents become movement and combat commands; tree loading, registries, personality profiles | AI commands pass the same validation as player commands | AI-1, NAV-1, CMB-2 | 32 |
| AI-3 | Perception lifecycle: Aggroable, warn and detect wake, target validation, leash, return home, sleep | A mob walks toward a warning, attacks on detection and returns past its leash | AI-1, PRC-3 | 33 |
| AI-4 | Reactive combat: SkillStarted perception, incoming threat, reaction timer, defense choice | A mob answers a telegraphed skill with a defensive action within its reaction time | AI-2, CMB-5 | 34 |
| AI-5 | Instrumentation: per-brain trace, memory, utility and intent diagnostics | The trace explains each decision | AI-2 | 34 |
| AI-6 | Scale tests with thousands of loaded NPCs and controlled awake percentages | The tick budget holds at the target counts | AI-4 | 38 |
| ENV-1 | Environment clock: world time, day and night phase, sun direction and the skylight input Perception reads; the world epoch comes from configuration until PCS-2 issues it | Two simulations agree on the time of day; night starts at the same world time everywhere | SIM-1, WORLD-1 | 25 |
| ENV-2 | Deterministic wind field with a cyclic baseline routine: per-region direction at any angle and a signed speed (0 has no effect; a negative speed reverses the push without changing the angle) | The same position and time give the same wind on two simulations; a negative speed reverses the push without changing its angle | ENV-1 | 42 |
| ENV-3 | Current modulation over time: cyclic baseline routine, tide phase, season, weather | Every simulation samples the same current at the same time | ENV-1, WATER-5 | 42 |
| ENV-4 | Tides and gameplay waves | A tide opens and closes a path on schedule | ENV-1 | 43 |
| ENV-5 | Weather state and time-boxed overrides (effective time and duration) for weather, wind and currents; the baseline routine returns when they end | A storm and a wind event reach two simulations at the same world time and end on schedule, after which the routine resumes | ENV-1 | 43 |
| ENV-6 | World routines: cooked routes and schedules evaluated as position = f(route, world time, seed), with slow deterministic drift (a cycle that does not match the clock plus a small seeded variation) | Two simulations compute the same routine position at the same world time, drift included | ENV-1 | 43a |
| WATER-1 | WaterSystem registry, planar water, WaterSample, cell fragments | Height and flow can be sampled anywhere; fragments load with cells | WORLD-3, PHYS-1 | 40 |
| WATER-2 | Spline rivers, flow, connections and routes | A descending river crosses two cells without a seam | WATER-1 | 40 |
| WATER-3 | BuoyantBody on Jolt bodies | A platform floats without drifting | WATER-1 | 41 |
| WATER-4 | Character and object water movement | A crate drifts with the current; a character swims with current influence | WATER-3, GPL-1 | 41 |
| WATER-5 | Ocean current fields from cooked vector grids | Current interpolates smoothly across the grid | WATER-1 | 42 |
| WATER-6 | WatercraftMovement: sail force, sail angle, lateral resistance | Faster downstream and with a favorable wind; a crosswind pushes sideways | WATER-4, ENV-2 | 42 |
| WATER-7 | Ship-group handoff: a watercraft, its crew, passengers and cargo registered as one transfer-snapshot group | A ship with passengers and cargo crosses a simulation border with nothing lost or left behind | WATER-6, DSIM-3 | 42b |

## Engine: networking

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| NET-1 | Transport interface, tick-safe ingress and egress queues, connect-ticket hook, the Network layer; the interface and hook ship as the header-only Runic::NetworkTransport target (Core types only) | A loopback transport passes ping and echo tests | SIM-1 | 12 |
| NET-2 | Snapshots: extraction, quantization, deltas against the last acknowledged state, interpolation buffers | Two clients see each other move smoothly under packet loss | NET-1, SIM-6 | 13 |
| NET-3 | Prediction and reconciliation: input history, replay, blend or snap | Movement stays responsive under simulated lag; corrections show in the debug view | NET-2, SIM-4 | 14 |
| NET-4 | Interest: candidates by cell, priority and budgets, visibility tiers applied | Only relevant entities arrive and per-client budgets hold | NET-2, WORLD-2, PRC-2 | 25 |
| NET-5 | Simulation links: ghost deltas and transfer messages encoded with the snapshot codec over simulation-to-simulation connections | Ghosts stay in sync across a border under packet loss; a message with a stale epoch is dropped | NET-2, DSIM-2 | 36 |
| NET-6 | Parallel replication and the degradation order | 500 bots in one region fit the per-client budget | NET-4, SIM-3 | 38 |
| NET-7 | Diagnostics layer: network and replication metrics recorded in the Core metrics registry; Editor attach channel over a development socket transport inside Network, streaming debug shapes and the traces modules publish in Core's debug-stream registry (compiled only into development and staging builds) | The Editor attaches to a development server and sees its shapes; a production build has no attach endpoint | NET-1, CORE-2 | 39 |
| NET-8 | World transitions: the session leaves one world's simulation after a checkpoint and joins another world's simulation with a new connect ticket; no handoff or ghosts across worlds | A character moves between two worlds with its checkpointed state, and the old simulation keeps no copy | NET-1, SIM-5, PCS-1 | 44+ |
| DSIM-1 | Authority state, epochs, GlobalEntityId mapping | Messages with a stale epoch are rejected | SIM-2 | 36 |
| DSIM-2 | Ghosts: ghost state near borders as plain data; Network encodes the deltas (NET-5) | Ghosts follow their owners near the border | DSIM-1 | 36 |
| DSIM-3 | Transfer: prepare, final snapshot, authority commit; a snapshot-contributor registry where modules add their state (sequences, quests, ship groups) | A player walks from A to B with no loading screen; the input sequence continues | DSIM-2 | 36 |
| DSIM-4 | Foreign-interaction contract | A request carries enough context for the owner to resolve it | DSIM-1 | 37 |
| DSIM-5 | Authority partitions inside one region (busy areas such as a city) that never cut an indivisible volume (an island, a castle, a docked ship) | Two simulations share a city, each entity has one owner, and no indivisible volume is split | DSIM-3 | 44+ |

## Engine: presentation

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| CLI-1 | Presentation layer and extractor registry; the first extractor writes entities as debug shapes; camera | Entities are drawn from the RenderFrame only | ECS-1, RND-2, CORE-3 | 5 |
| CLI-2 | Input mapping, actions and command builder | Key presses become movement commands | APP-2, SIM-2 | 7 |
| CLI-3 | Prediction layer driving the prediction subset and reconciliation each frame | The local player moves at once and corrects on server state | SIM-4, NET-3 | 14 |
| CLI-4 | Mesh-instance extractor with interpolation and camera-relative conversion | Cooked meshes appear at their entities' positions far from the origin | CLI-1, CORE-4 | 22 |
| CLI-5 | Skinned-instance extractor with poses from Animation | Characters animate in the debug pipeline | CLI-4, ANIM-1 | 23 |
| CLI-6 | World-anchor extractor: position, distance, visibility tier and priority per entity | Anchors follow entities; Hidden players produce none | CLI-1, CORE-4 | 23a |
| CLI-7 | Light, environment and volume extractors from Perception and Environment | Darkness and torches on screen match the server's light values | CLI-1, CORE-5, PRC-1 | 25a |
| CLI-8 | Water-surface and field-grid extractors from Water and Environment, change-driven | Grids re-extract only when the current or the wind changes | CLI-1, CORE-6, WATER-5 | 42a |
| RND-1 | bgfx device, view, camera, vertex-color shader, line batch | A grid and axes draw on Vulkan, the first backend; other backends follow when their shaders are built | APP-2 | 2 |
| RND-2 | Shapes, filled shapes with opacity, depth and overlay batches, debug text | Every debug shape renders batched in one frame | RND-1 | 2 |
| RND-3 | Debug pipeline layer registry and toggles; physics geometry layer with cached debug meshes | Collision shapes line up with gameplay shapes and each layer toggles | RND-2, PHYS-3, CORE-3 | 8 |
| RND-4 | Debug pipeline mesh layer: mesh instances, flat shading, camera-relative | Cooked meshes draw correctly far from the origin | RND-3, ASSET-1, CLI-4 | 22 |
| RND-5 | Debug pipeline skinned layer: skinned meshes with pose upload | An animated character renders | RND-4, CLI-5 | 23 |
| RND-6 | Production pipeline: materials, shaders, lighting and shadows, terrain, water shading, VFX, decals, UI renderer | Defined when a substantial game slice justifies it | RND-4 | 44+ |
| RND-7 | Debug pipeline layers for lights, environment and volumes | Torches, skylight, darkness zones and fog are visible and toggle | RND-4, CLI-7 | 25a |
| RND-8 | Debug pipeline layers for water surfaces and field grids (arrows, streaks, heatmaps) | A river surface and the ocean current draw from cooked data | RND-4, CLI-8 | 42a |
| RND-9 | Render targets and world quads for diegetic panels | A texture rendered offscreen appears on a quad in the world | RND-4 | 23a |
| RND-10 | Batched world-UI feature: instanced labels and bars from an atlas | 500 nameplates draw in one call | RND-9 | 38 |
| ANIM-1 | Skeletons, clips, animator, blending | Clips blend without pops | ASSET-1 | 23 |
| UI-1 | ImGui integration as the development overlay layer | The overlay exists only in development builds | APP-1, RND-1 | 2 |
| UI-2 | RmlUi integration as the GameUI layer; input reaches the UI first | UI clicks never become attacks | APP-1, RND-2 | 19 |
| UI-3 | Anchored world UI: RmlUi elements following world anchors, with distance scaling, culling and budgets | Nameplates follow entities and respect visibility tiers | UI-2, CLI-6 | 23a |
| UI-4 | Diegetic panels: RmlUi contexts rendered into world textures; input by ray into panel coordinates | A click on a world board reaches its RmlUi context and becomes a command | UI-3, RND-9 | 23a |
| UI-5 | Mode fallbacks: widgets move between diegetic, anchored, batched and hidden by distance, count and battle budgets; the atlas is built through RmlUi | In a 500-player fight the frame budget holds and the target keeps its full nameplate | UI-3, RND-10 | 38 |
| AUD-1 | Audio backend abstraction, buses, mixing, semantic sound events | A sound event plays through its bus with volume control | ASSET-1 | 24 |
| AUD-2 | MusicDirector: contexts, fallback chains, albums, presets, radio playback, local profile | A context change switches album and remembers the position | AUD-1 | 24 |

## Engine: tools

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| EDT-1 | Editor host, panel framework, world and ECS inspectors | The Editor opens a local world and inspects entities | APP-2, UI-1, SIM-2 | 6 |
| EDT-2 | Physics inspector | Bodies are selectable with their properties | EDT-1, PHYS-3 | 8 |
| EDT-3 | Resolver debugger | It shows which rules applied to a hit and why | EDT-1, CMB-4 | 17 |
| EDT-4 | Cooker launcher: background cooking with progress over a local protocol | A long cook never freezes the Editor | EDT-1, COOK-2 | 20 |
| EDT-5 | AI editor: behavior-tree canvas, palette, inspector, asset browser, XML save and load | A tree is edited, saved and reloaded | EDT-1, AI-2 | 35 |
| EDT-6 | AI tools: NPC definition and personality editors, content-ID validation, live NPC debugger | A running NPC's tree state is visible live | EDT-5, AI-5 | 35 |
| EDT-7 | Live attach to development and staging servers | Entity inspector and debug shapes come from a remote server | EDT-1, NET-7 | 39 |
| COOK-1 | First cooker proof: glTF through fastgltf into a Runic mesh and Jolt collision | A Blender mesh renders and is walkable | ASSET-1, PHYS-1 | 9 |
| COOK-2 | Validation of data-driven definitions | Invalid definitions fail the cook with clear errors | CNT-2 | 20 |
| COOK-3 | World cooking: regions, sectors, sparse 3D cells, authored volumes, manifests, packages, Zstandard | Two regions and a cave volume cook into random-access packages | COOK-1, WORLD-2 | 21 |
| COOK-4 | Textures (KTX2 and Basis), mesh LODs (meshoptimizer), tangents (MikkTSpace) | Textured meshes with LODs cook into client-only packages | COOK-1 | 22 |
| COOK-5 | Skinned meshes and animation clips | A rigged character and its clips cook | COOK-1 | 23 |
| COOK-6 | Navigation tiles with Recast | Tiles cook per cell | COOK-3 | 31 |
| COOK-7 | Water fragments and current fields | A river and an ocean field cook with their cells | COOK-3 | 40 |
| COOK-8 | Incremental cooking | Only changed inputs cook again | COOK-3 | 44+ |

## Platform

The server bindings implement the Engine's Layer interface and the Simulation's service port, and depend only on
Engine interface targets: Runic::Core, Runic::Application and Runic::SimulationPort (the service port), while the
Transport links Runic::NetworkTransport (the transport interface); the Engine never includes them. Those headers include only Core types, so a
Platform build configures Core and Application and never Flecs, Jolt or the gameplay modules. Platform CI builds them
against the Engine commit pinned as a submodule, so the Platform builds and tests on its own.

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| PCT-1 | Protobuf service APIs, fact messages and the connect-ticket format; C++ and Go code generation | Both languages round-trip the same message | BUILD-1 | 12 |
| PTR-1 | GameNetworkingSockets transport implementing the Engine transport interface | A client and a local server exchange reliable and unreliable messages | NET-1, PCT-1 | 12 |
| PTR-2 | Connect-ticket verification inside the simulation (signature, expiry, key ID) against a configured set of public keys; tickets are test-signed with a local key until PCS-1 issues real ones | Bad or expired tickets are rejected with no service call | PTR-1 | 12 |
| PTR-3 | Candidate connections (up to three), promotion, simulation-to-simulation links | Promotion happens with no loading screen | PTR-2, DSIM-3 | 36 |
| PTR-4 | Simulation links chosen by locality: an in-memory queue in one process, a local socket or shared-memory channel on one machine, GameNetworkingSockets between machines | The same transfer test passes on every channel with identical results; the link picks its channel from the peers' placement | PTR-3 | 44+ |
| PSDK-1 | Go service skeleton: configuration, gRPC wiring, metrics | A sample service answers a gRPC call and reports metrics | PCT-1 | 26 |
| PSDK-2 | Persistence: PostgreSQL, transactions, migrations, idempotency store | A retried operation commits once | PSDK-1 | 26 |
| PSDK-3 | NATS wiring: publish, subscribe, JetStream where replay is needed | A fact published once is consumed once per subscriber | PSDK-1 | 30 |
| PBND-1 | ServicesBridgeLayer: implements the Engine service port with asynchronous gRPC calls; completions queued for safe ticks | A slow service never delays a tick | PCT-1, APP-1, SIM-5 | 27 |
| PBND-2 | LifecycleLayer: Agones SDK ready, health and shutdown, plus a local stub mode for development without Agones | Against the Agones local SDK server the layer reports Ready, sends health pings and drains on shutdown; the stub mode runs with no Agones | APP-1 | 39 |
| PBND-3 | Prometheus exporter that publishes the Core metrics registry (tick-domain timings, and the network metrics the Diagnostics layer records) | A scrape returns time per tick domain in Prometheus format | CORE-2, SIM-1 | 39 |
| PBND-4 | NATS in the services bridge: durable facts published from the outbox; world directives and other subscriptions delivered to the inbox at safe ticks | A fact published from a tick reaches a test subscriber once; a slow broker never delays a tick | PBND-1, PSDK-3 | 30 |
| PCS-1 | Accounts and entitlements, GlobalPlayerId issue, login, connect tickets signed with rotating keys whose public halves the simulations load | Login returns a signed ticket the simulation accepts | PSDK-2, PTR-2 | 27 |
| PCS-2 | World Coordinator: worlds and region map, neighbor topology, handoff candidates, the world epoch | The Coordinator names the candidates for a border | PSDK-1 | 36 |
| PCS-3 | World directives with an effective time and a duration (weather, wind and current events) | A directive applies and expires at the same world time everywhere | PSDK-3, PBND-4 | 43 |
| PCS-4 | Instance allocation from Agones fleets | Defined when the expansion slices are planned | PCS-2, PINF-2 | 44+ |
| PCS-5 | Chat and party groups | Defined when the expansion slices are planned | PSDK-3 | 44+ |
| PCS-6 | Admission: per-world capacity limits, login queue and cohort waves for the launch phases | Defined when the expansion slices are planned | PCS-1 | 44+ |
| PCS-7 | Placement by load and traffic over Agones: scores by load, memory and inter-simulation traffic, with distance from the regions' global coordinates; moves high-traffic pairs onto one node by move-by-handoff, up to a per-node cap | Two simulations with heavy link traffic end on one node with no loading screen; no node passes its cap | PCS-2, PINF-2, PTR-4 | 44+ |
| PBB-1 | Item ledger: containers, slots, atomic transfers, operation IDs; mechanisms only | Concurrent transfers never duplicate or lose an item | PSDK-2 | 28 |
| PBB-2 | Currency wallets: balances and atomic transfers with operation IDs; mechanisms only | A balance changes once per operation ID under concurrent requests | PSDK-2 | 33a |
| PBB-3 | One-transaction transfers that move ledger items and wallet currency together; mechanisms only | Defined when the expansion slices are planned | PBB-1, PBB-2 | 44+ |
| PINF-1 | Local development stack (PostgreSQL, NATS) started by one command | The stack starts and the service tests pass against it | — | 26 |
| PINF-2 | Agones fleets, Kubernetes manifests, Prometheus and Grafana dashboards | A fleet runs in a development cluster, servers report Ready, and Grafana shows time per tick domain | PBND-2, PBND-3 | 39 |

## Game

Game work packages are defined in RunicGame `docs/game-packages.md`, not here, because their descriptions are Game
content. Their IDs are GAME-0, GSH-1 to GSH-7, GCNT-1, GCL-1 to GCL-4, GSVC-1 to GSVC-10, GSRV-1 to GSRV-8, GNAV-1 and
GWTH-1. They follow the same rules (one session, one pull request, one target; contract before code; Stage A first),
and the slices that need them are listed in [08-implementation-roadmap.md](08-implementation-roadmap.md). No Engine or
Platform package needs a Game package.
