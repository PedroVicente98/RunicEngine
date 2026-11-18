# Runic::Physics

Simulation core · Headless: yes · Packages: PHYS-1, PHYS-2, PHYS-3, PHYS-4, PHYS-5 · Status:
PHYS-1 and PHYS-2 implemented (slice 7), PHYS-3 implemented (slice 8), PHYS-4 implemented (slice 10); contract in [ARCHITECTURE.md](ARCHITECTURE.md)

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Jolt v5.6 in double precision behind Runic types: bodies, shapes, CharacterVirtual, queries, collision layers, debug-geometry extraction, the Jolt job system on the Core worker pool

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, World (Jolt private)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/PhysicsWorld/`](Source/PhysicsWorld/README.md) | PhysicsWorld, bodies and shapes on Jolt 5.6 in double precision, behind Runic types. | PHYS-1 (slice 7) |
| [`Source/CollisionLayers/`](Source/CollisionLayers/README.md) | Collision layers and filters. | PHYS-1 (slice 7) |
| [`Source/Characters/`](Source/Characters/README.md) | CharacterVirtual integration and ground movement. | PHYS-2 (slice 7) |
| [`Source/DebugExtraction/`](Source/DebugExtraction/README.md) | Jolt's debug interface implemented here and written into Core DebugGeometry buffers, so Physics and GodotBridge never link each other. | PHYS-3 (slice 8) |
| [`Source/JobSystem/`](Source/JobSystem/README.md) | A Jolt JobSystem adapter on the Core worker pool, so Jolt never owns threads. | PHYS-4 (slice 10) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | exists (PHYS-1 to PHYS-4) |
| [`CMakeLists.txt`](CMakeLists.txt) | One `runic_add_module(...)` call. | exists (PHYS-1) |


### Planned folders

Not created yet: each folder appears with the package that fills it, which takes the folder's README from the `layout` branch (`git checkout origin/layout -- Engine/SimulationCore/Physics/<folder>/README.md`), so the planned layout never moves (decided 2026-11-14).

| Folder | Holds | Packages |
| --- | --- | --- |
| `Source/Queries/` | Gameplay queries (ray, shape cast, overlap) with per-worker query queues. | PHYS-5 (slice 15) |

## How the folders fill

PHYS-1 (slice 7) wrote `ARCHITECTURE.md`, `CMakeLists.txt`, four public headers (`PhysicsHandles.hpp`, `CollisionLayers.hpp`, `PhysicsShapes.hpp`, `PhysicsWorld.hpp`), `Source/PhysicsWorld`, `Source/CollisionLayers` and the tests in `Tests/`; it moved the Jolt pin to 5.6.0 in double precision (`cmake/Dependencies.cmake`). PHYS-2 (slice 7) wrote
`Source/Characters` and the character API in `PhysicsWorld.hpp`. PHYS-3 (slice 8) wrote `Source/DebugExtraction`
and the debug view and inspection API in `PhysicsWorld.hpp`, and turned Jolt's debug renderer on
(`RUNIC_PHYSICS_DEBUG_VIEW`). PHYS-4 (slice 10) wrote `Source/JobSystem`, the worker pool in
`PhysicsWorldSpec` and the sorted contact facts. The later Physics packages create the other folders, each with its README from the `layout` branch, so the planned layout never moves (decided 2026-11-14); a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › Jolt rules, Flecs and Jolt together
- RunicDocs › Architecture › Debug drawing (Engine) — Path A and Path B
