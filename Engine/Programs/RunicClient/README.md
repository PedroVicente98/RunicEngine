# RunicClient

The Engine's own game-agnostic client: the Engine's client layers with the GodotLayer on top, with neutral `engine:` test content. Engine slices show their client results here (debug drawing, moving entities, the capsule on terrain); RunicGame's GameClient is the same composition plus game modules. Never shipped.

- **Status:** Empty until GDB-1 (slice 1).

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
