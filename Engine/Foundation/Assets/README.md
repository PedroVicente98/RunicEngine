# Runic::Assets

Foundation · Headless: yes · Packages: ASSET-1, ASSET-2 · Status:
ASSET-1 implemented (slice 9); contract in [ARCHITECTURE.md](ARCHITECTURE.md)

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): AssetId, handles, cache, native cooked formats, random-access packages with Zstandard decompression, manifests, client and server package selection, streaming

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, Content

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Handles/`](Source/Handles/README.md) | Asset handles over the AssetId value type from Core. | ASSET-1 (slice 9) |
| [`Source/Cache/`](Source/Cache/README.md) | The asset cache shared by client and server. | ASSET-1 (slice 9) |
| [`Source/CookedFormats/`](Source/CookedFormats/README.md) | Loading the native cooked formats (meshes, collision); the Cooker links each runtime module's writer. | ASSET-1 (slice 9) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | exists (ASSET-1 contract) |
| [`CMakeLists.txt`](CMakeLists.txt) | One `runic_add_module(...)` call. | exists (ASSET-1) |

### Planned folders

Not created yet: each folder appears with the package that fills it, which takes the folder's README from the `layout` branch (`git checkout origin/layout -- Engine/Foundation/Assets/<folder>/README.md`), so the planned layout never moves (decided 2026-11-14).

| Folder | Holds | Packages |
| --- | --- | --- |
| `Source/Packages/` | Random-access packages and manifests with Zstandard decompression; client and server package selection. | ASSET-2 (slice 21) |
| `Source/Streaming/` | Asynchronous streaming: cells load from a package without stalling a tick or a frame. | ASSET-2 (slice 21) |

## How the folders fill

ASSET-1 (slice 9) wrote `ARCHITECTURE.md`, `CMakeLists.txt`, four public headers (`AssetHandle.hpp`, `CookedFormats.hpp`, `AssetCatalog.hpp`, `AssetCache.hpp`), `Source/Handles`, `Source/Cache`, `Source/CookedFormats` and the tests in `Tests/` (with the golden file in `Tests/Data`). A planned folder appears when the package that fills it starts, with its README from the `layout` branch, so the planned layout never moves (decided 2026-11-14); a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
- RunicDocs › Architecture › World authoring and cooking (Engine, Game) › Cooked world files and packages
