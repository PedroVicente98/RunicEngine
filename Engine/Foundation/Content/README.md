# Runic::Content

Foundation · Headless: yes · Packages: CNT-1, CNT-2 · Status:
empty until CNT-1 (slice 9) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Namespaced ContentId (engine:, game:), registries, versioned schemas, validation, loading, live reload

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/ContentIds/`](Source/ContentIds/README.md) | ContentId with the `engine:` and `game:` namespaces, parsing and formatting. | CNT-1 (slice 9) |
| [`Source/Registries/`](Source/Registries/README.md) | Registries and lookup; duplicate and unknown IDs reported with their source. | CNT-1 (slice 9) |
| [`Source/Schemas/`](Source/Schemas/README.md) | Versioned schemas and validation for data-driven definitions. | CNT-2 (slice 20) |
| [`Source/Loading/`](Source/Loading/README.md) | Loading definitions from the project's data files into registries. | CNT-2 (slice 20) |
| [`Source/LiveReload/`](Source/LiveReload/README.md) | Live-reload hooks: a changed definition reloads without a restart. | CNT-2 (slice 20) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. CNT-1 (slice 9) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
- RunicDocs › Architecture › Tools (Engine) › Authoring path for every system
