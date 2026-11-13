# Runic::Physics

Simulation core · Headless: yes · Packages: PHYS-1, PHYS-2, PHYS-3, PHYS-4, PHYS-5 · Status:
empty until PHYS-1 (slice 7) starts the module

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
| [`Source/Queries/`](Source/Queries/README.md) | Gameplay queries (ray, shape cast, overlap) with per-worker query queues. | PHYS-5 (slice 15) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. PHYS-1 (slice 7) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › Jolt rules, Flecs and Jolt together
- RunicDocs › Architecture › Debug drawing (Engine) — Path A and Path B
