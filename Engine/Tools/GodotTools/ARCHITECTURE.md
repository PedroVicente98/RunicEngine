# Runic::GodotTools — module contract

Group: Tools · Headless: **never shipped** · Target: `Runic::GodotTools` (`runic_godottools`) · Packages: GDT-1 to GDT-16

Source: RunicDocs › Architecture › Godot integration (Engine) (Projects: create, open and the game's editor library;
Game code and the Run button; Open decisions), Tools (Engine), Repositories, build targets and processes (Engine
modules), World authoring and cooking. Status: **GDT-1 (spike) done** in slice 0; its findings are in RunicDocs ›
Architecture › Godot integration (Engine) › Slice 0 spike results (GDT-1). **GDT-2 implemented** (slice 6, on the
preview branch `preview/slices-3-11`): the project model and the runic-mmo template, New and Open, the project's
editor library loaded and reloaded, the IDE bridge, the Runic editor plugin and the world dock, with their acceptance
tests; slice 6's brief and walkthrough are [preview/slice-06-editor-shell.md](../../../preview/slice-06-editor-shell.md).
**GDT-3 implemented** (slice 8, same branch): the physics inspector (its model `PhysicsInspector` and the Runic Physics
dock) and the "see what the server sees" layer in the editor's viewports, with their acceptance tests; slice 8's brief
is [preview/slice-08-physics-view.md](../../../preview/slice-08-physics-view.md). **GDT-4 implemented** (slice 9, same
branch): the Cooker integration (the import plugin's collision presets, the naming convention, RunicCollider3D, the
scene reader, the cook from Project › Tools, before each export and with `--runic-cook`, the cook loaded into the
local world), with its acceptance tests; slice 9's brief is
[preview/slice-09-first-cooker-proof.md](../../../preview/slice-09-first-cooker-proof.md). The folder map is
[README.md](README.md).

## Purpose

The Runic tools inside the RunicEditor (the Godot editor run through LibGodot): marker nodes, schema inspectors and
Runic file formats in the editor, world and region tools, docks (world, ECS, physics, resolver, AI editor on
GraphEdit, live attach), the project template and assistants, Run orchestration, and the export plugin that calls
the Cooker. Editor code never ships to players. GDT-2 makes the editor a Runic editor: it knows a Runic project by its
manifest, makes one from a template, loads and reloads the game's editor library, writes what the IDE needs, and
shows the first dock over a local simulation in the editor process. GDT-3 adds the physics inspector: the local
simulation's bodies, selectable in a list or with a click in a viewport, with their properties, and its collision drawn
in the editor's viewports over the visuals, as a server holding that world would see it. GDT-4 connects the editor to
the Runic Cooker: Godot imports a glTF, the editor reads the scene Godot imported (no second glTF importer) and the
Cooker cooks it; the cook loads into the local world, so the layer draws the cooked collision over the visual.

## Public API

Namespace `Runic::GodotTools`. Headers are included by their path under `Public/`.

| Header | Holds | Since |
| --- | --- | --- |
| `GodotToolsModule.hpp` | `RegisterEditorTools(GodotRuntime&, const EditorToolsSpec&)`: registers the plugin and the docks through the runtime's initialization hooks; `EditorToolsSpec` (the local world, the world position the edited scene's origin stands for (`editedSceneOrigin`, GDT-3), the template folder, the Engine version, the IDE files' spec, `--runic-cook`'s `cookAndQuit`, `cookScenes` and `cookFinished` (GDT-4)); `kPluginLoadedLine`, `kPluginUnloadedLine`, `kCookSucceededLine`, `kCookFailedLine`, `kCookSucceededExitCode`, `kCookFailedExitCode` | GDT-1 to GDT-4 |
| `RunicProject.hpp` | `ProjectManifest` (format, Engine version, game name, `ProjectPaths`, `ProjectExecutables`, `CookSettings`), `ParseProjectManifest`, `WriteProjectManifest`, `RunicProject`, `IsRunicProject`, `OpenProject`, `NewProjectSpec`, `InitializeProject` (New); the names `kManifestFileName`, `kGodotProjectFileName`, `kGameEditorExtensionFileName`, `kGodotCacheFolderName`, `kDefaultTemplateName`; `kManifestFormat` (1) | GDT-2 |
| `IdeBridge.hpp` | `IdeFilesSpec` (the CMake preset and build folder), `IdeFilesReport`, `WriteIdeFiles`; the file names and `kGeneratedMarkerKey`, `kDefaultGoVersion` | GDT-2 |
| `WorldInspector.hpp` | `WorldInspector` (`Summary`, `Entities`, `Inspect`, `Queries`, `CommandQueues`, `States`), `WorldSummary`, `InspectedEntity`, `InspectedComponent`, `kDefaultEntityListLimit` | GDT-2 |
| `EditorCook.hpp` | `ReadImportedScene` (a scene Godot imported as a `Cooker::CookScene`), `CookEditorScenes`, `EditorCookSpec` (scenes, output, content folder, origin), `LoadCookedCells`, `CookedLoadStats`, `ContentNameOf`, `CollisionPreset`, `CollisionPresetHint`; `kCollisionOnlySuffix` (`-rcol`), `kCollisionPresetOption` (`runic/collision`), `kCollisionPresetMeta`, `kCollisionOnlyMeta`, `kDefaultContentFolder` | GDT-4 |
| `PhysicsInspector.hpp` | `PhysicsInspector` (`Summary`, `Bodies` near a point, `Inspect`, `Pick` with a ray, `Select`/`Selected`, `DrawServerView`), `PhysicsSummary`, `InspectedBody`, `InspectedProperty`, the property names (`kBody...Property`), `ServerViewSettings` (radius, over the visuals, faces, velocities), `ServerViewStats`; `kDefaultBodyListLimit` (500), `kDefaultInspectRadiusM` (250), `kDefaultPickDistanceM` (1 km), `kSelectedBodyColor` (the query color), `kSelectionPaddingM` (2 cm) | GDT-3 |

Approved exception: godot-cpp types may appear in this module's public headers, so a project's editor library can
extend marker nodes and docks (the fourth public-header exception). GDT-2 links godot-cpp PUBLIC for that reason: a
project's editor library (RunicGame::Editor's row allows only `Runic::GodotTools`) reaches godot-cpp through this
target. GDT-2's and GDT-3's own public headers do not name a godot-cpp type yet.

**The manifest rule:** `runic.project.json` is strict JSON in the shape RunicDocs shows. `runic` (the format, 1),
`engine` and `game.name` are required; `paths`, `executables` and `cook` are optional and take RunicDocs' values when
absent. A member the model does not know is a problem, not ignored, and every problem names its member
(`paths.content: must be a non-empty string`), all at once.

**The New rule** (open decision 2: a plugin, the recommendation): New makes an empty Godot project a Runic project. The
folder may hold the project manager's `project.godot` (kept: the editor has it open) and `.godot/`, nothing else; the
template's files are copied in with `{{GAME_NAME}}` filled, and the manifest is written. A folder with anything else
is refused untouched.

**The Cooker integration's rules** (GDT-4; RunicDocs › World authoring and cooking › How collision is marked):

- *Reading a scene.* The scene is loaded afresh (the scenes it instances too, so a cook reads what was imported
  last) and walked without entering the tree. Each MeshInstance3D is an asset and a placement. The asset is the
  imported file that holds the node (the nearest of the node and its ancestors that a file instanced) and the node's
  path in it: its AssetId is 64-bit FNV-1a of the file's Godot UID and that path (the path itself when the file has no
  UID), its content name `game:` plus the file's path under the content folder without its extension, then the
  node's path, in snake case (`res://Content/TestRegion/test_region.glb`, `Platform`:
  `game:test_region/test_region/platform`). Its triangles are read surface by surface and turned from Godot's
  clockwise to the formats' counter-clockwise; its normals kept when every surface has them. The placement is the
  node's path from the scene's root and its pose relative to it (composed from the local transforms), its scale
  taken from the basis. A RunicCollider3D is a collider, its node's scale baked into its size.
- *The presets.* The import plugin adds **Runic › Collision** to a glTF's Import dock (*The mesh itself*, the default;
  *None*; *Collision meshes (-rcol)*; *Convex hull* and *Simplified mesh*, listed and reported as not cooked yet) and
  puts the preset on every mesh node of the imported scene, which keeps it.
- *The naming convention.* A mesh node named `<Name>-rcol` (or `<Name>-rcol` followed by anything, for several
  pieces) is collision only, for the mesh node `<Name>` that is its parent or its sibling: the import plugin hides it,
  the reader transforms its triangles into `<Name>`'s space and makes them `<Name>`'s collision, whatever the file's
  preset. A collision-only mesh without such an owner fails the read with its path.
- *The cook.* `CookEditorScenes` reads every scene, then calls `Cooker::Cook` into the cooked folder. The plugin cooks
  the manifest's world scene (`cook.world`) into `paths.cooked` from Project › Tools › **Runic: cook the project** and
  before each export (the export plugin) — while the project has no world scene, the scene being edited; with `--runic-cook` it waits until the editor has scanned and imported (and
  a second more), cooks the scenes named by `--runic-cook-scene=` (or the world scene), opens a help page (Godot 4.7.2's
  help thread must finish before a quit, see below), and a second later stops the program with the cook's exit code
  (`cookFinished`) and quits Godot. A cook prints `RUNIC: cook succeeded in <ms> ms: <n> meshes, ...` or `RUNIC: cook
  failed: ...` and each problem.
- *Loading a cook.* `LoadCookedCells` puts every cell of a cooked folder into a simulation's physics: each placement's
  cooked collision shapes and each collider as a static body of an entity tagged `engine:cooked_body`, at the cell's
  origin plus their offsets; a later load destroys the entities the last one made (their bodies go at the next
  barrier). Physics shapes are named by a hash of their content, so a reload of the same cook reuses them. The plugin
  loads each cook into the editor's local world.

**The physics inspector's rules** (GDT-3): the inspector reads the local simulation's physics world between its
steps, through `SimulationRuntime::FindPhysicsBridge` and Physics' descriptions (`FindBodies`, `DescribeBody`,
`DescribeShape`, `ExtractDebugGeometry`), and never changes it; the selection is its own. A body's properties are, in
this order: shape (its name), kind (`box`, `sphere`, `capsule`, `mesh, <n> triangles`), half extents, motion, layer,
position (world metres, three decimals: a world position, never a float offset), rotation (the quaternion), velocity,
friction, restitution, state (`awake`, `asleep`, `never moves` for static bodies), sensor, owner (`entity <n>`, named as
the world dock names it, or `none`). A gone body has no properties and is no longer selected. **Picking:** the nearest
body a ray meets within 1 km, boxes and spheres by their exact shape, capsules by their box; meshes are not picked by a
ray (their bounds cover what stands on them, and Physics' `ShapeInfo` gives their bounds' size, not their centre) but
from the list, until Physics' ray queries (PHYS-5, slice 15). **The server view:** Physics' debug view around the
editor camera's 16 m cell, 250 m out, in the overlay batch (over the visuals, the default) or the depth-tested batch,
large shapes as cached meshes; the selected body's bounds, 2 cm larger, in the overlay batch in the query color (a mesh:
an arrow down onto its origin). Positions are relative to the cell's world position; the layer puts them at that
position minus `editedSceneOrigin` in the edited scene.

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
- The physics dock (`RunicPhysicsDock`, "Runic Physics", right lower dock) and its model, `PhysicsInspector`: the
  physics world's counts (bodies, active ones, shapes, characters, steps, those entities own), the switches **See what
  the server sees** (the layer) and **Over the visuals** (or depth-tested), **Pick a body in the viewport** (the next
  left click in a 3D viewport picks; another button gives up), the bodies near the editor camera (a page of 500, in
  handle order: body, shape, motion) and the selected body's properties. The lists refresh four times a second; the
  layer when the physics world stepped, the camera changed cell, the selection or a switch changed. When the layer's
  geometry is full (65,536 vertices a batch), the summary says how many vertices were not drawn.
- The "see what the server sees" layer on Godot's side (`ServerViewLayer`): two RenderingServer meshes (depth-tested
  and overlay batch, unshaded, vertex colors, translucent; the overlay with no depth test and the highest priority), a
  RenderingServer mesh per cached debug mesh and pooled instances, in the world the editor's 3D viewports render (the
  edited scene's, where the editor draws its own gizmos). It checks at creation that Godot lays a surface out as
  DebugGeometry does, as GodotBridge's debug view does, and draws nothing otherwise.
- The Cooker integration (GDT-4, `Source/Export`, `Source/MarkerNodes`): the scene reader, the editor's cook, loading a
  cook into a simulation, the import plugin (`RunicImportPlugin`), the export plugin (`RunicExportPlugin`),
  RunicCollider3D (box, sphere or capsule; public in the editor's Create Node dialog) and its gizmo, the collision
  convention's documentation in the project template (`Content/README.md`).

## Does not own

- The RunicEditor executable (`Engine/Programs/RunicEditor/`) and its local simulation: the program makes the
  `SimulationRuntime`, fills it and hands it over in `EditorToolsSpec`; the LibGodot instance is Runic::GodotBridge's.
- Building a project: the project's own CMake builds its editor library (the template's `CMakeLists.txt` shows how);
  the editor only loads what is built. Run orchestration (compile, cook, start) is GDT-9 (slice 13).
- The game's marker nodes and docks: the project's editor library (RunicGame::Editor, GAME-1).
- How states are listed: Runic::ECS offers no way to enumerate declared states by index, so the dock shows none
  (reported; see the slice brief).
- The physics world, its bodies and its debug drawing (Runic::Physics, PHYS-1 to PHYS-3) and the bridge from entities
  (Runic::Simulation, SIM-8); the client's physics layer (Runic::GodotBridge, GDB-4).
- Cooking itself (Runic::Cooker, COOK-1) and the cooked formats (Runic::Assets, ASSET-1). The other marker nodes
  (RunicVolume3D, RunicSpawnArea3D, RunicInteractable3D, RunicRoute3D) and incremental cooking are GDT-7's (slice 21);
  region scenes and their 64-bit origins GDT-8's.
- Building libgodot (BUILD-1). Cooking (Runic::Cooker).

## Allowed dependencies

Catalog row (ceiling): any module and Runic::Cooker; third parties libgodot (editor build) and godot-cpp (public by
the approved exception). GDT-2 links Runic::GodotBridge (public: the plugin is registered on its runtime),
Runic::Simulation (public: `WorldInspector` and `EditorToolsSpec` name the `SimulationRuntime`) and godot-cpp
(public). GDT-3 adds no link: `PhysicsInspector.hpp` names Runic::Physics and Core render data, which come with
Runic::Simulation (public there since SIM-8). GDT-4 links Runic::Cooker (public: `EditorCook.hpp` names its input and
report; Runic::Assets and Runic::Content come with it). The JSON reader is the module's own (`Source/Project/Json.cpp`): the row allows no JSON library, and the
model must work without Godot running.

## Forbidden dependencies

Any shipped executable linking this module (GameClient → GodotTools is forbidden); domain modules depending on it
(the AI docks depend on AI contracts, never the reverse); Platform and Game targets.

## Threading and tick model

Godot's main thread only: the plugin and the docks run inside the editor's frames (`GodotRuntime::Iterate`). The docks
read the local simulation between its steps: the Simulation layer below the GodotLayer runs the steps in the same
thread, earlier in the frame. The physics dock's layer extracts and uploads on that thread too (Physics' extraction
reads the world between steps, PHYS-3's rule). The project model and the IDE bridge are plain functions with file I/O, called on that
thread from the plugin's menus.

## Data ownership

Godot owns the plugin node; its dialogs are its children. The docks are added to the editor's docks by the plugin and
freed when the plugin leaves. The physics dock owns its layer's RenderingServer resources and frees them when it
leaves the tree, and the layer's geometry and cached-mesh library (reused every refresh, never reallocated). `RegisterEditorTools` keeps a copy of its spec for the classes Godot makes later (one
editor per process). The local simulation belongs to the program and outlives Godot.

## Integration points

- `RegisterEditorTools(runtime, spec)` adds hooks to the `GodotRuntime` before `Create()`; the classes register at
  `GodotInitLevel::Editor`.
- A project declares its editor library in `game_editor.gdextension` with `reloadable = true` and the entry symbol
  `game_editor_init`; it builds the library against godot-cpp (through `Runic::GodotTools`).
- The RunicEditor stages the template next to its executables (`<build>/templates`) and names it in the spec;
  `--runic-new-project <folder> <name>` calls `InitializeProject` without Godot.
- The physics dock shows the local simulation's physics when the program enabled it
  (`SimulationRuntime::EnablePhysics`) and says "The local simulation runs no physics" otherwise. The plugin forwards
  3D viewport input always (`set_input_event_forwarding_always_enabled`), and takes a click only while the dock picks.
- `--runic-cook` (the RunicEditor sets `cookAndQuit`, `cookScenes` and `cookFinished`); the export plugin cooks at the
  start of every export; Project › Tools › Runic: cook the project.
- The dock binds check methods for tests through Godot (its class is private): `shown_bodies`, `select_body(index)`,
  `selected_property(name)`, `pick_ray(origin, direction)` (in the scene's space), `server_view_surface(batch,
  primitive)`, `server_view_drawn_over_visuals`, `server_view_shown`; its controls are named `ServerView`,
  `OverVisuals`, `Pick`, `Bodies` and `Properties`.

## Where the code lives

- `Source/Project/`: `Json.hpp`/`Json.cpp`, `RunicProject.cpp`, `GameLibrary.hpp`/`GameLibrary.cpp`,
  `Template/runic-mmo/`.
- `Source/IDEBridge/IdeBridge.cpp`; `Source/Docks/WorldInspector.cpp`, `RunicWorldDock.hpp`/`.cpp`,
  `InspectorText.hpp` (how both docks name an entity), `PhysicsInspector.cpp`, `RunicPhysicsDock.hpp`/`.cpp`,
  `ServerViewLayer.hpp`/`.cpp`; `Source/Export/SceneReader.cpp`, `EditorCook.cpp`, `RunicImportPlugin.hpp`/`.cpp`;
  `Source/MarkerNodes/RunicCollider3D.hpp`/`.cpp`;
  `Source/Plugin/RunicEditorPlugin.hpp`/`.cpp`, `EditorContext.hpp`; `Source/GodotToolsModule.cpp`.
- `tests/godot/fixtures/game_editor/GameEditorFixture.cpp`: the fixture editor library, built twice (build 1 and 2).
  `Tests/EditorHarness.hpp`: the scratch project and the editor in the test process, shared by the tests with Godot.
  `tests/godot/fixtures/test_region/`: the test region (a glTF that stands in for a Blender export, its generator
  script, and a scene placing it with a RunicCollider3D); `tests/godot/CopyIntoFolder.cmake` puts it into the CTests'
  project.
- Formats the CTests rely on: `RUNIC: Runic editor plugin loaded`, `RUNIC: project <name> opened (Runic format <n>,
  made with Engine <v>)`, `RUNIC: <folder> is a Godot project without runic.project.json; ...`, `RUNIC: game editor
  library <res path> (loaded|not loaded yet)`, `RUNIC: game editor library reloaded (<n> so far)`, `RUNIC: world dock
  shows <n> entities of simulation <id> (...)`, `RUNIC: physics dock shows <n> bodies of simulation <id> (<a> active,
  <s> shapes, <c> characters)` (once physics has stepped), `RUNIC: physics dock: The local simulation runs no
  physics`, `RUNIC: server view draws <l> line vertices, <t> triangle vertices and <m> mesh instances of <b> bodies in
  the editor's viewport (over the visuals|depth-tested)`, `RUNIC: server view stopped after <n> refreshes of <us> us:
  ...`.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| GDT-1 | Spike: the RunicEditor executable on libgodot (editor target) opens the Godot editor with a test plugin; the editor target builds and opens without GDScript | The editor opens, loads the plugin and exits cleanly in CI; the spike's findings decide the fallback | Done in slice 0; its test plugin replaced by GDT-2's plugin |
| GDT-2 | Project model: runic.project.json, the runic-mmo template, New and Open, loading and reloading the project's editor library; IDE bridge; first dock with world and ECS inspectors over a local simulation in the editor process | The editor opens a project and inspects entities of a local world; a rebuilt editor library reloads without restarting | **Implemented** (slice 6, preview branch): `RunicGodotToolsTests` passes with the 19 tests below; the CTests `godot.editor_spike`, `godot.editor_new_project`, `godot.editor_project_library`, `godot.editor_opens_project`; awaiting the owner's review |
| GDT-3 | Physics inspector and the "see what the server sees" viewport layer | Bodies are selectable with their properties; cooked collision draws over the visual | **Implemented** (slice 8, preview branch): `RunicGodotToolsTests` passes with the 12 tests below; the CTest `godot.editor_opens_project` sees the physics dock and the layer; awaiting the owner's review |
| GDT-4 | Cooker integration: export plugin that calls the Cooker; collision presets in Godot's import settings; RunicCollider3D; the collision naming convention for glTF | A Blender mesh imported by Godot cooks into collision and is walkable | **Implemented** (slice 9, preview branch): `RunicGodotToolsTests` passes with the 4 tests below; the CTests `godot.editor_cook_fixture`, `godot.editor_cooks_test_region` and `godot.server_walks_cooked_region`; awaiting the owner's review |
| GDT-5 to GDT-16 | Docks, schema inspectors, region tools, Run, AI editor, live attach, water authoring, GDScript locks | See RunicDocs | Not started |

GDT-2's acceptance tests (`RunicGodotToolsTests`): `ProjectManifest.*` (RunicDocs' manifest read; written back the
same; defaults; every problem named, all at once; broken JSON with its line and column; escapes and Unicode),
`NewProject.*` (the template's layout with the game's name, `.gdignore` where code and builds are; the project
manager's `project.godot` kept; a busy folder refused; bad names refused), `OpenProject.*`, `IdeBridge.*` (go.work over
the services with the newest Go version, tasks and launch configurations; clangd's link; files the bridge did not
write left alone), `WorldInspector.*` (the summary; an entity's components with their values; a stable page of
entities), and with Godot headless in the test process `GameEditorLibrary.*` (**the editor loads the project's editor
library**; **a rebuilt library reloads without restarting**: build 1, replaced by build 2, is seen as build 2 through
`ClassDB`). The CTest `godot.editor_opens_project` checks **the editor opens a project and inspects entities of a
local world** (the dock's line with 13 entities; 16 since GDT-3 added the floor and two crates).

GDT-3's acceptance tests (`RunicGodotToolsTests`): `PhysicsInspector.*` without Godot (a simulation without physics
shows nothing; the summary; the bodies near a point in handle order, a far one left out, the page limit; **a body's
properties in world space**, the owner named as the world dock names it; a gone body has no properties and is no
longer selected; **picking**: the nearest body from above, sideways, a direction that is not a unit vector, a ray past a
sphere's silhouette, away or too short; a turned box by its own shape; a mesh not picked; **the server view** over the
visuals or depth-tested, at the right place relative to its origin; the selected body's bounds over everything), and
with Godot headless in the test process `RunicPhysicsDock.*` (**bodies are selectable with their properties**: the
dock lists the local simulation's three bodies 8,602 km out, selects one through its list and shows its world
position, then picks with a ray as a viewport click does; **the collision draws over the visual**: the layer's overlay
lines, read back from the RenderingServer, span the floor and the box at their place in the edited scene, with no
depth test; depth-tested, they move to the other batch; the layer hides). The CTest `godot.editor_opens_project`
checks the physics dock's line (15 bodies) and the layer's.

GDT-4's acceptance tests (`RunicGodotToolsTests`, with Godot headless in the test process): `EditorCook.*` (**the
reader takes what Godot imported**: four assets from the glTF, the rock's collision from its `-rcol` mesh in its own
space, every triangle counter-clockwise facing its normal, the placements' paths and poses, the wall collider, IDs
stable from Godot's UID, the collision-only mesh hidden by the import plugin while the platform draws; **a cooked
region is walkable and its collision draws where Godot draws the visual**: the cook's 4 meshes, 4 collisions and 44
triangles load into the local world as 5 bodies, a second load replaces the first, a character walks from the ground
up the ramp onto the platform and stops at the wall 8,600 km out, and the layer's lines, read back from Godot, span the
imported ground mesh's bounds to the millimetre; **the import preset decides**: *None* on the file takes the
collision of every mesh but the rock, whose collision meshes win; content names in snake case). The CTests: the test
region goes into the project New made, the RunicEditor cooks it without a window (`--runic-cook-scene=`) and loads it,
and **the RunicServer walks a character on the cooked cell** from the ground up to the wall (`--cooked`).

Decisions taken in GDT-4 that await the owner (also in the slice brief): the preset is per file (the Import dock's
global options), not per mesh node; the naming convention is `-rcol` (Godot's own `-col` suffixes create Godot physics
bodies, which are not in the build), for a parent or a sibling, and wins over the file's preset; a mesh's AssetId is a
hash of its file's Godot UID and its node path (Godot gives UIDs to files, not to the meshes inside them); content
names from the path under the content folder in snake case; the menu and the export cook the manifest's world scene,
`--runic-cook-scene=` names others; `--runic-cook` waits for the editor's imports and a second more; the editor loads
each cook into its local world; RunicCollider3D sizes are full sizes (a capsule's height includes its caps), its scale
baked in.

Reported, not made: GodotBridge's GodotLayer stops the application as a clean exit when Godot quits, whatever Godot's
exit code: the RunicEditor passes `--runic-cook`'s code through `cookFinished` and a layer of its own; a GodotLayer that
hands Godot's quit code to the application would make that unneeded. No runtime module may link both Assets and
Physics (Simulation's row has no Assets): loading a cooked cell into a world is written three times for slice 9 (the
editor's `LoadCookedCells`, the RunicServer's cooked walk, the Cooker's tests) until WORLD-3 and COOK-3 (slice 21) give
it an owner.

Costs, measured: the RunicEditor's cook of the test region (read from Godot, cooked, written) 3 ms; loading it into the
local world is 5 bodies. The editor's test run takes about 5 s, most of it Godot's start and first import.

Decisions taken in GDT-3 that await the owner (also in the slice brief): until the Cooker (slice 9), "cooked collision"
is the local simulation's own physics, which GDT-4 fills from cooked cells; the layer draws over the visuals by
default; viewport picking is a dock button and a forwarded click (no new shortcut); picking uses the shapes' bounds
until PHYS-5's rays, and leaves meshes to the list; the layer follows the editor camera's 16 m cell, 250 m out; the
editor's local world gains a floor, kinematic markers and two crates; `editedSceneOrigin` is the world's origin until
region tools (GDT-8).

Reported interface changes (not made: one package, one module): GodotBridge's debug layers (GDB-4) draw the same
geometry for the client but are private to GodotBridge and make their own camera current, which the editor's root
window must not have; a public "draw this DebugGeometry in that scenario" in GodotBridge would let GodotTools drop
`ServerViewLayer` (about 300 lines alike). Physics' `ShapeInfo` gives a shape's bounds' size but not their centre: a
mesh's selection is an arrow, not its bounds, until it does.

Costs, measured: the model on 500 static boxes (release, `-O2`): listing and inspecting them 0.66 ms (1.3 µs a body,
13 formatted properties each), a pick 15 µs, the server view 0.48 ms (36,000 line vertices). In the editor (debug
build, the only one with libgodot here), one refresh of the layer for 500 moving boxes, extraction and upload to
Godot, is 2.4 ms; for the RunicEditor's own 15 bodies a refresh draws 1,080 line vertices.

Decisions taken in GDT-2 that await the owner (also in the slice brief): New is a plugin menu (open decision 2's
recommendation) plus a command-line `--runic-new-project`; the template lives in `Source/Project/Template` and is staged
in `<build>/templates`; godot-cpp is PUBLIC; the JSON reader is the module's own; unknown manifest members are errors;
the watcher loads the library itself when Godot has not and reloads on a settled change; the spike plugin and its
folder are gone.

**A known Godot 4.7.2 crash at shutdown:** the editor loads its help cache on a thread, which ends by queuing the
generation of the extensions' docs; an editor shut down before that queued call ran crashes in `Main::cleanup`, which
flushes it after the docs were deleted (`EditorHelp::_gen_extensions_docs`). The editor tests open a help page before
shutting down (it waits for that thread) and run a second of frames after it; the RunicEditor itself can still crash
on a quit within the first seconds on a loaded machine, until Godot checks for the deleted docs (to report upstream,
or a later release pin).

## Future extensions

- Opening a build error at its line (with Run, GDT-9), and the assistants (GDT-10) that write files and register them.
- Listing states in the dock, once Runic::ECS can enumerate them.
- The Cooker integration: per-mesh presets (the Advanced Import Settings' node options), convex hulls and simplified
  meshes when the Cooker makes them, incremental cooking of the touched cells and the other marker nodes (GDT-7),
  region scenes with 64-bit origins (GDT-8), a cook in the background with progress for long cooks.
- The physics dock: characters in the list (the bridge's `PhysicsCharacter` entities), picking by Physics' rays
  (PHYS-5), selecting the body's owner in the world dock, volumes, spawn points and nav tiles in the server view
  (RunicDocs' list for the layer), and cooked collision from GDT-4.
- A patch of the project manager for New, if the plugin's flow is not enough (open decision 2).
- Every Runic tool's UI follows the editor: only its extension points (docks, the bottom panel, a main-screen tab,
  inspector plugins, Project › Tools and context menus), the editor theme and scale, never controls inserted into a
  container whose layout it does not own (RunicDocs › Tools (Engine)). A branding patch and the editor feature profile
  are tool-phase decisions.
