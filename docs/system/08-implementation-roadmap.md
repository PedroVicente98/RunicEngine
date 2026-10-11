# 08 — Implementation roadmap

This file lists every roadmap slice in order, with its runnable result, what it proves and the work packages it needs.

Source: ARCH § Vertical-slice roadmap, § World authoring and cooking (Authoring work per roadmap slice),
§ AI architecture (Implementation order), § Render data, extraction and world UI (Two render pipelines).

## How the roadmap works

- Each slice is small, ends in something **runnable and testable**, and adds one piece of the architecture on top of
  the previous ones.
- The Packages column names the work packages the slice needs. Engine and Platform packages are defined in
  [12-work-packages.md](12-work-packages.md). Game package IDs (GAME-0, GSH-*, GCL-*, GCNT-*, GSVC-*, GSRV-*, GNAV-*,
  GWTH-*) are listed by ID only; they are defined in RunicGame.
- **A slice is done when its packages are merged and its runnable result passes.**
- A package starts only when all its Needs are merged; packages with merged Needs may run in parallel
  ([10 § Parallel work](10-ai-development-workflow.md#parallel-work)).
- Dependency-first does not mean finishing foundations to production maturity first: every package starts at Stage A,
  and is generalized (B) and hardened (C) when a later slice needs it
  ([02 § Maturity stages](02-module-contract.md#maturity-stages)).
- Runnable results that ARCH states with Game content are written neutrally here; the Game's version is in RunicGame.

### Lettered slices

Slices with a letter (20a, 23a, 25a, 33a, 42a, 42b) were **inserted later between their neighbors, so existing
numbers never change**. A lettered slice runs after the slice with the same number and before the next number
(20a after 20 and before 21). Slice 44+ is the expansion range: its packages get their full definition when those
slices are planned.

## Slices

| Slice | Runnable result | Proves | Packages | Owner |
| --- | --- | --- | --- | --- |
| 0. Build skeleton | Engine library plus empty GameClient, GameServer, Editor and Cooker build in CI in all three repositories; the server links no graphics | CMake module helper, dependency and headless checks, test framework | BUILD-1 | Engine, Platform, Game |
| 1. Application and layers | A window opens; layers push, pop and replace at safe points; events stop when handled | Entry point, ApplicationHost, LayerStack, Window class | CORE-1, APP-1, APP-2, GAME-0 | Engine, Game |
| 2. Debug renderer | Grid, camera, colored shapes with opacity, text | bgfx, debug draw, ImGui development overlay | RND-1, RND-2, UI-1 | Engine |
| 3. Fixed-step clock | The scheduler runs 60/30/20/10 Hz on the server loop with metrics | Scheduler, TickContext, DomainTickCount, metrics registry | SIM-1, CORE-2 | Engine |
| 4. ECS and world coordinates | Entities with 64-bit positions are created and queried; one fact reaches two consumers | Flecs integration, coordinates and layers, event buffers | ECS-1, WORLD-1, EVT-1 | Engine |
| 5. Moving entities | Entities move on a plane through commands and are drawn | Phases, commands, RenderFrame, render extraction | SIM-2, CORE-3, CLI-1 | Engine |
| 6. Editor shell | The Editor opens a running local world and inspects entities | Editor host, panels, ECS inspector | EDT-1 | Engine |
| 7. Physics | A capsule walks on static terrain far from the origin under keyboard control | Jolt in double precision, CharacterVirtual, input mapping | PHYS-1, PHYS-2, CLI-2 | Engine |
| 8. Physics view | Collision shapes are drawn and can be inspected | Debug-geometry extraction, debug pipeline layers, physics inspector | PHYS-3, RND-3, EDT-2 | Engine |
| 9. First cooker proof | A mesh made in Blender is cooked, rendered and walked on | Content IDs, asset loading, glTF to mesh and collision | CNT-1, ASSET-1, COOK-1 | Engine |
| 10. Worker pool | The same scene with several workers gives identical results | Phases on workers, per-worker buffers, deterministic merge, Jolt on the worker pool | PHYS-4, SIM-3 | Engine |
| 11. Spatial index | Cells and layer membership drawn; queries visualized | Spatial index and queries | WORLD-2 | Engine |
| 12. Transport | A client connects to a local server with a test-signed ticket; ping and echo | Transport interface, GameNetworkingSockets transport, contracts | NET-1, PCT-1, PTR-1, PTR-2 | Engine, Platform |
| 13. Replication | Two clients see each other move | Snapshots, quantization, interpolation | SIM-6, NET-2 | Engine |
| 14. Prediction | Responsive movement with correction under simulated lag | Shared-code prediction, reconciliation, movement modes | SIM-4, GPL-1, NET-3, CLI-3, GSH-1 | Engine, Game |
| 15. Character state | A dummy has health, tags and modifiers; casts and overlaps are drawn | Attributes, modifiers, resources, gameplay queries | GPL-2, PHYS-5 | Engine |
| 16. Ability toolkit | One melee swing and one projectile hit a dummy | Timelines, hit queries, ServerTime hits | CMB-1, CMB-2, GSH-2 | Engine, Game |
| 17. Statuses and rules | Two Game statuses and one rule interaction between them | Status container, rule pipeline, the resolver and its debugger | CMB-3, CMB-4, EDT-3, GSH-3 | Engine, Game |
| 18. Defense | A dodge that spends a resource, a directional block, a crowd-control chain | Restrictions, damage interception, Game defense rules | GPL-3, CMB-5, GSH-4 | Engine, Game |
| 19. Combat prototype | The full combat prototype acceptance list ([09](09-validation-scenarios.md#combat-prototype-slice-19)) | Game systems end to end, threat and CC, input queue, first GameUI | CMB-6, CMB-7, UI-2, GCL-1, GSH-5 | Engine, Game |
| 20. Data-driven content | Skills, items and effects as data with live reload; one custom behavior | Registries, schemas, Cooker validation, Cooker launcher | CNT-2, COOK-2, EDT-4, GCNT-1 | Engine, Game |
| 20a. Arena prototype | A first Game build set fights 3v3 in an arena | Build variety on the combat systems, builds as data | GSH-6 | Game |
| 21. World partitions | Walk across regions and two layers with streaming | World cooking, packages, streaming bands | WORLD-3, ASSET-2, COOK-3 | Engine |
| 22. World rendering | Cooked meshes and terrain draw flat-shaded far from the origin | Mesh render data and extractor, debug mesh layer, textures and LODs | CORE-4, CLI-4, RND-4, COOK-4 | Engine |
| 23. Animated characters | A skinned character animates with blended clips | Animation runtime, skinned extractor and debug layer, skinned-mesh cooking | ANIM-1, CLI-5, RND-5, COOK-5 | Engine |
| 23a. World UI | Nameplates and health bars follow entities; a notice board in the world accepts clicks | World anchors, anchored RmlUi, render targets, diegetic panels | CLI-6, RND-9, UI-3, UI-4, GCL-3 | Engine, Game |
| 24. Sound and music | Sound events play through buses; a music context switches albums | Audio backend, MusicDirector, soundtrack settings | AUD-1, AUD-2, GCL-2 | Engine, Game |
| 25. Interest | Only relevant entities arrive; visibility tiers at night | Environment clock, light, visibility tiers, priority budgets | ENV-1, PRC-1, PRC-2, NET-4 | Engine |
| 25a. Night on screen | Torches light the night; darkness zones and fog draw, matching what the server treats as dark | Light, environment and volume render data, extractors and debug layers | CORE-5, CLI-7, RND-7 | Engine |
| 26. Service foundation | A Go service writes to PostgreSQL idempotently on the local stack | Go SDK, persistence, local development stack | PINF-1, PSDK-1, PSDK-2 | Platform |
| 27. Accounts and characters | Log in, receive a real ticket, load and save a character | Service port, accounts, real tickets, services-bridge layer, Character service | SIM-5, PCS-1, PBND-1, GSVC-1 | Platform, Game |
| 28. Persistence | Kill, loot, log out, keep the item | Item runtime, item ledger, Inventory service, loot grants | GPL-4, PBB-1, GSVC-2, GSRV-1 | Engine, Platform, Game |
| 29. Scripted sequences | A boss phase locks the arena; a cutscene protects the player; a disconnect releases everything | Interactions, GameplayFlow, restrictions, termination path | GPL-5, FLOW-1, FLOW-2, GSRV-2 | Engine, Game |
| 30. Quests | A persistent quest driven by gameplay facts | Quest runtime and API, NATS in the SDK and the services bridge, Quest service | QST-1, QST-2, PSDK-3, PBND-4, GSVC-3, GSRV-3 | Engine, Platform, Game |
| 31. Navigation and spawning | Spawned NPCs walk navmesh paths across cells | Factions and spawns, Recast cooking, Detour runtime | GPL-6, COOK-6, NAV-1 | Engine |
| 32. AI brains | An NPC patrols and chases through behavior-tree intents | BehaviorTree.CPP adapter, intents, command bridge | AI-1, AI-2 | Engine |
| 33. Perception at night | Mobs warn, detect, fight and leash at night | Perception radii, AI lifecycle, Game mob profiles | PRC-3, AI-3, GSRV-4 | Engine, Game |
| 33a. Exploration prototype | Explore at night with a light source, die to a mob, respawn at a respawn point, and keep both the uncovered map and the character build | The Game's exploration loop end to end: night, light, mobs, respawn points, map knowledge, build persistence | PBB-2, GSRV-5, GSRV-6, GSVC-5, GCL-4 | Platform, Game |
| 34. Reactive combat | A mob reacts defensively to a telegraphed skill; its trace explains why | Reactive combat, AI instrumentation | AI-4, AI-5 | Engine |
| 35. AI editor | A behavior tree is edited, saved and reloaded; a live NPC is inspected | AI editor panels and tools | EDT-5, EDT-6 | Engine |
| 36. Multiple simulations | Walk from simulation A into B | Authority, ghosts, simulation links, transfer, World Coordinator, candidate connections | DSIM-1, DSIM-2, DSIM-3, NET-5, PCS-2, PTR-3 | Engine, Platform |
| 37. Cross-border combat | An attack across the border damages its target once; a sequence and a quest survive handoff | Foreign interactions, foreign combat, sequence and quest handoff | DSIM-4, CMB-8, FLOW-3, QST-3 | Engine |
| 38. Battle scale | 200, then 500 bots fighting in one region; nameplates fall back to the batched path | Parallel replication, budgets, degradation order, AI at scale, batched world UI | NET-6, AI-6, RND-10, UI-5 | Engine |
| 39. Operations | Fleets on Agones, the Grafana panel, Editor live attach | Diagnostics layer, lifecycle layer, metrics export, infrastructure | NET-7, PBND-2, PBND-3, PINF-2, EDT-7 | Engine, Platform |
| 40. Water bodies | A descending river crosses two cells without a seam | WaterSystem, rivers, water cooking | WATER-1, WATER-2, COOK-7 | Engine |
| 41. Floating and swimming | A platform floats, a crate drifts, a character swims | Buoyancy, water movement components | WATER-3, WATER-4 | Engine |
| 42. Ships, wind and currents | A sailing ship is faster downstream and with a favorable wind; a crosswind pushes it sideways | Wind, current fields and modulation, watercraft | ENV-2, WATER-5, ENV-3, WATER-6, GNAV-1 | Engine, Game |
| 42a. Water and currents on screen | River and ocean surfaces draw; currents and wind show as arrows and heatmaps | Water and field render data, extractors and debug layers | CORE-6, CLI-8, RND-8 | Engine |
| 42b. Ships across borders | A ship with passengers and cargo sails from simulation A into B with nothing lost | Ship-group handoff through the snapshot-contributor registry | WATER-7 | Engine |
| 43. Tides, waves and weather | A tide opens a path; a storm reaches two simulations at the same world time | Tides, weather overrides, Platform directives | ENV-4, ENV-5, PCS-3, GWTH-1 | Engine, Game, Platform |
| 44+. Expansion | Game services (economy, housing and others), instances, busy-city partitions, the production render pipeline, Game production systems, collections, rankings, flight and climbing, staged access | The same boundaries, reused | DSIM-5, RND-6, CORE-7, COOK-8, GPL-7, PBB-3, PCS-4, PCS-5, PCS-6, GSVC-4, GSVC-6, GSVC-7, GSVC-8, GSRV-7 | All |

## Supporting views of the same roadmap

### Debug pipeline layers by slice

The debug pipeline is built first, one layer per kind of render data. The production pipeline (RND-6) comes in 44+.

| Debug layer | Draws | Slice | Package |
| --- | --- | --- | --- |
| Debug shapes | Lines, shapes and text issued by any system | 2 | RND-2 |
| Physics geometry | Collision bodies and shapes extracted by Physics, with cached debug meshes for large shapes | 8 | RND-3 |
| Meshes | Cooked mesh instances, flat-shaded | 22 | RND-4 |
| Skinned meshes | Characters with their poses, flat-shaded | 23 | RND-5 |
| Lights, environment, volumes | Light gizmos and simple lighting, skylight and darkness, fog, darkness zones | 25a | RND-7 |
| Water and fields | Flat water surfaces; currents and wind as arrows, streaks or heatmaps | 42a | RND-8 |

### AI implementation steps by slice

| Steps (ARCH § AI architecture) | Packages | Slice |
| --- | --- | --- |
| 1 Contracts, 2 Runtime shell | AI-1 | 32 |
| 4 Command bridge, 6 Data assets | AI-2 | 32 |
| 3 Perception lifecycle | AI-3 | 33 |
| 5 Reactive combat | AI-4 | 34 |
| 7 Instrumentation | AI-5 | 34 |
| 8 AI editor MVP, 9 Runic-specific tools | EDT-5, EDT-6 | 35 |
| 10 Scale tests | AI-6 | 38 |

### World and asset work by slice

The authoring pipeline grows inside the slices; gameplay never stops for a long "build the whole editor and cooker"
phase.

| Slice | World and asset work |
| --- | --- |
| 0. Build skeleton | Cooker target and basic asset and runtime module boundaries; no production cooking yet |
| 9. First cooker proof | Blender to GLB to the Cooker to one Runic mesh and one Jolt collision asset; walk on it |
| 13. Replication | Client and headless server consume compatible cooked map data |
| 15–19. Combat | Only the collision and metadata real combat obstacles and queries need |
| 20. Data-driven content | Stable asset IDs, versioned metadata, Blender custom-property conventions, content registry integration |
| 21. World partitions | Major milestone: region, sector and cell cooking, async cell streaming, terrain tiling, Jolt cell collision, manifests, debug visualization |
| 22. World rendering | KTX2 textures, mesh LODs and tangents in client-only render packages |
| 23. Animated characters | Skinned meshes and animation clips cooked from Blender |
| 25. Interest | Network relevance uses the same spatial vocabulary without coupling replication to renderer internals |
| 28–30. Persistence, sequences and quests | Persistent world-object IDs and authored gameplay markers in cooked regions |
| 31. Navigation and spawning | Recast cooking and Detour tile runtime |
| 36. Multiple simulations | Packages stay unchanged while different simulations own adjacent space and players transfer |
| 40–43. Water and environment | Water fragments, river splines, current and wind maps |
| 44+. Production world | HLOD, vegetation pipeline, advanced terrain materials, light baking and probes, the full Blender add-on, incremental cooking, layout tools, large-world validation |

First cooker proof: a test region exported to GLB, cooked into a Runic mesh and a Jolt collision file; the client
renders the mesh, client and server collision work, and a character walks on it. KTX2, Recast, xatlas, HLOD, terrain
splatmaps, world manifests, vegetation, compression, region splitting and the complete add-on all wait until after
this proof.

## Where we are

Each repository records which packages are merged and which slice is in progress in its own `docs/current-state.md`.
This shared folder does not track progress.
