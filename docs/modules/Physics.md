# Physics

> **Contract seed.** When `Engine/Physics/` is created, copy this file to
> `Engine/Physics/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.7 and the J.13.1 example. Other
> sections are derived and are proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Physics/` |
| CMake target | `RunicPhysics` → `Runic::Physics` |
| Namespace | `Runic::Physics` |
| Roadmap phase | 1 — authoritative backbone |
| Headless | yes (mandatory) |
| Status | Not started (Jolt 5.2.0 is already pinned) |

## Purpose

Jolt-backed physical world, collision, character queries, shapes, bodies, and
spatial queries behind Runic types.

## Owns

- `PhysicsWorld`.
- Body and shape handles.
- Character controller integration.
- Ray casts, shape casts, overlaps.
- Collision layers.
- Physics debug extraction (plain data for Renderer/Editor).

## Does not own

Damage rules, faction checks, ability rules, renderer commands, simulation
ownership, Game semantics. Physics answers geometric/physical questions only.

## Allowed dependencies

- Core, World.
- Jolt — **private** backend dependency.

## Forbidden dependencies

Simulation, Combat, Renderer, networking, Editor, RunicGame, RunicPlatform
services. [J.13.1, J.11.2]

## Primary consumers

Simulation, Combat, Water, Gameplay movement, Editor.

## Public API (conceptual)

```text
Physics/
├── Public/
│   ├── PhysicsWorld.hpp
│   ├── PhysicsTypes.hpp
│   ├── PhysicsBody.hpp
│   ├── PhysicsCharacter.hpp
│   ├── PhysicsShape.hpp
│   ├── PhysicsQuery.hpp
│   └── PhysicsDebug.hpp
├── Source/
│   ├── Queries/
│   ├── Characters/
│   └── Jolt/
├── Tests/
└── CMakeLists.txt
```

```cpp
PhysicsHit hit = physics.CastShape(shape, from, delta);
// hit says what geometry/body was intersected.
// Combat decides whether that hit means damage, block, dodge, etc.
```

Jolt types must not escape the backend unless an explicit public-contract
exception is approved. [J.4.7]

## Threading / tick model (derived)

- Physics has no clock of its own; Simulation owns "physics synchronization
  timing" and calls the step. [J.4.8]
- Queries are called from simulation phases; no mutation from other threads.
- Jolt's internal job threads are an implementation detail of `Source/Jolt/`.

## Data ownership (derived)

`PhysicsWorld` owns bodies and shapes; ECS components hold opaque handles.
Authoritative gameplay state (HP, faction, etc.) never lives here.

## Integration points (derived)

- Simulation steps the world and syncs transforms with Flecs.
- Combat calls casts/overlaps for hit detection and interprets results.
- `PhysicsDebug` exposes plain shape data; Renderer draws it. Physics never calls
  Renderer.
- Cooker produces cooked collision consumed here (phase 3).

## Acceptance tests (Stage A, derived)

- [ ] Create a world with a static ground and a dynamic body; step N fixed ticks.
- [ ] Ray cast hit/miss with correct body/handle and contact data.
- [ ] Shape cast and overlap queries return geometry results only.
- [ ] Character controller walks on ground and is blocked by a wall (phase 1
      "movement/collision" proof).
- [ ] Collision layers filter queries.
- [ ] A translation unit including every `Public/` header compiles without the
      Jolt include path.
- [ ] Builds and runs headless.

## Future extensions

Cooked collision loading (phase 3); per-cell collision streaming (phase 7);
buoyancy support needed by Water (phase 12).

## Open questions

- Physics step rate/tick domain and determinism requirements
  ([OQ-12](../system/11-open-questions.md#oq-12)).
- Character physics on the client for prediction
  ([OQ-15](../system/11-open-questions.md#oq-15)).
