# RunicClient

The Engine's own game-agnostic client: the Engine's client layers with the GodotLayer on top, with neutral `engine:` test content. Engine slices show their client results here (debug drawing, moving entities, the capsule on terrain); RunicGame's GameClient is the same composition plus game modules. Never shipped.

- **Status:** Has code since GDB-1 (slice 1): the GodotLayer on top of an input-trace layer, on the neutral test project in `Project/` (one Button); `--input-check` pushes scripted clicks and a key (the CTests `godot.runic_client_headless` and `godot.runic_client_input`).
- **Slice 2 (GDB-2):** `DebugDemo.cpp`, a layer `engine:debug_demo` between the two, writes every frame through a `RenderFrameExchange` the application owns: a grid, a capsule walking a circle with its facing ray and melee arc, a sphere, boxes, a cylinder, a cone, an arc volume, two overlapping fills with opacity, labels, the world axes and a frame counter on top, and a camera orbiting the scene. The GodotLayer shows it, with the development overlay at the top-right (a checkbox per debug layer and what the frame drew). CLI-1 (slice 5) replaces the demo with the client's own extraction.

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
