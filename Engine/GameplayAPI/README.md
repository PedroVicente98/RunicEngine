# GameplayAPI

Generic gameplay mechanisms that the Game fills with its own rules: attributes and pools, movement modes, restrictions, abilities, hit detection and hit resolution, statuses, sequences and quests. Headless. The umbrella target `Runic::GameplayAPI` links the four modules so the Game links one name; each module keeps its own boundary.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Gameplay](Gameplay/README.md) | `Runic::Gameplay` | GPL-1 (slice 14) | yes |
| [Combat](Combat/README.md) | `Runic::Combat` | CMB-1 (slice 16) | yes |
| [GameplayFlow](GameplayFlow/README.md) | `Runic::GameplayFlow` | FLOW-1 (slice 29) | yes |
| [Quest](Quest/README.md) | `Runic::Quest` | QST-1 (slice 30) | yes |

`Runic::GameplayAPI` is this group's umbrella target: an INTERFACE target that only links Gameplay, Combat, GameplayFlow and Quest (`runic_add_group`).

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/GameplayAPI/<Module>/` with the same shape; see [the Engine README](../README.md).
