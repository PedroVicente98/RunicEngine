# Runic::GodotTools — module contract

Group: Tools · Headless: **never shipped** · Target: `Runic::GodotTools` (`runic_godottools`) · Packages: GDT-1 to GDT-16

Source: RunicDocs › Architecture › Godot integration (Engine) (Projects: create, open and the game's editor library;
Game code and the Run button; Open decisions), Tools (Engine), Repositories, build targets and processes (Engine
modules), World authoring and cooking. Status: **GDT-1 (spike) done** in slice 0; its findings are in RunicDocs ›
Architecture › Godot integration (Engine) › Slice 0 spike results (GDT-1). **GDT-2 implemented** (slice 6, on the
preview branch `preview/slices-3-11`): the project model and the runic-mmo template, New and Open, the project's
editor library loaded and reloaded, the IDE bridge, the Runic editor plugin and the world dock, with their acceptance
tests; slice 6's brief and walkthrough are [preview/slice-06-editor-shell.md](../../../preview/slice-06-editor-shell.md).
The folder map is [README.md](README.md).

## Purpose

The Runic tools inside the RunicEditor (the Godot editor run through LibGodot): marker nodes, schema inspectors and
Runic file formats in the editor, world and region tools, docks (world, ECS, physics, resolver, AI editor on
GraphEdit, live attach), the project template and assistants, Run orchestration, and the export plugin that calls
the Cooker. Editor code never ships to players. GDT-2 makes the editor a Runic editor: it knows a Runic project by its
manifest, makes one from a template, loads and reloads the game's editor library, writes what the IDE needs, and
shows the first dock over a local simulation in the editor process.

## Public API

Namespace `Runic::GodotTools`. Headers are included by their path under `Public/`.

| Header | Holds | Since |
| --- | --- | --- |
| `GodotToolsModule.hpp` | `RegisterEditorTools(GodotRuntime&, const EditorToolsSpec&)`: registers the plugin and the dock through the runtime's initialization hooks; `EditorToolsSpec` (the local world, the template folder, the Engine version, the IDE files' spec); `kPluginLoadedLine`, `kPluginUnloadedLine` | GDT-1, GDT-2 |
| `RunicProject.hpp` | `ProjectManifest` (format, Engine version, game name, `ProjectPaths`, `ProjectExecutables`, `CookSettings`), `ParseProjectManifest`, `WriteProjectManifest`, `RunicProject`, `IsRunicProject`, `OpenProject`, `NewProjectSpec`, `InitializeProject` (New); the names `kManifestFileName`, `kGodotProjectFileName`, `kGameEditorExtensionFileName`, `kGodotCacheFolderName`, `kDefaultTemplateName`; `kManifestFormat` (1) | GDT-2 |
| `IdeBridge.hpp` | `IdeFilesSpec` (the CMake preset and build folder), `IdeFilesReport`, `WriteIdeFiles`; the file names and `kGeneratedMarkerKey`, `kDefaultGoVersion` | GDT-2 |
| `WorldInspector.hpp` | `WorldInspector` (`Summary`, `Entities`, `Inspect`, `Queries`, `CommandQueues`, `States`), `WorldSummary`, `InspectedEntity`, `InspectedComponent`, `kDefaultEntityListLimit` | GDT-2 |

Approved exception: godot-cpp types may appear in this module's public headers, so a project's editor library can
extend marker nodes and docks (the fourth public-header exception). GDT-2 links godot-cpp PUBLIC for that reason: a
project's editor library (RunicGame::Editor's row allows only `Runic::GodotTools`) reaches godot-cpp through this
target. GDT-2's own public headers do not name a godot-cpp type yet.

**The manifest rule:** `runic.project.json` is strict JSON in the shape RunicDocs shows. `runic` (the format, 1),
`engine` and `game.name` are required; `paths`, `executables` and `cook` are optional and take RunicDocs' values when
absent. A member the model does not know is a problem, not ignored, and every problem names its member
(`paths.content: must be a non-empty string`), all at once.

**The New rule** (open decision 2: a plugin, the recommendation): New makes an empty Godot project a Runic project. The
folder may hold the project manager's `project.godot` (kept: the editor has it open) and `.godot/`, nothing else; the
template's files are copied in with `{{GAME_NAME}}` filled, and the manifest is written. A folder with anything else
is refused untouched.

## Owns

- The project model: the manifest's format, Open and New, and the runic-mmo template (`Source/Project/Template/`,
  staged by the build in `<build>/templates/runic-mmo`, where a distribution keeps it).
- Loading and reloading the project's editor library: the watcher reads `game_editor.gdextension`, finds the library
  for this editor's platform by its feature tags, loads it if Godot has not, and reloads it through
  `GDExtensionManager::reload_extension` when a build replaces the file (seen changed, then unchanged half a second
  later, so a library still being written is never loaded).
- The IDE bridge's files: `compile_commands.json` (a link to the build's), `go.work` (every folder under
  `paths.services` with a `go.mod`, at the newest `go` version they declare), `.vscode/tasks.json` (configure, build,
  test with the CMake preset; Go tests when there are services) and `.vscode/launch.json` (debug the manifest's server
  and client). The bridge rewrites only files that carry its marker and a link it made.
- The Runic editor plugin (`RunicEditorPlugin`, always on, no `plugin.cfg`): it reports itself and whether GDScript
  exists, opens the manifest, offers Project › Tools › **Runic: make this a Runic project** when there is none,
  **Runic: generate IDE files**, **Runic: reload the game editor library**, polls the library twice a second, and
  shows the world dock. It adds nothing to the editor's toolbar.
- The world dock (`RunicWorldDock`, "Runic World", right upper dock) and its model, `WorldInspector`: the local
  simulation's ID, tick and counts, its entities (a page of 500, in handle order), the selected entity's components
  with their formatters' text, the queries and the command queues; refreshed four times a second.

## Does not own

- The RunicEditor executable (`Engine/Programs/RunicEditor/`) and its local simulation: the program makes the
  `SimulationRuntime`, fills it and hands it over in `EditorToolsSpec`; the LibGodot instance is Runic::GodotBridge's.
- Building a project: the project's own CMake builds its editor library (the template's `CMakeLists.txt` shows how);
  the editor only loads what is built. Run orchestration (compile, cook, start) is GDT-9 (slice 13).
- The game's marker nodes and docks: the project's editor library (RunicGame::Editor, GAME-1).
- How states are listed: Runic::ECS offers no way to enumerate declared states by index, so the dock shows none
  (reported; see the slice brief).
- Building libgodot (BUILD-1). Cooking (Runic::Cooker).

## Allowed dependencies

Catalog row (ceiling): any module and Runic::Cooker; third parties libgodot (editor build) and godot-cpp (public by
the approved exception). GDT-2 links Runic::GodotBridge (public: the plugin is registered on its runtime),
Runic::Simulation (public: `WorldInspector` and `EditorToolsSpec` name the `SimulationRuntime`) and godot-cpp
(public). The JSON reader is the module's own (`Source/Project/Json.cpp`): the row allows no JSON library, and the
model must work without Godot running.

## Forbidden dependencies

Any shipped executable linking this module (GameClient → GodotTools is forbidden); domain modules depending on it
(the AI docks depend on AI contracts, never the reverse); Platform and Game targets.

## Threading and tick model

Godot's main thread only: the plugin and the dock run inside the editor's frames (`GodotRuntime::Iterate`). The dock
reads the local simulation between its steps: the Simulation layer below the GodotLayer runs the steps in the same
thread, earlier in the frame. The project model and the IDE bridge are plain functions with file I/O, called on that
thread from the plugin's menus.

## Data ownership

Godot owns the plugin node; its dialogs are its children. The dock is added to the editor's docks by the plugin and
freed when the plugin leaves. `RegisterEditorTools` keeps a copy of its spec for the classes Godot makes later (one
editor per process). The local simulation belongs to the program and outlives Godot.

## Integration points

- `RegisterEditorTools(runtime, spec)` adds hooks to the `GodotRuntime` before `Create()`; the classes register at
  `GodotInitLevel::Editor`.
- A project declares its editor library in `game_editor.gdextension` with `reloadable = true` and the entry symbol
  `game_editor_init`; it builds the library against godot-cpp (through `Runic::GodotTools`).
- The RunicEditor stages the template next to its executables (`<build>/templates`) and names it in the spec;
  `--runic-new-project <folder> <name>` calls `InitializeProject` without Godot.

## Where the code lives

- `Source/Project/`: `Json.hpp`/`Json.cpp`, `RunicProject.cpp`, `GameLibrary.hpp`/`GameLibrary.cpp`,
  `Template/runic-mmo/`.
- `Source/IDEBridge/IdeBridge.cpp`; `Source/Docks/WorldInspector.cpp`, `RunicWorldDock.hpp`/`.cpp`;
  `Source/Plugin/RunicEditorPlugin.hpp`/`.cpp`, `EditorContext.hpp`; `Source/GodotToolsModule.cpp`.
- `tests/godot/fixtures/game_editor/GameEditorFixture.cpp`: the fixture editor library, built twice (build 1 and 2).
- Formats the CTests rely on: `RUNIC: Runic editor plugin loaded`, `RUNIC: project <name> opened (Runic format <n>,
  made with Engine <v>)`, `RUNIC: <folder> is a Godot project without runic.project.json; ...`, `RUNIC: game editor
  library <res path> (loaded|not loaded yet)`, `RUNIC: game editor library reloaded (<n> so far)`, `RUNIC: world dock
  shows <n> entities of simulation <id> (...)`.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| GDT-1 | Spike: the RunicEditor executable on libgodot (editor target) opens the Godot editor with a test plugin; the editor target builds and opens without GDScript | The editor opens, loads the plugin and exits cleanly in CI; the spike's findings decide the fallback | Done in slice 0; its test plugin replaced by GDT-2's plugin |
| GDT-2 | Project model: runic.project.json, the runic-mmo template, New and Open, loading and reloading the project's editor library; IDE bridge; first dock with world and ECS inspectors over a local simulation in the editor process | The editor opens a project and inspects entities of a local world; a rebuilt editor library reloads without restarting | **Implemented** (slice 6, preview branch): `RunicGodotToolsTests` passes with the 19 tests below; the CTests `godot.editor_spike`, `godot.editor_new_project`, `godot.editor_project_library`, `godot.editor_opens_project`; awaiting the owner's review |
| GDT-3 to GDT-16 | Physics inspector, Cooker integration, docks, schema inspectors, region tools, Run, AI editor, live attach, water authoring, GDScript locks | See RunicDocs | Not started |

GDT-2's acceptance tests (`RunicGodotToolsTests`): `ProjectManifest.*` (RunicDocs' manifest read; written back the
same; defaults; every problem named, all at once; broken JSON with its line and column; escapes and Unicode),
`NewProject.*` (the template's layout with the game's name, `.gdignore` where code and builds are; the project
manager's `project.godot` kept; a busy folder refused; bad names refused), `OpenProject.*`, `IdeBridge.*` (go.work over
the services with the newest Go version, tasks and launch configurations; clangd's link; files the bridge did not
write left alone), `WorldInspector.*` (the summary; an entity's components with their values; a stable page of
entities), and with Godot headless in the test process `GameEditorLibrary.*` (**the editor loads the project's editor
library**; **a rebuilt library reloads without restarting**: build 1, replaced by build 2, is seen as build 2 through
`ClassDB`). The CTest `godot.editor_opens_project` checks **the editor opens a project and inspects entities of a
local world** (the dock's line with 13 entities).

Decisions taken in GDT-2 that await the owner (also in the slice brief): New is a plugin menu (open decision 2's
recommendation) plus a command-line `--runic-new-project`; the template lives in `Source/Project/Template` and is staged
in `<build>/templates`; godot-cpp is PUBLIC; the JSON reader is the module's own; unknown manifest members are errors;
the watcher loads the library itself when Godot has not and reloads on a settled change; the spike plugin and its
folder are gone.

## Future extensions

- Opening a build error at its line (with Run, GDT-9), and the assistants (GDT-10) that write files and register them.
- Listing states in the dock, once Runic::ECS can enumerate them.
- A patch of the project manager for New, if the plugin's flow is not enough (open decision 2).
- Every Runic tool's UI follows the editor: only its extension points (docks, the bottom panel, a main-screen tab,
  inspector plugins, Project › Tools and context menus), the editor theme and scale, never controls inserted into a
  container whose layout it does not own (RunicDocs › Tools (Engine)). A branding patch and the editor feature profile
  are tool-phase decisions.
