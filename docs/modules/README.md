# RunicEngine module catalog

One contract per module. Each file follows the per-module `ARCHITECTURE.md`
contract ([system/02](../system/02-module-contract.md#the-per-module-architecturemd-contract-j33)).
When a module directory is created, its contract moves to
`Engine/<Module>/ARCHITECTURE.md` and the row below links there.

**Provenance legend.** In every file, *Purpose, Owns, Does not own, Allowed
dependencies, Primary consumers*, layouts, and API sketches come from Appendix J
§J.4. Sections marked **(derived)** — forbidden dependencies, threading, data
ownership, integration points, acceptance tests — were inferred from §J.7–J.14
and are proposals until the owner confirms them.

**Status values:** `Not started` · `Stage A` · `Stage B` · `Stage C`
([maturity stages](../system/02-module-contract.md#maturity-stages-j132-j12)).

| Module | Purpose (short) | Allowed dependencies | Phase | Headless | Status |
| --- | --- | --- | --- | --- | --- |
| [Core](Core.md) | Foundation types: IDs, Result, Assert, Time, Math, Logging | std, GLM | 0 | yes | Not started |
| [Application](Application.md) | Process shell, LayerStack, AppEvent | Core | 0 | yes | Not started |
| [Events](Events.md) | Tick-safe typed gameplay facts | Core | 1 | yes | Not started |
| [Content](Content.md) | Namespaced ContentId, schemas, registry | Core | 1 / 6 | yes | Not started |
| [ECS](ECS.md) | Flecs integration and conventions | Core, Flecs | 1 | yes | Not started |
| [World](World.md) | Region/Sector/Cell, spatial queries, streaming vocabulary | Core, ECS, Content | 1 / 7 | yes | Not started |
| [Physics](Physics.md) | Jolt-backed bodies, character, queries | Core, World, (Jolt) | 1 | yes | Not started |
| [Simulation](Simulation.md) | Fixed-step runtime, 60/30/20/10 Hz, phases, commands | Core, ECS, Events, World, Physics, Content | 1 | yes | Not started |
| [DistributedSimulation](DistributedSimulation.md) | Authority, ghosts, transfer semantics | Core, Events, World, Simulation | 13 | yes | Not started |
| [Assets](Assets.md) | Runtime asset loading, packages, cache | Core, Content | 2 / 3 / 7 | yes | Not started |
| [Renderer](Renderer.md) | bgfx rendering, camera, debug draw | Core, Assets, World, (bgfx) | 2 | no | Not started |
| [Client](Client.md) | Input mapping, prediction, reconciliation, interpolation | Application, Events, Simulation, NetworkAdapter, Renderer, Assets, presentation | 2 / 4 | no | Not started |
| [Animation](Animation.md) | Skeleton, clips, animator, graph | Core, Assets | 2 | no | Not started |
| [Audio](Audio.md) | Spatial/non-spatial playback | Core, Assets, (backend) | 2 | no | Not started |
| [UI](UI.md) | RmlUi integration, view/model bridge | Core, Application, Assets, (RmlUi) | 2 | no | Not started |
| [Navigation](Navigation.md) | Detour path queries | Core, World, (Detour) | 11 | yes | Not started |
| [Gameplay](Gameplay.md) | Character, stats, items, runtime inventory, interaction, faction, spawn, restrictions | Core, ECS, Events, Content, World, Simulation | 5 | yes | Not started |
| [GameplayFlow](GameplayFlow.md) | Sequence/action/condition runtime | Core, Events, Gameplay, Simulation, Content | 10 | yes | Not started |
| [Quest](Quest.md) | Quest/objective/trigger runtime | Core, Events, Gameplay, GameplayFlow, Content, Simulation | 10 | yes | Not started |
| [Combat](Combat.md) | Abilities, targeting, hit detection, effects, cooldowns, CC, foreign combat | Core, Events, Gameplay, Physics, World, Simulation, Content, DistributedSimulation | 5 / 14 | yes | Not started |
| [AI](AI.md) | Perception, aggro, BehaviorTree adapter, intents | Core, Events, Gameplay, Navigation, World, Simulation, Content | 11 | yes | Not started |
| [Water](Water.md) | Water bodies, buoyancy, currents, watercraft | Core, World, Physics, Gameplay, Simulation | 12 | yes | Not started |
| [NetworkAdapter](NetworkAdapter.md) | Simulation-safe ingress/egress seam to Platform | Core, Events, Simulation, DistributedSimulation, Platform contracts | 4 / 8 / 13 | yes | Not started |
| [Cooker](Cooker.md) | Offline content → native assets/packages | Core, Content, format/World defs, tool libs | 3 / 7 | tool | Not started |
| [Editor](Editor.md) | Inspectors and authoring (leaf) | Application, Renderer, ECS, World, Physics, AI, Water, Content, Assets, Simulation | 0+ / 11 | no (leaf) | Not started |

Full dependency rules: [system/03-dependency-rules.md](../system/03-dependency-rules.md).
