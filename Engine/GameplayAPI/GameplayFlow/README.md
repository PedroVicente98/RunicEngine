# Runic::GameplayFlow

GameplayAPI · Headless: yes · Packages: FLOW-1, FLOW-2, FLOW-3 · Status:
empty until FLOW-1 (slice 29) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Sequences with trigger, condition and action registries, timers on simulation time, presentation cues, source-owned cleanup, handoff policies

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Gameplay, DistributedSimulation (handoff snapshot)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Sequences/`](Source/Sequences/README.md) | The sequence runtime and its scheduler. | FLOW-1 (slice 29) |
| [`Source/Registries/`](Source/Registries/README.md) | Trigger, condition and action registries: the vocabulary sequences are written in. | FLOW-1 (slice 29) |
| [`Source/Timers/`](Source/Timers/README.md) | Timers on simulation time. | FLOW-1 (slice 29) |
| [`Source/Termination/`](Source/Termination/README.md) | The termination path, restriction ownership and the watchdog: every exit path releases what the sequence acquired. | FLOW-2 (slice 29) |
| [`Source/Cues/`](Source/Cues/README.md) | Presentation cues sent to clients. | FLOW-2 (slice 29) |
| [`Source/Handoff/`](Source/Handoff/README.md) | Handoff policies and sequence state in the transfer snapshot. | FLOW-3 (slice 37) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. FLOW-1 (slice 29) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Engine gameplay APIs (Engine) › Scripted sequences (GameplayFlow) and restrictions
