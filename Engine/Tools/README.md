# Tools

Code that never ships to players: the Runic tools inside the RunicEditor (GodotTools) and the Cooker.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [GodotTools](GodotTools/README.md) | `Runic::GodotTools` | GDT-1 (slice 0) | never shipped |
| [Cooker](Cooker/README.md) | `Runic::Cooker` | COOK-1 (slice 9) | tool (never shipped to players) |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/Tools/<Module>/` with the same shape; see [the Engine README](../README.md).
