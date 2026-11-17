# Simulation core

The authoritative simulation: Flecs behind the Runic API, 64-bit world space and the spatial partition, Jolt physics in double precision, and the multi-rate SimulationRuntime that owns the tick. Every module here is headless.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Simulation](Simulation/README.md) | `Runic::Simulation` | SIM-1 (slice 3) | yes |

## Planned modules

No folder yet: a module's folder appears with its first package, which takes the module's READMEs from the `layout`
branch (`git checkout origin/layout -- Engine/SimulationCore/<Module>/README.md`; decided 2026-11-14).

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| ECS | `Runic::ECS` | ECS-1 (slice 4) | yes |
| World | `Runic::World` | WORLD-1 (slice 4) | yes |
| Physics | `Runic::Physics` | PHYS-1 (slice 7) | yes |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/SimulationCore/<Module>/` with the same shape; see [the Engine README](../README.md).
