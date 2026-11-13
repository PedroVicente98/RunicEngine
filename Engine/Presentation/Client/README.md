# Runic::Client

Presentation · Headless: no (presentation) · Packages: CLI-1, CLI-2, CLI-3, CLI-4, CLI-5, CLI-6, CLI-7, CLI-8, CLI-9 · Status:
empty until CLI-1 (slice 5) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Client runtime: the Prediction and Presentation layers, the extractor registry and Engine extractors, camera, interpolation, input mapping and command builder; the RunicBot runtime

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Application, Simulation, Network, Perception, Environment, Water, Animation, Audio, Assets

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/PresentationLayer/`](Source/PresentationLayer/README.md) | The Presentation layer: interpolation, camera and render extraction into the RenderFrame each frame. | CLI-1 (slice 5) |
| [`Source/Extraction/`](Source/Extraction/README.md) | The extractor registry and the Engine extractors: entities as debug shapes first, then mesh instances, skinned instances, world anchors, lights, environment and volumes, water surfaces and field grids. | CLI-1 (slice 5), CLI-4 (slice 22), CLI-5 (slice 23), CLI-6 (slice 23a), CLI-7 (slice 25a), CLI-8 (slice 42a) |
| [`Source/Camera/`](Source/Camera/README.md) | The client camera. | CLI-1 (slice 5) |
| [`Source/InputMapping/`](Source/InputMapping/README.md) | Input mapping, actions and the command builder over the AppEvents the GodotLayer forwards (modifiers, chords, press and release, layouts and presets the Game defines). | CLI-2 (slice 7) |
| [`Source/Interpolation/`](Source/Interpolation/README.md) | Interpolation and camera-relative conversion for drawn instances. | CLI-4 (slice 22) |
| [`Source/PredictionLayer/`](Source/PredictionLayer/README.md) | The Prediction layer: drives the prediction subset and reconciliation each frame. | CLI-3 (slice 14) |
| [`Source/BotRuntime/`](Source/BotRuntime/README.md) | The RunicBot runtime: the client without presentation, many bot sessions per process, a simple brain. | CLI-9 (slice 38) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. CLI-1 (slice 5) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Client runtime (Game on Engine)
- RunicDocs › Architecture › Render data, extraction and world UI (Engine, Game)
