# Runic::GodotTools

Tools · Headless: never shipped · Packages: GDT-1, GDT-2, GDT-3, GDT-4, GDT-5, GDT-6, GDT-7, GDT-8, GDT-9, GDT-10, GDT-11, GDT-12, GDT-13, GDT-14, GDT-15, GDT-16 · Status:
has code; contract in [ARCHITECTURE.md](ARCHITECTURE.md)

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): RunicEditor plugins: marker nodes, schema inspectors and Runic file formats in the editor, world and region tools, docks (world, ECS, physics, resolver, AI editor on GraphEdit, live attach), the project template and assistants, Run orchestration, the export plugin that calls the Cooker

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Most modules, Cooker (libgodot editor build and godot-cpp private)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Spike/`](Source/Spike/README.md) | The GDT-1 test plugin (slice 0): proves the RunicEditor loads Runic tool code compiled into it. | GDT-1 (slice 0) |
| [`Source/Plugin/`](Source/Plugin/README.md) | The real Runic editor plugin: what registers the Runic tools at the editor level and the editor's Runic menus. | GDT-2 (slice 6) |
| [`Source/Project/`](Source/Project/README.md) | The project model: runic.project.json, New and Open (per the open decision), and loading and reloading the project's editor library. | GDT-2 (slice 6) |
| [`Source/IDEBridge/`](Source/IDEBridge/README.md) | The IDE bridge: compile_commands.json, go.work, VS Code tasks, opening a build error at its line. | GDT-2 (slice 6) |
| [`Source/Docks/`](Source/Docks/README.md) | Docks: world, ECS and physics inspectors over a local simulation in the editor process, and the resolver debugger. | GDT-2 (slice 6), GDT-3 (slice 8), GDT-5 (slice 17) |
| [`Source/Export/`](Source/Export/README.md) | Cooker integration: the export plugin that calls the Cooker, collision presets in Godot's import settings, the collision naming convention for glTF. | GDT-4 (slice 9) |
| [`Source/RunicData/`](Source/RunicData/README.md) | Runic data in the editor: ResourceFormatLoader and ResourceFormatSaver for Runic files, inspectors and validation generated from schemas, live reload into a running server, background cooking with progress. | GDT-6 (slice 20) |
| [`Source/MarkerNodes/`](Source/MarkerNodes/README.md) | Marker nodes (RunicVolume3D, RunicSpawnArea3D, RunicInteractable3D, RunicRoute3D, RunicCollider3D) and spawn-area previews; they exist only in the editor, the Cooker writes placements. | GDT-4 (slice 9), GDT-7 (slice 21) |
| [`Source/RegionTools/`](Source/RegionTools/README.md) | Region tools: World.tscn with RunicRegion3D proxies, New region from layout, read-only neighbor proxies, 64-bit region origins. | GDT-8 (slice 21) |
| [`Source/Run/`](Source/Run/README.md) | Run orchestration: compile what changed, cook, start the local server and development clients with a development ticket, development checkpoint on stop. | GDT-9 (slice 13) |
| [`Source/Assistants/`](Source/Assistants/README.md) | Assistants: New Flecs system, New behavior, New quest condition or action, New AI node, New data type, New marker node, New Go service. | GDT-10 (slice 20) |
| [`Source/AI/`](Source/AI/README.md) | The AI editor on GraphEdit and the AI tools (the folder named in RunicDocs › AI architecture › Repository layout). | GDT-11 (slice 35), GDT-12 (slice 35) |
| [`Source/LiveAttach/`](Source/LiveAttach/README.md) | Live-attach docks to development and staging servers. | GDT-13 (slice 39) |
| [`Source/GDScriptLocks/`](Source/GDScriptLocks/README.md) | GDScript for editor tools and its locks: the export lock, the development-client check, Run-scene interception, script templates, the problems dock. | GDT-14 (slice 21) |
| [`Source/WaterAuthoring/`](Source/WaterAuthoring/README.md) | Water and field authoring: RunicWaterBody3D on Path3D, flow profiles, current and wind map painting. | GDT-15 (slice 40) |
| [`Source/PluginCompatibility/`](Source/PluginCompatibility/README.md) | Plugin compatibility: the published extension API per build, the install validator, runic.plugin.json, Cooker adapters. | GDT-16 (slice 44+) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | exists (slice 0) |
| [`CMakeLists.txt`](CMakeLists.txt) | One `runic_add_module(...)` call. | exists (slice 0) |

- Editor UI that fits the editor (RunicDocs › Tools (Engine), decided 2026-11-13): Runic tools add UI only through the editor's extension points (docks, the bottom panel, a main-screen tab, inspector plugins, Project › Tools and context menus) and take colors, fonts, icons and sizes from the editor theme and the editor scale. They never insert controls into a container whose layout they do not own; the spike's first toolbar button broke the editor's top bar.

## How the folders fill

This module has code. Its contract is [ARCHITECTURE.md](ARCHITECTURE.md); a folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Tools (Engine)
- RunicDocs › Architecture › Godot integration (Engine)
- RunicDocs › Architecture › World authoring and cooking (Engine, Game)
- RunicDocs › Architecture › AI architecture (Engine, Game) › AI editor, Repository layout
