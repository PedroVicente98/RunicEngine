# Water

> **Contract seed.** When `Engine/Water/` is created, copy this file to
> `Engine/Water/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, and Primary consumers come
> from Appendix J §J.4.22. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Water/` |
| CMake target | `RunicWater` → `Runic::Water` |
| Namespace | `Runic::Water` |
| Roadmap phase | 12 — Water/Naval proof |
| Headless | yes |
| Status | Not started |

## Purpose

Queryable macro-water: rivers, oceans, lakes, buoyancy, water movement, and
reusable watercraft mechanics.

## Owns

- Water registry, water bodies, water samples.
- Spline rivers and flow.
- Buoyancy queries.
- Current movement.
- Character swimming hooks.
- Watercraft movement primitives.

## Does not own

Full fluid simulation; concrete ship classes, cargo, cannons, naval game rules
(Game `GameNaval`).

## Allowed dependencies

- Core, World, Physics, Gameplay, Simulation.

## Forbidden dependencies

Renderer (rendering of water is a consumer concern), Combat, RunicPlatform,
RunicGame.

## Primary consumers

RunicGame naval/world gameplay, Renderer, Cooker, AI/navigation consumers.

## Public API (conceptual)

```text
Water/
├── Public/
│   └── Water.hpp            # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

Queries from simulation phases; buoyancy/current forces applied in sync with the
physics step owned by Simulation.

## Data ownership (derived)

Owns water body definitions and runtime sampling structures; watercraft
authoritative state lives in Flecs components on the owning simulation.

## Integration points (derived)

- Physics: buoyancy/current forces on bodies.
- Gameplay movement: swimming hooks.
- Game `GameNaval`: ship rules on top of watercraft primitives.

## Acceptance tests (Stage A, derived — phase 12 proof)

- [ ] Sample water height/flow at a position (ocean, lake, spline river).
- [ ] A body floats with buoyancy and drifts with current.
- [ ] A character transitions to swimming at the surface.
- [ ] A watercraft moves with primitive controls.
- [ ] Builds headless.

## Open questions

- Renderer and Cooker are listed consumers, but Water is not in their allowed
  dependencies (Renderer: Core, Assets, World; Cooker: Core, Content, format/World
  definitions). Proposal: water data reaches them as World/Assets data or
  presentation data prepared by Client, not by linking Water.
- Water subsystem reference not imported ([OQ-12](../system/11-open-questions.md#oq-12)).
