# Runic::Gameplay

GameplayAPI · Headless: yes · Packages: GPL-1, GPL-2, GPL-3, GPL-4, GPL-5, GPL-6, GPL-7, GPL-8 · Status:
empty until GPL-1 (slice 14) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): The attribute API and modifiers, resource pools, tags, the lifecycle API (Kill, death and kill facts), the relation hook, the movement-mode registry and modes, restrictions, interaction framework, item and equipment runtime, spawn and routine spawning

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Simulation, Environment (wind for gliding)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Movement/`](Source/Movement/README.md) | The movement-mode registry and modes (walk, sprint, dash hook, glide, mount; Water registers swimming) with parameters from data; later flight and climbing as separate modes. | GPL-1 (slice 14), GPL-7 (slice 44+) |
| [`Source/Attributes/`](Source/Attributes/README.md) | The attribute API: attributes the Game declares (ID, type, default, limits, replication, prediction), modifier stacks with a replaceable aggregation. | GPL-2 (slice 15) |
| [`Source/Pools/`](Source/Pools/README.md) | Resource pools with a regeneration scheduler that runs Game rules. | GPL-2 (slice 15) |
| [`Source/Tags/`](Source/Tags/README.md) | Gameplay tags. | GPL-2 (slice 15) |
| [`Source/Lifecycle/`](Source/Lifecycle/README.md) | The lifecycle API: `Kill(entity, source)` emits the death and kill facts; the Engine never calls it on its own. | GPL-2 (slice 15) |
| [`Source/Relations/`](Source/Relations/README.md) | The relation hook: the function the Engine asks (hostile, friendly or neutral), registered by the Game. | GPL-2 (slice 15) |
| [`Source/Restrictions/`](Source/Restrictions/README.md) | Source-owned restriction sets, acquired and released by source, with command-validation hooks. | GPL-3 (slice 18) |
| [`Source/Items/`](Source/Items/README.md) | The item and equipment runtime that mirrors the Platform's ledger containers. | GPL-4 (slice 28) |
| [`Source/Interaction/`](Source/Interaction/README.md) | The interaction framework: interactables, reservation and interaction facts. | GPL-5 (slice 29) |
| [`Source/Spawning/`](Source/Spawning/README.md) | Spawn primitives, spawn areas driven by rule data over cooked candidate points, and routine spawning when a cell activates. | GPL-6 (slice 31), GPL-8 (slice 43a) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. GPL-1 (slice 14) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Engine gameplay APIs (Engine) › Mechanism and policy
- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
