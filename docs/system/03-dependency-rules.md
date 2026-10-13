# 03 — Dependency rules

This file lists which code may depend on which code: the direction rules, every module with its allowed dependencies, the forbidden links, how the repositories are wired, and how CI enforces it.

Source: ARCH § Repositories, build targets and processes (Engine modules, Game modules, Platform modules),
§ Modules, work packages and the implementation contract (Dependency rules, Automated checks), § Decisions log.

## Direction rules

1. **Dependencies point one way.** RunicGame depends on the Engine and the Platform.
2. **The Platform implements Engine interfaces** (the transport, the service port, and the services-bridge and
   lifecycle layers) and links **only the Engine interface targets** `Runic::Core`, `Runic::Application`,
   `Runic::NetworkTransport` and `Runic::SimulationPort`
   ([Engine interface targets for the Platform](#engine-interface-targets-for-the-platform)). It never links Flecs,
   Jolt or the gameplay modules. Platform Go code needs no Engine code at all.
3. **The Engine never includes Platform or Game code.**
4. **Inside the Engine, low-level modules never depend on the feature modules that use them.** A feature plugs in by
   registering into a registry or interface owned by the lower module.
5. **The module tables below are a ceiling.** A package links only what it uses. A module dependency is `PUBLIC` only
   when its types appear in the module's public headers; third-party libraries are `PRIVATE` except the approved exceptions
   ([02 § PUBLIC and PRIVATE dependencies](02-module-contract.md#public-and-private-dependencies)).
6. **A package's Needs orders merges; it does not grant a link.** Only the Depends on column grants links.
7. **Only `Public/` crosses a module boundary.** Never include another module's `Source/`.

## Engine modules

27 modules in seven groups. "Packages" are defined in [12-work-packages.md](12-work-packages.md).

| Group | Module | Contains | Depends on | Headless | Packages |
| --- | --- | --- | --- | --- | --- |
| Foundation | Runic::Core | Plain ID types, time types, math (GLM), memory, logging, config, worker pool, metrics registry, debug-stream registry; RenderData (plain render structs shared by the Client, both render pipelines, the world UI and debug tools, including DebugGeometry) | — | yes | CORE-1 to CORE-7 |
| Foundation | Runic::Application | Entry point, Application, ApplicationHost, LayerStack, loop policies, application events; the Window class (GLFW, compiled only in client and tools builds) | Core | yes (Window excluded) | APP-1, APP-2 |
| Foundation | Runic::Events | Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs | Core | yes | EVT-1 |
| Foundation | Runic::Content | Namespaced ContentId (`engine:`, `game:`), registries, versioned schemas, validation, loading, live reload | Core | yes | CNT-1, CNT-2 |
| Foundation | Runic::Assets | AssetId, handles, cache, native cooked formats, random-access packages with Zstandard decompression, manifests, client and server package selection, streaming | Core, Content | yes | ASSET-1, ASSET-2 |
| Simulation core | Runic::ECS | Flecs integration and conventions | Core; Flecs is re-exported only to Engine modules that register systems | yes | ECS-1 |
| Simulation core | Runic::World | 64-bit double coordinates (real Y, 1 cm minimum unit), worlds (WorldId), region-local origins, Regions, Sectors, sparse 3D Cells, authored volumes (including indivisible ones), spatial index and exact XYZ queries, activation volumes and streaming state | Core, ECS | yes | WORLD-1 to WORLD-3 |
| Simulation core | Runic::Physics | Jolt v5.6 in double precision behind Runic types: bodies, shapes, CharacterVirtual, queries, collision layers, debug-geometry extraction, the Jolt job system on the Core worker pool | Core, World (Jolt private) | yes | PHYS-1 to PHYS-5 |
| Simulation core | Runic::Simulation | SimulationRuntime: multi-rate scheduler, phases, barriers, commands, event flush, worker integration, client prediction subset, the replication registry, the service port (outgoing durable operations, accepted results applied at safe ticks; header-only Runic::SimulationPort target); the Simulation layer | Core, Application, ECS, Events, World, Physics, Content | yes | SIM-1 to SIM-6 |
| GameplayAPI | Runic::Gameplay | Attributes, modifiers, resources, tags, death and kill facts, the movement-mode registry and modes, restrictions, interaction framework, item and equipment runtime, factions, spawn and routine spawning | Simulation, Environment (wind for gliding) | yes | GPL-1 to GPL-8 |
| GameplayAPI | Runic::Combat | Ability toolkit and timelines, hit timing per skill, one-slot input queue, targeting modes and volumes, hit detection, statuses and effects, cooldowns, rule pipeline, behavior registries, damage pipeline and interception, invulnerability windows, threat table, crowd control, foreign combat | Gameplay, Physics, Environment (projectile wind), DistributedSimulation | yes | CMB-1 to CMB-8 |
| GameplayAPI | Runic::GameplayFlow | Sequences with trigger, condition and action registries, timers on simulation time, presentation cues, source-owned cleanup, handoff policies | Gameplay, DistributedSimulation (handoff snapshot) | yes | FLOW-1 to FLOW-3 |
| GameplayAPI | Runic::Quest | Quest runtime and the quest-creation API: definitions, objectives, triggers from facts, instances, durable-fact bridge | GameplayFlow, DistributedSimulation (handoff snapshot) | yes | QST-1 to QST-3 |
| World systems | Runic::Perception | Light emitters, ambient and skylight, luminosity queries, concealment, warn and detect radii, PvP visibility tiers | Gameplay, World, Environment (time of day, weather), Water (underwater light) | yes | PRC-1 to PRC-3 |
| World systems | Runic::Navigation | Detour and DetourTileCache runtime, nav tiles streamed by cell, path queries behind the MoveGoal contract, 3D flight paths for flying actors | World, Simulation, Assets (Detour private) | yes | NAV-1, NAV-2 |
| World systems | Runic::AI | BehaviorTree.CPP adapter, working memory, AI scheduling (wake, sleep, leash), intents to commands, reactive combat | Gameplay, Combat, Perception, Navigation (BehaviorTree.CPP private) | yes | AI-1 to AI-6 |
| World systems | Runic::Environment | Environment clock, wind and current routines (cyclic baseline, direction and signed speed), current modulation, tides, gameplay waves, weather state and time-boxed overrides, world routines (route positions from world time) | Core, World, Simulation | yes | ENV-1 to ENV-6 |
| World systems | Runic::Water | WaterSystem, bodies, rivers and routes, ocean current fields, cooked fragments, buoyancy, character, object and watercraft movement | World, Physics, Simulation, Gameplay, Environment, DistributedSimulation (ship-group handoff) | yes | WATER-1 to WATER-7 |
| Networking | Runic::Network | Transport interface (header-only Runic::NetworkTransport target), ingress and egress queues, all wire encoding (snapshots, quantization, deltas, simulation-link messages such as ghost deltas and transfers), interest and priority budgets, prediction and reconciliation, parallel replication, world transitions; the Network and Diagnostics layers | Application, Simulation, DistributedSimulation, Gameplay, Perception | yes | NET-1 to NET-8 |
| Networking | Runic::DistributedSimulation | Authority state and epochs, ghost state as plain data (Network encodes it), transfer (prepare, final snapshot, commit) with a snapshot-contributor registry that other modules register into, foreign-interaction contract, authority partitions that never cut indivisible volumes | Simulation, Events, World | yes | DSIM-1 to DSIM-5 |
| Presentation | Runic::Client | Client runtime: the Prediction and Presentation layers, the extractor registry and Engine extractors, camera, interpolation, input mapping and command builder | Application, Simulation, Network, Perception, Environment, Water, Renderer, Animation, Audio, UI, Assets | no | CLI-1 to CLI-8 |
| Presentation | Runic::Renderer | bgfx device and views; the debug pipeline with toggled layers (shapes, physics geometry, meshes, skinned meshes, lights and volumes, water and fields); render targets for diegetic UI; the batched world-UI feature; the production pipeline later | Core, Assets (bgfx private) | no | RND-1 to RND-10 |
| Presentation | Runic::Animation | Skeletons, clips, animator, blending, poses | Core, Assets | no | ANIM-1 |
| Presentation | Runic::UI | RmlUi (GameUI) and ImGui (development overlay) integrations as layers; world UI: diegetic panels, anchored elements, batched fallback, budgets, input routing | Application, Renderer, Assets (RmlUi, ImGui private) | no | UI-1 to UI-5 |
| Presentation | Runic::Audio | Backend abstraction, buses and mixing, semantic sound events, MusicDirector, albums, soundtrack presets, local profiles | Core, Assets | no | AUD-1, AUD-2 |
| Tools | Runic::Editor | Editor host and panels, world, ECS and physics inspectors, resolver debugger, AI editor (behavior-tree canvas, asset browser, personality editor, live AI debugger), live attach, Cooker launcher | Most modules (ImGui, imnodes private) | never shipped | EDT-1 to EDT-7 |
| Tools | Runic::Cooker | Cooking orchestration and Runic format writers using fastgltf, meshoptimizer, MikkTSpace, xatlas, KTX and Basis Universal, Jolt cooking, Recast and Zstandard | Core, Content, Assets, World, Physics, Navigation, Water (each runtime module owns its cooked format; the Cooker links its writer) | tool | COOK-1 to COOK-8 |

`Runic::GameplayAPI` is the umbrella target of the GameplayAPI group: it links Gameplay, Combat, GameplayFlow and
Quest so the Game links one name, while each module keeps its own boundary and tests.

Seams fixed by these rows (decided in ARCH v3; do not reopen):

- **Combat** owns the one-slot input queue; Network only carries inputs.
- **Network** owns all wire encoding, including ghost deltas and transfer messages: it encodes every message on the
  realtime connection, envelope included. The Platform transport only carries bytes. Protobuf (Platform Contracts)
  carries only service APIs, published facts and the connect-ticket format, never per-tick traffic.
- **Simulation's service port** (SIM-5) is the Engine's only door to durable services; the Platform's
  ServicesBridgeLayer implements it (gRPC calls in PBND-1, NATS in PBND-4).
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
  Editor.
- **Edges added in ARCH revision 369:** Gameplay → Environment (wind for gliding), Combat → Environment (projectile
  wind), Perception → Water (underwater light), Navigation → Assets, and Cooker → Physics, Navigation and
  Water (each runtime module owns its cooked format; the Cooker links its writer). ECS re-exports Flecs only to the
  Engine modules that register systems.
- **The Renderer never reads Physics or the simulation.** The Client extracts plain RenderData each frame; Physics
  fills plain DebugGeometry buffers defined in Core; neither Physics nor Renderer links the other. Collision shapes
  and render meshes are different data.
- **Metrics:** every process records its metrics in the Core metrics registry (CORE-2); the Diagnostics layer (NET-7)
  adds network and replication metrics; the Platform exporter (PBND-3) publishes the Core registry. The Editor attach
  channel runs over a development socket transport inside Network and is compiled only into development and staging
  builds.
- **Window** is a class in Application, not a module. There are no Platform, Input or Debug modules in the Engine.
- **ImGui** is private to UI (development overlay, client development builds) and Editor; never on servers.

## Platform modules

| Module | Contains | Depends on | Packages |
| --- | --- | --- | --- |
| Transport (C++) | GameNetworkingSockets implementation of the Engine transport interface; connect-ticket checks; authoritative and candidate connections; simulation-to-simulation links chosen by locality (in-memory queue, local socket or shared memory, GameNetworkingSockets) | Runic::NetworkTransport (the Engine transport interface), Contracts | PTR-1 to PTR-4 |
| Contracts | Protobuf service APIs, published facts and the connect-ticket format, generated for C++ and Go; games add their own messages. Realtime messages are encoded by Runic::Network, not Protobuf | — | PCT-1 |
| Server bindings (C++) | ServicesBridgeLayer (implements the Engine service port with asynchronous gRPC and NATS; completions queued for safe ticks), LifecycleLayer (Agones SDK), Prometheus exporter for the Core metrics registry; they implement the Engine's Layer interface | Engine interface targets only: Runic::Application (Layer), Runic::SimulationPort (service port), Runic::Core (metrics registry); Contracts | PBND-1 to PBND-4 |
| Go SDK | Service skeleton, configuration, gRPC and NATS wiring, persistence (PostgreSQL), transactions, migrations, idempotency store, metrics | Contracts | PSDK-1 to PSDK-3 |
| Core services (Go) | Accounts and entitlements, connect tickets, World Coordinator (worlds, regions and placement by load and traffic), instance allocation, world directives, chat and party groups, admission | Go SDK | PCS-1 to PCS-7 |
| Building blocks (Go) | Item ledger (containers, slots, atomic transfers, operation IDs), currency wallets and transfers that move items and currency in one transaction; mechanisms only, with no game rules | Go SDK | PBB-1 to PBB-3 |
| Infrastructure | Local development stack, Agones fleets, Kubernetes manifests, Prometheus and Grafana dashboards | — | PINF-1, PINF-2 |

- Platform C++ targets are named `RunicPlatform::<Module>`.
- Transport and the Go SDK depend on Contracts; Core services and Building blocks depend on the Go SDK.
- Platform CI builds the C++ bindings against the Engine commit pinned as a submodule, so the Platform builds and
  tests on its own. Because its Engine links are only the interface targets below, that standalone build configures
  Core and Application and never Flecs, Jolt or the gameplay modules.
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

Target level only; the concrete rules inside each target are defined in RunicGame.

| Target | Contains | Depends on |
| --- | --- | --- |
| RunicGame::Shared | Game rules compiled into both executables (client prediction runs the same code), client-safe content definitions | Runic::GameplayAPI, Perception |
| RunicGame::Server | Server-only rules; GameCore registers Game systems, hooks and actions; service clients and the persistence bridge | Shared, Runic::Simulation, Network, AI, Water, Environment; Platform server bindings and contracts |
| RunicGame::Client | Client presentation, Game render extractors, GameUI screens, audio content, input bindings | Shared, Runic::Client, Renderer, Animation, UI, Audio |
| GameServer (executable `RunicGameServer`) | Composes the server layers: the Engine's Network, Simulation and Diagnostics layers and the Platform's services-bridge and lifecycle layers | RunicGame::Server, Runic::Application, Platform transport and server bindings |
| GameClient (executable `RunicGameClient`) | Composes the client layers | RunicGame::Client, Runic::Application, Platform transport |
| Game services (Go) | The named Game services (list in [07 § Services](07-service-backed-systems.md#services)) | Platform Go SDK, item ledger, wallets |
| Game content | Data and assets split into Shared, Client and Server, cooked by the Cooker | — |

- `RunicGame::Shared` and `RunicGame::Server` are static libraries linked into the server; GameCore owns the
  registration entry point.
- `RunicGame::Client` may use `Shared` but never `Server` or server-only content.

## Forbidden dependencies

| Forbidden dependency | Why |
| --- | --- |
| Engine → Platform or Game | Wrong direction: the Platform implements Engine interfaces and the Game composes both |
| Engine → Agones, Kubernetes, gRPC, NATS, databases | Engine code runs locally and headless with no infrastructure; the Platform supplies those layers |
| Physics → Combat, Gameplay or Game | Physics answers geometric questions; it never knows damage, factions, blocks or dodges |
| Physics ↔ Renderer | Physics extracts debug geometry as plain data and the Renderer draws plain data; neither links the other |
| Renderer → Simulation or Physics state | The Renderer draws an extracted scene; it never becomes game logic |
| Simulation → feature modules or Game code | Gameplay modules and the Game register systems into the Simulation, which stays reusable |
| AI → direct combat, physics or persistence mutation | AI emits intents that become validated commands |
| GameplayFlow or Quest → Combat or AI internals | Sequences act through Simulation commands and Gameplay APIs, the same doors players use |
| Platform → Game rules | The Platform stays reusable: mechanisms, never policy |
| Game client → server-only rules or content | The client gets presentation data and request contracts only |
| Region, Sector or Cell → simulation process identity | The world partition is stable; which simulation owns what is an operational decision |
| Any module → another module's Source/ | Only Public/ headers cross a module boundary |
| Headless modules → GLFW, bgfx, RmlUi, ImGui, audio backend, Editor | The server stays headless |
| UI → Flecs or simulation state | The UI reads world anchors from the RenderFrame and content from Game view models |
| Runic::AI → ImGui, bgfx, RmlUi, editor windows, client presentation | The Editor's AI panels depend on AI contracts, never the reverse (ARCH § AI architecture) |
| Runic::Audio → combat, AI, quests, simulation authority, Platform networking or persistence | Audio is client presentation only and depends only on Core and Assets (ARCH § Audio) |

## Toolchain libraries per executable

From ARCH § World authoring and cooking. A library can live in a third-party folder and still be tool-only; build
targets keep toolchain libraries out of the server and the shipping client.

| Executable | Links |
| --- | --- |
| GameClient | bgfx, bimg, Jolt (for prediction and local collision), Flecs, Zstandard, Engine runtime |
| GameServer | Jolt, Flecs, Detour, Zstandard, Engine runtime |
| Cooker | fastgltf, meshoptimizer, MikkTSpace, xatlas (optional), KTX and Basis tools, Jolt cooking, Recast, Zstandard, Runic format writers |

## Repository wiring

Decided: **hybrid**.

| Repository | Pins as git submodules | Why |
| --- | --- | --- |
| RunicGame (superproject) | RunicEngine and RunicPlatform | CI and releases build from exact pinned commits |
| RunicPlatform | RunicEngine | Its C++ bindings build and test on their own |
| RunicEngine | Nothing | The Engine depends on no other repository |

For daily work a CMake option points at sibling checkouts instead of the submodules:

| Option | Points at |
| --- | --- |
| `RUNIC_ENGINE_DIR` | A RunicEngine checkout (typically `../RunicEngine`) |
| `RUNIC_PLATFORM_DIR` | A RunicPlatform checkout (typically `../RunicPlatform`) |

BUILD-1 sets up the pins and the option in all three repositories.

## CPU baseline

Decided: **x86-64-v3 (AVX2, FMA)** for servers and clients. The Jolt target is version 5.6.0 (tag `v5.6.0`), pinned
in the build, built in double precision (`JPH_DOUBLE_PRECISION`) with its AVX2 path enabled; the Engine builds with
AVX2 on.

This is a target, not a statement about the current pins. BUILD-1 delivers the CPU baseline and PHYS-1 delivers Jolt
in double precision. Until they are merged, a repository may still pin an older Jolt or build without SIMD flags; its
`docs/current-state.md` records the actual pin as a known gap.

## How CI enforces the rules

BUILD-1 delivers these checks in all three repositories (ARCH § Automated checks).

| Check | Catches |
| --- | --- |
| Headless build of GameServer | Graphics, UI, audio or editor code reaching the server |
| Dependency check per module | Links outside the allowed list; includes of another module's Source/ |
| Public-header scan | Library types in Public/ (outside the three approved exceptions) |
| Module tests | The Done when behavior of every merged package |
| Docs check | A code change without its ARCHITECTURE.md or package-status update |

- The **headless check** builds the server target alone and fails if GLFW, bgfx, RmlUi, ImGui, an audio backend or
  editor code is linked.
- The **forbidden-link check** fails CI when a module links something outside its row in the tables above.
- A failing check is fixed by changing the code, never by widening a module's allowed list. Widening the list is an
  owner decision recorded in ARCH first.
