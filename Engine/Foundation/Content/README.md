# Runic::Content

Foundation · Headless: yes · Packages: CNT-1, CNT-2 · Status:
CNT-1 implemented (slice 9); contract in [ARCHITECTURE.md](ARCHITECTURE.md)

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Namespaced ContentId (engine:, game:), registries, versioned schemas, validation, loading, live reload

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/ContentIds/`](Source/ContentIds/README.md) | ContentId with the `engine:` and `game:` namespaces, parsing and formatting. | CNT-1 (slice 9) |
| [`Source/Registries/`](Source/Registries/README.md) | Registries and lookup; duplicate and unknown IDs reported with their source. | CNT-1 (slice 9) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | exists (CNT-1 contract) |
| [`CMakeLists.txt`](CMakeLists.txt) | One `runic_add_module(...)` call. | exists (CNT-1) |

### Planned folders

Not created yet: each folder appears with the package that fills it, which takes the folder's README from the `layout` branch (`git checkout origin/layout -- Engine/Foundation/Content/<folder>/README.md`), so the planned layout never moves (decided 2026-11-14).

| Folder | Holds | Packages |
| --- | --- | --- |
| `Source/Schemas/` | Versioned schemas and validation for data-driven definitions. | CNT-2 (slice 20) |
| `Source/Loading/` | Loading definitions from the project's data files into registries. | CNT-2 (slice 20) |
| `Source/LiveReload/` | Live-reload hooks: a changed definition reloads without a restart. | CNT-2 (slice 20) |

## How the folders fill

CNT-1 (slice 9) wrote `ARCHITECTURE.md`, `CMakeLists.txt`, two public headers (`ContentId.hpp`, `ContentRegistry.hpp`), `Source/ContentIds`, `Source/Registries` and the tests in `Tests/`. A planned folder appears when the package that fills it starts, with its README from the `layout` branch, so the planned layout never moves (decided 2026-11-14); a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
- RunicDocs › Architecture › Tools (Engine) › Authoring path for every system
