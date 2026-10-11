# DistributedSimulation

Contract seed for `Runic::DistributedSimulation`: what a Claude Code session needs to implement the DistributedSimulation work packages. When `Engine/Networking/DistributedSimulation/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Networking |
| Directory | `Engine/Networking/DistributedSimulation/` |
| CMake target | `Runic::DistributedSimulation` |
| Namespace | `Runic::DistributedSimulation` |
| Headless | yes |
| Status | Not started |

## Purpose

DistributedSimulation lets one logical world run on many simulation processes with no loading screen. It owns authority state and epochs, ghost state as plain data, the transfer protocol (prepare, final snapshot, commit) with a snapshot-contributor registry, the foreign-interaction contract and authority partitions (ARCH § Networking: replication, interest and handoff). Network encodes everything this module produces; the Platform transport carries the bytes.

## Owns

- Authority state, authority epochs and the mapping between `GlobalEntityId` and local entities (DSIM-1). Core declares the plain ID types; this module owns their authority semantics (OQ-2).
- Ghosts: ghost state near borders as plain data (DSIM-2). Network encodes the deltas (NET-5).
- Transfer: prepare, final snapshot, authority commit (DSIM-3).
- The snapshot-contributor registry (DSIM-3). Modules add their state to the final snapshot: GameplayFlow sequences (FLOW-3), Quest instances (QST-3), Water ship groups (WATER-7). Players and their state transfer with the input sequence continuing.
- The foreign-interaction contract (DSIM-4): a request carries enough context for the owner to act on it. Combat uses it for foreign combat (CMB-8).
- Authority partitions inside one region (DSIM-5, expansion).

Rules:

- Exactly one simulation is authoritative for an entity at any moment; authority epochs reject stale messages.
- Near a border, neighbor simulations receive ghost state, and the client preconnects to up to three candidates; crossing promotes one and drops the others, with no loading screen.
- The world partition is not simulation ownership. Regions, Sectors and Cells stay fixed, while an AuthorityPartition (an operational area with an owner and an epoch) can split a busy area between simulations. Every participating simulation loads the same static geometry; each dynamic entity keeps exactly one owner and appears as a ghost elsewhere. Players are never spread round-robin across simulations.
- Cross-boundary combat: the target's owner resolves the hit (ForeignCombatVolume, then RemoteHitResult). A world boss keeps its health, phase, AI and cooldowns on one simulation; attacks from elsewhere arrive as foreign combat, and its area effects reach other simulations as foreign effect volumes that each applies only to its own entities.
- Groups transfer together: a ship with its passengers and cargo hands off in one authority epoch, prepared before authority commits (make before break).

## Does not own

- Encoding of ghost deltas and transfer messages, and the realtime connections (Network, NET-5; Platform Transport, PTR-3).
- Which simulation owns which region, neighbor topology and handoff candidates (Platform World Coordinator, PCS-2).
- Combat resolution (Combat), sequences (GameplayFlow), quests (Quest), ship groups (Water): they register their state as contributors.
- Placement, pod sizing and drains (Platform and Agones).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Simulation, Events, World |
| Third-party | — |

### Must never depend on

- Network, Combat, GameplayFlow, Quest, Water or any module that registers into it.
- GameNetworkingSockets, NATS, gRPC or databases. Simulation-to-simulation traffic never uses NATS or gRPC in the hot path.
- Region, Sector or Cell → simulation process identity: spatial keys never encode server identity.
- RunicPlatform or RunicGame code; Agones or Kubernetes.

## Public API sketch

Sketch, fixed in the contract session: `AuthorityState.hpp` (owner, epoch), `GlobalEntityMap.hpp`, `GhostState.hpp`, `Transfer.hpp` (prepare, final snapshot, commit), `SnapshotContributor.hpp` (the registry other modules register into), `ForeignInteraction.hpp`, `AuthorityPartition.hpp`.

The contributor registry is the extension point: a contributor writes its state into the final snapshot on the source simulation and restores it on the target, so a sequence, a quest or a whole ship group moves with its owner.

## Threading and tick rules

- Authority changes, transfer commits and contributor restores happen at safe ticks inside simulation phases; never from an I/O callback.
- Ghost state is built as plain data each tick for Network to encode; ghosts are never damaged or mutated locally.
- A message with a stale epoch is rejected before it changes anything.
- The player's input sequence continues across a transfer.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| DSIM-1 | Authority state, epochs, GlobalEntityId mapping | Messages with a stale epoch are rejected | SIM-2 | 36 |
| DSIM-2 | Ghosts: ghost state near borders as plain data; Network encodes the deltas (NET-5) | Ghosts follow their owners near the border | DSIM-1 | 36 |
| DSIM-3 | Transfer: prepare, final snapshot, authority commit; a snapshot-contributor registry where modules add their state (sequences, quests, ship groups) | A player walks from A to B with no loading screen; the input sequence continues | DSIM-2 | 36 |
| DSIM-4 | Foreign-interaction contract | A request carries enough context for the owner to resolve it | DSIM-1 | 37 |
| DSIM-5 | Authority partitions inside one region (busy cities) | Two simulations share a city and each entity has one owner | DSIM-3 | 44+ |

## Acceptance tests

The Done when column is the acceptance list. Slice 36 walks from simulation A into B (with NET-5, PCS-2, PTR-3); slice 37 proves that an attack across the border damages its target once and that a sequence and a quest survive handoff; slice 42b moves a ship with passengers and cargo across a border with nothing lost. Tests use Engine test contributors, never RunicGame state.

## Open items

- [OQ-2](../system/11-open-questions.md#oq-2) (decided): plain ID and time types live in Core; DistributedSimulation owns authority semantics; wire encodings live in Engine Network and Platform Contracts.
