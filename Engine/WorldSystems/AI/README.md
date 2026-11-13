# Runic::AI

World systems · Headless: yes · Packages: AI-1, AI-2, AI-3, AI-4, AI-5, AI-6 · Status:
empty until AI-1 (slice 32) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): BehaviorTree.CPP adapter, working memory, AI scheduling (wake, sleep, leash), intents to commands, reactive combat

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Gameplay, Combat, Perception, Navigation (BehaviorTree.CPP private)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Runtime/`](Source/Runtime/README.md) | BehaviorTreeRuntime, the brain pool, working memory, wake and sleep (the folder named in RunicDocs › AI architecture › Repository layout). | AI-1 (slice 32), AI-4 (slice 34) |
| [`Source/BehaviorTree/`](Source/BehaviorTree/README.md) | The BehaviorTree.CPP adapter: conditions, actions and utility decisions. | AI-1 (slice 32), AI-4 (slice 34) |
| [`Source/Systems/`](Source/Systems/README.md) | The AI systems registered into the simulation: aggro, perception, think, target and sleep (perception lifecycle: warn and detect wake, target validation, leash, return home). | AI-1 (slice 32), AI-3 (slice 33), AI-4 (slice 34) |
| [`Source/Commands/`](Source/Commands/README.md) | The command bridge: intents become ordinary movement and combat commands, validated like a player's. | AI-2 (slice 32) |
| [`Source/TreeAssets/`](Source/TreeAssets/README.md) | Behavior-tree assets: tree loading, the AIDefinition registry, personality and perception profiles. | AI-2 (slice 32) |
| [`Source/Debugging/`](Source/Debugging/README.md) | Instrumentation: per-brain execution trace, memory, utility and intent diagnostics. | AI-5 (slice 34) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |

- AI-6 (scale tests) lives in `Tests/`, not in a subsystem folder.
- Packages with no folder of their own: AI-6.

## How the folders fill

Nothing here is code yet. AI-1 (slice 32) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › AI architecture (Engine, Game) › Repository layout, Behavior-tree runtime, Intents become ordinary commands, Runtime AI debugging
