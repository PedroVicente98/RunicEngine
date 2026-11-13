# RunicBot

The client runtime without presentation (Runic::Client's bot runtime): many bot sessions in one process against a server, for scale tests. Links no Godot. Never shipped.

- **Status:** Empty until CLI-9 (slice 38).

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
