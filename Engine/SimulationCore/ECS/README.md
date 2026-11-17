# Runic::ECS

Simulation core · Headless: yes · Packages: ECS-1, ECS-2 · Status:
ECS-1 and ECS-2 implemented (slice 4); contract in [ARCHITECTURE.md](ARCHITECTURE.md)

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Flecs integration behind the Runic API, conventions, state declarations and their storage categories; the opt-in Runic::ECSNative target

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core; Flecs stays private, reachable only through Runic::ECSNative

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`ECSNative/`](ECSNative/README.md) | Headers of the opt-in `Runic::ECSNative` target: the only door to the native Flecs world. | ECS-1 (slice 4) |
| [`Source/FlecsWorld/`](Source/FlecsWorld/README.md) | The Flecs world wrapper (Flecs 4.2). | ECS-1 (slice 4) |
| [`Source/Registration/`](Source/Registration/README.md) | Component, system and query registration through Runic types, and the conventions every module follows. | ECS-1 (slice 4) |
| [`Source/States/`](Source/States/README.md) | State declarations: one declaration per state, whose category picks its Flecs storage (hot flag, fact with a target, content flag, value, derived), with owner and observers. | ECS-2 (slice 4) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | exists (ECS-1 contract) |
| [`CMakeLists.txt`](CMakeLists.txt) | One `runic_add_module(...)` call, and `runic_add_native_target(...)` for `Runic::ECSNative`. | exists (ECS-1) |



## How the folders fill

ECS-1 (slice 4) wrote `ARCHITECTURE.md`, `CMakeLists.txt`, four public headers (`Entity.hpp`, `ComponentInfo.hpp`, `EntityQuery.hpp`, `EntityWorld.hpp`), `ECSNative/ECSNative.hpp`, `Source/FlecsWorld`, `Source/Registration` and the tests in `Tests/`. ECS-2 (slice 4) added `EntityStates.hpp`, `Source/States`, `Source/FlecsWorld/WorldAccess.hpp`, its tests and the storage benchmark (`Tests/StateBenchmark.cpp`). A package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › The Runic API over Flecs and Jolt, State declarations, Flecs rules
- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
