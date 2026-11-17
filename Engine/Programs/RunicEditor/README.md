# RunicEditor

The Godot editor run through LibGodot (editor target) with the Runic tools compiled in (GodotTools) and, from slice 9, the Cooker; it opens with no project (Godot's project manager: New and Open) or opens a project folder such as RunicGame, and later loads that project's editor library. Never shipped.

- **Status:** Has code since slice 0 (GDT-1); on the Engine's entry point since GDB-1 (slice 1): one GodotLayer (editor library, no input forwarding) with the GodotTools hooks. Since GDT-2 (slice 6): a Simulation layer below it with the editor's local world (`LocalWorld.cpp`: a region and 12 drifting markers, `engine:` names), which the Runic World dock inspects; the Runic editor plugin instead of the test plugin; `--runic-new-project <folder> <game name>` makes a project from the runic-mmo template without starting Godot. CTests: `godot.editor_spike` (a plain Godot project), `godot.editor_new_project`, `godot.editor_project_library` and `godot.editor_opens_project` (New, then the editor opens the project, loads its editor library and shows the dock).

- **Test project:** `tests/godot/SpikeProject/`, a small neutral project the build copies to `<build>/godot/SpikeProject`
  (the GDT-1 spike's editor checks; since GDT-2 the Runic plugin says it is not a Runic project). It is not
  RunicGame: the Game opens its own folder (RunicGame's launch configuration "RunicEditor: open the RunicGame project").
- **Known message:** every start prints `ERROR: Index p_which = 0 is out of bounds (popup->get_item_count() = 0)`.
  It is Godot 4.7.2's Create Plugin dialog (`PluginConfigDialog`, built with Project Settings) selecting the first
  scripting language when the build has none: GDScript is off in this editor until GDT-14 (slice 21) and there is no
  C#. Harmless: the dialog checks for a missing language, and the message goes away once GDScript is on.

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
