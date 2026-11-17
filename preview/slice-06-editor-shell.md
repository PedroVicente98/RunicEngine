# Slice 6 · Editor shell

**Runnable result:** the RunicEditor opens the RunicGame project and a dock inspects entities of a local world.
**Packages:** GDT-2 (Runic::GodotTools, the RunicEditor) in RunicEngine; GAME-1 (RunicGame::Editor) in RunicGame, on
RunicGame's branch `preview/slices-3-11` (cut from its `main`: RunicGame has no `slice/1` branch).
**Status:** done on the preview branches; 19 new tests in `RunicGodotToolsTests`, 4 new or changed editor CTests in
the Engine, 1 CTest in RunicGame. The Engine's debug build runs 392 tests, all passing.

```sh
# RunicEngine
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/RunicEditor --runic-new-project /tmp/MyGame "My Game"   # New, without Godot
./build/debug/bin/RunicEditor --editor --path /tmp/MyGame                  # the project; the Runic World dock

# RunicGame (an editor build: the Engine's RunicEditor and the game's editor library in one tree)
cmake -S . -B Build/editor -DRUNIC_GAME_BUILD_EDITOR=ON -DRUNIC_ENGINE_DIR=../RunicEngine \
      -DRUNIC_PLATFORM_DIR=../RunicPlatform && cmake --build Build/editor
./Build/editor/bin/RunicEditor --editor --path .     # RunicGame in the RunicEditor, its editor library loaded
ctest --test-dir Build/editor -R RunicGame.EditorOpensProject
```

Opening RunicGame headless prints, among Godot's own lines:

```text
RUNIC: RunicGame editor library loaded
RUNIC: Runic editor plugin loaded: the RunicEditor runs the Godot editor through LibGodot, Godot 4.7.2-stable (custom_build)
RUNIC: GDScript in this editor build: no
RUNIC: project RunicGame opened (Runic format 1, made with Engine 0.1.0)
RUNIC: game editor library res://Build/bin/libgame_editor.so (loaded)
RUNIC: world dock shows 13 entities of simulation 1 (4 component types, 1 queries, 0 command queues)
RUNIC: RunicEditor exited cleanly after 300 frames
```

The first line comes before the plugin's: once the project has opened once, Godot loads the library itself at
startup (its `.godot/extension_list.cfg` lists it); on a first open the plugin's watcher loads it.

In a window: Godot's editor on RunicGame, a **Runic World** dock at the upper right (the local simulation's ID and
tick, its counts, the entity list, the selected entity's components with their values updating four times a second,
the queries and the command queues), Project › Tools › **Runic: generate IDE files** and **Runic: reload the game
editor library**, and the game's `GameMarker3D` in the Create Node dialog.

## What was built

| Package | What | Where |
| --- | --- | --- |
| GDT-2 | The project model (`runic.project.json`, Open, New), the runic-mmo template, the watcher that loads and reloads a project's editor library, the IDE bridge, the Runic editor plugin (replacing the spike's), the Runic World dock and its model | `Engine/Tools/GodotTools`: `Public/` (four headers), `Source/Project` (with `Template/runic-mmo`), `Source/IDEBridge`, `Source/Docks`, `Source/Plugin`, `Tests/`; `tests/godot/fixtures/game_editor` (a fixture library) |
| (program) | The RunicEditor's local world (a Simulation layer under the GodotLayer), the tools' spec, `--runic-new-project`, the new CTests | `Engine/Programs/RunicEditor`: `Main.cpp`, `LocalWorld.cpp`, `CMakeLists.txt` |
| GAME-1 | RunicGame as a RunicEngine project: `runic.project.json`, `game_editor.gdextension`, the editor library target with the game's first marker node | RunicGame: the root files, `Source/Editor` (a new module) |

## The order to read it in

1. **`GodotTools/Public/RunicProject.hpp`**: the manifest model, Open and New, with their rules in the comments.
   Then `Source/Project/RunicProject.cpp`: `ParseProjectManifest` (the `Reader` collects every problem) and
   `InitializeProject` (the template copied, `{{GAME_NAME}}` filled, the manifest written).
2. **`Source/Project/Template/runic-mmo/`**: what a new project is. `game_editor.gdextension` is the key file.
3. **`Source/Project/GameLibrary.cpp`**: `Open` reads the `.gdextension` with Godot's `ConfigFile` and picks the
   library by feature tags; `Poll` and `Reload` are the hot reload (about 40 lines).
4. **`Public/IdeBridge.hpp`**, then `Source/IDEBridge/IdeBridge.cpp`: `WriteIdeFiles` at the bottom.
5. **`Public/WorldInspector.hpp`**, `Source/Docks/WorldInspector.cpp` (the model), `RunicWorldDock.cpp` (the
   layout: a label, two trees, a list; `Refresh`).
6. **`Source/Plugin/RunicEditorPlugin.cpp`**: `_enter_tree` wires everything; `_process` polls the library.
7. **`Programs/RunicEditor/Main.cpp`**: the stack, `--runic-new-project`; `LocalWorld.cpp`.
8. **RunicGame: `runic.project.json`, `game_editor.gdextension`, `Source/Editor/CMakeLists.txt`,
   `GameEditorLibrary.cpp`, `Source/MarkerNodes/GameMarker3D.cpp`.**

## A rebuilt editor library, traced

The RunicEditor has RunicGame open; a developer rebuilds `game_editor`.

1. The linker writes `Build/bin/libgame_editor.so`. A build replaces the file (or rewrites it) while the editor runs.
2. Twice a second the plugin's `_process` calls `GameLibrary::Poll`. It sees a modification time newer than the one
   it loaded and remembers it; half a second later it sees the same time again, so the file has settled.
3. `Reload` calls `GDExtensionManager::reload_extension("res://game_editor.gdextension")`. Godot (editor builds only)
   tells the library to deinitialize, closes it, opens the new file (the editor loads a temporary copy, so the build
   can overwrite the original), initializes it again and rebinds the instances of its classes that exist in open
   scenes.
4. The new library's `game_editor_init` runs; `GameMarker3D` is registered again, with the new code.
5. The log says `RUNIC: game editor library reloaded (1 so far)`. No restart, no scene reopened.

The test `GameEditorLibrary.ARebuiltLibraryReloadsWithoutRestarting` does exactly this with a fixture built twice:
it renames build 2 over build 1 under a running headless editor and sees `RunicFixtureNode.build_id()` turn from 1
to 2 through `ClassDB`.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `ProjectManifest.*` | 6 | RunicDocs' manifest is read and written back the same; defaults; every problem named by its member, all at once (7 in one test); broken JSON reported with line and column; escapes and Unicode |
| `NewProject.*`, `OpenProject.*` | 5 | New makes the template's layout with the game's name and `.gdignore` where code and builds are; keeps the project manager's `project.godot`; refuses a busy folder and bad names; a folder without a manifest is not a Runic project |
| `IdeBridge.*` | 3 | `go.work` over the services at the newest Go version, VS Code tasks and launch configurations; clangd's link to the build; files the bridge did not write are left alone |
| `WorldInspector.*` | 3 | The local simulation's summary; an entity's components with their formatters' values; a stable page of entities |
| `GameEditorLibrary.*` (Godot) | 2 | **The editor loads the project's editor library; a rebuilt library reloads without restarting** |
| `godot.editor_spike` (changed) | 1 | The Runic plugin loads in a plain Godot project and says it is not a Runic project yet |
| `godot.editor_new_project`, `_project_library`, `godot.editor_opens_project` | 3 (+1 cleanup) | **The editor opens a project and inspects entities of a local world**: New from the command line, then the editor opens the project, loads its library, and the dock shows 13 entities |
| RunicGame `RunicGame.EditorOpensProject` | 1 | **The RunicEditor opens RunicGame and loads its editor library** |

The plain GodotTools tests also pass under AddressSanitizer and UBSan and compile cleanly with clang and warnings as
errors (built ad hoc: GodotTools links godot-cpp, which only the debug tree builds); every Godot-side file compiles
cleanly with GCC and clang under `-Wall -Wextra -Wpedantic -Werror`.

## What it costs

| Operation | Cost |
| --- | --- |
| A dock refresh's model (summary, a page of 500 entities, each one's components and values), Release | 0.25 ms with 500 entities in the world, 0.4 ms with 5,000, 1.3 ms with 50,000; four refreshes a second |
| From a rebuilt library to the reload | at most about a second: two looks half a second apart |
| Opening a project headless to the dock's first refresh (debug build) | about 2.5 s, most of it Godot's editor start |

The dock's Godot side (rebuilding two trees) was not measured; at 500 rows it is what the editor does for its own docks.

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | New is a plugin menu (Project › Tools › Runic: make this a Runic project) on a project the project manager made, plus `RunicEditor --runic-new-project <folder> <name>` | Open decision 2's recommendation: the plugin first; the command line serves scripts and tests | A patch of Godot's project manager |
| D2 | The template lives in `GodotTools/Source/Project/Template/runic-mmo` and the build stages it in `<build>/templates/` | A package writes in its module; `templates/` is where a distribution keeps it | A `templates/` folder at the repository's root (a layout change in RunicDocs) |
| D3 | godot-cpp is linked PUBLIC by GodotTools | RunicGame::Editor's row allows GodotTools only; the approved exception is what lets the game's library reach godot-cpp | Give the Editor row godot-cpp as a third party |
| D4 | The manifest is read by GodotTools' own strict JSON reader | The row allows no JSON library, and the model must work without Godot running | Godot's JSON class (Godot running only) |
| D5 | A manifest member the Engine does not know is an error | A typo must not be silently ignored | Ignore unknown members (forward compatibility) |
| D6 | New keeps an existing `project.godot` and fills `{{GAME_NAME}}` in the template's text files | The editor has that file open and would overwrite it | Replace it |
| D7 | The watcher loads the library itself when Godot has not, and reloads after a change has settled for half a second | Headless editors get no focus events, which is when Godot reloads by itself; a half-written library must not load | Rely on Godot's focus-in reload only |
| D8 | The IDE bridge rewrites only files carrying its marker (`runicGenerated`) and the `compile_commands.json` link it made | Never destroy a developer's own files | Overwrite |
| D9 | The dock shows a page of 500 entities in handle order and refreshes four times a second | A dock is not for 100,000 rows | Paging and search |
| D10 | The spike plugin and `Source/Spike` are deleted | Its README said GDT-2 deletes it | Keep both plugins |
| D11 | RunicGame's library entry point is `Source/Editor/GameEditorLibrary.cpp`, outside `Source/`, compiled only into the shared library; the library goes to `Build/bin/` for every preset | Module sources become the static library; `game_editor.gdextension` names one path | A planned folder for library entries (a layout change) |
| D12 | RunicGame's first marker is `GameMarker3D` (a node with a name, no rule) | GAME-1 asks for the library target, not for game design | Wait for a package that needs a real marker |
| D13 | RunicGame's manifest names Engine 0.1.0 and its executables | The Engine's version today | — |

## Notes for other packages and for the plan

- **Runic::ECS cannot list declared states.** `StateId`'s constructor from an index is private, and `EntityStates`
  has no `StateAt(index)`, so a tool cannot enumerate them. The dock shows component values, queries and command
  queues, and no states. A one-function addition to ECS would fix it (an interface change, not made here).
- **Engine code inside a game's editor library.** The Engine's libraries are static and not position-independent.
  GAME-1's library calls no Engine function, so nothing of them is linked in. When a game's library uses GodotTools'
  marker bases or docks, the Engine will need position-independent builds or a shared SDK, and two copies of Engine
  state (the editor's and the library's) must be thought about.
- **Godot behavior worth knowing:** loading an extension queues a regeneration of its documentation; an editor shut
  down in the same moment crashes in that deferred call. The tests let the editor run one more second.
- RunicGame has no `slice/1` branch, so its preview branch was cut from `main`. **RunicGame's `main` still links the
  transitional `Runic::Runtime` bundle**, which the Engine removed in slice 1 (decision D1, "once RunicGame's
  executables linked per-module targets (GAME-0)"); GAME-0 is not on RunicGame's `main`. So RunicGame's server and
  client do not configure against this Engine branch (its build tests `build.game_server_headless` and
  `build.game_client_and_server` fail). GAME-1's editor build does not build them; its CTest and RunicGame's
  checks (`checks.catalog_sync`, the header and include scans, the workflow pins) pass.
- Godot writes a `.uid` beside `game_editor.gdextension`; RunicGame keeps it in version control, as Godot asks.

## What is finished and what is set up for later

- **Finished:** both packages' "Done when".
- **Set up for slice 8:** GDT-3's physics inspector is a second dock next to Runic World, built the same way.
- **Set up for slice 9:** GDT-4's export plugin hangs off the same plugin; the manifest's `cook` section is read.
- **Set up for slice 13:** Run (GDT-9) uses the manifest's executables and the IDE bridge's preset.
- **Not built, by the rule "Stage A only":** opening a build error at its line, assistants, states in the dock,
  editing values from the dock, a project-manager patch.
