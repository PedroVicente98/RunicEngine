# RunicEngine module catalog

This file lists every Engine module with its group, purpose, allowed dependencies, packages and status, and links to each module's contract seed.

Source: ARCH § Repositories, build targets and processes (Engine modules) and ARCH § Modules, work packages and the implementation contract (Work packages). The full cross-repository rules are in [../system/02-module-contract.md](../system/02-module-contract.md), [../system/03-dependency-rules.md](../system/03-dependency-rules.md) and [../system/12-work-packages.md](../system/12-work-packages.md).

## How to use this catalog

- Each `<Module>.md` file is the contract seed for one module. When `Engine/<Group>/<Module>/` is created, the file becomes that module's `ARCHITECTURE.md`, and from then on the module's file is authoritative. Update the row below to link there.
- The Depends on column is a ceiling: a package links only what it uses. A link that is not in the column is an architecture change: stop and report it.
- A package's Needs orders merges; it never grants a link.
- First slice is the earliest roadmap slice that needs one of the module's packages ([../system/08-implementation-roadmap.md](../system/08-implementation-roadmap.md)).
- Status values: `Not started`, `Stage A`, `Stage B`, `Stage C` ([../system/02-module-contract.md](../system/02-module-contract.md)). Update the row in the same pull request as the code.

## Modules

27 modules in seven groups, in ARCH's group order. Group folders are `Foundation`, `SimulationCore`, `GameplayAPI`, `WorldSystems`, `Networking`, `Presentation` and `Tools`. Third-party libraries in parentheses are private unless the row says otherwise (GLM is a PUBLIC dependency of Core).

| Group | Module | Purpose | Depends on | Headless | Packages | First slice | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Foundation | [Core](Core.md) | Plain ID and time types (including value types other modules give meaning to, such as AssetId and WaterBodyId), math (GLM), memory, logging, config, worker pool, metrics registry, debug-stream registry; RenderData (plain render structs, including DebugGeometry) | — (GLM is PUBLIC, an approved exception) | yes | CORE-1 to CORE-7 | 1 | Not started |
| Foundation | [Application](Application.md) | Entry point, Application, ApplicationHost, LayerStack, loop policies, application events; the Window class (GLFW, client and tools builds only) | Core | yes (Window excluded) | APP-1, APP-2 | 1 | Not started |
| Foundation | [Events](Events.md) | Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs | Core | yes | EVT-1 | 4 | Not started |
| Foundation | [Content](Content.md) | Namespaced ContentId (`engine:`, `game:`), registries, versioned schemas, validation, loading, live reload | Core | yes | CNT-1, CNT-2 | 9 | Not started |
| Foundation | [Assets](Assets.md) | AssetId handles, cache, native cooked formats, random-access packages with Zstandard decompression, manifests, client and server package selection, streaming | Core, Content (Zstandard) | yes | ASSET-1, ASSET-2 | 9 | Not started |
| Simulation core | [ECS](ECS.md) | Flecs integration and conventions | Core (Flecs, re-exported only to Engine modules that register systems) | yes | ECS-1 | 4 | Not started |
| Simulation core | [World](World.md) | 64-bit coordinates, region-local origins, SpatialLayers, Regions, Sectors, Cells, spatial index and queries, streaming state | Core, ECS | yes | WORLD-1 to WORLD-3 | 4 | Not started |
| Simulation core | [Physics](Physics.md) | Jolt v5.6 in double precision behind Runic types: bodies, shapes, CharacterVirtual, queries, collision layers, debug-geometry extraction, Jolt jobs on the Core worker pool | Core, World (Jolt) | yes | PHYS-1 to PHYS-5 | 7 | Not started |
| Simulation core | [Simulation](Simulation.md) | SimulationRuntime: multi-rate scheduler, phases, barriers, commands, event flush, worker integration, client prediction subset, the replication registry, the service port; the Simulation layer | Core, Application, ECS, Events, World, Physics, Content | yes | SIM-1 to SIM-6 | 3 | Not started |
| GameplayAPI | [Gameplay](Gameplay.md) | Attributes, modifiers, resources, tags, death and kill facts, the movement-mode registry and modes, restrictions, interaction framework, item and equipment runtime, factions, spawn | Simulation, Environment (wind for gliding) | yes | GPL-1 to GPL-7 | 14 | Not started |
| GameplayAPI | [Combat](Combat.md) | Ability toolkit and timelines, hit timing, one-slot input queue, targeting, hit detection, statuses, cooldowns, rule pipeline, behavior registries, damage interception, threat, crowd control, foreign combat | Gameplay, Physics, Environment (projectile wind), DistributedSimulation | yes | CMB-1 to CMB-8 | 16 | Not started |
| GameplayAPI | [GameplayFlow](GameplayFlow.md) | Sequences with trigger, condition and action registries, timers on simulation time, presentation cues, source-owned cleanup, handoff policies | Gameplay, DistributedSimulation (handoff snapshot) | yes | FLOW-1 to FLOW-3 | 29 | Not started |
| GameplayAPI | [Quest](Quest.md) | Quest runtime and quest-creation API: definitions, objectives, triggers from facts, instances, durable-fact bridge | GameplayFlow, DistributedSimulation (handoff snapshot) | yes | QST-1 to QST-3 | 30 | Not started |
| World systems | [Perception](Perception.md) | Light emitters, ambient and skylight, luminosity queries, concealment, warn and detect radii, PvP visibility tiers | Gameplay, World, Environment (time of day, weather), Water (underwater light) | yes | PRC-1 to PRC-3 | 25 | Not started |
| World systems | [Navigation](Navigation.md) | Detour and DetourTileCache runtime, nav tiles streamed by cell, path queries behind the MoveGoal contract | World, Simulation, Assets (Detour) | yes | NAV-1 | 31 | Not started |
| World systems | [AI](AI.md) | BehaviorTree.CPP adapter, working memory, wake, sleep and leash, intents to commands, reactive combat | Gameplay, Combat, Perception, Navigation (BehaviorTree.CPP) | yes | AI-1 to AI-6 | 32 | Not started |
| World systems | [Environment](Environment.md) | Environment clock, wind, current modulation, tides, gameplay waves, weather state and overrides | Core, World, Simulation | yes | ENV-1 to ENV-5 | 25 | Not started |
| World systems | [Water](Water.md) | WaterSystem, bodies, rivers and routes, ocean current fields, cooked fragments, buoyancy, character, object and watercraft movement | World, Physics, Simulation, Gameplay, Environment, DistributedSimulation (ship-group handoff) | yes | WATER-1 to WATER-7 | 40 | Not started |
| Networking | [Network](Network.md) | Transport interface (header-only `Runic::NetworkTransport`), ingress and egress queues, all realtime wire encoding, interest and budgets, prediction and reconciliation, parallel replication; the Network and Diagnostics layers | Application, Simulation, DistributedSimulation, Gameplay, Perception | yes | NET-1 to NET-7 | 12 | Not started |
| Networking | [DistributedSimulation](DistributedSimulation.md) | Authority state and epochs, ghost state as plain data, transfer with a snapshot-contributor registry, foreign-interaction contract, authority partitions | Simulation, Events, World | yes | DSIM-1 to DSIM-5 | 36 | Not started |
| Presentation | [Client](Client.md) | Prediction and Presentation layers, extractor registry and Engine extractors, camera, interpolation, input mapping and command builder | Application, Simulation, Network, Perception, Environment, Water, Renderer, Animation, Audio, UI, Assets | no | CLI-1 to CLI-8 | 5 | Not started |
| Presentation | [Renderer](Renderer.md) | bgfx device and views; the layered debug pipeline; render targets for diegetic UI; the batched world-UI feature; the production pipeline later | Core, Assets (bgfx) | no | RND-1 to RND-10 | 2 | Not started |
| Presentation | [Animation](Animation.md) | Skeletons, clips, animator, blending, poses | Core, Assets | no | ANIM-1 | 23 | Not started |
| Presentation | [UI](UI.md) | RmlUi (GameUI) and ImGui (development overlay) as layers; world UI: diegetic panels, anchored elements, batched fallback, budgets, input routing | Application, Renderer, Assets (RmlUi, ImGui) | no | UI-1 to UI-5 | 2 | Not started |
| Presentation | [Audio](Audio.md) | Backend abstraction, buses and mixing, semantic sound events, MusicDirector, albums, soundtrack presets, local profiles | Core, Assets | no | AUD-1, AUD-2 | 24 | Not started |
| Tools | [Editor](Editor.md) | Editor host and panels, inspectors, resolver debugger, AI editor, live attach, Cooker launcher | Most modules (ImGui, imnodes) | never shipped | EDT-1 to EDT-7 | 6 | Not started |
| Tools | [Cooker](Cooker.md) | Cooking orchestration and Runic format writers over fastgltf, meshoptimizer, MikkTSpace, xatlas, KTX and Basis Universal, Jolt cooking, Recast and Zstandard | Core, Content, Assets, World, Physics, Navigation, Water (each runtime module owns its cooked format; the Cooker links its writer) | tool | COOK-1 to COOK-8 | 9 | Not started |

The build skeleton package BUILD-1 (slice 0) belongs to no module: it creates the `runic_add_module` helper, presets, CI, the chosen test framework, the headless and forbidden-dependency checks and the x86-64-v3 CPU baseline ([../current-state.md](../current-state.md)).

## Group and interface targets

| Target | Kind | What it is | Defined by | First slice |
| --- | --- | --- | --- | --- |
| `Runic::GameplayAPI` | Umbrella (INTERFACE) | Links Gameplay, Combat, GameplayFlow and Quest so the Game links one name; each module keeps its own boundary and tests. No code of its own | The GameplayAPI group folder | 14 (first GameplayAPI package) |
| `Runic::NetworkTransport` | Header-only interface target | The transport interface and the connect-ticket hook. Headers include only Core types. RunicPlatform's Transport implements it | Network, NET-1 | 12 |
| `Runic::SimulationPort` | Header-only interface target | The service port: outbox of durable operations, inbox of accepted results. Headers include only Core types. RunicPlatform's ServicesBridgeLayer implements it | Simulation, SIM-5 | 27 |

RunicPlatform's C++ links only `Runic::Core`, `Runic::Application`, `Runic::NetworkTransport` and `Runic::SimulationPort`, never Flecs, Jolt or a gameplay module.

Transitional targets: today's bundles `Runic::Runtime` and `Runic::Presentation` stay until RunicGame links per-module targets (decided in [OQ-8](../system/11-open-questions.md#oq-8)). Do not add new code to them.
