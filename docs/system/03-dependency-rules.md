---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# 03 — Dependency rules

This file lists which code may depend on which code: the direction rules, every module with its allowed dependencies, the forbidden links, how the repositories are wired, and how CI enforces it.

Source: ARCH[^arch] v4 (RunicDocs: RunicEngine — Technical Architecture) § Repositories, build targets and processes
(Engine modules, Game modules, Platform modules), § Modules, work packages and the implementation contract (Dependency
rules, Automated checks), § Godot integration (Engine), § Decisions log. The CMake catalogs (`cmake/*Catalog.cmake`)
repeat the module tables below, and `checks.catalog_sync` keeps them equal.

## Direction rules

1. **Dependencies point one way.** RunicGame depends on the Engine and the Platform.
2. **The Platform implements Engine interfaces** (the transport, the service port, and the services-bridge and
   lifecycle layers) and links **only the Engine interface targets** `Runic::Core`[^engine-core], `Runic::Application`[^engine-application],
   `Runic::NetworkTransport`[^engine-network] and `Runic::SimulationPort`[^engine-simulation]
   ([Engine interface targets for the Platform](#engine-interface-targets-for-the-platform)). It never links Flecs,
   Jolt, Godot or the gameplay modules. Platform Go code needs no Engine code at all.
3. **The Engine never includes Platform or Game code.**
4. **Inside the Engine, low-level modules never depend on the feature modules that use them.** A feature plugs in by
   registering into a registry or interface owned by the lower module.
5. **The module tables below are a ceiling.** A package links only what it uses. A module dependency is `PUBLIC` only
   when its types appear in the module's public headers; third-party libraries are `PRIVATE` except the four approved
   exceptions ([02 § PUBLIC and PRIVATE dependencies](02-module-contract.md#public-and-private-dependencies)).
6. **A package's Needs orders merges; it does not grant a link.** Only the Depends on column grants links.
7. **Only `Public/` crosses a module boundary.** Never include another module's `Source/`.
8. **Godot is presentation and tooling only.** Only GodotBridge and GodotTools (and the game's editor library) talk to
   Godot; the server never links it, and the RenderFrame is the only door from the Runic client into Godot.

## Engine modules

26 modules in seven groups. "Packages" are defined in [12-work-packages.md](12-work-packages.md).

| Group | Module | Contains | Depends on | Headless | Packages |
| --- | --- | --- | --- | --- | --- |
| Foundation | Runic::Core[^engine-core] | Plain ID types, time types, math (GLM), memory, logging, config, worker pool, metrics registry, debug-stream registry; RenderData (plain render structs shared by the Client, GodotBridge and debug tools, including DebugGeometry and the debug-draw API) | — | yes | CORE-1 to CORE-7 |
| Foundation | Runic::Application[^engine-application] | Entry point, Application, ApplicationHost (Run and RunFrame), LayerStack, loop policies, application events; no window class (the window comes from Godot) | Core | yes | APP-1 |
| Foundation | Runic::Events[^engine-events] | Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs | Core | yes | EVT-1 |
| Foundation | Runic::Content[^engine-content] | Namespaced ContentId (engine:, game:), registries, versioned schemas, validation, loading, live reload | Core | yes | CNT-1, CNT-2 |
| Foundation | Runic::Assets[^engine-assets] | AssetId, handles, cache, native cooked formats, random-access packages with Zstandard decompression, manifests, client and server package selection, streaming | Core, Content | yes | ASSET-1, ASSET-2 |
| Simulation core | Runic::ECS[^engine-ecs] | Flecs integration and conventions | Core; Flecs is re-exported only to Engine modules that register systems | yes | ECS-1 |
| Simulation core | Runic::World[^engine-world] | 64-bit double coordinates (real Y, 1 cm minimum unit), worlds (WorldId), region-local origins, Regions, Sectors, sparse 3D Cells, authored volumes (including indivisible ones), spatial index and exact XYZ queries, activation volumes and streaming state | Core, ECS | yes | WORLD-1 to WORLD-3 |
| Simulation core | Runic::Physics[^engine-physics] | Jolt v5.6 in double precision behind Runic types: bodies, shapes, CharacterVirtual, queries, collision layers, debug-geometry extraction, the Jolt job system on the Core worker pool | Core, World (Jolt private) | yes | PHYS-1 to PHYS-5 |
| Simulation core | Runic::Simulation[^engine-simulation] | SimulationRuntime: multi-rate scheduler, phases, barriers, commands, event flush, worker integration, client prediction subset, the replication registry, the service port (outgoing durable operations, accepted results applied at safe ticks; header-only Runic::SimulationPort target); the Simulation layer | Core, Application, ECS, Events, World, Physics, Content | yes | SIM-1 to SIM-6 |
| GameplayAPI | Runic::Gameplay[^engine-gameplay] | Attributes, modifiers, resources, tags, death and kill facts, the movement-mode registry and modes, restrictions, interaction framework, item and equipment runtime, factions, spawn and routine spawning | Simulation, Environment[^engine-environment] (wind for gliding) | yes | GPL-1 to GPL-8 |
| GameplayAPI | Runic::Combat[^engine-combat] | Ability toolkit and timelines, hit timing per skill, one-slot input queue, targeting modes and volumes, hit detection, statuses and effects, cooldowns, rule pipeline, behavior registries, damage pipeline and interception, invulnerability windows, threat table, crowd control, foreign combat | Gameplay, Physics, Environment (projectile wind), DistributedSimulation[^engine-distributedsimulation] | yes | CMB-1 to CMB-8 |
| GameplayAPI | Runic::GameplayFlow[^engine-gameplayflow] | Sequences with trigger, condition and action registries, timers on simulation time, presentation cues, source-owned cleanup, handoff policies | Gameplay, DistributedSimulation (handoff snapshot) | yes | FLOW-1 to FLOW-3 |
| GameplayAPI | Runic::Quest[^engine-quest] | Quest runtime and the quest-creation API: definitions, objectives, triggers from facts, instances, durable-fact bridge | GameplayFlow, DistributedSimulation (handoff snapshot) | yes | QST-1 to QST-3 |
| World systems | Runic::Perception[^engine-perception] | Light emitters, ambient and skylight, luminosity queries, concealment, warn and detect radii, PvP visibility tiers | Gameplay, World, Environment (time of day, weather), Water[^engine-water] (underwater light) | yes | PRC-1 to PRC-3 |
| World systems | Runic::Navigation[^engine-navigation] | Detour and DetourTileCache runtime, nav tiles streamed by cell, path queries behind the MoveGoal contract, 3D flight paths for flying actors | World, Simulation, Assets (Detour private) | yes | NAV-1, NAV-2 |
| World systems | Runic::AI[^engine-ai] | BehaviorTree.CPP adapter, working memory, AI scheduling (wake, sleep, leash), intents to commands, reactive combat | Gameplay, Combat, Perception, Navigation (BehaviorTree.CPP private) | yes | AI-1 to AI-6 |
| World systems | Runic::Environment | Environment clock, wind and current routines (cyclic baseline, direction and signed speed), current modulation, tides, gameplay waves, weather state and time-boxed overrides, world routines (route positions from world time) | Core, World, Simulation | yes | ENV-1 to ENV-6 |
| World systems | Runic::Water | WaterSystem, bodies, rivers and routes, ocean current fields, cooked fragments, buoyancy, character, object and watercraft movement | World, Physics, Simulation, Gameplay, Environment, DistributedSimulation (ship-group handoff) | yes | WATER-1 to WATER-7 |
| Networking | Runic::Network[^engine-network] | Transport interface (header-only Runic::NetworkTransport target), ingress and egress queues, all wire encoding (snapshots, quantization, deltas, simulation-link messages such as ghost deltas and transfers), interest and priority budgets, prediction and reconciliation, parallel replication, world transitions; the Network and Diagnostics layers | Application, Simulation, DistributedSimulation, Gameplay, Perception | yes | NET-1 to NET-8 |
| Networking | Runic::DistributedSimulation | Authority state and epochs, ghost state as plain data (Network encodes it), transfer (prepare, final snapshot, commit) with a snapshot-contributor registry that other modules register into, foreign-interaction contract, authority partitions that never cut indivisible volumes | Simulation, Events, World | yes | DSIM-1 to DSIM-5 |
| Presentation | Runic::Client[^engine-client] | Client runtime: the Prediction and Presentation layers, the extractor registry and Engine extractors, camera, interpolation, input mapping and command builder; the RunicBot runtime | Application, Simulation, Network, Perception, Environment, Water, Animation[^engine-animation], Audio[^engine-audio], Assets | no | CLI-1 to CLI-9 |
| Presentation | Runic::GodotBridge[^engine-godotbridge] | The GodotLayer (LibGodot instance, window, input), the presenter (RenderFrame to RenderingServer), debug layers, world UI (anchored, diegetic, batched), the view-model binder, the audio backend on the AudioServer; the production look later | Application, Client, Audio, Assets (libgodot and godot-cpp private) | no | GDB-1 to GDB-13 |
| Presentation | Runic::Animation | Skeletons, clips, animator, blending, poses (scope set by the open animation decision) | Core, Assets | no | ANIM-1 |
| Presentation | Runic::Audio | Backend interface (implemented by GodotBridge on the AudioServer), buses and mixing, semantic sound events, MusicDirector, albums, soundtrack presets, local profiles | Core, Assets | no | AUD-1, AUD-2 |
| Tools | Runic::GodotTools[^engine-godottools] | RunicEditor plugins: marker nodes, schema inspectors and Runic file formats in the editor, world and region tools, docks (world, ECS, physics, resolver, AI editor on GraphEdit, live attach), the project template and assistants, Run orchestration, the export plugin that calls the Cooker | Most modules, Cooker[^engine-cooker] (libgodot editor build and godot-cpp private) | never shipped | GDT-1 to GDT-16 |
| Tools | Runic::Cooker | Cooking orchestration and Runic format writers: Jolt cooking, Recast, meshoptimizer for collision, Zstandard; reads geometry from the scenes Godot imported; runs inside the RunicEditor and headless on build machines | Core, Content, Assets, World, Physics, Navigation, Water (each runtime module owns its cooked format; the Cooker links its writer) | tool | COOK-1 to COOK-8 |

`Runic::GameplayAPI` is the umbrella target of the GameplayAPI group: it links Gameplay, Combat, GameplayFlow and
Quest so the Game links one name, while each module keeps its own boundary and tests.

Third-party library IDs in the catalog (`THIRD_PARTY`): glm (Core), zstd (Assets), flecs (ECS), jolt (Physics,
Cooker), detour (Navigation), behaviortree (AI), recast and meshoptimizer (Cooker), libgodot and godot-cpp
(GodotBridge, GodotTools), dl (GodotBridge's library loader). libgodot, godot-cpp, GodotBridge and GodotTools are
catalogued as **presentation**: a headless target that reaches any of them fails configure.

Approved public-header exceptions (exactly four): GLM math types in Core's public headers, the Flecs world handle for
modules that register systems (ECS, AI), BehaviorTree.CPP node base types for Game custom AI nodes (AI), and
godot-cpp types in GodotTools' public headers so the Game's editor library can extend marker nodes and docks.
Jolt, Detour and GameNetworkingSockets stay hidden, and Godot types appear nowhere else.

Seams fixed by these rows (decided in the architecture; do not reopen):

- **Combat** owns the one-slot input queue; Network only carries inputs.
- **Network** owns all wire encoding, including ghost deltas and transfer messages: it encodes every message on the
  realtime connection, envelope included. The Platform transport only carries bytes. Protobuf (Platform Contracts[^platform-contracts])
  carries only service APIs, published facts and the connect-ticket format, never per-tick traffic.
- **Simulation's service port** (SIM-5) is the Engine's only door to durable services; the Platform's
  ServicesBridgeLayer[^platform-serverbindings] implements it (gRPC calls in PBND-1, NATS in PBND-4).
- **Interfaces the Platform implements ship as header-only Engine targets with Core types only:**
  `Runic::NetworkTransport` (NET-1) and `Runic::SimulationPort` (SIM-5).
- **Physics** runs Jolt jobs on the Core worker pool (PHYS-4); Jolt never owns threads.
- **DistributedSimulation** carries other modules' state through a snapshot-contributor registry; GameplayFlow, Quest
  and Water register into it.
- **Simulation's replication registry** (SIM-6): modules register replicated components and world-state values
  (schema, quantization profile, relevance); Network encodes only what is registered, so it never depends on the
  modules that own the data.
- **Gameplay's movement-mode registry** (GPL-1): Gameplay owns walk, sprint, dash, glide and mount; Water registers
  swimming. Gameplay publishes death and kill facts when a vital resource reaches zero, and Combat fills in the source.
- **Core's debug-stream registry** (CORE-2): modules publish traces there; the Diagnostics layer streams them to the
  RunicEditor.
- **Godot never reads Physics or the simulation.** The client builds a list of what to show each frame (render
  extraction) and GodotBridge hands it to Godot. Physics fills plain debug-geometry buffers defined in Core and a
  GodotBridge debug layer draws them, so neither module links the other. Collision shapes and render meshes are
  different data.
- **Metrics:** every process records its metrics in the Core metrics registry (CORE-2); the Diagnostics layer (NET-7)
  adds network and replication metrics; the Platform exporter (PBND-3) publishes the Core registry. The RunicEditor
  attach channel is compiled only into development and staging builds.
- **No window class.** The window, input, rendering, audio and player UI come from Godot through the GodotLayer
  (GDB-1). There are no Platform, Input, Debug, Renderer, UI or Editor modules in the Engine: no bgfx, GLFW, RmlUi or
  ImGui.

## Platform modules

| Module | Contains | Depends on | Packages |
| --- | --- | --- | --- |
| Transport (C++) | GameNetworkingSockets implementation of the Engine transport interface; connect-ticket checks; authoritative and candidate connections; simulation-to-simulation links chosen by locality (in-memory queue, local socket or shared memory, GameNetworkingSockets) | Runic::NetworkTransport[^engine-network] (the Engine transport interface), Contracts[^platform-contracts] | PTR-1[^platform-transport] to PTR-4 |
| Contracts | Protobuf service APIs, published facts and the connect-ticket format, generated for C++ and Go; games add their own messages. Realtime messages are encoded by Runic::Network, not Protobuf | — | PCT-1 |
| Server bindings[^platform-serverbindings] (C++) | ServicesBridgeLayer (implements the Engine service port with asynchronous gRPC and NATS; completions queued for safe ticks), LifecycleLayer (Agones SDK), Prometheus exporter for the Core metrics registry; they implement the Engine's Layer interface | Engine interface targets only: Runic::Application[^engine-application] (Layer), Runic::SimulationPort[^engine-simulation] (service port), Runic::Core[^engine-core] (metrics registry); Contracts | PBND-1 to PBND-4 |
| Go SDK[^platform-gosdk] | Service skeleton, configuration, gRPC and NATS wiring, persistence (PostgreSQL), transactions, migrations, idempotency store, metrics | Contracts | PSDK-1 to PSDK-3 |
| Core services[^platform-coreservices] (Go) | Accounts and entitlements, connect tickets, World Coordinator (worlds, regions and placement by load and traffic), instance allocation, world directives, chat and party groups, admission | Go SDK | PCS-1 to PCS-7 |
| Building blocks[^platform-buildingblocks] (Go) | Item ledger (containers, slots, atomic transfers, operation IDs), currency wallets and transfers that move items and currency in one transaction; mechanisms only, with no game rules | Go SDK | PBB-1 to PBB-3 |
| Infrastructure | Local development stack, Agones fleets, Kubernetes manifests, Prometheus and Grafana dashboards | — | PINF-1[^platform-infrastructure], PINF-2 |

- Platform C++ targets are named `RunicPlatform::<Module>`.
- Transport and the Go SDK depend on Contracts; Core services and Building blocks depend on the Go SDK.
- Platform CI builds the C++ bindings against the Engine commit pinned as a submodule, so the Platform builds and
  tests on its own. Because its Engine links are only the interface targets below, that standalone build configures
  the Engine's helpers (and, from CORE-1, Core and Application) and never Flecs, Jolt, Godot or the gameplay modules.
- The Platform owns mechanisms, never game policy.

### Engine interface targets for the Platform

| Target | Exported by | Contains | Package |
| --- | --- | --- | --- |
| `Runic::Core` | Core | Plain ID and time types, the metrics registry (the exporter publishes it) | CORE-1, CORE-2 |
| `Runic::Application` | Application | The Layer interface the services-bridge and lifecycle layers implement | APP-1 |
| `Runic::NetworkTransport` | Network | Header-only: the transport interface and the connect-ticket hook; Core types only | NET-1 |
| `Runic::SimulationPort` | Simulation | Header-only: the service port (outbox of durable operations, inbox of accepted results); Core types only | SIM-5 |

| Platform module | Engine targets it may link |
| --- | --- |
| Transport (C++) | `Runic::NetworkTransport` (and `Runic::Core` through it) |
| Server bindings (C++) | `Runic::Core`, `Runic::Application`, `Runic::SimulationPort` |
| Contracts, Go SDK, Core services, Building blocks, Infrastructure | None |

## Game targets

The Game modules of the architecture; the concrete rules inside each target are defined in RunicGame. RunicGame is a
RunicEngine project (project.godot, runic.project.json).

| Target | Contains | Depends on |
| --- | --- | --- |
| RunicGame::Shared | Rules compiled into both executables. GameCombat: skills mapped to the ability toolkit, damage types and formula, resolver order, defense, threat, custom behaviors. GameBuilds: ten fixed loadouts, named classes (loadout + Discipline, one role each), weapon skills unlocked by level and one merged tree per class with points in the Discipline skills, context kits (mount and aerial trees), armor rules. Movement parameters, item rules, client-safe content definitions | Runic::GameplayAPI, Perception[^engine-perception] |
| RunicGame::Server | Server-only rules: GameCore, GameWorld, GameAI, GameQuests, GameEncounters, GameNaval, GameInstances, GameExploration, GameProduction (expansion); spawns, loot, world events, service clients, persistence bridge | Shared, Runic::Simulation[^engine-simulation], Network[^engine-network], AI[^engine-ai], Water[^engine-water], Environment[^engine-environment]; Platform server bindings[^platform-serverbindings] and contracts |
| RunicGame::Client | GameClientPresentation: camera rules, animation, VFX and audio binding, cutscene presentation. GameUI: Godot UI scenes bound to view models, including the map with fog of war. Audio content, input bindings | Shared, Runic::Client[^engine-client], GodotBridge[^engine-godotbridge], Animation[^engine-animation], Audio[^engine-audio] |
| RunicGame::Editor | The game's marker nodes, docks and inspectors, built as the project's editor library and loaded by the RunicEditor | Runic::GodotTools[^engine-godottools] |
| GameServer (executable `RunicGameServer`) | Composes the server layers: the Engine's Network, Simulation and Diagnostics layers and the Platform's services-bridge and lifecycle layers | RunicGame::Server, Runic::Application[^engine-application], Platform transport and server bindings |
| GameClient (executable `RunicGameClient`) | Composes the client layers, the GodotLayer on top; loads libgodot (editor target in development, template when shipping) | RunicGame::Client, Runic::Application, Platform transport |
| Game services (Go) | The named Game services (list in [07 § Services](07-service-backed-systems.md#services)) | Platform Go SDK[^platform-gosdk], item ledger[^platform-buildingblocks], wallets |
| Game content | Scenes, assets and data authored in the RunicEditor (Content/, Data/), split into Shared, Client and Server, cooked by the Cooker and exported as the client .pck | — |

- `RunicGame::Shared` and `RunicGame::Server` are static libraries linked into the server; GameCore owns the
  registration entry point.
- `RunicGame::Client` may use `Shared` but never `Server` or server-only content.
- `RunicGame::Editor` is never shipped; the client never links it.

## Forbidden dependencies

| Forbidden dependency | Why |
| --- | --- |
| Engine → Platform or Game | Wrong direction: the Platform implements Engine interfaces and the Game composes both |
| Engine → Agones, Kubernetes, gRPC, NATS, databases | Engine code runs locally and headless with no infrastructure; the Platform supplies those layers |
| Physics → Combat, Gameplay or Game | Physics answers geometric questions; it never knows damage, factions, blocks or dodges |
| Physics ↔ GodotBridge | Physics extracts debug geometry as plain data and GodotBridge draws plain data; neither links the other |
| GodotBridge or Godot → Simulation or Physics state | Godot draws an extracted frame; it never becomes game logic |
| Simulation → feature modules or Game code | Gameplay modules and the Game register systems into the Simulation, which stays reusable |
| AI → direct combat, physics or persistence mutation | AI emits intents that become validated commands |
| GameplayFlow[^engine-gameplayflow] or Quest → Combat or AI internals | Sequences act through Simulation commands and Gameplay APIs, the same doors players use |
| Platform → Game rules | The Platform stays reusable: mechanisms, never policy |
| Game client → server-only rules or content | The client gets presentation data and request contracts only |
| Region, Sector or Cell → simulation process identity | The world partition is stable; which simulation owns what is an operational decision |
| Any module → another module's Source/ | Only Public/ headers cross a module boundary |
| Headless modules → libgodot, godot-cpp, GodotBridge, GodotTools, audio backend | The server stays headless |
| Domain modules → Godot or godot-cpp types | Godot is a presentation and tool library; only GodotBridge and GodotTools include it |
| GameClient → GodotTools or Godot's editor build | Editor code never ships to players |
| Player UI scenes → Flecs or simulation state | The UI reads world anchors from the RenderFrame and content from Game view models |
| Runic::AI[^engine-ai] → GodotTools, editor docks, client presentation | The RunicEditor's AI docks depend on AI contracts, never the reverse |
| Runic::Audio[^engine-audio] → combat, AI, quests, simulation authority, Platform networking or persistence | Audio is client presentation only and depends only on Core and Assets |

## Toolchain libraries per executable

A library can live in a third-party folder and still be tool-only; build targets keep toolchain libraries out of the
server and the shipping client.

| Executable | Links |
| --- | --- |
| GameClient | godot-cpp; libgodot loaded at runtime (editor target in development, template target when shipping); Jolt (prediction and local collision), Flecs, Zstandard, Engine runtime |
| GameServer | Jolt, Flecs, Detour, Zstandard, Engine runtime; no Godot and no graphics library |
| RunicEditor | godot-cpp; libgodot editor target loaded at runtime; GodotTools and the Cooker's libraries |
| Cooker (inside the RunicEditor, or headless on build machines) | Jolt cooking, Recast, meshoptimizer, Zstandard, Runic format writers; geometry comes from the scenes Godot imported (no second glTF importer) |

## Repository wiring

Decided: **hybrid**.

| Repository | Pins as git submodules | Why |
| --- | --- | --- |
| RunicGame (superproject) | RunicEngine and RunicPlatform | CI and releases build from exact pinned commits |
| RunicPlatform | RunicEngine | Its C++ bindings build and test on their own |
| RunicEngine | Godot and godot-cpp, at a release tag (`external/godot`, `external/godot-cpp`, shallow) | libgodot is built from source in the editor and template targets; godot-cpp is compiled into the Runic executables |

For daily work a CMake option points at sibling checkouts instead of the submodules:

| Option | Points at |
| --- | --- |
| `RUNIC_ENGINE_DIR` | A RunicEngine checkout (typically `../RunicEngine`) |
| `RUNIC_PLATFORM_DIR` | A RunicPlatform checkout (typically `../RunicPlatform`) |
| `RUNIC_GODOT_SOURCE_DIR`, `RUNIC_GODOT_CPP_SOURCE_DIR` | Another Godot or godot-cpp checkout (default: the Engine's submodules) |

BUILD-1[^doc-build-build-1] sets up the pins and the options in all three repositories.

## CPU baseline

Decided: **x86-64-v3 (AVX2, FMA)** for servers and clients. The Jolt target is version 5.6.0 (tag `v5.6.0`), pinned
in the build, built in double precision (`JPH_DOUBLE_PRECISION`) with its AVX2 path enabled; the Engine builds with
AVX2 on.

This is a target, not a statement about the current pins. BUILD-1[^doc-build-build-1] delivers the CPU baseline and PHYS-1[^engine-physics] delivers Jolt
in double precision. Until they are merged, a repository may still pin an older Jolt; its `docs/current-state.md`
records the actual pin as a known gap.

## How CI enforces the rules

BUILD-1[^doc-build-build-1] delivers these checks in all three repositories (ARCH[^arch] § Automated checks).

| Check | Catches |
| --- | --- |
| Headless build of GameServer | Godot, UI, audio or editor code reaching the server |
| Dependency check per module | Links outside the allowed list; includes of another module's Source/ |
| Public-header scan | Library types in Public/ (outside the four approved exceptions), and Godot types outside GodotBridge and GodotTools |
| Client export check | Scripts, editor-only classes or Tools/ content in the client .pck (from COOK-4[^engine-cooker]) |
| Module tests | The Done when behavior of every merged package |
| Docs check | A code change without its ARCHITECTURE.md or package-status update |
| Workflow pin check | A GitHub Action referenced by tag or branch instead of a full commit SHA |

- The **headless check** walks the link closure of the server target and of every headless module at the end of
  configure, and fails if libgodot, godot-cpp, GodotBridge, GodotTools, an audio backend or editor code is reached.
- The **forbidden-link check** fails configure when a module links something outside its row in the tables above.
- A failing check is fixed by changing the code, never by widening a module's allowed list. Widening the list is an
  owner decision recorded in the architecture first.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Engine modules:** [[../../../RunicEngine/docs/modules/Core|Core]] · [[../../../RunicEngine/docs/modules/Application|Application]] · [[../../../RunicEngine/docs/modules/Events|Events]] · [[../../../RunicEngine/docs/modules/Content|Content]] · [[../../../RunicEngine/docs/modules/Assets|Assets]] · [[../../../RunicEngine/docs/modules/ECS|ECS]] · [[../../../RunicEngine/docs/modules/World|World]] · [[../../../RunicEngine/docs/modules/Physics|Physics]] · [[../../../RunicEngine/docs/modules/Simulation|Simulation]] · [[../../../RunicEngine/docs/modules/Gameplay|Gameplay]] · [[../../../RunicEngine/docs/modules/Combat|Combat]] · [[../../../RunicEngine/docs/modules/GameplayFlow|GameplayFlow]] · [[../../../RunicEngine/docs/modules/Quest|Quest]] · [[../../../RunicEngine/docs/modules/Perception|Perception]] · [[../../../RunicEngine/docs/modules/Navigation|Navigation]] · [[../../../RunicEngine/docs/modules/AI|AI]] · [[../../../RunicEngine/docs/modules/Environment|Environment]] · [[../../../RunicEngine/docs/modules/Water|Water]] · [[../../../RunicEngine/docs/modules/Network|Network]] · [[../../../RunicEngine/docs/modules/DistributedSimulation|DistributedSimulation]] · [[../../../RunicEngine/docs/modules/Client|Client]] · [[../../../RunicEngine/Engine/Presentation/GodotBridge/ARCHITECTURE|GodotBridge]] · [[../../../RunicEngine/docs/modules/Animation|Animation]] · [[../../../RunicEngine/docs/modules/Audio|Audio]] · [[../../../RunicEngine/Engine/Tools/GodotTools/ARCHITECTURE|GodotTools]] · [[../../../RunicEngine/docs/modules/Cooker|Cooker]] · [[../../../RunicEngine/docs/modules/Renderer|Renderer]] · [[../../../RunicEngine/docs/modules/UI|UI]] · [[../../../RunicEngine/docs/modules/Editor|Editor]]
- **Platform modules:** [[../../../RunicPlatform/docs/modules/Transport|Transport]] · [[../../../RunicPlatform/docs/modules/Contracts|Contracts]] · [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] · [[../../../RunicPlatform/docs/modules/GoSDK|GoSDK]] · [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] · [[../../../RunicPlatform/docs/modules/BuildingBlocks|BuildingBlocks]] · [[../../../RunicPlatform/docs/modules/Infrastructure|Infrastructure]]
- **Game:** [[../../../RunicGame/docs/game-packages|Game work packages]]
- **See also:** [[../system/README|System README]] · [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture" (v4, in the RunicDocs vault): the architecture source of truth
[^engine-core]: [[../../../RunicEngine/docs/modules/Core|Core]] — Engine module · Foundation
[^engine-application]: [[../../../RunicEngine/docs/modules/Application|Application]] — Engine module · Foundation
[^engine-network]: [[../../../RunicEngine/docs/modules/Network|Network]] — Engine module · Networking
[^engine-simulation]: [[../../../RunicEngine/docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-events]: [[../../../RunicEngine/docs/modules/Events|Events]] — Engine module · Foundation
[^engine-content]: [[../../../RunicEngine/docs/modules/Content|Content]] — Engine module · Foundation
[^engine-assets]: [[../../../RunicEngine/docs/modules/Assets|Assets]] — Engine module · Foundation
[^engine-ecs]: [[../../../RunicEngine/docs/modules/ECS|ECS]] — Engine module · Simulation core
[^engine-world]: [[../../../RunicEngine/docs/modules/World|World]] — Engine module · Simulation core
[^engine-physics]: [[../../../RunicEngine/docs/modules/Physics|Physics]] — Engine module · Simulation core
[^engine-gameplay]: [[../../../RunicEngine/docs/modules/Gameplay|Gameplay]] — Engine module · GameplayAPI
[^engine-environment]: [[../../../RunicEngine/docs/modules/Environment|Environment]] — Engine module · World systems
[^engine-combat]: [[../../../RunicEngine/docs/modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-distributedsimulation]: [[../../../RunicEngine/docs/modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-gameplayflow]: [[../../../RunicEngine/docs/modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-quest]: [[../../../RunicEngine/docs/modules/Quest|Quest]] — Engine module · GameplayAPI
[^engine-perception]: [[../../../RunicEngine/docs/modules/Perception|Perception]] — Engine module · World systems
[^engine-water]: [[../../../RunicEngine/docs/modules/Water|Water]] — Engine module · World systems
[^engine-navigation]: [[../../../RunicEngine/docs/modules/Navigation|Navigation]] — Engine module · World systems
[^engine-ai]: [[../../../RunicEngine/docs/modules/AI|AI]] — Engine module · World systems
[^engine-client]: [[../../../RunicEngine/docs/modules/Client|Client]] — Engine module · Presentation
[^engine-animation]: [[../../../RunicEngine/docs/modules/Animation|Animation]] — Engine module · Presentation
[^engine-audio]: [[../../../RunicEngine/docs/modules/Audio|Audio]] — Engine module · Presentation
[^engine-godotbridge]: [[../../../RunicEngine/Engine/Presentation/GodotBridge/ARCHITECTURE|GodotBridge]] — Engine module · Presentation
[^engine-godottools]: [[../../../RunicEngine/Engine/Tools/GodotTools/ARCHITECTURE|GodotTools]] — Engine module · Tools
[^engine-cooker]: [[../../../RunicEngine/docs/modules/Cooker|Cooker]] — Engine module · Tools
[^platform-contracts]: [[../../../RunicPlatform/docs/modules/Contracts|Contracts]] — Platform module · Protobuf
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^platform-transport]: [[../../../RunicPlatform/docs/modules/Transport|Transport]] — Platform module · C++
[^platform-gosdk]: [[../../../RunicPlatform/docs/modules/GoSDK|GoSDK]] — Platform module · Go
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^platform-buildingblocks]: [[../../../RunicPlatform/docs/modules/BuildingBlocks|BuildingBlocks]] — Platform module · Go
[^platform-infrastructure]: [[../../../RunicPlatform/docs/modules/Infrastructure|Infrastructure]] — Platform module · configuration
[^doc-build-build-1]: [[../build/BUILD-1|BUILD-1 contract]] — this repository
