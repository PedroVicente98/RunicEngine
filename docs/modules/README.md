---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/catalog
---

# RunicEngine module catalog

This file lists every Engine module with its group, purpose, allowed dependencies, packages and status, and links to each module's contract seed.

Source: ARCH[^arch] § Repositories, build targets and processes (Engine modules) and ARCH § Modules, work packages and the implementation contract (Work packages). The full cross-repository rules are in [../system/02-module-contract.md](../system/02-module-contract.md), [../system/03-dependency-rules.md](../system/03-dependency-rules.md) and [../system/12-work-packages.md](../system/12-work-packages.md).

## How to use this catalog

- Each `<Module>.md` file is the contract seed for one module. When `Engine/<Group>/<Module>/` is created, the file becomes that module's `ARCHITECTURE.md`, and from then on the module's file is authoritative. Update the row below to link there.
- The Depends on column is a ceiling: a package links only what it uses. A link that is not in the column is an architecture change: stop and report it.
- A package's Needs orders merges; it never grants a link.
- First slice is the earliest roadmap slice that needs one of the module's packages ([../system/08-implementation-roadmap.md](../system/08-implementation-roadmap.md)).
- Status values: `Not started`, `Stage A`, `Stage B`, `Stage C` ([../system/02-module-contract.md](../system/02-module-contract.md)). Update the row in the same pull request as the code.

## Modules

27 modules in seven groups, in ARCH[^arch]'s group order. Group folders are `Foundation`, `SimulationCore`, `GameplayAPI`, `WorldSystems`, `Networking`, `Presentation` and `Tools`. Third-party libraries in parentheses are private unless the row says otherwise (GLM is a PUBLIC dependency of Core).

| Group | Module | Purpose | Depends on | Headless | Packages | First slice | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Foundation | [Core](Core.md) | Plain ID and time types (including value types other modules give meaning to, such as AssetId and WaterBodyId), math (GLM), memory, logging, config, worker pool, metrics registry, debug-stream registry; RenderData (plain render structs, including DebugGeometry) | — (GLM is PUBLIC, an approved exception) | yes | CORE-1[^engine-core] to CORE-7 | 1 | Not started |
| Foundation | [Application](Application.md) | Entry point, Application, ApplicationHost, LayerStack, loop policies, application events; the Window class (GLFW, client and tools builds only) | Core | yes (Window excluded) | APP-1[^engine-application], APP-2 | 1 | Not started |
| Foundation | [Events](Events.md) | Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs | Core | yes | EVT-1[^engine-events] | 4 | Not started |
| Foundation | [Content](Content.md) | Namespaced ContentId (`engine:`, `game:`), registries, versioned schemas, validation, loading, live reload | Core | yes | CNT-1[^engine-content], CNT-2 | 9 | Not started |
| Foundation | [Assets](Assets.md) | AssetId handles, cache, native cooked formats, random-access packages with Zstandard decompression, manifests, client and server package selection, streaming | Core, Content (Zstandard) | yes | ASSET-1[^engine-assets], ASSET-2 | 9 | Not started |
| Simulation core | [ECS](ECS.md) | Flecs integration and conventions | Core (Flecs, re-exported only to Engine modules that register systems) | yes | ECS-1[^engine-ecs] | 4 | Not started |
| Simulation core | [World](World.md) | 64-bit double coordinates (real Y, 1 cm minimum unit), worlds (WorldId), region-local origins, Regions, Sectors, sparse 3D Cells, authored volumes (including indivisible ones), spatial index and exact XYZ queries, activation volumes and streaming state | Core, ECS | yes | WORLD-1[^engine-world] to WORLD-3 | 4 | Not started |
| Simulation core | [Physics](Physics.md) | Jolt v5.6 in double precision behind Runic types: bodies, shapes, CharacterVirtual, queries, collision layers, debug-geometry extraction, Jolt jobs on the Core worker pool | Core, World (Jolt) | yes | PHYS-1[^engine-physics] to PHYS-5 | 7 | Not started |
| Simulation core | [Simulation](Simulation.md) | SimulationRuntime: multi-rate scheduler, phases, barriers, commands, event flush, worker integration, client prediction subset, the replication registry, the service port; the Simulation layer | Core, Application, ECS, Events, World, Physics, Content | yes | SIM-1[^engine-simulation] to SIM-6 | 3 | Not started |
| GameplayAPI | [Gameplay](Gameplay.md) | Attributes, modifiers, resources, tags, death and kill facts, the movement-mode registry and modes, restrictions, interaction framework, item and equipment runtime, factions, spawn and routine spawning | Simulation, Environment[^engine-environment] (wind for gliding) | yes | GPL-1[^engine-gameplay] to GPL-8 | 14 | Not started |
| GameplayAPI | [Combat](Combat.md) | Ability toolkit and timelines, hit timing, one-slot input queue, targeting, hit detection, statuses, cooldowns, rule pipeline, behavior registries, damage interception, threat, crowd control, foreign combat | Gameplay, Physics, Environment (projectile wind), DistributedSimulation[^engine-distributedsimulation] | yes | CMB-1[^engine-combat] to CMB-8 | 16 | Not started |
| GameplayAPI | [GameplayFlow](GameplayFlow.md) | Sequences with trigger, condition and action registries, timers on simulation time, presentation cues, source-owned cleanup, handoff policies | Gameplay, DistributedSimulation (handoff snapshot) | yes | FLOW-1[^engine-gameplayflow] to FLOW-3 | 29 | Not started |
| GameplayAPI | [Quest](Quest.md) | Quest runtime and quest-creation API: definitions, objectives, triggers from facts, instances, durable-fact bridge | GameplayFlow, DistributedSimulation (handoff snapshot) | yes | QST-1[^engine-quest] to QST-3 | 30 | Not started |
| World systems | [Perception](Perception.md) | Light emitters, ambient and skylight, luminosity queries, concealment, warn and detect radii, PvP visibility tiers | Gameplay, World, Environment (time of day, weather), Water[^engine-water] (underwater light) | yes | PRC-1[^engine-perception] to PRC-3 | 25 | Not started |
| World systems | [Navigation](Navigation.md) | Detour and DetourTileCache runtime, nav tiles streamed by cell, path queries behind the MoveGoal contract, 3D flight paths for flying actors | World, Simulation, Assets (Detour) | yes | NAV-1[^engine-navigation], NAV-2 | 31 | Not started |
| World systems | [AI](AI.md) | BehaviorTree.CPP adapter, working memory, wake, sleep and leash, intents to commands, reactive combat | Gameplay, Combat, Perception, Navigation (BehaviorTree.CPP) | yes | AI-1[^engine-ai] to AI-6 | 32 | Not started |
| World systems | [Environment](Environment.md) | Environment clock, wind and current routines (cyclic baseline, direction and signed speed), current modulation, tides, gameplay waves, weather state and time-boxed overrides, world routines (route positions from world time) | Core, World, Simulation | yes | ENV-1 to ENV-6 | 25 | Not started |
| World systems | [Water](Water.md) | WaterSystem, bodies, rivers and routes, ocean current fields, cooked fragments, buoyancy, character, object and watercraft movement | World, Physics, Simulation, Gameplay, Environment, DistributedSimulation (ship-group handoff) | yes | WATER-1 to WATER-7 | 40 | Not started |
| Networking | [Network](Network.md) | Transport interface (header-only `Runic::NetworkTransport`[^engine-network]), ingress and egress queues, all realtime wire encoding, interest and budgets, prediction and reconciliation, parallel replication, world transitions; the Network and Diagnostics layers | Application, Simulation, DistributedSimulation, Gameplay, Perception | yes | NET-1 to NET-8 | 12 | Not started |
| Networking | [DistributedSimulation](DistributedSimulation.md) | Authority state and epochs, ghost state as plain data, transfer with a snapshot-contributor registry, foreign-interaction contract, authority partitions that never cut indivisible volumes | Simulation, Events, World | yes | DSIM-1 to DSIM-5 | 36 | Not started |
| Presentation | [Client](Client.md) | Prediction and Presentation layers, extractor registry and Engine extractors, camera, interpolation, input mapping and command builder | Application, Simulation, Network, Perception, Environment, Water, Renderer[^engine-renderer], Animation[^engine-animation], Audio[^engine-audio], UI[^engine-ui], Assets | no | CLI-1[^engine-client] to CLI-8 | 5 | Not started |
| Presentation | [Renderer](Renderer.md) | bgfx device and views; the layered debug pipeline; render targets for diegetic UI; the batched world-UI feature; the production pipeline later | Core, Assets (bgfx) | no | RND-1 to RND-10 | 2 | Not started |
| Presentation | [Animation](Animation.md) | Skeletons, clips, animator, blending, poses | Core, Assets | no | ANIM-1 | 23 | Not started |
| Presentation | [UI](UI.md) | RmlUi (GameUI) and ImGui (development overlay) as layers; world UI: diegetic panels, anchored elements, batched fallback, budgets, input routing | Application, Renderer, Assets (RmlUi, ImGui) | no | UI-1 to UI-5 | 2 | Not started |
| Presentation | [Audio](Audio.md) | Backend abstraction, buses and mixing, semantic sound events, MusicDirector, albums, soundtrack presets, local profiles | Core, Assets | no | AUD-1, AUD-2 | 24 | Not started |
| Tools | [Editor](Editor.md) | Editor[^engine-editor] host and panels, inspectors, resolver debugger, AI editor, live attach, Cooker[^engine-cooker] launcher | Most modules (ImGui, imnodes) | never shipped | EDT-1 to EDT-7 | 6 | Not started |
| Tools | [Cooker](Cooker.md) | Cooking orchestration and Runic format writers over fastgltf, meshoptimizer, MikkTSpace, xatlas, KTX and Basis Universal, Jolt cooking, Recast and Zstandard | Core, Content, Assets, World, Physics, Navigation, Water (each runtime module owns its cooked format; the Cooker links its writer) | tool | COOK-1 to COOK-8 | 9 | Not started |

The build skeleton package BUILD-1[^doc-build-build-1] (slice 0) belongs to no module ([contract](../build/BUILD-1.md)): it creates the `runic_add_module` helper, presets, CI, the chosen test framework, the headless and forbidden-dependency checks and the x86-64-v3 CPU baseline ([../current-state.md](../current-state.md)).

## Group and interface targets

| Target | Kind | What it is | Defined by | First slice |
| --- | --- | --- | --- | --- |
| `Runic::GameplayAPI` | Umbrella (INTERFACE) | Links Gameplay, Combat[^engine-combat], GameplayFlow[^engine-gameplayflow] and Quest so the Game links one name; each module keeps its own boundary and tests. No code of its own | The GameplayAPI group folder | 14 (first GameplayAPI package) |
| `Runic::NetworkTransport`[^engine-network] | Header-only interface target | The transport interface and the connect-ticket hook. Headers include only Core types. RunicPlatform's Transport implements it | Network, NET-1 | 12 |
| `Runic::SimulationPort`[^engine-simulation] | Header-only interface target | The service port: outbox of durable operations, inbox of accepted results. Headers include only Core types. RunicPlatform's ServicesBridgeLayer[^platform-serverbindings] implements it | Simulation, SIM-5 | 27 |

RunicPlatform's C++ links only `Runic::Core`[^engine-core], `Runic::Application`[^engine-application], `Runic::NetworkTransport` and `Runic::SimulationPort`, never Flecs, Jolt or a gameplay module.

Transitional targets: today's bundles `Runic::Runtime` and `Runic::Presentation` stay until RunicGame links per-module targets (decided in [OQ-8](../system/11-open-questions.md#oq-8)). Do not add new code to them.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Catalog:** [[../modules/Core|Core]] · [[../modules/Application|Application]] · [[../modules/Events|Events]] · [[../modules/Content|Content]] · [[../modules/Assets|Assets]] · [[../modules/ECS|ECS]] · [[../modules/World|World]] · [[../modules/Physics|Physics]] · [[../modules/Simulation|Simulation]] · [[../modules/Gameplay|Gameplay]] · [[../modules/Combat|Combat]] · [[../modules/GameplayFlow|GameplayFlow]] · [[../modules/Quest|Quest]] · [[../modules/Perception|Perception]] · [[../modules/Navigation|Navigation]] · [[../modules/AI|AI]] · [[../modules/Environment|Environment]] · [[../modules/Water|Water]] · [[../modules/Network|Network]] · [[../modules/DistributedSimulation|DistributedSimulation]] · [[../modules/Client|Client]] · [[../modules/Renderer|Renderer]] · [[../modules/Animation|Animation]] · [[../modules/UI|UI]] · [[../modules/Audio|Audio]] · [[../modules/Editor|Editor]] · [[../modules/Cooker|Cooker]]
- **See also:** [[../README|Engine docs README]] · [[../system/README|System README]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
[^engine-application]: [[../modules/Application|Application]] — Engine module · Foundation
[^engine-events]: [[../modules/Events|Events]] — Engine module · Foundation
[^engine-content]: [[../modules/Content|Content]] — Engine module · Foundation
[^engine-assets]: [[../modules/Assets|Assets]] — Engine module · Foundation
[^engine-ecs]: [[../modules/ECS|ECS]] — Engine module · Simulation core
[^engine-world]: [[../modules/World|World]] — Engine module · Simulation core
[^engine-physics]: [[../modules/Physics|Physics]] — Engine module · Simulation core
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-environment]: [[../modules/Environment|Environment]] — Engine module · World systems
[^engine-gameplay]: [[../modules/Gameplay|Gameplay]] — Engine module · GameplayAPI
[^engine-distributedsimulation]: [[../modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-gameplayflow]: [[../modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-quest]: [[../modules/Quest|Quest]] — Engine module · GameplayAPI
[^engine-water]: [[../modules/Water|Water]] — Engine module · World systems
[^engine-perception]: [[../modules/Perception|Perception]] — Engine module · World systems
[^engine-navigation]: [[../modules/Navigation|Navigation]] — Engine module · World systems
[^engine-ai]: [[../modules/AI|AI]] — Engine module · World systems
[^engine-network]: [[../modules/Network|Network]] — Engine module · Networking
[^engine-renderer]: [[../modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-animation]: [[../modules/Animation|Animation]] — Engine module · Presentation
[^engine-audio]: [[../modules/Audio|Audio]] — Engine module · Presentation
[^engine-ui]: [[../modules/UI|UI]] — Engine module · Presentation
[^engine-client]: [[../modules/Client|Client]] — Engine module · Presentation
[^engine-editor]: [[../modules/Editor|Editor]] — Engine module · Tools
[^engine-cooker]: [[../modules/Cooker|Cooker]] — Engine module · Tools
[^doc-build-build-1]: [[../build/BUILD-1|Engine BUILD-1 contract]] — RunicEngine doc
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
