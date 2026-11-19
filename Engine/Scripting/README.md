# Scripting

Content logic written in Lua: what belongs to one named piece of content and runs when something happens (quest
conditions and actions, sequence steps, world-event phases, one-of-a-kind interactables). Systemic logic stays in
C++ systems configured by data. Headless.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Scripting](Scripting/README.md) | `Runic::Scripting` | SCR-1 (slice 37a) | yes |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/Scripting/<Module>/` with the same shape; see [the Engine README](../README.md).
