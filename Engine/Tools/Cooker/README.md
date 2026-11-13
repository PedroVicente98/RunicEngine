# Runic::Cooker

Tools · Headless: tool (never shipped to players) · Packages: COOK-1, COOK-2, COOK-3, COOK-4, COOK-5, COOK-6, COOK-7, COOK-8 · Status:
empty until COOK-1 (slice 9) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Cooking orchestration and Runic format writers: Jolt cooking, Recast, meshoptimizer for collision, Zstandard; reads geometry from the scenes Godot imported; runs inside the RunicEditor and headless on build machines

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, Content, Assets, World, Physics, Navigation, Water (each runtime module owns its cooked format; the Cooker links its writer)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Orchestration/`](Source/Orchestration/README.md) | Cook jobs: what to cook, progress, incremental cooking of only what changed. | COOK-1 (slice 9), COOK-8 (slice 44+) |
| [`Source/Inputs/`](Source/Inputs/README.md) | What the Cooker reads: geometry and placements from the scenes Godot imported, handed over as plain data by GodotTools' export plugin (the Cooker links no Godot and has no second glTF importer). | COOK-1 (slice 9) |
| [`Source/Collision/`](Source/Collision/README.md) | Jolt collision cooking, meshoptimizer for collision meshes. | COOK-1 (slice 9) |
| [`Source/Validation/`](Source/Validation/README.md) | Validation of data-driven definitions: invalid definitions fail the cook with clear errors. | COOK-2 (slice 20) |
| [`Source/WorldCooking/`](Source/WorldCooking/README.md) | World cooking from scenes: regions, sectors, sparse 3D cells, authored volumes, spawn areas, client cell visual lists. | COOK-3 (slice 21) |
| [`Source/Packages/`](Source/Packages/README.md) | Random-access packages and manifests with Zstandard, in the format Runic::Assets reads. | COOK-3 (slice 21) |
| [`Source/ClientPackages/`](Source/ClientPackages/README.md) | The client cook packages written next to the .pck from Godot's export. | COOK-4 (slice 22) |
| [`Source/SkinnedMeshes/`](Source/SkinnedMeshes/README.md) | Skinned meshes and animation clips. | COOK-5 (slice 23) |
| [`Source/NavTiles/`](Source/NavTiles/README.md) | Navigation tiles with Recast; spawn-area candidate points checked against the navmesh. | COOK-6 (slice 31) |
| [`Source/WaterFragments/`](Source/WaterFragments/README.md) | Water fragments and current fields. | COOK-7 (slice 40) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. COOK-1 (slice 9) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › World authoring and cooking (Engine, Game) › Cooker, Cooked world files and packages
- RunicDocs › Architecture › Tools (Engine)
