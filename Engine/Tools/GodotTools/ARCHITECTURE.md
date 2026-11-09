# Runic::GodotTools — module contract

Group: Tools · Headless: **never shipped** · Target: `Runic::GodotTools` (`runic_godottools`) · Packages: GDT-1 to GDT-16

Source: RunicDocs › Architecture › Godot integration (Engine), Tools (Engine), Repositories, build targets and processes
(Engine modules), World authoring and cooking. Status: **GDT-1 (spike) done locally**, see
[docs/build/GDT-1.md](../../../docs/build/GDT-1.md).

## Purpose

The Runic tools inside the RunicEditor (the Godot editor run through LibGodot): marker nodes, schema inspectors and
Runic file formats in the editor, world and region tools, docks (world, ECS, physics, resolver, AI editor on
GraphEdit, live attach), the project template and assistants, Run orchestration, and the export plugin that calls
the Cooker. Editor code never ships to players.

## Public API

| Header | Types | Since |
| --- | --- | --- |
| `GodotToolsModule.hpp` | `RegisterEditorTools(GodotRuntime&)`: registers the tool classes through the runtime's initialization hooks; the lines the test plugin prints | GDT-1 |

Approved exception: godot-cpp types may appear in this module's public headers, so the Game's editor library can
extend marker nodes and docks (the fourth public-header exception). GDT-1's header does not need it.

## Owns

- The classes registered at Godot's editor initialization level, which the client never reaches. GDT-1: the test
  plugin `RunicSpikePlugin` (always on, no `plugin.cfg`): it reports that it loaded and whether GDScript exists,
  adds the **Runic** toolbar button and a Project › Tools item, and removes them on exit.

## Does not own

- The RunicEditor executable (`Engine/Programs/RunicEditor/`) and the LibGodot instance (Runic::GodotBridge).
- Building libgodot (BUILD-1). Cooking (Runic::Cooker). Game marker nodes and docks (RunicGame::Editor).

## Allowed dependencies

Catalog row (ceiling): most modules and Runic::Cooker; third parties libgodot (editor build) and godot-cpp.
GDT-1 links Runic::GodotBridge (public) and godot-cpp (private).

## Forbidden dependencies

Any shipped executable linking this module (GameClient → GodotTools is forbidden); domain modules depending on it
(the AI docks depend on AI contracts, never the reverse); Platform and Game targets.

## Threading and tick model

Godot's main thread only: plugins run inside the editor's frames (`GodotRuntime::Iterate`).

## Data ownership

Godot owns the plugin node; its children (the About dialog) are freed with it. The toolbar button is created and
deleted by the plugin.

## Integration points

`RegisterEditorTools` adds hooks to the `GodotRuntime` before `Create()`; the registration runs at
`GodotInitLevel::Editor`.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| GDT-1 | Spike: the RunicEditor executable on libgodot (editor target) opens the Godot editor with a test plugin; the editor target builds and opens without GDScript | The editor opens, loads the plugin and exits cleanly in CI; the spike's findings decide the fallback | Done locally (`godot.editor_spike`); findings in docs/build/GDT-1.md |
| GDT-2 | Project model: runic.project.json, the runic-mmo template, New and Open, the project's editor library, IDE bridge, first dock | See RunicDocs › Modules, work packages | Not started (needs APP-1, SIM-2) |
| GDT-3 to GDT-16 | Physics inspector, Cooker integration, docks, schema inspectors, region tools, Run, AI editor, live attach, water authoring, GDScript locks | See RunicDocs | Not started |

## Future extensions

The test plugin is the spike's proof and is replaced by the real plugin in GDT-2. A branding patch (separate editor
settings folder) and the editor feature profile (hide the Script tab, physics nodes, plugin creation) are tool-phase
decisions.
