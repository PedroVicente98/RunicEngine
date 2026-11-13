# Runic::Navigation

World systems · Headless: yes · Packages: NAV-1, NAV-2 · Status:
empty until NAV-1 (slice 31) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Detour and DetourTileCache runtime, nav tiles streamed by cell, path queries behind the MoveGoal contract, 3D flight paths for flying actors

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): World, Simulation, Assets (Detour private)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Tiles/`](Source/Tiles/README.md) | The Detour and DetourTileCache runtime, nav tiles streamed by cell (Detour stays private). | NAV-1 (slice 31) |
| [`Source/PathQueries/`](Source/PathQueries/README.md) | Path queries behind the MoveGoal contract. | NAV-1 (slice 31) |
| [`Source/FlightPaths/`](Source/FlightPaths/README.md) | 3D paths through free space over the sparse cells for flying actors. | NAV-2 (slice 44+) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. NAV-1 (slice 31) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › AI architecture (Engine, Game)
- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
