# Runic::Perception

World systems · Headless: yes · Packages: PRC-1, PRC-2, PRC-3 · Status:
empty until PRC-1 (slice 25) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Light emitters, ambient and skylight, luminosity queries, concealment, warn and detect radii, visibility tiers chosen by a Game function

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Gameplay, World, Environment (time of day, weather), Water (underwater light)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Light/`](Source/Light/README.md) | Light emitters, ambient profiles, the skylight curve driven by the Environment clock, and luminosity at a point. | PRC-1 (slice 25) |
| [`Source/Visibility/`](Source/Visibility/README.md) | Visibility tiers as data, picked per pair by a Game-registered function, with a reveal hook. | PRC-2 (slice 25) |
| [`Source/Detection/`](Source/Detection/README.md) | Perception profiles: warn and detect radii from light and concealment, with a minimum. | PRC-3 (slice 33) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. PRC-1 (slice 25) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Engine gameplay APIs (Engine) › Light and perception
- RunicDocs › Architecture › Networking replication, interest and handoff (Engine) › Interest and visibility tiers (decided)
