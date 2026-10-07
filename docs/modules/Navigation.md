# Navigation

> **Contract seed.** When `Engine/Navigation/` is created, copy this file to
> `Engine/Navigation/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, and Primary consumers come
> from Appendix J §J.4.16. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Navigation/` |
| CMake target | `RunicNavigation` → `Runic::Navigation` |
| Namespace | `Runic::Navigation` |
| Roadmap phase | 11 — Navigation/Spawn/AI |
| Headless | yes |
| Status | Not started — Detour not yet added |

## Purpose

Runtime path and navigation queries over cooked navigation data.

## Owns

`NavigationWorld`, path query/result, agent/query handles, the Detour-backed
runtime.

## Does not own

Recast navmesh building (Cooker); AI decision policy (AI); Game route rules.

## Allowed dependencies

- Core, World.
- Detour backend (private).

## Forbidden dependencies

Recast (tool-only, Cooker), AI, Combat, Simulation mutation, RunicPlatform,
RunicGame.

## Primary consumers

AI, Gameplay movement, Game NPC logic.

## Public API (conceptual)

```text
Navigation/
├── Public/
│   └── Navigation.hpp       # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

Path queries are called from simulation phases; long queries may be sliced
across ticks but never block a tick.

## Data ownership (derived)

Owns loaded navmesh data (per Region/Sector/Cell as streamed) and query state.

## Integration points (derived)

- Cooker produces navmesh data with Recast; Navigation loads it.
- AI requests paths; movement follows them via commands.

## Acceptance tests (Stage A, derived)

- [ ] Load a cooked navmesh; find a path between two reachable points.
- [ ] Unreachable target returns a clear failure result.
- [ ] No Detour type in `Public/`; builds headless.

## Future extensions

Per-cell navmesh streaming; dynamic obstacles if a consumer requires them.
