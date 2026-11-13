# Runic::Audio

Presentation · Headless: no (presentation) · Packages: AUD-1, AUD-2 · Status:
empty until AUD-1 (slice 24) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Backend interface (implemented by GodotBridge on the AudioServer), buses and mixing, semantic sound events, MusicDirector, albums, soundtrack presets, local profiles

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, Assets

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/BackendInterface/`](Source/BackendInterface/README.md) | The backend interface that GodotBridge implements on the AudioServer (GDB-12). | AUD-1 (slice 24) |
| [`Source/Buses/`](Source/Buses/README.md) | Buses and mixing. | AUD-1 (slice 24) |
| [`Source/SoundEvents/`](Source/SoundEvents/README.md) | Semantic sound events resolved through a registry to backend calls. | AUD-1 (slice 24) |
| [`Source/MusicDirector/`](Source/MusicDirector/README.md) | The MusicDirector: contexts, fallback chains, albums, presets and radio playback. | AUD-2 (slice 24) |
| [`Source/LocalProfiles/`](Source/LocalProfiles/README.md) | The player's local soundtrack profile. | AUD-2 (slice 24) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. AUD-1 (slice 24) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Audio (Engine, Game)
