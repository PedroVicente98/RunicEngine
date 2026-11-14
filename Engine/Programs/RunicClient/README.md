# RunicClient

The Engine's own game-agnostic client: the Engine's client layers with the GodotLayer on top, with neutral `engine:` test content. Engine slices show their client results here (debug drawing, moving entities, the capsule on terrain); RunicGame's GameClient is the same composition plus game modules. Never shipped.

- **Status:** Has code since GDB-1 (slice 1): the GodotLayer on top of an input-trace layer, on the neutral test project in `Project/` (one Button); `--input-check` pushes scripted clicks and a key (the CTests `godot.runic_client_headless` and `godot.runic_client_input`).

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
