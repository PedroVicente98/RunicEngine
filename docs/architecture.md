# RunicEngine architecture

This file explains how RunicEngine is organized: its groups and folders, the build bands, the application layers it supplies, render data and the two render pipelines, and the rules every Engine module follows.

Source: ARCH (RunicEngine — Technical Architecture v3). The cross-repository rules live in [system/](system/README.md); this file covers only what is specific to this repository.

## Role

RunicEngine is the public C++20 library of generic mechanisms any MMORPG can use. RunicGame composes it into its two executables and its own services; RunicPlatform supplies the shared services, the realtime transport and the infrastructure ([system/01-architecture-overview.md](system/01-architecture-overview.md)).

| Repository | Owns |
| --- | --- |
| RunicEngine (this repository, MIT) | Engine modules, the entry point and ApplicationHost, the reusable layers, the Editor and the Cooker |
| RunicPlatform (MIT) | GameNetworkingSockets transport, Protobuf contracts, server bindings (services bridge, lifecycle, metrics exporter), Go SDK, core services, building blocks, infrastructure |
| RunicGame (proprietary) | The GameClient and GameServer executables, game modules, Go game services, content |

The Engine must:

- run headless: the authoritative server links no GLFW, bgfx, RmlUi, ImGui, audio backend or editor code;
- run without production infrastructure: no Agones, Kubernetes, database, gRPC or NATS anywhere in the Engine;
- never include Platform or Game code; the Platform implements Engine interfaces, and the Game composes both;
- contain no game rules, names, numbers or content, tests included (tests use neutral IDs such as `engine:test_ability`).

## Groups and folder layout

27 modules in seven groups. The catalog with every module's dependencies and packages is [modules/README.md](modules/README.md).

| Group | Modules |
| --- | --- |
| Foundation | Core, Application, Events, Content, Assets |
| Simulation core | ECS, World, Physics, Simulation |
| GameplayAPI | Gameplay, Combat, GameplayFlow, Quest (umbrella target `Runic::GameplayAPI`) |
| World systems | Perception, Navigation, AI, Environment, Water |
| Networking | Network, DistributedSimulation |
| Presentation | Client, Renderer, Animation, UI, Audio |
| Tools | Editor, Cooker |

A module inside a group nests under the group folder. The group folders are `Foundation`, `SimulationCore`, `GameplayAPI`, `WorldSystems`, `Networking`, `Presentation` and `Tools` (ARCH); BUILD-1 creates them.

```text
RunicEngine/
├── Engine/
│   └── <Group>/
│       └── <Module>/
│           ├── Public/            # the only headers other modules may include
│           ├── Source/            # implementation; one folder per subsystem
│           ├── Tests/             # module tests, registered with CTest
│           ├── ARCHITECTURE.md    # the module contract (seeded from docs/modules/<Module>.md)
│           └── CMakeLists.txt     # one runic_add_module(...) call
├── apps/                          # executables (today: the RunicSandbox dependency check)
├── cmake/                         # dependency pins, warnings, target helpers
├── ThirdParty/                    # downloaded sources (git-ignored)
└── docs/
```

- **One target per module**, named `Runic::<Module>`, namespace `Runic::<Module>`. The `runic_add_module` helper (BUILD-1) creates the target and alias, wires `Public/`, `Source/` and `Tests/`, sets warnings, registers tests and adds the headless check.
- **A group** is a folder plus, where useful, an INTERFACE umbrella that only links its modules. `Runic::GameplayAPI` links Gameplay, Combat, GameplayFlow and Quest so the Game links one name.
- **Header-only interface targets.** A module may export one header-only target for an interface the Platform implements; its headers include only Core types. There are two: `Runic::NetworkTransport` (transport interface and connect-ticket hook, from Network, NET-1) and `Runic::SimulationPort` (service port, from Simulation, SIM-5).
- **Transitional bundles.** `Runic::Runtime` and `Runic::Presentation` stay until RunicGame links per-module targets (OQ-8). Today's `include/RunicEngine/` and `src/` scaffold moves to `Engine/` in BUILD-1 ([current-state.md](current-state.md)).

## Layered build view

Read it as build dependencies: each band links only the bands below it, and nothing points up. Inside a band, tiers are separated by semicolons, highest first: a module links only lower tiers, and only the modules its catalog row names. The bands are derived from the Depends on column of ARCH's Engine-modules table.

| Band (top to bottom) | Tiers inside the band | Links |
| --- | --- | --- |
| Tools | Editor, Cooker | Any band below; never linked by a runtime |
| Presentation | Client; UI; Renderer, Animation, Audio | Gameplay and world, Simulation core, Foundation, lower tiers |
| Gameplay and world | Quest, AI, Network; Perception; Combat, GameplayFlow, Water; Gameplay; DistributedSimulation, Navigation, Environment | Simulation core, Foundation, lower tiers |
| Simulation core | Simulation; Physics; World; ECS | Foundation, lower tiers |
| Foundation | Assets; Application, Events, Content; Core | Lower tiers; Core links nothing |

Groups are folders and names; bands are build order. The gameplay-and-world band mixes three groups because, for example, Combat links DistributedSimulation, Gameplay links Environment (wind for gliding), Perception links Water (underwater light) and Network links Gameplay and Perception.

```text
              RunicGame (GameClient, GameServer, game modules)
                 │ links Engine modules and Platform bindings
     RunicPlatform C++ (Transport, server bindings)
                 │ implements Engine interfaces through
                 │ Runic::Core, Runic::Application,
                 │ Runic::NetworkTransport, Runic::SimulationPort
  ┌──────────────┴───────────────────────────────────────────┐
  │ RunicEngine: Tools → Presentation → Gameplay and world → │
  │              Simulation core → Foundation                │
  └──────────────────────────────────────────────────────────┘
```

The Platform sits above the Engine: its C++ transport and server bindings include Engine interface headers, and because those headers include only Core types, a Platform build configures Core and Application and never Flecs, Jolt or the gameplay modules. Platform Go services need no Engine code. The Engine never includes Platform or Game code.

## Application, LayerStack and the layers the Engine supplies

Every executable is an Application on the Engine's ApplicationHost: a stack of layers that receives events top-down and updates bottom-up, while the simulation's internal order stays inside its scheduler. The Engine owns `main()` and calls `CreateApplication()`: RunicGame defines it for GameClient and GameServer, and the Engine defines it for the Editor and the Cooker ([modules/Application.md](modules/Application.md)).

| Executable | Layers, bottom to top | Overlays | Loop |
| --- | --- | --- | --- |
| GameServer | Network → Simulation → Services bridge (Platform) → Lifecycle (Platform) → Diagnostics | None | Fixed 60 Hz loop |
| GameClient | Network → Prediction → Presentation | GameUI (RmlUi); ImGui in development builds only | Frame loop |
| Editor | Editor host → tool panels | ImGui | Frame loop |
| Cooker | Job runner | None | Job loop |

| Layer | Supplied by | Module |
| --- | --- | --- |
| Network | Engine | Network (NET-1) |
| Simulation | Engine | Simulation (SIM-1) |
| Diagnostics | Engine | Network (NET-7): network and replication metrics into the Core registry; Editor attach channel over a development socket transport (development and staging builds only), streaming debug shapes and the traces in Core's debug-stream registry |
| Prediction | Engine | Client (CLI-3) |
| Presentation | Engine | Client (CLI-1): interpolation, camera, render extraction, audio |
| GameUI and ImGui overlays | Engine | UI (UI-2, UI-1) |
| Services bridge | Platform | PBND-1 (gRPC) and PBND-4 (NATS): implements the Simulation's service port |
| Lifecycle | Platform | PBND-2: Agones SDK, with a local stub mode |

The Engine never supplies infrastructure or communication layers: only the Platform talks to gRPC, NATS and Agones. Layer changes are queued and applied at one safe point per loop; no layer blocks; simulation order lives in the scheduler, never in layer order.

## RenderData and the two render pipelines

The Renderer and the UI never read Flecs, Jolt or the domain modules. Each frame the Client extracts what must be shown into plain RenderData structs that live in Core (`Core/Public/RenderData/`, namespace `Runic::RenderData`), and both render pipelines and the world UI consume only those.

```text
Simulation, Water, Perception, Environment, Animation
        │ read-only
        ▼
Client extractors (engine: and game: IDs) ──► RenderFrame (Core RenderData, double-buffered)
                                                 │
                       ┌─────────────────────────┼──────────────────────┐
                       ▼                         ▼                      ▼
              Debug pipeline (Renderer)  Production pipeline     World UI (UI)
                                         (Renderer, later)
Physics ──► DebugGeometry (Core) ──► Renderer physics view
Server DebugGeometry ──► Diagnostics layer ──► Editor (development builds)
```

- RenderData is plain data: header-only structs, camera-relative positions under one 64-bit origin per frame, stable references (AssetId, GlobalEntityId), double-buffered. A kind is added by the package that first needs it (CORE-3 to CORE-7).
- **The debug pipeline comes first.** One layer per kind of render data, each drawn simply and toggled from the ImGui overlay or the Editor, built layer by layer from slice 2 (RND-2 debug shapes, RND-3 physics geometry, RND-4 meshes, RND-5 skinned meshes, RND-7 lights and volumes, RND-8 water and fields).
- **The production pipeline comes later** (RND-6, slice 44+): materials, shaders, lighting and shadows, terrain, water shading, VFX, decals and the UI renderer. Both pipelines read the same RenderFrame, so switching needs no gameplay change.
- Physics and Renderer never link each other: Physics fills DebugGeometry (PHYS-3), and the Renderer's physics view draws it (RND-3).

## Public headers and the three approved exceptions

Library types never appear in `Public/`; they stay in `Source/` behind Runic types. Exactly three exceptions are approved, and each module that uses one records it in its `ARCHITECTURE.md`:

1. **GLM math types in Core's public headers**, the Engine's math vocabulary. GLM is a PUBLIC dependency of Core.
2. **The Flecs world handle**, for modules that register systems into the simulation (for example `AIModule::Register(flecs::world&)`). ECS re-exports Flecs only to the Engine modules that register systems.
3. **BehaviorTree.CPP node base types**, for Game custom AI nodes.

Jolt, bgfx, RmlUi, ImGui, Detour and GameNetworkingSockets always stay hidden. A module dependency is PUBLIC only when its types appear in public headers; third-party libraries are PRIVATE except these approved exceptions.

## Third-party placement

Pins live in `cmake/Dependencies.cmake`; sources are fetched into `ThirdParty/` and never edited. A library not yet pinned is added by the package that first needs it.

| Library | Pin today | Owner module | Builds |
| --- | --- | --- | --- |
| Flecs (C++ API) | 4.0.4 | ECS | Server, client, tools |
| GLM | 1.0.1 | Core | All |
| Jolt Physics | 5.2.0 today; target 5.6.0, double precision, AVX2 path (BUILD-1, PHYS-1) | Physics; Cooker for cooking | Server, client, Cooker |
| GLFW | 3.4 | Application (Window class) | Client and tools only |
| bgfx, bx, bimg (bgfx.cmake `0fb9ec06…`) | Pinned revisions | Renderer | Client and Editor |
| Dear ImGui | 1.91.8, core only | UI (development overlay), Editor | Client development builds, Editor |
| RmlUi | Not pinned | UI | Client |
| BehaviorTree.CPP | Not pinned | AI | Server (client only if ever needed) |
| Detour, DetourTileCache | Not pinned | Navigation | Server |
| Recast, fastgltf, meshoptimizer, MikkTSpace, xatlas, KTX-Software, Basis Universal | Not pinned | Cooker | Cooker only |
| Zstandard | Not pinned | Cooker; runtime package decompression in Assets (ASSET-2) | Cooker, server, client |
| imnodes | Not pinned | Editor | Editor only |
| Audio backend and decoders | Not chosen | Audio | Client |

GameNetworkingSockets, Protobuf, gRPC, NATS and the Agones SDK belong to RunicPlatform and never appear in this repository. The CPU baseline is x86-64-v3 (AVX2, FMA) for servers and clients, set in BUILD-1.

## Engine-wide rules

1. The C++ simulation is authoritative. Clients send intentions; they never decide position, hits, damage, loot or quest completion.
2. The Simulation owns the Flecs world, the 60 Hz master clock with 30, 20 and 10 Hz domains, phases, commands and physics timing. Feature modules register into it; it never depends on them.
3. Nothing blocks a tick. Network, service and disk work runs on its own threads and only enqueues; results are applied at safe ticks. No I/O callback touches Flecs.
4. The Simulation's service port (SIM-5) is the Engine's only door to durable services. Every durable change is an operation with a stable ID, so retries are safe.
5. Network encodes every realtime message, envelope included. Protobuf (Platform Contracts) carries only service APIs, published facts and the connect-ticket format.
6. Local Flecs entity IDs never leave the process. Across processes use `GlobalEntityId`, `GlobalPlayerId` and the other plain ID types in Core.
7. Simulation facts go through Events buffers; every consumer receives them, and each carries a stable event ID.
8. Physics answers geometric questions; Combat and the Game decide meaning. AI emits intents that become validated commands. GameplayFlow and Quest act only through Simulation commands and Gameplay APIs.
9. Restrictions are owned by their source and released by one termination path.
10. Data first: definitions are data with stable namespaced IDs and versioned schemas, reloadable without recompiling.
11. Every clock domain and major system reports its cost per tick in the Core metrics registry.

## Executables

| Executable | Owner | Links | Shipped |
| --- | --- | --- | --- |
| RunicSandbox | Engine (today, disposable) | `Runic::Runtime`, `Runic::Presentation` | No |
| Editor | Engine | Editor and most modules | Never |
| Cooker (RunicCooker) | Engine | Cooker and the toolchain libraries | Never |
| GameClient (RunicGameClient) | RunicGame | Game client modules, Engine client modules, Platform transport | Yes; shipping builds leave out ImGui |
| GameServer (RunicGameServer) | RunicGame | Game server modules, headless Engine modules, Platform transport and server bindings | Yes, to the server fleet |

## Related documents

- [system/README.md](system/README.md), [system/01-architecture-overview.md](system/01-architecture-overview.md), [system/02-module-contract.md](system/02-module-contract.md), [system/03-dependency-rules.md](system/03-dependency-rules.md)
- [system/04-events-and-messages.md](system/04-events-and-messages.md), [system/05-distributed-simulation.md](system/05-distributed-simulation.md), [system/06-gameplayflow-quests-cutscenes.md](system/06-gameplayflow-quests-cutscenes.md), [system/07-service-backed-systems.md](system/07-service-backed-systems.md)
- [system/08-implementation-roadmap.md](system/08-implementation-roadmap.md), [system/09-validation-scenarios.md](system/09-validation-scenarios.md), [system/10-ai-development-workflow.md](system/10-ai-development-workflow.md), [system/11-open-questions.md](system/11-open-questions.md), [system/12-work-packages.md](system/12-work-packages.md), [system/glossary.md](system/glossary.md)
- [current-state.md](current-state.md) and [modules/README.md](modules/README.md)
