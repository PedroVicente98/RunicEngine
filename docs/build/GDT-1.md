---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/doc
---

# GDT-1 — Spike: the RunicEditor executable on libgodot (findings)

| Field | Value |
| --- | --- |
| Package | GDT-1[^engine-godottools] (slice 0) · [RunicEngine#7](https://github.com/PedroVicente98/RunicEngine/issues/7) |
| Needs | BUILD-1[^doc-build-build-1] ([BUILD-1.md](BUILD-1.md)) |
| Done when | The editor opens, loads the plugin and exits cleanly in CI; the spike's findings decide the fallback |
| Status | **Done locally, awaiting owner review.** `godot.editor_spike` passes; the CI job `godot` runs it on GitHub once merged |
| Pinned | Godot 4.7.2-stable (`ed1daf0b`), godot-cpp 10.0.0-stable (`507ed9d8`), API 4.7 |
| Code | `Engine/Programs/RunicEditor/Main.cpp`; `Engine/Tools/GodotTools/` (the test plugin); `Engine/Presentation/GodotBridge/` (the LibGodot host, shared with the GameClient) |

## What was built

`RunicEditor` is a Runic executable that owns `main()`. It loads `libgodot.linuxbsd.editor.x86_64.so` (Godot's editor
target built as a shared library from `external/godot` with `scripts/godot/runic_editor.py`), creates the one Godot
instance of the process with the command line unchanged, starts it and calls `iteration()` until the editor quits.
Runic code reaches Godot only through godot-cpp compiled into the executable. GodotTools[^engine-godottools] registers a test
`EditorPlugin` (`RunicSpikePlugin`) at Godot's editor initialization level through GodotBridge[^engine-godotbridge]'s level hooks: always
on, with no `plugin.cfg`. It prints what the spike has to establish, adds a **Runic** toolbar button and a
**Project › Tools › Runic: About this editor** item, and removes them on exit.

## Results

| Check | Result |
| --- | --- |
| The editor target builds as a library with the module lists | Yes: 178 MB `libgodot…editor…so`, about 5 minutes with clang and lld at 8 jobs (16-core laptop); the template target (73 MB, with the build profile) about 3 minutes |
| The RunicEditor opens a project through LibGodot, headless and windowed | Yes. Headless: imports the project, loads the editor layout. Windowed: Vulkan 1.4, Forward+ |
| The test plugin loads | Yes: `RUNIC: test plugin loaded … Godot 4.7.2-stable (custom_build)` |
| The editor target builds and opens without GDScript | Yes: `RUNIC: GDScript in this editor build: no` (the class does not exist) |
| Exits cleanly | Yes: the plugin unloads, `libgodot_destroy_godot_instance` runs, exit code 0 |
| No project: the project manager | Opens and exits cleanly (`godot.editor_project_manager`) |
| Quitting in CI | Godot's own `--quit-after N` works through LibGodot; no Runic code is needed |
| One instance per process | Confirmed: a second `Create()` after `Destroy()` is refused (`GodotRuntime.StartsGodotHeadlessAndExits`) |

## Findings

1. **LibGodot works for the editor (plan A holds).** Open decision 5 of RunicDocs › Godot integration ("fallback if
   the editor cannot run as a library") does not need its plan B for now. Recommendation: keep the RunicEditor on
   LibGodot; keep plan B approved as the fallback, as the architecture recommends.
2. **One benign editor error without GDScript.** At editor start Godot prints
   `ERROR: Index p_which = 0 is out of bounds (popup->get_item_count() = 0)`. Traced with gdb to
   `PluginConfigDialog::PluginConfigDialog()` (`editor/plugins/plugin_config_dialog.cpp:299`, the "Create Plugin"
   dialog in Project Settings › Plugins), which selects the first script language when there is none. Nothing
   breaks. Options: report it upstream (a one-line guard), carry it as a small documented patch, or hide plugin
   creation with the editor feature profile (GDT-2[^engine-godottools]). It disappears when GDScript returns in slices 20–21.
3. **Export templates refuse `--path`.** Godot builds export templates with `disable_path_overrides=yes` by default,
   so the template library refuses `--path`, `--main-pack` and `--scene` and runs only the `.pck` next to the
   executable. That is the right lock for shipping and stays on. Consequence: until the client `.pck` export (COOK-4[^engine-cooker],
   slice 22) the client always runs the editor library in development, as RunicDocs' table already says; the
   template library is checked by loading it (`GodotRuntime.LoadsTheTemplateLibrary`). The Game's `shipping`
   preset builds the shipping configuration (template library only, godot-cpp for `template_release`).
4. **libgodot is loaded, not linked.** The host opens the library at runtime (`dlopen`, `LoadLibraryW`), so the same
   client binary runs the editor library in development and the template library when shipping. "Links libgodot" in
   the architecture's tables is realized as "loads libgodot"; the headless check uses the link closure, where the
   server can reach neither godot-cpp, GodotBridge[^engine-godotbridge] nor the `Runic::LibGodot*` targets.
5. **No physics module means Godot's dummy physics servers.** Godot warns at startup unless the project says so;
   projects set `physics/3d/physics_engine` and `physics/2d/physics_engine` to `"Dummy"` (the spike project and
   RunicGame do). The project template (GDT-2) must carry these two settings.
6. **godot-cpp 10 ships the 4.7 API.** Bindings come from its bundled `extension_api-4-7.json`, so slice 0 needs no
   API dump. "Published API per build" (the exact class list of the Runic build) is still to do: the reference
   finding is that `--dump-extension-api` does not work through LibGodot and needs the same profile built once as
   an executable. `RUNIC_GODOT_API_FILE` is ready to take the dump.
7. **Godot's in-tree build shares generated headers between targets.** Building the editor after the template
   (or the reverse) recompiles a few files (about 15 seconds), because files such as `disabled_classes.gen.h`
   follow the last target built. Harmless; CI keeps a SCons cache.
8. **Shared editor settings.** Without a branding patch the RunicEditor uses Godot's editor settings folder
   (`~/.config/godot/`), shared with an official Godot on the same machine. The reference's
   `0001-runic-branding.patch` renames it; not applied in slice 0 (no patch is needed yet). Owner decision.
9. **The editor's UI language follows the system locale** (Portuguese here); nothing to do.

## Not verified

- The project manager's **Open** and **New**, which relaunch this executable with `--path` (GDT-2[^engine-godottools], slice 6).
- Godot's Play buttons relaunching the RunicEditor: `main()` has the safety net (`--editor-pid` without `--editor`
  exits with a message) but Run orchestration is GDT-9 (slice 13).
- Windows and macOS: the loader has their code paths (UTF-8 command line, `LoadLibraryW`), untested.
- The CI jobs on GitHub (run locally with the same presets only).

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../README|Engine docs README]] · [[../system/README|System README]]

[^engine-godottools]: [[../../Engine/Tools/GodotTools/ARCHITECTURE|GodotTools]] — Engine module · Tools
[^doc-build-build-1]: [[../build/BUILD-1|Engine BUILD-1 contract]] — RunicEngine doc
[^engine-godotbridge]: [[../../Engine/Presentation/GodotBridge/ARCHITECTURE|GodotBridge]] — Engine module · Presentation
[^engine-cooker]: [[../modules/Cooker|Cooker]] — Engine module · Tools
