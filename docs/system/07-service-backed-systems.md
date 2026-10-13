---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# 07 — Service-backed systems

This file defines how the simulation reaches durable services without ever blocking a tick, which services exist and who owns them, and how simulations are placed, monitored and run locally.

Source: ARCH[^arch] § Platform and operations, § Engine gameplay APIs (Quest runtime and the quest-creation API),
§ Repositories, build targets and processes (Platform modules, Game modules), § Water system, § Environment.

## Services

Every service is built on the Platform Go SDK[^platform-gosdk]. Persistence is PostgreSQL.

### Platform services (mechanisms, reusable by any MMORPG)

| Service | Owns | Package |
| --- | --- | --- |
| Accounts and entitlements | Login, entitlements, `GlobalPlayerId` issue, connect tickets signed with rotating keys whose public halves the simulations load | PCS-1[^platform-coreservices] |
| World Coordinator | Worlds and the region-to-simulation map per world, neighbor topology, handoff candidates, drains, the world epoch (PCS-2); placement by load, memory and inter-simulation traffic, placement requests to Agones (PCS-7) | PCS-2, PCS-7 |
| World directives | Weather, wind and current events and other world-wide directives with an effective world time and a duration | PCS-3 |
| Instance allocation | Instances (for example dungeons and raids) from Agones fleets | PCS-4 |
| Chat and party groups | Social transport and party membership; group rules come from game data | PCS-5 |
| Admission | Per-world capacity limits, login queue and cohort waves | PCS-6 |
| Item ledger[^platform-buildingblocks] | Item instances, containers, slots, atomic transfers, operation IDs; mechanisms only, with no game rules | PBB-1 |
| Currency wallets | Balances and atomic transfers (PBB-2), and one-transaction transfers together with the item ledger (PBB-3); mechanisms only | PBB-2, PBB-3 |

### Game services (RunicGame, named only)

Their rules are RunicGame code and are not described here.

| Service | One line |
| --- | --- |
| Character | Characters and their durable character state |
| Inventory | The game's item rules, applied through the Platform item ledger |
| Quest | Stores quest progress, deduplicates quest facts and commits rewards; quest logic runs in the simulation |
| Economy | The game's economy rules, including regional auction houses |
| Trade | Trade between players; items and currency move in one transaction through the ledger and wallets (PBB-3) |
| Guild | Guild membership and rules |
| Mail | Mail delivery, with attachments through the ledger and wallets |
| Housing | Housing and land rules |
| World events | Decides world-wide overrides (such as storms, or wind and current events from an operator panel) and delivers them as Platform world directives |
| World routines | Stores player-caused exceptions to world routines and how they fade back into the routine |
| Rankings | Rankings and contribution tracking |

Rules:

- **The Platform owns mechanisms, never policy.** No game rule enters a Platform service, its tests or its contracts.
- **Game services are built on Platform building blocks**, never by forking them.
- **Durable water state** (for example a lock or level that must survive restarts) is stored by a Game service on the
  Platform Go SDK. The Game decides it; it reaches the simulation through the service port when the owning region
  loads. The Platform provides only the SDK persistence, never a water-specific service.

## The simulation never waits

- The simulation never waits for a service or the database. Network, service and database work is queued and
  applied at defined points.
- Durable writes happen at explicit boundaries: logout, trade, periodic checkpoint and other explicit operations.
- Flecs keeps only what the active simulation needs; durable state lives in Go services and PostgreSQL.
- The Engine reaches services **only** through the Simulation's service port (SIM-5[^engine-simulation]), shipped as the header-only
  `Runic::SimulationPort` target. The Platform's ServicesBridgeLayer[^platform-serverbindings] implements it on its own threads: asynchronous
  gRPC calls from slice 27 (PBND-1), and NATS from slice 30 (PBND-4): durable facts published from the outbox, world
  directives and other subscriptions delivered to the inbox.
- Completions are queued and drained at **safe ticks**. A service callback never touches Flecs.

## The durable-fact path

Step by step, for any fact that must become durable (quest progress is the reference case):

1. A simulation system emits the fact once, with a stable event ID (SimulationId plus sequence, EVT-1[^engine-events]).
2. In-simulation consumers (for example the quest runtime) react in the same tick.
3. The fact or operation is placed in the service port's outbox with its stable ID.
4. The ServicesBridgeLayer[^platform-serverbindings] sends it asynchronously: a gRPC call when an answer is needed (PBND-1), a NATS publish
   when not (PBND-4).
5. The owning service validates the source, deduplicates by ID and commits in one transaction.
6. The accepted result is queued back; the simulation drains the inbox at a later safe tick and applies it.
7. Replaying the same fact, or delivering the same result twice, changes nothing.

| Step owner | Engine | Platform | Game |
| --- | --- | --- | --- |
| Emit fact, apply result | Simulation, Events, the consuming module | — | Game rules registered into the simulation |
| Carry it | Service port interface (SIM-5[^engine-simulation], `Runic::SimulationPort`) | ServicesBridgeLayer (PBND-1 gRPC, PBND-4 NATS), Go SDK[^platform-gosdk] (PSDK-1 to PSDK-3) | — |
| Store it | — | Transactions and idempotency store (PSDK-2) | The service's domain logic |

## Idempotency

- Every durable change is an operation with an ID, so retries are safe.
- The Go SDK[^platform-gosdk] owns transactions, migrations and the idempotency store: a retried operation commits once (PSDK-2).
- Item movements use the ledger's atomic transfers with operation IDs: concurrent transfers never duplicate or lose an
  item (PBB-1[^platform-buildingblocks]). A loot grant is applied once across reconnects.
- A transfer that moves ledger items and wallet currency together (a trade, a purchase) commits in one transaction
  (PBB-3); never split it into two operations that can half-succeed.
- A fact published once on NATS is consumed once per subscriber (PSDK-3); JetStream is used only where replay is
  required.

## World directives

- World directives[^platform-coreservices] (PCS-3) carry an effective world time a few seconds ahead and a duration, so every simulation that
  owns an affected region applies them on the same tick and the baseline returns on the same tick when they end.
- They reach the simulations through the services bridge[^platform-serverbindings]: NATS subscriptions delivered to the service port's inbox at
  safe ticks (PBND-4). A Game service decides the content (for example a storm); the Platform only delivers it.
- World time itself counts from the world epoch the World Coordinator issues (PCS-2, slice 36). Until PCS-2 ships, the
  Environment clock (ENV-1[^engine-environment]) reads the epoch from configuration.

## Simulation placement

- **One region per simulation pod by default** (decided); quiet neighboring regions may share a pod, and several
  simulations may share one process, to save cost.
- Pods request whole vCPUs, and the simulation sizes its worker pool from them.
- **Growing in place:** Kubernetes in-place pod resize (stable since Kubernetes 1.35) raises a running pod's CPU
  without a restart; the simulation picks up the new worker count at a safe point.
- **Pinned cores:** exclusive cores (the static CPU manager) give the steadiest tick, but Kubernetes does not allow
  in-place resize with them. Use them only for hot fleets after profiling; those pods grow by moving.
- **Moving to a bigger machine:** Kubernetes never moves a running process. The World Coordinator[^platform-coreservices] starts a new pod on
  a bigger node, transfers the region's entities with the handoff protocol and retires the old pod, with no loading
  screen for players.
- **Busy areas:** a busy area such as a city can get its own simulation through an AuthorityPartition, using the same
  border handoff; a partition never cuts an indivisible volume
  ([05 § Authority partitions](05-distributed-simulation.md#authority-partitions)).
- **Placement by load and traffic (decided, PCS-7).** The World Coordinator, a Platform Go service on top of Agones,
  places simulations by load, memory and traffic. The distance between simulations comes from their regions' global
  coordinates; pairs that exchange much traffic move onto the same node by move-by-handoff, up to a per-node cap.
- **Links by locality (decided, PTR-4[^platform-transport]).** Simulation links use the cheapest channel for where the peers run: an
  in-memory queue in one process, a local socket or shared memory on one machine, GameNetworkingSockets between
  machines. DistributedSimulation[^engine-distributedsimulation] semantics do not change with the channel.
- A new open-world simulation is never allocated inside a handoff's critical path; neighbors already exist.

## Instances and geography

- Instances are Agones-allocated game servers from fleets; the client preloads while the player approaches the
  entrance. Entering one, like moving to any other map, is a world transition: a checkpoint plus a new connect ticket
  (NET-8[^engine-network]).
- Worlds are hosted in regional datacenters near their players, each with its own Kubernetes cluster and Agones.
  Accounts are global.

## Staged access mechanisms

The Platform provides access-control mechanisms; when and how a game uses them is a Game decision.

| Mechanism | Owner |
| --- | --- |
| Entitlement check at login | Platform accounts and entitlements (PCS-1[^platform-coreservices]) |
| Per-world capacity limits | Platform admission (PCS-6, slice 44+) |
| Admission waves by account cohort and a login queue per world, paced by server metrics | Platform admission (PCS-6, slice 44+) |

## Monitoring: one panel

Decided.

- **Grafana is the single operations panel.** Agones exports its metrics (fleets, allocations, game server states) to
  Prometheus and ships Grafana dashboards for them.
- **The Core registry records, the Platform exporter publishes.** Each GameServer records its metrics in the Core
  metrics registry (CORE-2[^engine-core]); the Diagnostics layer (NET-7[^engine-network]) adds network and replication metrics. The Platform's
  Prometheus exporter (PBND-3[^platform-serverbindings]) publishes the registry to the same Prometheus: time per clock domain and system, tick
  backlog, players, entities, bandwidth per client, handoffs, ghosts. PBND-3 needs only CORE-2 and SIM-1[^engine-simulation]; it does not wait
  for the Diagnostics layer.
- Go services export their metrics through the SDK. Logs join the same panel.
- **Live inspection is the Editor's job, not the server's.** In development and staging the Editor attaches to a
  running GameServer through a privileged debug channel (entity inspector, live behavior trees, resolver traces, a
  stream of debug shapes). Production builds compile the attach channel out, expose metrics only through the Platform
  exporter and never link ImGui.

Metrics every process exports (ARCH[^arch] § Tools):

| Scope | Metrics |
| --- | --- |
| Per simulation tick | Time per clock domain and per major system (p50, p95, p99), backlog, Jolt counts, queries, events, worker utilization, time per layer |
| Per client | Replication bytes, entities per visibility tier, snapshot age |
| Per simulation | CPU, players, bandwidth, handoffs, ghosts |

## Local development

- **Gameplay work never needs Kubernetes.** Run GameServer directly, without a lifecycle layer or with the
  LifecycleLayer[^platform-serverbindings]'s local stub mode (PBND-2), and, only when a feature needs them, local NATS and Go services.
- PBND-2 is tested against the Agones local SDK server (Ready, health pings, drain on shutdown); its stub mode runs
  with no Agones at all.
- The local development stack (PINF-1[^platform-infrastructure]) starts PostgreSQL and NATS with one command; the service tests pass against it.
- Connect tickets are test-signed with a local key from slice 12 until PCS-1[^platform-coreservices] (slice 27) issues real ones, signed with
  rotating keys; the simulation verifies signature, expiry and key ID against its configured public keys (PTR-2[^platform-transport]).
- Until PCS-2 (slice 36), the world epoch comes from configuration.
- Agones fleets, Kubernetes manifests and dashboards (PINF-2) arrive in slice 39 and are needed only for operations.

## Rules for any session touching this area

- Never block a tick on a service, the network or disk.
- Never touch Flecs from a service or I/O callback; queue the result for a safe tick.
- Never give a durable operation an ID that changes on retry or replay.
- Never link gRPC, NATS, Agones or a database client into an Engine module.
- Never link a Platform C++ target to anything in the Engine but `Runic::Core`[^engine-core], `Runic::Application`[^engine-application],
  `Runic::NetworkTransport`[^engine-network] and `Runic::SimulationPort`[^engine-simulation].
- Never carry per-tick traffic in Protobuf, NATS or gRPC.
- Never put game rules into a Platform service or building block.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../system/README|System README]] · [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^platform-gosdk]: [[../../../RunicPlatform/docs/modules/GoSDK|GoSDK]] — Platform module · Go
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^platform-buildingblocks]: [[../../../RunicPlatform/docs/modules/BuildingBlocks|BuildingBlocks]] — Platform module · Go
[^engine-simulation]: [[../../../RunicEngine/docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^engine-events]: [[../../../RunicEngine/docs/modules/Events|Events]] — Engine module · Foundation
[^engine-environment]: [[../../../RunicEngine/docs/modules/Environment|Environment]] — Engine module · World systems
[^platform-transport]: [[../../../RunicPlatform/docs/modules/Transport|Transport]] — Platform module · C++
[^engine-distributedsimulation]: [[../../../RunicEngine/docs/modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-network]: [[../../../RunicEngine/docs/modules/Network|Network]] — Engine module · Networking
[^engine-core]: [[../../../RunicEngine/docs/modules/Core|Core]] — Engine module · Foundation
[^platform-infrastructure]: [[../../../RunicPlatform/docs/modules/Infrastructure|Infrastructure]] — Platform module · configuration
[^engine-application]: [[../../../RunicEngine/docs/modules/Application|Application]] — Engine module · Foundation
