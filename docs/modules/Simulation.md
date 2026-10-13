---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/simulation-core
  - headless/yes
  - slice/3
aliases:
  - "Runic::Simulation"
---

# Simulation

Contract seed for `Runic::Simulation`: what a Claude Code session needs to implement the Simulation work packages. When `Engine/SimulationCore/Simulation/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Simulation core |
| Directory | `Engine/SimulationCore/Simulation/` |
| CMake target | `Runic::Simulation` |
| Namespace | `Runic::Simulation` |
| Headless | yes |
| Interface target | `Runic::SimulationPort`: header-only, Core types only (SIM-5) |
| Status | Not started |

## Purpose

The SimulationRuntime advances the authoritative world in fixed steps on a 60 Hz master clock with 30, 20 and 10 Hz domains, and runs every step's work across a pool of worker threads (ARCH[^arch] § Simulation core: clock and threads). It owns the scheduler, phases, barriers, commands, the event flush, worker integration, the client prediction subset, the service port and the Simulation layer. Feature modules and RunicGame register their systems into it; it stays reusable.

## Owns

- The SimulationRuntime and the Flecs world it owns; feature modules register components and systems into it.
- The multi-rate scheduler, `TickContext` and domain-tagged durations (`DomainTickCount`), plus per-domain metrics (SIM-1).
- The Simulation layer that the GameServer stack composes (SIM-1).
- Phases, commands and the command queue, deferred mutation, and the event flush at barriers (SIM-2).
- Phases on the worker pool, per-worker buffers and the deterministic merge; the physics step runs through PHYS-4[^engine-physics] (SIM-3).
- The client prediction subset: selected systems run on the client with RunicGame::Shared (SIM-4; decided in OQ-15[^oq-15]).
- The service port: an outbox of durable operations with stable operation IDs and an inbox of accepted results drained at safe ticks (SIM-5). It is the Engine's only door to durable services. It ships as the header-only `Runic::SimulationPort` target, whose headers include only Core types, so RunicPlatform can implement it without configuring Flecs, Jolt or any gameplay module. The Platform ServicesBridgeLayer[^platform-serverbindings] implements it: gRPC calls in PBND-1, NATS facts and subscriptions (for example world directives[^platform-coreservices]) in PBND-4.
- The replication registry: modules register replicated components and world-state values (schema, quantization profile, relevance), and Network encodes only what is registered (SIM-6).
- Tick-domain timings recorded in the Core metrics registry (SIM-1); the Platform exporter (PBND-3) publishes them.

| Domain | Interval | Typical work |
| --- | --- | --- |
| Hz60 | 16.7 ms | Master clock; no mandatory gameplay work |
| Hz30 | 33.3 ms | Commands, movement, CharacterVirtual, skill timelines, projectiles, AoE queries, ships, the Jolt step, replication extraction |
| Hz20 | 50 ms | AI decisions and perception |
| Hz10 | 100 ms | Aggro wake scans, luminosity updates, weather and tide state, quest and loot consumers, spawning, world maintenance |

Realtime 30 Hz phase, in order:

1. Apply queued player and AI commands; start queued skills whose recovery has ended.
2. Advance movement and skill timelines; advance projectiles.
3. Commit structural physics changes in batches (bodies created and removed).
4. Apply kinematic targets, buoyancy forces and water-movement forces; step Jolt once.
5. Update CharacterVirtuals against the new world; write positions to Flecs and spatial membership; record position history for skills that use capped rewind.
6. Run combat, projectile and AoE queries; pass hit candidates to the resolver.
7. Commit events and build replication state.

## Does not own

- The systems that run in the phases: Gameplay, Combat[^engine-combat], AI, Water[^engine-water] and the other feature modules (and RunicGame) register them.
- Physics[^engine-physics] internals (Physics), wire encoding and replication (Network), authority and transfer (DistributedSimulation[^engine-distributedsimulation]).
- Durable services: gRPC, NATS and databases are reached only through the service port, implemented by the Platform. Durable state that the simulation needs back (for example a character, quest progress or durable water state) arrives as accepted results in the inbox.
- Executable composition (Application and RunicGame).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core[^engine-core], Application[^engine-application], ECS[^engine-ecs], Events[^engine-events], World[^engine-world], Physics[^engine-physics], Content[^engine-content] |
| Third-party | — (Flecs reaches it through ECS) |

### Must never depend on

- Feature modules (Gameplay, Combat[^engine-combat], GameplayFlow[^engine-gameplayflow], Quest, AI, Water[^engine-water], Environment[^engine-environment], Perception[^engine-perception], Navigation[^engine-navigation], Network, DistributedSimulation[^engine-distributedsimulation]) or Game code. They register into the Simulation.
- gRPC, NATS, databases, Agones or Kubernetes. The service port is an interface; the Platform implements it.
- Anything but Core types in the `Runic::SimulationPort` headers: that target links only `Runic::Core`.
- The modules that register replicated data: the replication registry (SIM-6) is how they reach Network, never the reverse.
- RunicPlatform or RunicGame code.

## Public API sketch

Sketch, fixed in the contract session: `SimulationRuntime.hpp`, `TickContext.hpp`, `TickDomain.hpp` (`DomainTickCount`), `SimulationPhase.hpp`, `CommandQueue.hpp`, `SimulationLayer.hpp`, `PredictionSubset.hpp`, `ServicePort.hpp`.

`ServicePort.hpp` belongs to the header-only `Runic::SimulationPort` target and is consumed by RunicPlatform (PBND-1[^platform-serverbindings], PBND-4), so it includes only Core types (IDs, time, Result). The SIM-5 contract session fixes its header names and where they sit inside the module folder. The service port contract:

- Outgoing: durable operations, each with a stable operation ID.
- Incoming: accepted results, queued by the Platform's completion threads and drained by the simulation at safe ticks.
- A result delivered twice is applied once. No I/O callback touches Flecs.

## Threading and tick rules

- Wall-clock time only decides how many 60 Hz steps are owed; the simulation advances in exact fixed steps.
- Rates decide when a system is due; explicit phases decide order: 20/10 Hz producers, then the 30 Hz realtime phase, then 20/10 Hz consumers, then the Hz60 hook.
- Systems receive a `TickContext` (domain, master tick, domain tick, dt, simulation time); nothing reads the wall clock.
- Durations are times, or tick counts tagged with their domain (`DomainTickCount`). An untagged "9 ticks" is a bug.
- Falling behind is measured and alarmed, never silently skipped.
- One SimulationRuntime per process, with a worker pool sized from the pod's CPU allotment. Early slices run one worker; the code is phase-safe from day one.
- Phases stay in a fixed order; work inside a phase is split across workers, and no phase starts until every worker finishes the current one (a barrier).
- Inside a parallel phase, each worker writes only to the entities it owns. Effects on other entities go to per-worker buffers merged at the barrier in a fixed order.
- Jolt runs on the same pool through Physics[^engine-physics]; Flecs multi-threaded systems run only in phases that allow them.
- Network I/O and service clients run on their own threads and only enqueue. Nothing blocks the tick.
- The worker count can change at a safe point between ticks.

| Work | Parallel? | Why |
| --- | --- | --- |
| Movement and CharacterVirtual updates | Yes, by entity | Each character is independent |
| AI brains and perception | Yes, by NPC | Each brain reads state and writes intents |
| Combat[^engine-combat] queries | Yes, by attack or projectile | Read-only geometry; results buffered |
| Hit resolution (damage, effects) | Grouped by target | Two attacks on one target must apply in a fixed order |
| Per-client replication building | Yes, by client | The largest cost in big battles, and fully independent |
| Structural changes (spawn, despawn, bodies) | No, batched at the barrier | Keeps Flecs and Jolt consistent |

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| SIM-1 | SimulationRuntime: 60 Hz master clock, 30/20/10 Hz domains, TickContext, the Simulation layer, domain metrics | Domain tick counts stay exact over a long run, with no drift | APP-1[^engine-application], CORE-2[^engine-core] | 3 |
| SIM-2 | Phases, commands and command queue, deferred mutation, event flush at barriers | Commands apply in phase order; facts are visible to the next phase | SIM-1, ECS-1[^engine-ecs], EVT-1[^engine-events] | 5 |
| SIM-3 | Phases on the worker pool, per-worker buffers, deterministic merge; the physics step runs through PHYS-4[^engine-physics] | The same scene with one worker and with many gives identical results | SIM-2, PHYS-4 | 10 |
| SIM-4 | Client prediction subset: selected systems run on the client with RunicGame::Shared | The same inputs give the same result on client and server | SIM-3 | 14 |
| SIM-5 | Service port: an outbox of durable operations with stable operation IDs and an inbox of accepted results drained at safe ticks; the Engine interface the Platform services bridge[^platform-serverbindings] implements, shipped as the header-only Runic::SimulationPort target (Core types only) | A fake port that delivers a result twice is applied once; no I/O callback touches Flecs | SIM-2, EVT-1 | 27 |
| SIM-6 | Replication registry: modules register replicated components and world-state values (schema, quantization profile, relevance); Network encodes only what is registered | A test component and a world-state value replicate without Network depending on the modules that own them | SIM-2 | 13 |

## Acceptance tests

The Done when column is the acceptance list. Slice 3 also runs the scheduler at 60/30/20/10 Hz on the server loop with metrics; slice 10 shows identical results with several workers. SIM-4 tests use an Engine test system set, never RunicGame code. SIM-5 also proves that `Runic::SimulationPort` compiles in a target that links only `Runic::Core`[^engine-core].

## Open items

- [OQ-15](../system/11-open-questions.md#oq-15) (decided): the SIM-4 prediction subset runs RunicGame::Shared on the client.
- [OQ-16](../system/11-open-questions.md#oq-16) (decided): the Platform Go SDK[^platform-gosdk] owns transactions and the idempotency store behind the service port.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Core|Core]] · [[../modules/Application|Application]] · [[../modules/ECS|ECS]] · [[../modules/Events|Events]] · [[../modules/World|World]] · [[../modules/Physics|Physics]] · [[../modules/Content|Content]]
- **Depended on by:** [[../modules/Gameplay|Gameplay]] · [[../modules/Navigation|Navigation]] · [[../modules/Environment|Environment]] · [[../modules/Water|Water]] · [[../modules/Network|Network]] · [[../modules/DistributedSimulation|DistributedSimulation]] · [[../modules/Client|Client]] · [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]]
- **Packages that need it:** [[../modules/Combat|Combat]] · [[../modules/Quest|Quest]] · [[../modules/AI|AI]] · [[../modules/Editor|Editor]] · [[../../../RunicGame/docs/game-packages|Game work packages]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-physics]: [[../modules/Physics|Physics]] — Engine module · Simulation core
[^oq-15]: [[../system/11-open-questions#OQ-15 — What the client runs for prediction (decided 2026-10-09)|OQ-15]] — open question or decision
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-water]: [[../modules/Water|Water]] — Engine module · World systems
[^engine-distributedsimulation]: [[../modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
[^engine-application]: [[../modules/Application|Application]] — Engine module · Foundation
[^engine-ecs]: [[../modules/ECS|ECS]] — Engine module · Simulation core
[^engine-events]: [[../modules/Events|Events]] — Engine module · Foundation
[^engine-world]: [[../modules/World|World]] — Engine module · Simulation core
[^engine-content]: [[../modules/Content|Content]] — Engine module · Foundation
[^engine-gameplayflow]: [[../modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-environment]: [[../modules/Environment|Environment]] — Engine module · World systems
[^engine-perception]: [[../modules/Perception|Perception]] — Engine module · World systems
[^engine-navigation]: [[../modules/Navigation|Navigation]] — Engine module · World systems
[^platform-gosdk]: [[../../../RunicPlatform/docs/modules/GoSDK|GoSDK]] — Platform module · Go
