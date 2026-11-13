# Runic::Core

Foundation · Headless: yes · Packages: CORE-1, CORE-2, CORE-3, CORE-4, CORE-5, CORE-6, CORE-7, CORE-8 · Status:
empty until CORE-1 (slice 1) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Plain ID types, time types, math (GLM), world-space types (WorldPosition, WorldDelta), memory, logging, config, worker pool, metrics registry, debug-stream registry; RenderData (plain render structs shared by the Client, GodotBridge and debug tools, including DebugGeometry and the debug-draw API)

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): —

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Public/RenderData/`](Public/RenderData/README.md) | Public RenderData headers (namespace `Runic::RenderData`): plain, header-only structs shared by the Client's extractors, GodotBridge and the debug tools, included as `#include "RenderData/<Name>.hpp"`. | CORE-3 (slice 2), CORE-4 (slice 22), CORE-5 (slice 25a), CORE-6 (slice 42a), CORE-7 (slice 44+) |
| [`Source/Ids/`](Source/Ids/README.md) | The plain ID types every module and process shares: GlobalEntityId, GlobalPlayerId, SimulationId, WorldId (one seamless world: every place reachable without a loading screen), InstanceId (one copy of a world), TickId, AuthorityEpoch, and value types other modules give meaning to (AssetId, WaterBodyId, FieldId, WidgetId). | CORE-1 (slice 1) |
| [`Source/Errors/`](Source/Errors/README.md) | `Result` and asserts: how a Runic function reports failure without exceptions crossing module boundaries. | CORE-1 (slice 1) |
| [`Source/Logging/`](Source/Logging/README.md) | Logging hooks: a sink the host plugs in. | CORE-1 (slice 1) |
| [`Source/Time/`](Source/Time/README.md) | Time types and conversions: integer ticks or double seconds from one monotonic clock, never accumulated floats. | CORE-1 (slice 1) |
| [`Source/Math/`](Source/Math/README.md) | The math vocabulary: GLM types (the approved public exception), and `Public/WorldSpace.hpp` (WorldPosition and WorldDelta in 64-bit double, ToLocal to 32-bit). | CORE-1 (slice 1) |
| [`Source/Memory/`](Source/Memory/README.md) | Memory utilities: fixed-capacity containers for allocation-free paths (ring buffers, small vectors; CORE-1, which APP-1's allocation-free events need) and the per-tick arenas and per-worker buffers the simulation phases write into (CORE-2). | CORE-1 (slice 1), CORE-2 (slice 3) |
| [`Source/Config/`](Source/Config/README.md) | Process configuration: typed settings read once at start from defaults, a configuration file, the environment and the command line, in that order of precedence (worker count, ports, paths, development flags); an unknown key or a bad value is reported with its source. | CORE-8 (slice 3) |
| [`Source/WorkerPool/`](Source/WorkerPool/README.md) | The worker pool every simulation runs its phases on; Jolt's job system is adapted onto it inside Physics (PHYS-4), so no library owns threads. | CORE-2 (slice 3) |
| [`Source/Metrics/`](Source/Metrics/README.md) | The metrics registry: per-tick timings and counters per clock domain. | CORE-2 (slice 3) |
| [`Source/DebugStreams/`](Source/DebugStreams/README.md) | The debug-stream registry where modules publish traces; the Diagnostics layer streams them to the RunicEditor's live attach. | CORE-2 (slice 3) |
| [`Source/RenderData/`](Source/RenderData/README.md) | The code behind `Public/RenderData/` (namespace `Runic::RenderData`): the double-buffered RenderFrame, DebugGeometry and the debug-draw API that turns shapes into lines and triangles, then each render-data kind as the package that first needs it adds it (mesh, skinned, anchors; lights, environment, volumes; water and field grids; effects). | CORE-3 (slice 2), CORE-4 (slice 22), CORE-5 (slice 25a), CORE-6 (slice 42a), CORE-7 (slice 44+) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |

- `Public/RenderData/` is the one public subfolder the architecture defines: RenderData is a separate folder and namespace (`Runic::RenderData`) inside Core, included as `#include "RenderData/RenderFrame.hpp"`. Everything else in `Public/` is flat.
- `Public/WorldSpace.hpp` (WorldPosition, WorldDelta, ToLocal) lives in Core so every module can use 64-bit positions without linking World; CORE-1 adds it (decided 2026-11-12).

## How the folders fill

Nothing here is code yet. CORE-1 (slice 1) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
- RunicDocs › Architecture › World space coordinates, cells and activation (Engine) — WorldSpace.hpp and the type rule
- RunicDocs › Architecture › Render data, extraction and world UI (Engine, Game) › RenderData in Core
- RunicDocs › Architecture › Debug drawing (Engine) › Debug-draw API
- RunicDocs › Architecture › Simulation core clock and threads (Engine) — the worker pool
