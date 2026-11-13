# RunicEditor

The Godot editor run through LibGodot (editor target) with the Runic tools compiled in (GodotTools) and, from slice 9, the Cooker; it opens with no project (Godot's project manager: New and Open) or opens a project folder such as RunicGame, and later loads that project's editor library. Never shipped.

- **Status:** Has code since slice 0 (GDT-1).

A program is a composition root, not a module: no catalog row and no `Public/`. From APP-1 (slice 1) on it defines
`CreateApplication` for the Engine's entry point and stacks layers that modules supply. See
[Engine/Programs](../README.md).
