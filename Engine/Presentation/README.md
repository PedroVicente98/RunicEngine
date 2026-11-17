# Presentation

The client side: the client runtime and render extraction, Godot as renderer, window, input, UI and audio backend (GodotBridge), animation and audio. No headless target may reach these modules.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Client](Client/README.md) | `Runic::Client` | CLI-1 (slice 5) | no (presentation) |
| [GodotBridge](GodotBridge/README.md) | `Runic::GodotBridge` | GDB-1 (slice 1) | no (presentation) |

## Planned modules

No folder yet: a module's folder appears with its first package, which takes the module's READMEs from the `layout`
branch (`git checkout origin/layout -- Engine/Presentation/<Module>/README.md`; decided 2026-11-14).

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| Animation | `Runic::Animation` | ANIM-1 (slice 23) | no (presentation) |
| Audio | `Runic::Audio` | AUD-1 (slice 24) | no (presentation) |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/Presentation/<Module>/` with the same shape; see [the Engine README](../README.md).
