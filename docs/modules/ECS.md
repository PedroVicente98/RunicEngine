---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/simulation-core
  - headless/yes
  - slice/4
aliases:
  - "Runic::ECS"
---

# ECS

Contract seed for `Runic::ECS`: what a Claude Code session needs to implement the ECS work package. When `Engine/SimulationCore/ECS/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Simulation core |
| Directory | `Engine/SimulationCore/ECS/` |
| CMake target | `Runic::ECS` |
| Namespace | `Runic::ECS` |
| Headless | yes |
| Status | Not started |

## Purpose

ECS integrates Flecs and fixes the conventions every module follows when it stores state in the simulation's Flecs world. Geometry answers spatial questions; Flecs answers what entities are, how they relate and which rules apply (ARCH[^arch] § Simulation core: Flecs and Jolt).

## Owns

- The Flecs world wrapper, component registration, query helpers and conventions (ECS-1).
- The Flecs rules below, which every module that registers components or systems must follow.

| Concern | Owner | Role |
| --- | --- | --- |
| Gameplay state and rules | Flecs | Components, relationships, systems |
| Geometry and collision | Jolt + Runic spatial index | Answers "what intersects?" |
| Durable state | Go services + PostgreSQL (outside the Engine) | Flecs keeps only what the active simulation needs |
| Client visuals and prediction | Client ECS + renderer | Non-authoritative mirror |

### Flecs rules

- Hot state is dense and owned by the entity: Transform, Velocity, Health, resources, combat state, cooldowns.
- Definitions are prefabs (IsA); hot mutable state is never inherited.
- Relationships hold lasting facts: OwnedBy, PlantedOn, PassengerOf, CarriedBy, CargoOf, MemberOf, IsA. High-cardinality targets (any player) use DontFragment after a benchmark.
- Per-tick geometry results (overlaps, line of sight) are never relationships; they are scratch lists.
- Recurring work uses cached queries; per-owner lookups use one variable-bound query, never one query per player.
- States that flip often (AIActive, Replicated, CombatEnabled) are toggles, not add and remove.
- Many dynamic buffs or quest states live in compact containers, not one tag each.
- Observers handle lifecycle bookkeeping only; hot combat facts use event buffers.
- Only the simulation's own phases mutate the Flecs world.
- `GlobalEntityId`, stable across simulations, is separate from local Flecs IDs.

## Does not own

- The world instance and when it is mutated: the SimulationRuntime owns the Flecs world; feature modules register into it (SIM-1[^engine-simulation], SIM-2).
- `GlobalEntityId` (Core) and its mapping to local entities (DistributedSimulation[^engine-distributedsimulation], DSIM-1).
- Spatial queries (World), physics state (Physics[^engine-physics]), fact delivery (Events).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core[^engine-core] |
| Third-party | Flecs (C++ API): PRIVATE, except that ECS re-exports it only to the Engine modules that register systems (the approved Flecs world-handle exception) |

### Must never depend on

- Any Engine module other than Core.
- Flecs types in `Public/`, except the approved exception: the Flecs world handle for modules that register systems into the simulation (for example `AIModule::Register(flecs::world&)`). ECS-1 must prove that this is the only way Flecs reaches a public header.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

Sketch, fixed in the contract session: `EcsWorld.hpp` (wrapper and the world-handle exception), `ComponentRegistration.hpp`, `QueryHelpers.hpp`.

## Threading and tick rules

- Only the simulation's own phases mutate the Flecs world. Never mutate Flecs from an I/O callback.
- Flecs multi-threaded systems run only in phases that allow them.
- Structural changes (spawn, despawn) are batched at the barrier.
- Local Flecs IDs never leave the process; anything that crosses a process uses `GlobalEntityId`.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| ECS-1 | Flecs world wrapper, component registration, query helpers, conventions | Entities are created, queried and destroyed; Flecs types appear in public headers only through the approved world-registration exception | CORE-1[^engine-core] | 4 |

## Acceptance tests

The Done when column is the acceptance list. Slice 4 also requires entities with 64-bit positions (WORLD-1[^engine-world]) to be created and queried.

## Open items

- The approved exception is recorded here and in every module that uses it (ARCH[^arch] § Modules, work packages and the implementation contract).
- Decided in ARCH revision 369: ECS re-exports Flecs only to the Engine modules that register systems (for example AI); other modules reach the world through Runic types. The ECS-1 contract session fixes the CMake shape of the re-export and proves no other module sees Flecs.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Core|Core]]
- **Depended on by:** [[../modules/World|World]] · [[../modules/Simulation|Simulation]]
- **Packages that need it:** [[../modules/Client|Client]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-distributedsimulation]: [[../modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-physics]: [[../modules/Physics|Physics]] — Engine module · Simulation core
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
[^engine-world]: [[../modules/World|World]] — Engine module · Simulation core
