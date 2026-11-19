# Runic::Scripting

Scripting · Headless: yes · Packages: SCR-1, SCR-2, SCR-3, SCR-4 · Status: empty until SCR-1 (slice 37a) starts the
module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): The Lua runtime (one sandboxed VM per simulation, memory and instruction budgets), bindings over the Runic API with opaque handles, Lua entries behind the GameplayFlow, Quest and interaction registries, the ScriptBinding component, hot reload, the bytecode format the Cooker writes

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Gameplay, GameplayFlow, Quest, Simulation, Content (Lua private)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types: no Lua type appears in a public header. | every package that adds API |
| [`Source/Runtime/`](Source/Runtime/README.md) | One Lua VM per simulation on the simulation thread, the sandboxed environment (no io, os, load, debug or package), the memory cap per VM and the instruction budget per call, errors isolated per entry and logged with the script, line and entity. | SCR-1 (slice 37a) |
| [`Source/Bindings/`](Source/Bindings/README.md) | Entity, GlobalEntity and WorldPosition as opaque handles; the read-only contexts of GameplayFlow, Quest and interactions; Lua functions registered as actions, conditions and triggers behind the same registries as C++ entries; the ScriptBinding component. | SCR-2 (slice 37a) |
| [`Source/ScriptPipeline/`](Source/ScriptPipeline/README.md) | The bytecode format the Cooker writes for the pinned Lua version, hot reload at the next safe tick keeping the simulation state, the lua-language-server annotation file generated from the bindings. | SCR-3 (slice 37a) |
| [`Source/ScriptViews/`](Source/ScriptViews/README.md) | Components declared from Lua, observers on add, set and remove, low-rate script systems with an interval and a time budget. | SCR-4 (slice 44+) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. SCR-1 (slice 37a) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. The Lua version and the C++ binding library are open (RunicDocs › Open questions; decided in SCR-1's contract session). The `Source/` folders here are one per package, named for what each package builds; the contract session may rename them before code exists.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-19, commit 401fc09). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Engine gameplay APIs (Engine) › Content scripting (Lua)
- RunicDocs › Architecture › Modules, work packages and the implementation contract › SCR-1 to SCR-4
- RunicDocs › Development › Slice 37a · Lua scripting
