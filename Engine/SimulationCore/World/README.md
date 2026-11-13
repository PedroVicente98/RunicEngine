# Runic::World

Simulation core · Headless: yes · Packages: WORLD-1, WORLD-2, WORLD-3 · Status:
empty until WORLD-1 (slice 4) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Region-local origins and conversions over Core’s 64-bit world-space types (real Y, 1 cm minimum unit), the world registry (what each WorldId is), Regions, Sectors, sparse 3D Cells, authored volumes (including indivisible ones), spatial index and exact XYZ queries, activation volumes and streaming state

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, ECS

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Coordinates/`](Source/Coordinates/README.md) | Region-local origins and conversions on top of Core's WorldPosition (64-bit, real Y, 1 cm minimum unit). | WORLD-1 (slice 4) |
| [`Source/Partition/`](Source/Partition/README.md) | The world registry (what each WorldId is: one seamless world with its coordinate space, origin and partition; instances are InstanceIds of a world), Regions, Sectors and sparse 3D Cell keys. | WORLD-1 (slice 4) |
| [`Source/SpatialIndex/`](Source/SpatialIndex/README.md) | The sparse 3D index (XZ columns holding only occupied Y cells), exact XYZ candidate queries (radius, box, ellipsoid) and membership updates. | WORLD-2 (slice 11) |
| [`Source/Volumes/`](Source/Volumes/README.md) | Authored volumes (caves, rule areas, indivisible volumes) with point-in-volume queries; the volume kind picks the backend. | WORLD-2 (slice 11) |
| [`Source/Activation/`](Source/Activation/README.md) | Activation volumes (asymmetric ellipsoids per player), their union, prepare, commit and release hysteresis, and load and unload requests per cell. | WORLD-3 (slice 21) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. WORLD-1 (slice 4) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › World space coordinates, cells and activation (Engine)
