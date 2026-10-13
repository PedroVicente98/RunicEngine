---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/foundation
  - headless/yes
  - slice/4
aliases:
  - "Runic::Events"
---

# Events

Contract seed for `Runic::Events`: what a Claude Code session needs to implement the Events work package. When `Engine/Foundation/Events/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Foundation |
| Directory | `Engine/Foundation/Events/` |
| CMake target | `Runic::Events` |
| Namespace | `Runic::Events` |
| Headless | yes |
| Status | Not started |

## Purpose

Events carries simulation facts (for example HitEvent, DamageEvent, EntityKilled, RegionChanged) in typed, tick-safe buffers that every consumer receives. It is one of three event families (ARCH[^arch] § Repositories, build targets and processes):

| Family | Examples | Delivery | Owner |
| --- | --- | --- | --- |
| Application events | Key press, resize, UI focus, connection state | LayerStack: top layer first, can be marked handled | Application |
| Simulation facts | HitEvent, DamageEvent, EntityKilled, RegionChanged | Tick-safe buffers; every consumer receives them | **Events** |
| Distributed messages | Quest facts, inventory operations, world-event directives | Transport, gRPC or NATS, with explicit reliability and idempotency | Realtime messages: Network encoding over the Platform transport. Durable operations and facts: the Simulation's service port, implemented by the Platform services bridge[^platform-serverbindings] |

## Owns

- Typed tick-safe event buffers and readers (EVT-1).
- Multi-consumer delivery: a fact is never "handled away"; every reader sees it.
- Stable event IDs: `SimulationId` plus a sequence number (EVT-1; decided in OQ-2[^oq-2]). Services deduplicate durable facts by these IDs, so a replayed fact changes nothing.

## Does not own

- When facts become visible: the Simulation flushes events at phase barriers (SIM-2[^engine-simulation]).
- Concrete fact types. Each feature module defines its own (for example `CombatEvents.hpp` in Combat[^engine-combat]).
- Application events (Application) and distributed messages (Network encodes realtime messages; durable facts leave through the Simulation's service port).
- Lifecycle bookkeeping through Flecs observers (ECS[^engine-ecs] conventions).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core[^engine-core] |
| Third-party | — |

### Must never depend on

- Any Engine module other than Core (no ECS[^engine-ecs], no Simulation).
- Flecs or any other library.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

Sketch, fixed in the contract session: `EventBuffer.hpp` (typed buffer, per-worker writers), `EventReader.hpp` (independent read cursor per consumer), `EventId.hpp` (`SimulationId` plus sequence).

## Threading and tick rules

- Inside a parallel phase, workers write facts into per-worker buffers that are merged at the barrier in a fixed order, so results never depend on thread timing (ARCH[^arch] § Simulation core: clock and threads).
- Facts written in one phase are visible to the next phase after the flush (SIM-2[^engine-simulation]).
- Hot facts (combat, damage, death) travel in event buffers, never through Flecs observers (ARCH § Simulation core: Flecs and Jolt).
- A fact carries its stable ID from the moment it is emitted; consumers never re-number it.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| EVT-1 | Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs (SimulationId plus sequence) | Two consumers read the same fact; IDs stay stable on replay | CORE-1[^engine-core] | 4 |

## Acceptance tests

The Done when column is the acceptance list. Slice 4 also requires that one fact reaches two consumers.

## Open items

- [OQ-2](../system/11-open-questions.md#oq-2) (decided): stable event IDs are `SimulationId` plus sequence; the ID types live in Core.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Core|Core]]
- **Depended on by:** [[../modules/Simulation|Simulation]] · [[../modules/DistributedSimulation|DistributedSimulation]]
- **Packages that need it:** [[../modules/Gameplay|Gameplay]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^oq-2]: [[../system/11-open-questions#OQ-2 — Where shared ID and time types live (decided 2026-10-09)|OQ-2]] — open question or decision
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-ecs]: [[../modules/ECS|ECS]] — Engine module · Simulation core
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
