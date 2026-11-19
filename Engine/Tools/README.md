# Tools

Code that never ships to players: the Runic tools inside the RunicEditor (GodotTools) and the Cooker.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [GodotTools](GodotTools/README.md) | `Runic::GodotTools` | GDT-1 (slice 0) | never shipped |

## Planned modules

No folder yet: a module's folder appears with its first package, which takes the module's READMEs from the `layout`
branch (`git checkout origin/layout -- Engine/Tools/<Module>/README.md`; decided 2026-11-14).

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| Cooker | `Runic::Cooker` | COOK-1 (slice 9) | tool (never shipped to players) |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/Tools/<Module>/` with the same shape; see [the Engine README](../README.md).
