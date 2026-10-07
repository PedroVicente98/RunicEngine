# NetworkAdapter

> **Contract seed.** When `Engine/NetworkAdapter/` is created, copy this file to
> `Engine/NetworkAdapter/ARCHITECTURE.md`; from then on that file is
> authoritative. Purpose, Owns, Does not own, Allowed dependencies, and Primary
> consumers come from Appendix J §J.4.23. Other sections are derived and are
> proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/NetworkAdapter/` |
| CMake target | `RunicNetworkAdapter` → `Runic::NetworkAdapter` |
| Namespace | `Runic::NetworkAdapter` |
| Roadmap phase | 4 — first multiplayer (interest filtering in 8, distributed bridge in 13) |
| Headless | yes |
| Status | Not started |

## Purpose

A small, intentionally narrow seam exposing simulation-safe ingress/egress and
state extraction/application to Platform-owned networking. Its value is
isolation, not code volume. [J.1.1]

## Owns

- `SimulationIO`.
- Network ingress and egress queues.
- Replication extraction/application hooks.
- Reconciliation hooks.
- The distributed-state bridge.

## Does not own

Sockets, transport reliability, endpoint discovery, handoff topology, gRPC/NATS,
databases.

## Allowed dependencies

- Core, Events, Simulation, DistributedSimulation.
- Platform contract/binding package at the integration boundary
  ([OQ-1](../system/11-open-questions.md#oq-1)).

## Forbidden dependencies

Any socket/transport/serialization implementation, Agones/Kubernetes, database,
Renderer, gameplay modules' internals, RunicGame.

## Primary consumers

Platform Realtime, server/client runtime.

## Public API (conceptual)

```text
NetworkAdapter/
├── Public/
│   └── NetworkAdapter.hpp   # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

- Ingress queue: written by network threads, drained by the simulation at the
  tick's ingress point.
- Egress queue: filled at the end of a tick (state deltas, facts that leave the
  process, ghost/transfer/control messages), drained by network threads.
- The simulation never blocks on a slow consumer; overflow follows an explicit,
  diagnosable policy.

## Data ownership (derived)

Owns queue buffers only. Messages identify entities by `GlobalEntityId`; local
Flecs IDs never appear. [J.8.2]

## Integration points (derived)

- Simulation command queue (ingress) and fact/state extraction (egress).
- Client prediction/reconciliation hooks (client side).
- DistributedSimulation ghost/transfer/foreign-combat messages.
- Platform Realtime (transport) and Contracts (schemas) on the other side.

## Acceptance tests (Stage A, derived — phase 4)

Using an in-memory loopback instead of a real transport:

- [ ] Client commands enter through ingress and are applied only at the tick
      boundary.
- [ ] State extraction produces snapshots/deltas that a second (client) instance
      applies to converge.
- [ ] Spawn/despawn replicate.
- [ ] No local entity ID can appear in egress messages.
- [ ] A stalled consumer never blocks the simulation tick.
- [ ] Builds headless; links no transport library.

## Future extensions

Phase 8 interest filtering hooks; phase 13 distributed-state bridge.

## Open questions

- [OQ-1](../system/11-open-questions.md#oq-1) — binding direction with Platform.
- Snapshot/replication format ([OQ-12](../system/11-open-questions.md#oq-12)).
