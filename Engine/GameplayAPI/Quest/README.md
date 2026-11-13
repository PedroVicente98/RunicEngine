# Runic::Quest

GameplayAPI · Headless: yes · Packages: QST-1, QST-2, QST-3 · Status:
empty until QST-1 (slice 30) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Quest runtime and the quest-creation API: definitions, objectives, triggers from facts, instances, durable-fact bridge

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): GameplayFlow, DistributedSimulation (handoff snapshot)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Definitions/`](Source/Definitions/README.md) | The quest-creation API: definitions and objectives. | QST-1 (slice 30) |
| [`Source/Instances/`](Source/Instances/README.md) | Quest instances, objective progress and triggers from gameplay facts. | QST-1 (slice 30) |
| [`Source/DurableBridge/`](Source/DurableBridge/README.md) | Outgoing quest facts with stable IDs through the service port; accepted results applied at safe ticks. | QST-2 (slice 30) |
| [`Source/Handoff/`](Source/Handoff/README.md) | Quest instances registered as a transfer-snapshot contributor. | QST-3 (slice 37) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. QST-1 (slice 30) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Engine gameplay APIs (Engine) › Quest runtime and the quest-creation API
