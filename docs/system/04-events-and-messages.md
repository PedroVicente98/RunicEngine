---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# 04 — Events and messages

This file defines how information moves: application events inside a process, commands and facts inside the simulation, durable operations to services, and messages on the network.

Source: ARCH[^arch] § Repositories, build targets and processes (Three event families), § Application and LayerStack,
§ Simulation core: clock and threads, § Simulation core: Flecs and Jolt, § Engine gameplay APIs, § AI architecture,
§ Networking: stack, sessions and responsiveness, § Platform and operations.

## Three event families

| Family | Examples | Delivery | Owner |
| --- | --- | --- | --- |
| Application events | Key press, resize, UI focus, connection state | LayerStack: top layer first, can be marked handled | Runic::Application[^engine-application] |
| Simulation facts | HitEvent, DamageEvent, EntityKilled, RegionChanged | Tick-safe buffers; every consumer receives them | Runic::Events[^engine-events], Runic::Simulation[^engine-simulation] |
| Distributed messages | Quest facts, inventory operations, world-event directives | Transport, gRPC or NATS, with explicit reliability and idempotency | Engine service port and Network; Platform transport, bridge and SDK |

Never mix the families: an application event is never a gameplay fact, a fact is never "handled" away, and a
distributed message never mutates the simulation directly.

## Application events

- Input and window events dispatch immediately, top layer first, without heap allocation (stack events, no
  strings). A layer may mark an event handled; lower layers then do not see it. This is why UI clicks never become
  attacks: the UI overlay sits on top.
- Other events (connections, service replies) are queued and drained once per loop by the owning layer.
- No layer blocks: network, service and disk work run on their own threads and only enqueue.
- Raw keyboard, mouse and gamepad input becomes application events (APP-2[^engine-application]). Mapping input to gameplay commands is the
  Client's input mapping and command builder (CLI-2[^engine-client]).

## Commands, facts and durable facts

| Kind | What it is | Who produces it | Who decides | Example |
| --- | --- | --- | --- | --- |
| Command | A request to attempt an action. Validated when it is applied, never trusted | Player input (through the Client's command builder and the network), AI intents, GameplayFlow[^engine-gameplayflow] actions | The authoritative simulation | Move, UseAbility, MoveGoal, a restriction-checked interaction |
| Fact | Something the authority decided happened, in this tick | Simulation systems | Already decided | EntityKilled, DamageEvent, SkillStartedEvent, SequenceEnded |
| Durable fact or operation | A fact or request that must reach a durable service, with a stable ID | The simulation, through the service port | The owning Go service accepts or rejects it | A quest fact, a loot grant request |

Rules:

- **One command path.** AI intents and GameplayFlow actions become the same commands players send and pass the same
  validation. AI never mutates combat, movement, physics or persistent state directly.
- **Commands apply in phase order.** Structural changes are deferred and applied at barriers (SIM-2[^engine-simulation]).
- **Blocked commands are rejected on the server.** Restrictions are checked by command validation hooks (GPL-3[^engine-gameplay]);
  a modified client gains nothing by sending them.
- **A queued action is validated when it starts, not when it arrives.** Combat owns the one-slot input queue: at most
  one queued action per character, started on the first legal tick after recovery ends; a stun in between cancels
  it; priority defensive inputs skip the queue when the current skill's cancel window allows (CMB-7[^engine-combat]). Network only
  carries the inputs.

## Stable event IDs

- Every fact has a stable event ID: **SimulationId plus a monotonic sequence** (EVT-1[^engine-events]).
- IDs stay stable on replay, so durable consumers can deduplicate. An ID unique only within one process run is not
  enough.
- The plain ID and time types (`GlobalEntityId`, `GlobalPlayerId`, `SimulationId`, `TickId`, `AuthorityEpoch`) live
  in Core (CORE-1[^engine-core]). PCS-1[^platform-coreservices] issues `GlobalPlayerId`. DistributedSimulation[^engine-distributedsimulation] owns the authority semantics.
- Encodings: Runic::Network[^engine-network] encodes every realtime message, envelope included. Platform Contracts[^platform-contracts] (Protobuf) define
  service APIs, published facts and the connect-ticket format.
- Durable operations carry stable **operation IDs** (SIM-5[^engine-simulation]); the item ledger[^platform-buildingblocks] uses operation IDs for every transfer
  (PBB-1).

## Event buffers and barriers

The simulation advances in fixed steps on a 60 Hz master clock with 30, 20 and 10 Hz domains.

| Domain | Interval | Typical work |
| --- | --- | --- |
| Hz60 | 16.7 ms | Master clock; no mandatory gameplay work |
| Hz30 | 33.3 ms | Commands, movement, CharacterVirtual, skill timelines, projectiles, AoE queries, watercraft, the Jolt step, replication extraction |
| Hz20 | 50 ms | AI decisions and perception |
| Hz10 | 100 ms | Aggro wake scans, luminosity updates, weather and tide state, quest and loot consumers, spawning, world maintenance |

- Rates decide when a system is due; explicit phases decide order: 20/10 Hz producers, then the 30 Hz realtime phase,
  then 20/10 Hz consumers, then the Hz60 hook.
- Systems receive a TickContext (domain, master tick, domain tick, dt, simulation time). Nothing reads the wall clock.
- Durations are times, or tick counts tagged with their domain (`DomainTickCount`). An untagged "9 ticks" is a bug.
- Falling behind is measured and alarmed, never silently skipped.

**Realtime 30 Hz phase, in order**

1. Apply queued player and AI commands; start queued skills whose recovery has ended.
2. Advance movement and skill timelines; advance projectiles.
3. Commit structural physics changes in batches (bodies created and removed).
4. Apply kinematic targets, buoyancy forces and water-movement forces; step Jolt once.
5. Update CharacterVirtuals against the new world; write positions to Flecs and spatial membership; record position
   history for skills that use capped rewind.
6. Run combat, projectile and AoE queries; pass hit candidates to the resolver.
7. Commit events and build replication state.

**Buffers and barriers**

- Facts are written to typed, tick-safe event buffers and **flushed at barriers**; they are visible to the next phase,
  and every consumer receives them (multi-consumer delivery).
- Inside a parallel phase, each worker writes only to the entities it owns. Effects on other entities (damage,
  events, spawns) go to per-worker buffers that are merged at the barrier **in a fixed order**, so results never
  depend on thread timing.
- Hot combat facts travel in event buffers, not through Flecs observers. Observers handle lifecycle bookkeeping only.
- Only the simulation's own phases mutate the Flecs world.
- Example of ordering: a SkillStarted fact produced in the realtime phase reaches AI memory only in the 20 Hz
  post-realtime phase and is acted on in a later pre-realtime tick.

## The service port

The Simulation's service port (SIM-5[^engine-simulation]) is the Engine's **only door to durable services**.

| Part | What it is |
| --- | --- |
| Outbox | Outgoing durable operations, each with a stable operation ID |
| Inbox | Accepted results from services, drained at safe ticks |
| Interface target | Header-only `Runic::SimulationPort` (Core types only), so the Platform implements the port without linking the Simulation module |
| Implementation | The Platform's ServicesBridgeLayer[^platform-serverbindings], on its own threads: asynchronous gRPC calls (PBND-1, slice 27); NATS publishing of durable facts from the outbox and subscriptions such as world directives[^platform-coreservices] delivered to the inbox (PBND-4, slice 30). Completions are queued for safe ticks |

Rules:

- The simulation never waits for a service. A slow service never delays a tick.
- A service callback never touches Flecs. Results are applied by the simulation at a later safe tick.
- A result delivered twice is applied once (tested with a fake port in SIM-5).
- Engine code never names gRPC, NATS or a database; it sees only the port.

The full durable path, step by step, is in [07 § The durable-fact path](07-service-backed-systems.md#the-durable-fact-path).

## Network channels and delivery classes

Clients talk directly to their simulation through the Engine's transport interface (NET-1[^engine-network]), implemented by the
Platform on GameNetworkingSockets (PTR-1[^platform-transport]). The transport interface is the only door between a process and the
network; it has tick-safe ingress and egress queues. The interface and the connect-ticket hook ship as the header-only
`Runic::NetworkTransport` target (Core types only), which is all the Platform transport links from the Engine.

| Piece | Owner | Used for |
| --- | --- | --- |
| Transport interface | Engine | The only door between a process and the network; tick-safe ingress and egress queues |
| GameNetworkingSockets transport | Platform | Real-time UDP, client ↔ simulation and simulation ↔ simulation: reliable and unreliable messages, encryption, congestion control |
| Replication, interest, prediction, handoff roles | Engine | What to send, to whom, and how clients correct themselves |
| Realtime message encoding | Engine | Every message on the realtime connection, envelope included: inputs, snapshots, deltas, simulation-link messages |
| Protobuf contracts | Platform (games add messages) | Service APIs, published facts and the connect-ticket format; never per-tick traffic |
| gRPC | Platform SDK | Calls that need an answer: load character, consume item, commit trade, allocate instance |
| NATS | Platform SDK | Publish and subscribe without an answer; JetStream only where replay is required |
| Go services | Platform and Game | Core services[^platform-coreservices] and game services |
| Agones and Kubernetes | Platform | Simulation lifecycle, placement, fleets, configuration |

Runic::Network encodes every message on the realtime connection, envelope included; high-frequency payloads are
bit-packed and quantized. Protobuf (Platform Contracts[^platform-contracts]) defines service APIs, published facts and the connect-ticket
format, never per-tick traffic.

| Traffic | Delivery |
| --- | --- |
| Movement input and transform state | Unreliable, sequenced; the latest wins |
| Skill and interaction commands | Reliable or acknowledged per command type |
| Facts (damage, death, skill result) | Reliable |
| Snapshots | Unreliable deltas against the last acknowledged state |
| Handoff and authority control | Reliable, ordered, with authority epochs |

Other delivery rules:

- **Simulation-to-simulation traffic** (ghost deltas, transfer messages) is encoded by Network with the snapshot
  codec (NET-5) and carried directly over GameNetworkingSockets, never NATS or gRPC in the hot path. A message with a
  stale authority epoch is dropped.
- **Presentation cues** from sequences are facts sent to clients; they carry no authority.
- **Environment overrides** (for example a storm) are world directives (PCS-3) delivered through the services bridge[^platform-serverbindings]
  (PBND-4) to every simulation owning the affected regions, with an effective world time; they reach clients once as
  a reliable fact. Wind, current and wave values are never replicated per tick.
- **Projectiles and effects from hidden players** are sent without the caster's identity
  ([05 § Interest and visibility tiers](05-distributed-simulation.md#interest-and-visibility-tiers)).
- **Connect tickets** are presented first on a new connection and checked locally (signature, expiry and key ID,
  against a configured set of public keys) with no service call in the hot path (PTR-2). They are test-signed with a
  local key from slice 12 until PCS-1 (slice 27) issues real ones, signed with rotating keys whose public halves the
  simulations load. The ticket format is a Contracts message (PCT-1).

## Idempotency

- Every durable change is an operation with an ID, so retries are safe.
- The Platform Go SDK[^platform-gosdk] owns transactions and the idempotency store (PSDK-2): a retried operation commits once.
- Services deduplicate durable facts by their stable event ID; replaying the same fact changes nothing.
- The item ledger[^platform-buildingblocks]'s atomic transfers carry operation IDs; concurrent transfers never duplicate or lose an item.
- NATS subscribers consume a fact published once exactly once per subscriber (PSDK-3).

## Local Flecs IDs never leave the process

- `GlobalEntityId` is the stable identity across simulations, clients and services. Local Flecs entity IDs are
  process-local and never appear on the wire, in schemas, in durable records or in RenderData.
- `GlobalPlayerId` is the plain player ID type in Core (CORE-1[^engine-core]); the accounts service (PCS-1[^platform-coreservices]) issues it. Like
  `GlobalEntityId`, it is what schemas and durable records use.
- DistributedSimulation[^engine-distributedsimulation] maps GlobalEntityId to local entities (DSIM-1).
- RenderData refers to entities by GlobalEntityId and to resources by AssetId; nothing points into Flecs memory.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../system/README|System README]] · [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-application]: [[../../../RunicEngine/docs/modules/Application|Application]] — Engine module · Foundation
[^engine-events]: [[../../../RunicEngine/docs/modules/Events|Events]] — Engine module · Foundation
[^engine-simulation]: [[../../../RunicEngine/docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-client]: [[../../../RunicEngine/docs/modules/Client|Client]] — Engine module · Presentation
[^engine-gameplayflow]: [[../../../RunicEngine/docs/modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-gameplay]: [[../../../RunicEngine/docs/modules/Gameplay|Gameplay]] — Engine module · GameplayAPI
[^engine-combat]: [[../../../RunicEngine/docs/modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-core]: [[../../../RunicEngine/docs/modules/Core|Core]] — Engine module · Foundation
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^engine-distributedsimulation]: [[../../../RunicEngine/docs/modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-network]: [[../../../RunicEngine/docs/modules/Network|Network]] — Engine module · Networking
[^platform-contracts]: [[../../../RunicPlatform/docs/modules/Contracts|Contracts]] — Platform module · Protobuf
[^platform-buildingblocks]: [[../../../RunicPlatform/docs/modules/BuildingBlocks|BuildingBlocks]] — Platform module · Go
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^platform-transport]: [[../../../RunicPlatform/docs/modules/Transport|Transport]] — Platform module · C++
[^platform-gosdk]: [[../../../RunicPlatform/docs/modules/GoSDK|GoSDK]] — Platform module · Go
