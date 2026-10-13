---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/world-systems
  - headless/yes
  - slice/31
aliases:
  - "Runic::Navigation"
---

# Navigation

Contract seed for `Runic::Navigation`: what a Claude Code session needs to implement the Navigation work package. When `Engine/WorldSystems/Navigation/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | World systems |
| Directory | `Engine/WorldSystems/Navigation/` |
| CMake target | `Runic::Navigation` |
| Namespace | `Runic::Navigation` |
| Headless | yes |
| Status | Not started |

## Purpose

Navigation is the runtime pathfinding of the authoritative server: the Detour and DetourTileCache runtime, nav tiles streamed by cell, path queries behind the MoveTo and MoveGoal contract that AI and scripted movement use, and 3D flight paths for flying actors (ARCH[^arch] § Repositories, build targets and processes; ARCH § AI architecture).

## Owns

- The Detour and DetourTileCache runtime, kept private (NAV-1).
- Nav tiles streamed by cell: tiles load and unload with their cells (NAV-1).
- Path queries behind the MoveTo and MoveGoal contract (NAV-1). An AI intent becomes a `MoveGoalCommand` with an actor and a destination; Navigation answers the path that movement follows.
- Flight navigation (NAV-2): 3D paths through free space over the sparse 3D cells for flying actors, respecting authored volumes and altitude ceilings.
- Navmesh tiles as the module's own spatial structure; Region, Sector and the sparse 3D Cell stay the shared vocabulary (ARCH[^arch] § World space: coordinates, cells and activation).

## Does not own

- Navmesh cooking with Recast (Cooker[^engine-cooker], COOK-6). Recast is a tool library and never ships in the runtime.
- Decisions about where to go (AI, GameplayFlow[^engine-gameplayflow]) and movement execution (Gameplay movement modes on Physics[^engine-physics] CharacterVirtual).
- The decision which cells are loaded (World activation, WORLD-3[^engine-world]) and package I/O (see OQ-3[^oq-3]).
- Water[^engine-water] navigation graphs and AI vessel routes (Water, a later stage).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | World[^engine-world], Simulation[^engine-simulation], Assets[^engine-assets] |
| Third-party (PRIVATE) | Detour and DetourTileCache |

### Must never depend on

- AI, Gameplay, Combat[^engine-combat] or any module above it; they call Navigation.
- Detour types in `Public/`. Detour has no approved exception.
- Recast at runtime.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH[^arch]'s command bridge turns move intents into this command (ARCH § AI architecture, intents become ordinary commands):

```cpp
// Illustrative
case AIIntentType::MoveTo:
case AIIntentType::Investigate: commands.Push(MoveGoalCommand{ .actor = intent.actor,
                                    .destination = intent.position }); break;
```

Headers (sketch, fixed in the contract session): `NavigationWorld.hpp`, `NavTile.hpp`, `PathQuery.hpp`, `MoveGoal.hpp`.

## Threading and tick rules

- Path queries run inside simulation phases; AI emits move goals at 20 Hz and movement runs at 30 Hz.
- Tile registration follows cell streaming at safe points; loading never blocks a tick.
- Queries are read-only and may run on workers in parallel phases; results reach entities through commands, never by direct mutation.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| NAV-1 | Detour runtime, nav tiles streamed by cell, path queries behind MoveGoal | Paths cross cell borders; tiles load and unload with cells | WORLD-3[^engine-world], COOK-6[^engine-cooker] | 31 |
| NAV-2 | Flight navigation: 3D paths through free space over the sparse cells for flying actors, respecting volumes and altitude ceilings | A flying NPC reaches a target behind an obstacle without entering a forbidden volume | NAV-1, WORLD-2 | 44+ |

## Acceptance tests

The Done when column is the acceptance list. Slice 31 shows spawned NPCs walking navmesh paths across cells, using tiles cooked by COOK-6[^engine-cooker] from Engine test geometry.

## Open items

- [OQ-3](../system/11-open-questions.md#oq-3) (still open, narrowed): who performs the load of streamed cell data. Navigation may link Assets (ARCH[^arch] revision 369) to read cooked nav tiles, and it owns the nav-tile format the Cooker[^engine-cooker] writes. Who triggers the load of a cell's tiles is confirmed with OQ-3[^oq-3] in the NAV-1 contract session; do not decide it silently.
- The exact MoveTo and MoveGoal contract is fixed in the NAV-1 contract session together with its first consumer, AI-2[^engine-ai].

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/World|World]] · [[../modules/Simulation|Simulation]] · [[../modules/Assets|Assets]]
- **Depended on by:** [[../modules/AI|AI]] · [[../modules/Cooker|Cooker]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-cooker]: [[../modules/Cooker|Cooker]] — Engine module · Tools
[^engine-gameplayflow]: [[../modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-physics]: [[../modules/Physics|Physics]] — Engine module · Simulation core
[^engine-world]: [[../modules/World|World]] — Engine module · Simulation core
[^oq-3]: [[../system/11-open-questions#OQ-3 — Asset loading for streamed cells (narrowed)|OQ-3]] — open question or decision
[^engine-water]: [[../modules/Water|Water]] — Engine module · World systems
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-assets]: [[../modules/Assets|Assets]] — Engine module · Foundation
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-ai]: [[../modules/AI|AI]] — Engine module · World systems
