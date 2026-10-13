# Network

Contract seed for `Runic::Network`: what a Claude Code session needs to implement the Network work packages. When `Engine/Networking/Network/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Networking |
| Directory | `Engine/Networking/Network/` |
| CMake target | `Runic::Network` |
| Namespace | `Runic::Network` |
| Headless | yes |
| Interface target | `Runic::NetworkTransport`: header-only, Core types only (NET-1) |
| Status | Not started |

## Purpose

Network is the only door between a process and the network. Clients talk directly to their simulation through the Engine's transport interface, which RunicPlatform implements on GameNetworkingSockets. Network owns that interface, the tick-safe ingress and egress queues, all realtime wire encoding, interest and priority budgets, prediction and reconciliation, parallel replication, world transitions, and the Network and Diagnostics layers (ARCH § Networking: stack, sessions and responsiveness; ARCH § Networking: replication, interest and handoff). Network replaces the v0 NetworkAdapter module.

Two rules are fixed:

- **The transport interface and the connect-ticket hook ship as the header-only `Runic::NetworkTransport` target, whose headers include only Core types.** RunicPlatform's Transport (PTR-1 to PTR-3) implements it by linking that target and `Runic::Core`, never Flecs, Jolt or a gameplay module.
- **Network encodes every realtime message, envelope included:** inputs, snapshots, deltas and simulation-link messages (ghost deltas, transfer messages). Protobuf (Platform Contracts) carries only service APIs, published facts and the connect-ticket format; it never carries per-tick traffic.

## Owns

- The transport interface, the connect-ticket hook, tick-safe ingress and egress queues, and the Network layer (NET-1).
- Snapshots: extraction, quantization, deltas against the last acknowledged state, interpolation buffers (NET-2).
- Prediction and reconciliation: input history, replay, blend or snap (NET-3). The Client's Prediction layer drives it each frame (CLI-3).
- Interest: candidates by cell, priority and per-client budgets, visibility tiers applied (NET-4).
- Simulation links: ghost deltas and transfer messages encoded with the snapshot codec over simulation-to-simulation connections (NET-5).
- Parallel replication and the degradation order (NET-6).
- Snapshots encode only what modules registered in Simulation's replication registry (SIM-6): components and world-state values with a schema, quantization profile and relevance. Network never depends on the modules that own them.
- The Diagnostics layer: network and replication metrics recorded in the Core metrics registry; the Editor attach channel over a development socket transport inside Network, streaming debug shapes and the traces modules publish in Core's debug-stream registry; compiled only into development and staging builds (NET-7).
- World transitions (NET-8): the session leaves one world's simulation after a checkpoint and joins another world's simulation with a new connect ticket. Worlds never share a border, so there is no handoff or ghost between them; portals to other maps and entries to instances use the same path, and presentation covers the load.
- The connection roles of a WorldSession: Primary (authoritative), WarmCandidate (connected and prepared for promotion), Dormant (kept with minimal traffic) and Retiring (the old authority during a grace window).

### Sessions

1. The client logs in through the Platform accounts service and receives a signed, short-lived connect ticket for its simulation.
2. The client opens a transport connection directly to the simulation and presents the ticket first.
3. The simulation checks the ticket locally through the connect-ticket hook (signature, expiry, key ID), with no service call in the hot path. The Platform implements the check (PTR-2). Tickets are test-signed with a local key from slice 12 until the accounts service (PCS-1, slice 27) issues real ones; PCS-1 also issues the `GlobalPlayerId`.
4. The WorldSession survives handoffs: one authoritative connection plus up to three candidate connections near borders. The Platform tells the client to prepare, promote, sleep, retire or drop connections; gameplay code never reasons about addresses.

| Traffic | Delivery |
| --- | --- |
| Movement input and transform state | Unreliable, sequenced; the latest wins |
| Skill and interaction commands | Reliable or acknowledged per command type |
| Facts (damage, death, skill result) | Reliable |
| Snapshots | Unreliable deltas against the last acknowledged state |
| Handoff and authority control | Reliable, ordered, with authority epochs |

### Replication, budgets and interest

- Per client, the simulation builds a delta against the last state that client acknowledged; unchanged fields cost nothing.
- Values are quantized: positions relative to the entity's 3D cell origin on all three axes (16 bits per axis on 64 m cells ≈ 1 mm), angles in 16 bits, health in steps where acceptable. High-frequency payloads are bit-packed.
- Each client has a byte budget per update. Entities are ranked by distance, combat involvement, targeting, party membership and importance; the highest go first and the rest accumulate priority until they are sent.
- Candidates come from the 3D cells around the client inside its interest volume (plus a preload margin), filtered by category; each candidate pair then gets its visibility tier from Perception. Projectiles and effects from hidden players are sent without the caster's identity.
- Beyond the budget, degradation follows a fixed order: lower update rates for distant entities, then less cosmetic detail, then a cap on visible effects. Combat correctness never degrades.

| Battle size | Plan per client | Download per client | Upload per simulation |
| --- | --- | --- | --- |
| 200 (100 vs 100) | Everyone nearby at 20 Hz | ≈ 48 KB/s | ≈ 9.6 MB/s |
| 500 | The ~100 most relevant at 20 Hz, the rest at 5 Hz | ≈ 48 KB/s | ≈ 24 MB/s |

Planning assumes about 12 bytes per entity update; the targets are about 50 KB/s download per player and about 25 MB/s upload per simulation in a 500-player battle (ARCH § Principles and performance targets).

## Does not own

- Sockets, GameNetworkingSockets, encryption, congestion control, candidate connections and promotion (Platform Transport, PTR-1 to PTR-3).
- Ticket verification code and signing keys (Platform PTR-2 and PCS-1); Network only defines the hook.
- Protobuf service APIs, fact messages and the ticket format (Platform Contracts, PCT-1).
- Authority state, epochs, ghost state and the transfer protocol (DistributedSimulation). Network only encodes them.
- The input queue (Combat, CMB-7). Network only carries inputs.
- Checkpointing a character before a world transition (the Simulation's service port, SIM-5) and issuing the destination ticket (Platform PCS-1, World Coordinator).
- The prediction subset (Simulation, SIM-4) and the code it runs (RunicGame::Shared).
- Publishing metrics to Prometheus (Platform exporter, PBND-3); the Editor that attaches (Editor, EDT-7).
- Game messages: RunicGame adds its own.

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Application, Simulation, DistributedSimulation, Gameplay, Perception |
| Third-party | — |

`Runic::NetworkTransport` links only `Runic::Core`; `Runic::Network` links it and the modules above.

### Must never depend on

- GameNetworkingSockets or any socket library; Protobuf, gRPC or NATS; Agones or Kubernetes. The Platform supplies them.
- Anything but Core types in the `Runic::NetworkTransport` headers.
- Renderer, UI, Audio, Client or any presentation library (the module is headless).
- Combat, AI, Quest, GameplayFlow, Water or Environment internals.
- RunicPlatform or RunicGame code. The Platform includes `Runic::NetworkTransport`; the Engine never includes Platform headers.

## Public API sketch

ARCH has no code sample for this module. Headers (sketch, fixed in the contract session):

| Target | Header | Holds | Package |
| --- | --- | --- | --- |
| `Runic::NetworkTransport` | `Transport.hpp` | The transport interface: connections, reliable and unreliable channels, send and receive into tick-safe queues | NET-1 |
| `Runic::NetworkTransport` | `ConnectTicketHook.hpp` | The hook a transport calls to accept or reject the first message's ticket | NET-1 |
| `Runic::Network` | `NetworkLayer.hpp`, `IngressQueue.hpp`, `EgressQueue.hpp`, `LoopbackTransport.hpp` | The layer, the queues, the test loopback | NET-1 |
| `Runic::Network` | `SnapshotCodec.hpp`, `Quantization.hpp`, `InterpolationBuffer.hpp` | Encoding and interpolation | NET-2 |
| `Runic::Network` | `Reconciliation.hpp` | Input history, replay, blend or snap | NET-3 |
| `Runic::Network` | `Interest.hpp`, `ReplicationBudget.hpp` | Candidates, priority, budgets | NET-4 |
| `Runic::Network` | `SimulationLink.hpp` | Ghost and transfer message encoding | NET-5 |
| `Runic::Network` | `DiagnosticsLayer.hpp` | Metrics recording, attach channel, debug-shape stream | NET-7 |
| `Runic::Network` | `WorldTransition.hpp` | Leaving a world after a checkpoint and joining another with a new ticket | NET-8 |

## Threading and tick rules

- Transport I/O runs on its own threads and only enqueues. The simulation drains ingress at a defined point of the tick and fills egress when it commits events and builds replication state (step 7 of the 30 Hz realtime phase). No I/O callback touches Flecs.
- Replication extraction is 30 Hz domain work; per-client snapshot building runs in parallel on the worker pool, one client per task.
- Only `GlobalEntityId` and `GlobalPlayerId` cross the wire; local Flecs IDs never leave the process.
- Messages carry authority epochs; a message with a stale epoch is dropped.
- Simulation-to-simulation traffic uses the realtime transport directly, never NATS or gRPC in the hot path.
- Production builds compile the attach channel out; they expose metrics only through the Platform exporter.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| NET-1 | Transport interface, tick-safe ingress and egress queues, connect-ticket hook, the Network layer; the interface and hook ship as the header-only Runic::NetworkTransport target (Core types only) | A loopback transport passes ping and echo tests | SIM-1 | 12 |
| NET-2 | Snapshots: extraction, quantization, deltas against the last acknowledged state, interpolation buffers | Two clients see each other move smoothly under packet loss | NET-1, SIM-6 | 13 |
| NET-3 | Prediction and reconciliation: input history, replay, blend or snap | Movement stays responsive under simulated lag; corrections show in the debug view | NET-2, SIM-4 | 14 |
| NET-4 | Interest: candidates by cell, priority and budgets, visibility tiers applied | Only relevant entities arrive and per-client budgets hold | NET-2, WORLD-2, PRC-2 | 25 |
| NET-5 | Simulation links: ghost deltas and transfer messages encoded with the snapshot codec over simulation-to-simulation connections | Ghosts stay in sync across a border under packet loss; a message with a stale epoch is dropped | NET-2, DSIM-2 | 36 |
| NET-6 | Parallel replication and the degradation order | 500 bots in one region fit the per-client budget | NET-4, SIM-3 | 38 |
| NET-7 | Diagnostics layer: network and replication metrics recorded in the Core metrics registry; Editor attach channel over a development socket transport inside Network, streaming debug shapes and the traces modules publish in Core's debug-stream registry (compiled only into development and staging builds) | The Editor attaches to a development server and sees its shapes; a production build has no attach endpoint | NET-1, CORE-2 | 39 |
| NET-8 | World transitions: the session leaves one world's simulation after a checkpoint and joins another world's simulation with a new connect ticket; no handoff or ghosts across worlds | A character moves between two worlds with its checkpointed state, and the old simulation keeps no copy | NET-1, SIM-5, PCS-1 | 44+ |

## Acceptance tests

The Done when column is the acceptance list. Runnable results by slice: 12, a client connects to a local server with a test-signed ticket and passes ping and echo (with PCT-1, PTR-1, PTR-2); 13, two clients see each other move; 14, responsive movement with correction under simulated lag; 25, only relevant entities arrive with visibility tiers at night; 36, walking from simulation A into B; 38, 200 then 500 bots fighting in one region; 39, Editor live attach. NET-1 also proves that `Runic::NetworkTransport` compiles in a target that links only `Runic::Core`.

## Open items

- Decided in ARCH revision 369: AI, Combat and other modules publish traces in Core's debug-stream registry (CORE-2); the Diagnostics layer streams them without depending on those modules.
- Decided in ARCH revision 369: world time, weather overrides and dynamic water state reach replication through Simulation's replication registry (SIM-6) as world-state values; Network never depends on Environment or Water. NET-2 needs SIM-6.
- Decided in ARCH revision 369: the attach channel runs over a development socket transport inside Network, compiled only into development and staging builds; production builds have no attach endpoint.
- [OQ-1](../system/11-open-questions.md#oq-1) (decided): the Engine owns the transport interface; the Platform transport implements it.
