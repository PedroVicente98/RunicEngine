# Presentation

The client side: the client runtime and render extraction, Godot as renderer, window, input, UI and audio backend (GodotBridge), animation and audio. No headless target may reach these modules.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Client](Client/README.md) | `Runic::Client` | CLI-1 (slice 5) | no (presentation) |
| [GodotBridge](GodotBridge/README.md) | `Runic::GodotBridge` | GDB-1 (slice 1) | no (presentation) |
| [Animation](Animation/README.md) | `Runic::Animation` | ANIM-1 (slice 23) | no (presentation) |
| [Audio](Audio/README.md) | `Runic::Audio` | AUD-1 (slice 24) | no (presentation) |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/Presentation/<Module>/` with the same shape; see [the Engine README](../README.md).
