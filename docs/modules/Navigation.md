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

Navigation is the runtime pathfinding of the authoritative server: the Detour and DetourTileCache runtime, nav tiles streamed by cell, and path queries behind the MoveTo and MoveGoal contract that AI and scripted movement use (ARCH § Repositories, build targets and processes; ARCH § AI architecture).

## Owns

- The Detour and DetourTileCache runtime, kept private (NAV-1).
- Nav tiles streamed by cell: tiles load and unload with their cells (NAV-1).
- Path queries behind the MoveTo and MoveGoal contract (NAV-1). An AI intent becomes a `MoveGoalCommand` with an actor and a destination; Navigation answers the path that movement follows.
- Navmesh tiles as the module's own spatial structure; Region, Sector and Cell stay the shared vocabulary (ARCH § World space: coordinates, layers and partitions).

## Does not own

- Navmesh cooking with Recast (Cooker, COOK-6). Recast is a tool library and never ships in the runtime.
- Decisions about where to go (AI, GameplayFlow) and movement execution (Gameplay movement modes on Physics CharacterVirtual).
- The decision which cells are loaded (World streaming state, WORLD-3) and package I/O (see OQ-3).
- Water navigation graphs and AI vessel routes (Water, a later stage).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | World, Simulation, Assets |
| Third-party (PRIVATE) | Detour and DetourTileCache |

### Must never depend on

- AI, Gameplay, Combat or any module above it; they call Navigation.
- Detour types in `Public/`. Detour has no approved exception.
- Recast at runtime.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH's command bridge turns move intents into this command (ARCH § AI architecture, intents become ordinary commands):

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
| NAV-1 | Detour runtime, nav tiles streamed by cell, path queries behind MoveGoal | Paths cross cell borders; tiles load and unload with cells | WORLD-3, COOK-6 | 31 |

## Acceptance tests

The Done when column is the acceptance list. Slice 31 shows spawned NPCs walking navmesh paths across cells, using tiles cooked by COOK-6 from Engine test geometry.

## Open items

- [OQ-3](../system/11-open-questions.md#oq-3) (still open, narrowed): who performs the load of streamed cell data. Navigation may link Assets (ARCH revision 369) to read cooked nav tiles, and it owns the nav-tile format the Cooker writes. Who triggers the load of a cell's tiles is confirmed with OQ-3 in the NAV-1 contract session; do not decide it silently.
- The exact MoveTo and MoveGoal contract is fixed in the NAV-1 contract session together with its first consumer, AI-2.
