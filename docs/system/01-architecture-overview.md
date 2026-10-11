# 01 — Architecture overview

This file says who owns what across the three repositories, which processes exist, how the build is layered, and which principles every change must keep.

Source: ARCH § Architecture overview, § Principles and performance targets, § Repositories, build targets and
processes, § Application and LayerStack.

## Ownership in one sentence

The **Engine** supplies generic mechanisms, the **Game** composes them into its two executables and its own
services, and the **Platform** runs the shared services and infrastructure both depend on.

| Owner | Repository | Owns |
| --- | --- | --- |
| Engine | RunicEngine | Generic mechanisms any MMORPG can use |
| Game | RunicGame | This game's rules, content and executables |
| Platform | RunicPlatform | Services, SDK and infrastructure any MMORPG can use |

The goal is the most performant architecture that does not sacrifice the game, generic enough that another team
could build a different MMORPG on Engine and Platform, including a much larger world.

### Placement test for new code

| If the code… | It belongs in |
| --- | --- |
| is a mechanism another game could reuse and runs inside a simulation, client or tool | RunicEngine |
| is a service, SDK piece, transport or infrastructure another game could reuse | RunicPlatform |
| encodes how this game plays: rules, numbers, content, scripts, screens, game services | RunicGame |

### Ownership map

Game cells are written neutrally here; the concrete Game rules live only in RunicGame.

| Area | Engine | Game | Platform |
| --- | --- | --- | --- |
| Executables | Entry point, Application, ApplicationHost, reusable layers; Editor and Cooker | GameClient and GameServer compose the layers | Services-bridge and lifecycle layers (C++ server bindings) |
| Simulation | Scheduler, threads, Flecs, Jolt, world space | Which systems run and their rules | — |
| Skills and combat | GameplayAPI group (Gameplay and Combat modules): ability toolkit, hit timing, tags, statuses, rule pipeline, damage interception | Skill definitions, damage types and formula, resolver order, every concrete status and control effect | — |
| Defense and threat | Invulnerability windows, interception hooks, threat table | Defense rules, armor effects, threat rules | — |
| Custom code | Behavior registries, hooks, context and output buffers | The behaviors | — |
| Movement | CharacterVirtual, movement modes, prediction, validation | Speeds, dashes, mount and glider stats | — |
| Light, perception, AI | The APIs, computation, behavior-tree framework and editor | Values, mob profiles, trees, custom nodes | — |
| Items and inventory | Runtime item and container mirrors | Item definitions and rules; Inventory service rules | Item ledger and currency wallets as mechanisms, with no game rules |
| Quests, economy, housing, factions | Interaction framework, GameplayFlow sequences, Quest runtime with its quest-creation API | Quest definitions and encounter scripts; Go services that store quest progress and character state and run the game's economy and housing | Go SDK they are built on |
| Scripted sequences and restrictions | GameplayFlow runtime, source-owned restrictions, presentation cues | Boss phases, cutscenes, dungeon and world-event scripts | — |
| Networking | Transport interface, replication, interest, prediction, handoff | Game messages | GameNetworkingSockets transport, Protobuf contracts, gRPC and NATS wiring |
| Accounts, world control, instances | — | Configuration; guild and mail services | Core services: accounts, entitlements, World Coordinator, instance allocation, chat and party groups |
| Infrastructure and monitoring | Metrics recorded in every process; the Platform exports them (see note) | Dashboards for game metrics | Agones, Kubernetes, Prometheus, Grafana |
| Presentation | Renderer (including the physics debug view), assets, animation, RmlUi and ImGui integrations, audio | GameUI screens, Game render extractors, art | — |
| Water and environment | WaterSystem, buoyancy, water movement, wind, tides, waves, weather evaluation | Water content, profiles, watercraft rules, weather patterns | Weather-event directives; the Go SDK persistence that Game services use for durable water state |

Note on metrics: the decided seam is that every GameServer records its metrics in the Core metrics registry (CORE-2;
the Engine's Diagnostics layer, NET-7, adds network and replication metrics) and the Platform exporter (PBND-3)
**publishes the Core registry** to Prometheus. The Engine never talks to Prometheus itself
(see [07 § Monitoring](07-service-backed-systems.md#monitoring-one-panel)).

## Repositories and licenses

| Repository | License | Owns |
| --- | --- | --- |
| RunicEngine | Open source (MIT), C++ | Engine modules, the entry point and ApplicationHost, Editor, Cooker |
| RunicPlatform | Open source (MIT), C++ and Go | GameNetworkingSockets transport, server bindings (services-bridge and lifecycle layers), Protobuf contracts, Go SDK, core services, building blocks, Agones and Kubernetes manifests, dashboards |
| RunicGame | Closed source | GameClient and GameServer executables (`RunicGameClient`, `RunicGameServer`), game modules, Go game services, content |

RunicGame is the superproject. How the repositories are wired together (submodules plus a sibling-checkout option)
is in [03 § Repository wiring](03-dependency-rules.md#repository-wiring). ARCH calls the Game executables
GameClient and GameServer; their build targets are `RunicGameClient` and `RunicGameServer`.

## Processes

| Process | Owner | Runs | Shipped |
| --- | --- | --- | --- |
| GameClient | Game | Client layers on the Engine | Yes; shipping builds leave out the ImGui layer |
| GameServer | Game | Server layers on the Engine, one region per pod | Yes, to the server fleet |
| Editor | Engine | Editor layers; attaches to development and staging servers | Never |
| Cooker | Engine | Cooking jobs, launched by the Editor or by build machines | Never |
| Game services | Game | Go services built on the Platform SDK | Yes, to the server fleet |
| Platform services | Platform | Core services and building blocks | Yes, to the server fleet |

### Application, layers and loops

The Engine owns `main()`. It calls `CreateApplication()`, which each executable defines (RunicGame twice, the Engine
for the Editor and the Cooker). The Application builds its LayerStack and hands control to the ApplicationHost.
Layers receive events top-down and update bottom-up; the simulation's internal order stays in its scheduler.

| Executable | Loop | Layers, bottom to top | Overlays |
| --- | --- | --- | --- |
| GameServer | Fixed 60 Hz loop that sleeps until the next quantum; no rendering | Network (Engine) → Simulation (Engine) → Services bridge (Platform binding: gRPC, NATS) → Lifecycle (Platform binding: Agones) → Diagnostics (Engine: metrics, Editor attach endpoint in development and staging builds) | None |
| GameClient | Frame loop (vsync or capped) | Network → Prediction → Presentation (interpolation, camera, render extraction, audio) | GameUI (RmlUi); ImGui only in development builds |
| Editor | Frame loop | Editor host → tool panels | ImGui |
| Cooker | Job loop | Job runner | None |

- The Engine supplies the Network, Simulation, Prediction, Presentation and Diagnostics layers as reusable classes.
- The Platform supplies the services-bridge and lifecycle layers as C++ server bindings, because only the Platform
  may talk to gRPC, NATS and Agones. The Engine never supplies infrastructure or communication layers.
- RunicGame configures and composes all of them and adds its own layers.
- State changes are layer replacements applied at the loop's safe point, never mid-update. Client: Boot → Login →
  Character select → World. Server: Starting → Ready → Draining → Shutdown; draining hands regions off before exit.

## Layered build view

Read it as build dependencies: each band links only the bands below it, and nothing points up.

| Band (top to bottom) | Contains |
| --- | --- |
| Executables | GameClient, GameServer (Game); Editor, Cooker (Engine tools, development only) |
| RunicGame | `RunicGame::Shared`, `RunicGame::Server`, `RunicGame::Client`, Go game services, content |
| RunicPlatform | C++ parts (Transport, Server bindings) implement Engine interfaces and link only the Engine interface targets Runic::Core, Runic::Application, Runic::NetworkTransport and Runic::SimulationPort; Contracts; Go SDK, core services and building blocks need no Engine code |
| Engine presentation | Client, Renderer, Animation, UI, Audio |
| Engine systems | GameplayAPI group (Gameplay, Combat, GameplayFlow, Quest); World systems (Perception, Navigation, AI, Environment, Water); Networking (Network, DistributedSimulation) |
| Simulation core | ECS, World, Physics, Simulation |
| Engine foundation | Core (including RenderData), Application (including the Window class), Events, Content, Assets |
| Infrastructure | Agones, Kubernetes, PostgreSQL, NATS, Prometheus, Grafana. **Runtime only, never linked.** Platform services and bindings reach it at runtime |

Inside a band, modules depend on each other only as the module table in
[03 § Engine modules](03-dependency-rules.md#engine-modules) allows. RunicPlatform sits above the Engine because its
C++ transport and server bindings include Engine interface headers; the Engine never includes Platform code. The
interfaces the Platform implements ship as header-only Engine targets whose headers include only Core types
(Runic::NetworkTransport from Network, Runic::SimulationPort from Simulation), so a Platform build configures Core and
Application and never Flecs, Jolt or the gameplay modules
([03 § Engine interface targets for the Platform](03-dependency-rules.md#engine-interface-targets-for-the-platform)).

## Four axes that must stay separate

| Axis | Question it answers | Mechanism |
| --- | --- | --- |
| Compile-time dependencies | Which code may use which code? | CMake targets |
| Process composition | Which coarse features run in this executable, and in what order? | ApplicationHost + LayerStack |
| Simulation scheduling | What simulation work runs now, and before what? | 60 Hz master scheduler, 30/20/10 Hz domains, explicit phases |
| Distributed topology | Which process owns this region or entity? | World Coordinator, SimulationOwner, authority epochs |

Never use one axis to answer another axis's question. Example: simulation order lives in the scheduler, never in
layer order; a Region never encodes which process owns it.

## Three event families

| Family | Examples | Delivery |
| --- | --- | --- |
| Application events | Key press, resize, UI focus, connection state | LayerStack: top layer first, can be marked handled |
| Simulation facts | HitEvent, DamageEvent, EntityKilled, RegionChanged | Tick-safe buffers; every consumer receives them |
| Distributed messages | Quest facts, inventory operations, world-event directives | Transport, gRPC or NATS, with explicit reliability and idempotency |

Details: [04-events-and-messages.md](04-events-and-messages.md).

## Non-negotiable principles

1. The C++ simulation is authoritative for everything happening in the world. Clients send intentions; they never
   decide position, hits, damage, loot or quest completion.
2. The simulation runs headless and never depends on rendering, UI or tools.
3. The engine provides mechanisms; the game provides data and rules. Anything game-specific lives in RunicGame as
   data or as registered C++ behaviors.
4. Data first: skills, items, effects, AI, light values and balance are data with stable namespaced IDs and
   versioned schemas, reloadable without recompiling.
5. Nothing blocks the tick: network, service and database work is queued and applied at defined points.
6. Spatial partitions are indexes, not servers. Which process owns what is a separate, explicit assignment.
7. Every feature is built as a vertical slice first, generalized second, hardened third.
8. Performance is measured, not assumed: every clock domain and major system reports its cost per tick.

Rules that follow directly from these and that every session must obey:

- Never mutate Flecs from an I/O callback; completions are queued and applied at safe ticks.
- Local Flecs entity IDs never leave the process; use `GlobalEntityId`.
- The realtime path is direct: client ↔ simulation and simulation ↔ simulation traffic uses the transport, never
  NATS or gRPC in the hot path. Runic::Network encodes every realtime message, envelope included; Protobuf (Platform
  Contracts) carries only service APIs, published facts and the connect-ticket format, never per-tick traffic.
- Durable changes are operations with IDs, so retries are safe.

## Performance targets

Engine-level targets from ARCH § Principles and performance targets. They are planning targets unless the decisions
log marks them as decided (scale and clock rates are decided).

| Target | Value |
| --- | --- |
| Players per world | Up to 5,000 concurrent |
| Battle that must feel perfect | 100 vs 100 in one region |
| Largest battle that must still work | 500 players in one region, with graceful degradation |
| Combat and physics rate | 30 Hz |
| Scheduler master clock | 60 Hz |
| World coordinates | 64-bit |
| Expansion | New continents and regions added to a live world by patch and streamed in seamlessly |
| Engine ceiling | Continent-scale worlds for other teams without architectural change |

ARCH also states the size of the first world. That is a Game decision; it is recorded in RunicGame, not here.

## Planning budgets

| Budget | Planning value | Why |
| --- | --- | --- |
| Simulation work per 30 Hz tick | ≤ 25 ms of the 33 ms window at p99 | Headroom for spikes and I/O |
| Download per player in a 500-player battle | ≈ 50 KB/s | Fits any broadband connection |
| Upload per simulation in a 500-player battle | ≈ 25 MB/s | Fits a standard server network card |
| Input to visible action | Immediate on the client; server confirmation within one round trip | Combat feels responsive at any ping |

The per-battle bandwidth plan behind these numbers is in
[05 § Priority and bandwidth budget](05-distributed-simulation.md#priority-and-bandwidth-budget).
