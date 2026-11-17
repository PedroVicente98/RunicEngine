# Simulation core

The authoritative simulation: Flecs behind the Runic API, 64-bit world space and the spatial partition, Jolt physics in double precision, and the multi-rate SimulationRuntime that owns the tick. Every module here is headless.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [ECS](ECS/README.md) | `Runic::ECS` (and the opt-in `Runic::ECSNative`) | ECS-1 (slice 4) | yes |
| [Simulation](Simulation/README.md) | `Runic::Simulation` | SIM-1 (slice 3) | yes |
| [World](World/README.md) | `Runic::World` | WORLD-1 (slice 4) | yes |

## Planned modules

No folder yet: a module's folder appears with its first package, which takes the module's READMEs from the `layout`
branch (`git checkout origin/layout -- Engine/SimulationCore/<Module>/README.md`; decided 2026-11-14).

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| Physics | `Runic::Physics` | PHYS-1 (slice 7) | yes |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/SimulationCore/<Module>/` with the same shape; see [the Engine README](../README.md).
