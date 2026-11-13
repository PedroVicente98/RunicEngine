# Runic::Assets

Foundation · Headless: yes · Packages: ASSET-1, ASSET-2 · Status:
empty until ASSET-1 (slice 9) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): AssetId, handles, cache, native cooked formats, random-access packages with Zstandard decompression, manifests, client and server package selection, streaming

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, Content

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Handles/`](Source/Handles/README.md) | Asset handles over the AssetId value type from Core. | ASSET-1 (slice 9) |
| [`Source/Cache/`](Source/Cache/README.md) | The asset cache shared by client and server. | ASSET-1 (slice 9) |
| [`Source/CookedFormats/`](Source/CookedFormats/README.md) | Loading the native cooked formats (meshes, collision); the Cooker links each runtime module's writer. | ASSET-1 (slice 9) |
| [`Source/Packages/`](Source/Packages/README.md) | Random-access packages and manifests with Zstandard decompression; client and server package selection. | ASSET-2 (slice 21) |
| [`Source/Streaming/`](Source/Streaming/README.md) | Asynchronous streaming: cells load from a package without stalling a tick or a frame. | ASSET-2 (slice 21) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. ASSET-1 (slice 9) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
- RunicDocs › Architecture › World authoring and cooking (Engine, Game) › Cooked world files and packages
