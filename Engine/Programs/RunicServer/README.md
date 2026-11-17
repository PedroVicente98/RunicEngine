# RunicServer

The Engine's own game-agnostic server: the Engine's server layers as they arrive (Network, Simulation, Diagnostics) on the fixed 60 Hz loop, with neutral `engine:` test content and no Platform layers. Engine slices show their server results here; RunicGame's GameServer is the same composition plus game modules and the Platform's layers. Never shipped.

- **Status:** Has code since APP-1 (slice 1): the fixed 60 Hz loop on the Engine's entry point, and `--layer-demo` (neutral probe layers that show a push, a replace and a pop at the frames' safe points; the CTest `programs.runic_server`). SIM-1 (slice 3) puts the Simulation layer on it: the settings come from the process configuration (`--config <file>`, `RUNIC_CFG_*`, `--set key=value`; a problem stops the server with exit code 1, the CTest `programs.runic_server_bad_setting`), `--clock-demo` adds a neutral probe system to every slot of the scheduler, and at a clean stop the server reports the ticks and the time of every clock domain from the metrics registry (the CTest `programs.runic_server_clock`).

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
