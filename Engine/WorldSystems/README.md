# World systems

Systems that make the world react: light and perception, navigation, AI brains, the environment (time of day, wind, currents, tides, waves, weather, world routines) and water. Every module here is headless.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Perception](Perception/README.md) | `Runic::Perception` | PRC-1 (slice 25) | yes |
| [Navigation](Navigation/README.md) | `Runic::Navigation` | NAV-1 (slice 31) | yes |
| [AI](AI/README.md) | `Runic::AI` | AI-1 (slice 32) | yes |
| [Environment](Environment/README.md) | `Runic::Environment` | ENV-1 (slice 25) | yes |
| [Water](Water/README.md) | `Runic::Water` | WATER-1 (slice 40) | yes |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/WorldSystems/<Module>/` with the same shape; see [the Engine README](../README.md).
