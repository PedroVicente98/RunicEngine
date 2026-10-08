# DistributedSimulation

> **Contract seed.** When `Engine/DistributedSimulation/` is created, copy this
> file to `Engine/DistributedSimulation/ARCHITECTURE.md`; from then on that file
> is authoritative. Purpose, Owns, Does not own, Allowed dependencies, Primary
> consumers, layout, and API sketch come from Appendix J §J.4.9 and §J.8. Other
> sections are derived and are proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/DistributedSimulation/` |
| CMake target | `RunicDistributedSimulation` → `Runic::DistributedSimulation` |
| Namespace | `Runic::DistributedSimulation` |
| Roadmap phase | 13 — distributed simulation (foreign combat in 14) |
| Headless | yes |
| Status | Not started |

## Purpose

Transport-independent semantics for authority, ghosts, transfers, remote
interactions, and reconstruction across simulation processes.

## Owns

- `SimulationId`, `GlobalEntityId`, `AuthorityEpoch`, `SimulationOwner`.
- Ghost state, transfer state, authority state.
- Foreign-interaction contracts.

## Does not own

Sockets, serialization, endpoints, Agones, Kubernetes, topology decisions, Game
damage rules.

## Allowed dependencies

- Core, Events, World, Simulation.

## Forbidden dependencies

NetworkAdapter (it depends on this module), any transport/serialization library,
Agones/Kubernetes, Combat (Combat depends on this module for foreign
interactions), RunicPlatform, RunicGame.

## Primary consumers

NetworkAdapter, Combat distributed extension, Platform bindings, server runtime.

## Public API (conceptual)

```text
DistributedSimulation/
├── Public/
│   ├── SimulationId.hpp
│   ├── GlobalEntityId.hpp
│   ├── AuthorityEpoch.hpp
│   ├── SimulationOwner.hpp
│   ├── GhostEntity.hpp
│   ├── GhostState.hpp
│   ├── AuthorityState.hpp
│   ├── TransferState.hpp
│   ├── ForeignInteraction.hpp
│   └── DistributedSimulation.hpp
├── Source/
│   ├── Ghosts/
│   ├── Authority/
│   ├── Transfer/
│   └── ForeignInteractions/
├── Tests/
└── CMakeLists.txt
```

```cpp
enum class EntityAuthority { LocalAuthority, RemoteGhost, TransferringOut, TransferringIn };

struct AuthorityInfo {
    SimulationId   owner;
    AuthorityEpoch epoch;
};
```

**Invariant.** Exactly one simulation may mutate an authoritative transferable
entity at a time. Ghosts are read-mostly mirrors and never independently decide
gameplay outcomes for the remote entity. [J.4.9]

## Threading / tick model (derived)

Runs on the simulation thread. Incoming ghost/transfer/foreign messages arrive
through NetworkAdapter ingress and are applied at safe tick boundaries.

## Data ownership (derived)

- Authority table: `GlobalEntityId → (owner, epoch, EntityAuthority)`.
- `GlobalEntityId ↔` local Flecs entity mapping; local IDs never leave the
  process.
- Messages carrying a stale epoch are rejected.

## Integration points (derived)

- NetworkAdapter: egress of ghost deltas, transfer snapshots, authority commits,
  foreign requests; ingress of the same.
- Combat: `ForeignCombatRequest` / remote hit results.
- GameplayFlow: `HandoffPolicy` and serialization of active sequences into the
  `FinalTransferSnapshot` ([06](../system/06-gameplayflow-quests-cutscenes.md#handoff-policy-for-active-sequences-j99)).
- Platform WorldControl decisions reach the simulation as Control messages via
  NetworkAdapter; this module enforces them but does not make them.

## Acceptance tests (Stage A, derived)

Run two `Simulation` instances in one test process connected by an in-memory
test transport (no sockets):

- [ ] Ghost of a remote entity is created, updated by deltas, and rejected if
      something tries to mutate it locally.
- [ ] Transfer: `TransferringOut` → `TransferringIn` → `LocalAuthority`, epoch
      +1, last processed input sequence preserved.
- [ ] Messages with a stale epoch are rejected.
- [ ] No local entity ID can be placed in an outgoing message (type-enforced).
- [ ] `PinAuthority` blocks transfer; `AbortOnTransfer` runs cleanup first.
- [ ] Builds headless with no transport library.

## Future extensions

Phase 14: foreign combat volumes/effect context, AuthorityPartition boundaries,
hot-city behavior ([05](../system/05-distributed-simulation.md)).

## Open questions

- [OQ-2](../system/11-open-questions.md#oq-2) — IDs declared in Core vs. here.
