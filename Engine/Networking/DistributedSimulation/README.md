# Runic::DistributedSimulation

Networking · Headless: yes · Packages: DSIM-1, DSIM-2, DSIM-3, DSIM-4, DSIM-5 · Status:
empty until DSIM-1 (slice 36) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Authority state and epochs, ghost state as plain data (Network encodes it), transfer (prepare, final snapshot, commit) with a snapshot-contributor registry that other modules register into, foreign-interaction contract, authority partitions that never cut indivisible volumes

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Simulation, Events, World

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Authority/`](Source/Authority/README.md) | Authority state, epochs and the GlobalEntityId mapping. | DSIM-1 (slice 36) |
| [`Source/Ghosts/`](Source/Ghosts/README.md) | Ghost state near borders as plain data (Network encodes the deltas). | DSIM-2 (slice 36) |
| [`Source/Transfer/`](Source/Transfer/README.md) | Transfer (prepare, final snapshot, authority commit) and the snapshot-contributor registry where modules add their state. | DSIM-3 (slice 36) |
| [`Source/ForeignInteraction/`](Source/ForeignInteraction/README.md) | The foreign-interaction contract. | DSIM-4 (slice 37) |
| [`Source/Partitions/`](Source/Partitions/README.md) | Authority partitions inside one region that never cut an indivisible volume. | DSIM-5 (slice 44+) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. DSIM-1 (slice 36) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Networking replication, interest and handoff (Engine) › Simulations, handoff and ghosts
