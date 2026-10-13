---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/simulation-core
  - headless/yes
  - slice/7
aliases:
  - "Runic::Physics"
---

# Physics

Contract seed for `Runic::Physics`: what a Claude Code session needs to implement the Physics work packages. When `Engine/SimulationCore/Physics/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Simulation core |
| Directory | `Engine/SimulationCore/Physics/` |
| CMake target | `Runic::Physics` |
| Namespace | `Runic::Physics` |
| Headless | yes |
| Status | Not started |

## Purpose

Physics answers geometric questions with Jolt v5.6 in double precision, kept behind Runic types: bodies, shapes, CharacterVirtual, queries, collision layers, debug-geometry extraction and the Jolt job system on the Core worker pool (ARCH[^arch] § Simulation core: Flecs and Jolt). Jolt results are facts; Runic decides what they mean.

## Owns

- `PhysicsWorld`, bodies, shapes and collision layers on Jolt v5.6 in double precision (`JPH_DOUBLE_PRECISION`), kept private (PHYS-1).
- CharacterVirtual integration and ground movement (PHYS-2).
- Debug-geometry extraction: Jolt's debug interface implemented inside Physics (`Source/Jolt`), written into Core `DebugGeometry` buffers (PHYS-3).
- The JobSystem adapter that runs Jolt on the Core worker pool, so Jolt never owns threads (PHYS-4).
- Gameplay queries (ray, shape cast, overlap) with per-worker query queues (PHYS-5).
- A shape registry: shapes are immutable and shared (many abilities reuse one arc shape).

| Runic concept | Jolt representation |
| --- | --- |
| Terrain, buildings | Static Body |
| Doors, elevators, platforms | Kinematic Body |
| Ships, crates, physical objects | Dynamic Body |
| Players and combat NPCs | CharacterVirtual (not a Body) |
| Melee volumes, AoEs | Shape used in CollideShape |
| Projectiles | Shape used in CastShape, one cast per tick segment |
| Triggers | Sensor Body |

Rules:

- Structural changes are batched at the 30 Hz boundary (AddBodiesPrepare and AddBodiesFinalize).
- Characters are discovered through the Runic cell grid and tested exactly with `CharacterVirtual::GetTransformedShape`. The same test with a past transform serves capped-rewind abilities.
- CharacterVirtuals update every 30 Hz tick even without input, so moving platforms and ships carry them.
- The Jolt version is pinned in the build; v5.6 (tag v5.6.0) is the documented baseline, built with its AVX2 path on the x86-64-v3 baseline.

## Does not own

- What a contact or hit means: damage, factions, blocks and dodges (Combat[^engine-combat], Gameplay, RunicGame).
- Movement modes and their parameters (Gameplay), water forces (Water[^engine-water] applies them before the step), the scheduler (Simulation).
- Drawing anything: the Renderer[^engine-renderer]'s physics view draws the plain buffers Physics fills.
- Collision cooking (Cooker[^engine-cooker] uses Jolt cooking itself). Collision shapes and render meshes are different data.

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core[^engine-core], World[^engine-world] |
| Third-party (PRIVATE) | Jolt Physics v5.6, double precision |

### Must never depend on

- Combat[^engine-combat], Gameplay or Game code: Physics never knows damage, factions, blocks or dodges.
- Renderer[^engine-renderer], and the Renderer never links Physics.
- Jolt types in `Public/`. Jolt has no approved exception.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

Physics' public API carries no Jolt types (ARCH[^arch] § Debug renderer, physics debug view):

```cpp
// Physics public API, no Jolt types:
physicsWorld.ExtractDebugGeometry(cameraOrigin, settings, debugGeometry);
```

Inside `Source/Jolt`, development builds only:

```cpp
// Runic::Physics, Source/Jolt (development builds only)
class JoltDebugExtractor final : public JPH::DebugRendererSimple {
public:
    explicit JoltDebugExtractor(DebugGeometry& out) : m_out(out) {}

    void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override {
        m_out.AddLine(ToCameraRelative(from), ToCameraRelative(to), ToABGR(color));
    }
    void DrawTriangle(JPH::RVec3Arg a, JPH::RVec3Arg b, JPH::RVec3Arg c,
                      JPH::ColorArg color, ECastShadow) override {
        DrawLine(a, b, color); DrawLine(b, c, color); DrawLine(c, a, color);   // wireframe first
    }
    void DrawText3D(JPH::RVec3Arg, const JPH::string_view&, JPH::ColorArg, float) override {
        // Optional; later exported as debug text.
    }
private:
    DebugGeometry& m_out;   // plain Core type: lines and triangles, no bgfx
};
```

Because Jolt runs in double precision, `ToCameraRelative` subtracts the render origin from each `JPH::RVec3` before converting to floats. Gameplay rendering never depends on `JPH_DEBUG_RENDERER`.

Headers (sketch, fixed in the contract session): `PhysicsWorld.hpp`, `PhysicsBody.hpp`, `PhysicsShape.hpp`, `CollisionLayers.hpp`, `CharacterController.hpp`, `PhysicsQueries.hpp`, `PhysicsDebug.hpp`.

## Threading and tick rules

Physics runs inside the 30 Hz realtime phase (ARCH[^arch] § Simulation core: clock and threads):

1. Step 3: commit structural physics changes in batches (bodies created and removed).
2. Step 4: apply kinematic targets, buoyancy forces and water-movement forces; step Jolt once.
3. Step 5: update CharacterVirtuals against the new world; write positions to Flecs and spatial membership; record position history for capped-rewind abilities.
4. Step 6 (Combat[^engine-combat]): combat, projectile and AoE queries use the gameplay queries.

- Jolt jobs run on the Core worker pool through the adapter; no thread is created outside the pool.
- Queries go through per-worker query queues; results are identical for a fixed scene with any worker count.
- Only the simulation's phases mutate physics state; never from an I/O callback.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| PHYS-1 | PhysicsWorld, bodies, shapes and collision layers on Jolt v5.6 in double precision, kept private | Bodies collide without jitter far from the origin | WORLD-1[^engine-world] | 7 |
| PHYS-2 | CharacterVirtual integration and ground movement | A capsule walks, climbs steps and stops at walls | PHYS-1 | 7 |
| PHYS-3 | Debug-geometry extraction: Jolt's debug interface implemented inside Physics, written into Core DebugGeometry buffers | Bodies and shapes come out as plain lines and triangles; Physics links no Renderer[^engine-renderer] | PHYS-1, CORE-3[^engine-core] | 8 |
| PHYS-4 | Jolt job system on the Core worker pool: a JobSystem adapter inside Physics, so Jolt never owns threads | A physics step on the pool matches a single-thread step; no thread is created outside the pool | PHYS-1, CORE-2 | 10 |
| PHYS-5 | Gameplay queries: ray, shape cast, overlap, per-worker query queues | Results are identical for a fixed scene with any worker count | PHYS-1, SIM-3[^engine-simulation] | 15 |

## Acceptance tests

The Done when column is the acceptance list. Slice 7 also requires a capsule that walks on static terrain far from the origin under keyboard control (with CLI-2[^engine-client]).

## Open items

- The build pin is still Jolt 5.2.0 in single precision with every SIMD extension off (see [current-state.md](../current-state.md)). The target is Jolt 5.6.0 (tag v5.6.0) in double precision with its AVX2 path on the x86-64-v3 baseline. BUILD-1[^doc-build-build-1] sets the CPU baseline and PHYS-1 moves the pin; the archive's SHA-256 is computed when the pin moves. `cmake/Dependencies.cmake` reuses `ThirdParty/JoltPhysics` whenever that folder exists, so the change must also make sure an old source tree cannot be reused silently.
- The Jolt debug renderer is configured off (`DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF`). The PHYS-3 contract session decides how development builds enable it.
- [OQ-5](../system/11-open-questions.md#oq-5) (decided): Physics extracts Jolt debug geometry; the debug pipeline lives in Renderer[^engine-renderer]; DebugGeometry lives in Core RenderData.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Core|Core]] · [[../modules/World|World]]
- **Depended on by:** [[../modules/Simulation|Simulation]] · [[../modules/Combat|Combat]] · [[../modules/Water|Water]] · [[../modules/Cooker|Cooker]]
- **Packages that need it:** [[../modules/Gameplay|Gameplay]] · [[../modules/Renderer|Renderer]] · [[../modules/Editor|Editor]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-water]: [[../modules/Water|Water]] — Engine module · World systems
[^engine-renderer]: [[../modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-cooker]: [[../modules/Cooker|Cooker]] — Engine module · Tools
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
[^engine-world]: [[../modules/World|World]] — Engine module · Simulation core
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-client]: [[../modules/Client|Client]] — Engine module · Presentation
[^doc-build-build-1]: [[../build/BUILD-1|Engine BUILD-1 contract]] — RunicEngine doc
