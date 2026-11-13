# Runic::Environment

World systems · Headless: yes · Packages: ENV-1, ENV-2, ENV-3, ENV-4, ENV-5, ENV-6 · Status:
empty until ENV-1 (slice 25) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Environment clock, wind and current routines (cyclic baseline, direction and signed speed), current modulation, tides, gameplay waves, weather state and time-boxed overrides, world routines (route positions from world time)

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, World, Simulation

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Clock/`](Source/Clock/README.md) | The environment clock: world time, day and night phase, sun direction and the skylight input Perception reads. | ENV-1 (slice 25) |
| [`Source/Wind/`](Source/Wind/README.md) | The deterministic wind field with a cyclic baseline routine: direction at any angle and a signed speed per region. | ENV-2 (slice 42) |
| [`Source/Currents/`](Source/Currents/README.md) | Current modulation over time: baseline routine, tide phase, season, weather. | ENV-3 (slice 42) |
| [`Source/Tides/`](Source/Tides/README.md) | Tides as a function of world time. | ENV-4 (slice 43) |
| [`Source/Waves/`](Source/Waves/README.md) | Gameplay waves (the server's macro surface; Godot adds the visual detail). | ENV-4 (slice 43) |
| [`Source/Weather/`](Source/Weather/README.md) | Weather state and time-boxed overrides for weather, wind and currents. | ENV-5 (slice 43) |
| [`Source/Routines/`](Source/Routines/README.md) | World routines: cooked routes evaluated as position = f(route, world time, seed), with slow deterministic drift. | ENV-6 (slice 43a) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. ENV-1 (slice 25) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Environment wind, currents, tides, waves and weather (Engine)
