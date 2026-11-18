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
is [preview/slice-08-physics-view.md](../../../preview/slice-08-physics-view.md). The folder map is
[README.md](README.md).

## Purpose

The Runic tools inside the RunicEditor (the Godot editor run through LibGodot): marker nodes, schema inspectors and
Runic file formats in the editor, world and region tools, docks (world, ECS, physics, resolver, AI editor on
GraphEdit, live attach), the project template and assistants, Run orchestration, and the export plugin that calls
the Cooker. Editor code never ships to players. GDT-2 makes the editor a Runic editor: it knows a Runic project by its
manifest, makes one from a template, loads and reloads the game's editor library, writes what the IDE needs, and
shows the first dock over a local simulation in the editor process. GDT-3 adds the physics inspector: the local
simulation's bodies, selectable in a list or with a click in a viewport, with their properties, and its collision drawn
in the editor's viewports over the visuals, as a server holding that world would see it.

## Public API

Namespace `Runic::GodotTools`. Headers are included by their path under `Public/`.

| Header | Holds | Since |
| --- | --- | --- |
| `GodotToolsModule.hpp` | `RegisterEditorTools(GodotRuntime&, const EditorToolsSpec&)`: registers the plugin and the docks through the runtime's initialization hooks; `EditorToolsSpec` (the local world, the world position the edited scene's origin stands for (`editedSceneOrigin`, GDT-3), the template folder, the Engine version, the IDE files' spec); `kPluginLoadedLine`, `kPluginUnloadedLine` | GDT-1, GDT-2, GDT-3 |
| `RunicProject.hpp` | `ProjectManifest` (format, Engine version, game name, `ProjectPaths`, `ProjectExecutables`, `CookSettings`), `ParseProjectManifest`, `WriteProjectManifest`, `RunicProject`, `IsRunicProject`, `OpenProject`, `NewProjectSpec`, `InitializeProject` (New); the names `kManifestFileName`, `kGodotProjectFileName`, `kGameEditorExtensionFileName`, `kGodotCacheFolderName`, `kDefaultTemplateName`; `kManifestFormat` (1) | GDT-2 |
| `IdeBridge.hpp` | `IdeFilesSpec` (the CMake preset and build folder), `IdeFilesReport`, `WriteIdeFiles`; the file names and `kGeneratedMarkerKey`, `kDefaultGoVersion` | GDT-2 |
| `WorldInspector.hpp` | `WorldInspector` (`Summary`, `Entities`, `Inspect`, `Queries`, `CommandQueues`, `States`), `WorldSummary`, `InspectedEntity`, `InspectedComponent`, `kDefaultEntityListLimit` | GDT-2 |
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

## Does not own

- The RunicEditor executable (`Engine/Programs/RunicEditor/`) and its local simulation: the program makes the
  `SimulationRuntime`, fills it and hands it over in `EditorToolsSpec`; the LibGodot instance is Runic::GodotBridge's.
- Building a project: the project's own CMake builds its editor library (the template's `CMakeLists.txt` shows how);
  the editor only loads what is built. Run orchestration (compile, cook, start) is GDT-9 (slice 13).
- The game's marker nodes and docks: the project's editor library (RunicGame::Editor, GAME-1).
- How states are listed: Runic::ECS offers no way to enumerate declared states by index, so the dock shows none
  (reported; see the slice brief).
- The physics world, its bodies and its debug drawing (Runic::Physics, PHYS-1 to PHYS-3) and the bridge from entities
  (Runic::Simulation, SIM-8); the client's physics layer (Runic::GodotBridge, GDB-4). Cooked collision (the Cooker,
  COOK-1, and GDT-4's Cooker integration, slice 9): until then the layer draws the local simulation's own bodies.
- Building libgodot (BUILD-1). Cooking (Runic::Cooker).

## Allowed dependencies

Catalog row (ceiling): any module and Runic::Cooker; third parties libgodot (editor build) and godot-cpp (public by
the approved exception). GDT-2 links Runic::GodotBridge (public: the plugin is registered on its runtime),
Runic::Simulation (public: `WorldInspector` and `EditorToolsSpec` name the `SimulationRuntime`) and godot-cpp
(public). GDT-3 adds no link: `PhysicsInspector.hpp` names Runic::Physics and Core render data, which come with
Runic::Simulation (public there since SIM-8). The JSON reader is the module's own (`Source/Project/Json.cpp`): the row allows no JSON library, and the
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
- The dock binds check methods for tests through Godot (its class is private): `shown_bodies`, `select_body(index)`,
  `selected_property(name)`, `pick_ray(origin, direction)` (in the scene's space), `server_view_surface(batch,
  primitive)`, `server_view_drawn_over_visuals`, `server_view_shown`; its controls are named `ServerView`,
  `OverVisuals`, `Pick`, `Bodies` and `Properties`.

## Where the code lives

- `Source/Project/`: `Json.hpp`/`Json.cpp`, `RunicProject.cpp`, `GameLibrary.hpp`/`GameLibrary.cpp`,
  `Template/runic-mmo/`.
- `Source/IDEBridge/IdeBridge.cpp`; `Source/Docks/WorldInspector.cpp`, `RunicWorldDock.hpp`/`.cpp`,
  `InspectorText.hpp` (how both docks name an entity), `PhysicsInspector.cpp`, `RunicPhysicsDock.hpp`/`.cpp`,
  `ServerViewLayer.hpp`/`.cpp`;
  `Source/Plugin/RunicEditorPlugin.hpp`/`.cpp`, `EditorContext.hpp`; `Source/GodotToolsModule.cpp`.
- `tests/godot/fixtures/game_editor/GameEditorFixture.cpp`: the fixture editor library, built twice (build 1 and 2).
  `Tests/EditorHarness.hpp`: the scratch project and the editor in the test process, shared by the tests with Godot.
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
| GDT-4 to GDT-16 | Cooker integration, docks, schema inspectors, region tools, Run, AI editor, live attach, water authoring, GDScript locks | See RunicDocs | Not started |

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
- The physics dock: characters in the list (the bridge's `PhysicsCharacter` entities), picking by Physics' rays
  (PHYS-5), selecting the body's owner in the world dock, volumes, spawn points and nav tiles in the server view
  (RunicDocs' list for the layer), and cooked collision from GDT-4.
- A patch of the project manager for New, if the plugin's flow is not enough (open decision 2).
- Every Runic tool's UI follows the editor: only its extension points (docks, the bottom panel, a main-screen tab,
  inspector plugins, Project › Tools and context menus), the editor theme and scale, never controls inserted into a
  container whose layout it does not own (RunicDocs › Tools (Engine)). A branding patch and the editor feature profile
  are tool-phase decisions.
