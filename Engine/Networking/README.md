# Networking

Replication between server and clients and between simulations: the transport interface, all wire encoding, interest, prediction support, authority, ghosts and handoff. Every module here is headless.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Network](Network/README.md) | `Runic::Network` | NET-1 (slice 12) | yes |
| [DistributedSimulation](DistributedSimulation/README.md) | `Runic::DistributedSimulation` | DSIM-1 (slice 36) | yes |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/Networking/<Module>/` with the same shape; see [the Engine README](../README.md).
