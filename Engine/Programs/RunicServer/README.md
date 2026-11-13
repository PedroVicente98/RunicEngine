# RunicServer

The Engine's own game-agnostic server: the Engine's server layers as they arrive (Network, Simulation, Diagnostics) on the fixed 60 Hz loop, with neutral `engine:` test content and no Platform layers. Engine slices show their server results here; RunicGame's GameServer is the same composition plus game modules and the Platform's layers. Never shipped.

- **Status:** Empty until APP-1 (slice 1).

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
