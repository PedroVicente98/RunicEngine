# World

> **Contract seed.** When `Engine/World/` is created, copy this file to
> `Engine/World/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, and layout
> come from Appendix J §J.4.6 and §J.8.2. Other sections are derived and are
> proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/World/` |
| CMake target | `RunicWorld` → `Runic::World` |
| Namespace | `Runic::World` |
| Roadmap phase | 1 (basic) — streaming in 7 |
| Headless | yes |
| Status | Not started |

## Purpose

Reusable logical-world, spatial partition, streaming vocabulary, and spatial
query foundation.

## Owns

- World position.
- Region, and the automatic Sector/Cell structures beneath it.
- `SpatialLayer`, bounds, membership.
- Streaming state.
- Spatial candidate queries (broad phase).

## Does not own

Simulation-process ownership policy (Platform WorldControl); combat rules;
renderer implementation; Game continents/territories (Game `GameWorld`).

## Allowed dependencies

- Core, ECS, Content where needed.

## Forbidden dependencies

Physics (Physics depends on World), Simulation, DistributedSimulation, every
gameplay module, Renderer, RunicPlatform, RunicGame.
**Region/Sector/Cell must never use `SimulationId` as identity.** [J.11.2]

## Primary consumers

Physics, Simulation, Navigation, AI, Water, Renderer streaming,
DistributedSimulation, Cooker, Editor.

## Public API (conceptual)

```text
World/
├── Public/
│   ├── World.hpp
│   ├── WorldPosition.hpp
│   ├── Region.hpp
│   ├── Sector.hpp
│   ├── Cell.hpp
│   ├── SpatialLayer.hpp
│   ├── SpatialBounds.hpp
│   ├── SpatialQuery.hpp
│   └── WorldStreaming.hpp
├── Source/
│   ├── Spatial/
│   ├── Streaming/
│   └── Regions/
├── Tests/
└── CMakeLists.txt
```

```text
Continent            (Game content)
└── Region
    └── Sector       (automatic)
        └── Cell     (automatic)
```

Region/Sector/Cell describe the logical/spatial world. They stay independent of
which simulation process owns dynamic entities in that space. [J.4.6]

## Threading / tick model (derived)

- Membership updates happen on the simulation thread during simulation phases.
- Spatial queries are read-only and may be called by any system during a phase.
- Streaming state changes are requested asynchronously and applied at safe tick
  boundaries.

## Data ownership (derived)

World owns the spatial structure and membership index. Entities' authoritative
positions live in Flecs components; World indexes them.

## Integration points (derived)

- Emits a `RegionChanged`-style fact through Events when an entity changes
  Region (example fact from [J.7.1]).
- Physics, Navigation, Water consume bounds and cells for their own data.
- WorldControl (Platform) assigns `AuthorityPartition`s over `SpatialBounds`,
  but World never knows about them.

## Acceptance tests (Stage A, derived)

- [ ] Sectors/Cells are derived automatically from a Region definition.
- [ ] Position → Region/Sector/Cell lookup, including boundaries.
- [ ] Membership updates as an entity moves; a region change emits a fact.
- [ ] Spatial candidate query returns all entities in bounds (exact geometry is
      Physics's job).
- [ ] No type in `Public/` references `SimulationId`.
- [ ] Builds headless.

## Future extensions

Phase 7: Region/Sector/Cell streaming of collision/render data with ownership
still separate.

## Open questions

- World-position precision for a large seamless world (double precision vs.
  cell-relative coordinates) is not specified in Appendix J
  ([OQ-12](../system/11-open-questions.md#oq-12)).
- Streaming ↔ Assets relationship ([OQ-3](../system/11-open-questions.md#oq-3)).
