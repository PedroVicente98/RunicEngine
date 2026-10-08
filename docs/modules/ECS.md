# ECS

> **Contract seed.** When `Engine/ECS/` is created, copy this file to
> `Engine/ECS/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, and layout
> come from Appendix J §J.4.5. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/ECS/` |
| CMake target | `RunicECS` → `Runic::ECS` |
| Namespace | `Runic::ECS` |
| Roadmap phase | 1 — authoritative backbone |
| Headless | yes |
| Status | Not started (Flecs 4.0.4 is already pinned in `cmake/Dependencies.cmake`) |

## Purpose

Flecs integration layer and Engine-wide ECS conventions.

## Owns

- Flecs world wrapper and lifecycle helpers.
- `Entity` abstraction.
- Registration and query helpers.
- ECS module conventions (how a module registers components/systems).

## Does not own

Gameplay policy, physics, persistence, networking, renderer commands. The
*instance* of the authoritative Flecs world is owned by Simulation [J.4.8]; ECS
provides the wrapper and conventions.

> Flecs is the authoritative simulation state/rule graph, not the database,
> geometry engine, distributed coordinator, or renderer. [J.4.5]

## Allowed dependencies

- Core.
- Flecs — private, or public only as a deliberate, documented choice.

## Forbidden dependencies

World, Simulation, every gameplay module, Physics/Jolt, Renderer/bgfx,
RunicPlatform, RunicGame.

## Primary consumers

World, Simulation, Gameplay modules, Editor inspectors.

## Public API (conceptual)

```text
ECS/
├── Public/
│   ├── ECSWorld.hpp
│   ├── Entity.hpp
│   ├── EntityId.hpp
│   ├── ECSModule.hpp
│   └── ECSQuery.hpp
├── Source/
│   └── Flecs/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

- The world is mutated only on the simulation thread, inside Simulation phases.
- Structural changes requested mid-phase are deferred and applied at the phase
  boundary (Simulation owns that timing).

## Data ownership (derived)

- `EntityId` is a process-local handle. It never appears on the wire or in
  durable storage. [J.8.2]
- The mapping `GlobalEntityId ↔ EntityId` is maintained above ECS (Simulation /
  DistributedSimulation).

## Integration points (derived)

- `ECSModule` convention: each feature module exposes a registration function
  that declares its components and systems.
- Editor inspectors read the world through ECS helpers.

## Acceptance tests (Stage A, derived)

- [ ] Create/destroy a world; create/destroy entities; alive checks.
- [ ] Register components via the module convention; query and iterate.
- [ ] `EntityId` and `GlobalEntityId` are distinct, non-convertible types.
- [ ] Builds headless.

## Future extensions

Editor reflection/inspection helpers; serialization helpers for transfer
snapshots (data extraction only — wire format belongs to Platform).

## Open questions

- Whether Flecs types appear in `Public/` (so gameplay modules write native Flecs
  systems) or stay hidden behind wrappers. The source allows either "as
  deliberately chosen"; decide before Simulation and Gameplay are written.
