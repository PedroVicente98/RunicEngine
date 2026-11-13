# Runic::Water

World systems · Headless: yes · Packages: WATER-1, WATER-2, WATER-3, WATER-4, WATER-5, WATER-6, WATER-7 · Status:
empty until WATER-1 (slice 40) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): WaterSystem, bodies, rivers and routes, ocean current fields, cooked fragments, buoyancy, character, object and watercraft movement

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): World, Physics, Simulation, Gameplay, Environment, DistributedSimulation (ship-group handoff)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/WaterSystem/`](Source/WaterSystem/README.md) | The WaterSystem registry, planar water, WaterSample and cell fragments: water reports facts, it never decides an entity's response. | WATER-1 (slice 40) |
| [`Source/Rivers/`](Source/Rivers/README.md) | Spline rivers, flow, connections and routes. | WATER-2 (slice 40) |
| [`Source/Buoyancy/`](Source/Buoyancy/README.md) | BuoyantBody on Jolt bodies, with an optional righting strength. | WATER-3 (slice 41) |
| [`Source/WaterMovement/`](Source/WaterMovement/README.md) | Character and object water movement. | WATER-4 (slice 41) |
| [`Source/OceanCurrents/`](Source/OceanCurrents/README.md) | Ocean current fields from cooked vector grids. | WATER-5 (slice 42) |
| [`Source/Watercraft/`](Source/Watercraft/README.md) | WatercraftMovement: sail force, sail angle, lateral resistance. | WATER-6 (slice 42) |
| [`Source/ShipHandoff/`](Source/ShipHandoff/README.md) | Ship-group handoff: a watercraft with its crew, passengers and cargo as one transfer-snapshot group. | WATER-7 (slice 42b) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. WATER-1 (slice 40) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Water system (Engine)
