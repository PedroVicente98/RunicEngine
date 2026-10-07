# 04 — Events and message taxonomy

## Three event families — never merge them [J.7.1]

| Family | Examples | Dispatch / transport | Owner |
| --- | --- | --- | --- |
| **Application/UI events** | Window resize, key/mouse/text input, UI focus. | Usually immediate LayerStack traversal; may be consumed/handled. | Engine `Application` / client runtime. |
| **Simulation gameplay facts** | HitEvent, DamageApplied, EntityKilled, RegionChanged, InteractionCompleted, BossPhaseChanged. | Tick-safe typed buffers/streams; many consumers; **not handled-away**. | Engine `Events` + authoritative `Simulation`. |
| **Distributed/service messages** | Quest facts/results, inventory requests, world directives, ghost/transfer/control messages. | Platform Realtime, gRPC/NATS/service runtime with explicit reliability and idempotency. | RunicPlatform, plus Engine adapter and Game service consumers. |

Everything that crosses a process boundary is a *message*, but not every message
is a gameplay event. The wire representation of `EntityKilled` is a distributed
message carrying an authoritative fact; the in-simulation `EntityKilled` fact is
owned by Engine `Events`.

### Why gameplay facts are not "handled" [J.4.3]

UI events may be consumed by the first layer that handles them. Gameplay facts
must not: Combat, AI, Quest, GameplayFlow, Spawn, Game modules, and
NetworkAdapter may all legitimately observe the same authoritative fact.

```cpp
// Conceptual
struct EntityKilled {
    EventId        id;
    TickId         tick;
    GlobalEntityId killer;
    GlobalEntityId victim;
};

events.Publish(EntityKilled{
    .id     = events.NextId(),
    .tick   = tick,
    .killer = attacker,
    .victim = target,
});
```

Every fact carries a stable `EventId` so downstream durable consumers (e.g. a
private QuestService) can deduplicate replays ([09 §J.14.4](09-validation-scenarios.md#j144-quest-durability-test)).

### Application layers are not a gameplay scheduler [J.4.2]

`Application` layers compose the process (client, editor, headless bootstrap).
They must not be used to order movement, combat, physics, AI, replication, or
quest consumers — that ordering belongs to `Simulation` phases and tick domains.

## Boundary message classes [J.7.2]

| Message class | Meaning | Examples |
| --- | --- | --- |
| **Command / Intent** | Please attempt this action. | MoveInput, UseSkill, Interact, PlaceCrop. |
| **Event / Fact** | Authority has decided this happened. | EntityKilled, DamageApplied, CropPlanted, BossPhaseChanged. |
| **State / Replication** | Latest authoritative state. | Transform, velocity, HP, ghost delta, snapshot delta. |
| **Request / Reply** | Service operation requiring an answer. | ConsumeItem, LoadCharacter, CommitTrade, ValidateSession. |
| **Transfer / Snapshot** | State required to reconstruct elsewhere. | PlayerTransferSnapshot, region bootstrap, recovery snapshot. |
| **Control** | Authority/lifecycle/topology management. | AssignPartition, DrainSimulation, AuthorityCommit, connection-role changes. |

Practical consequences (derived):

- A client never sends a *fact*; it sends *commands/intents* that the authority
  validates (including against active gameplay restrictions —
  [06](06-gameplayflow-quests-cutscenes.md#client-control-suppression-is-not-security-j97)).
- AI also emits *intents*, not mutations ([J.4.21]).
- Request/Reply traffic never runs on the simulation thread; see the invariant
  below.
- Schemas for anything on the wire are owned by Platform `Contracts`
  (generic) or by RunicGame (game-specific payloads carried through Platform).

## Simulation thread invariant [J.7.2]

External I/O, gRPC, NATS, Agones, and database latency **never block a
simulation tick**. I/O completes off-thread and enqueues typed work that is
applied only at explicit safe tick boundaries.

```text
simulation tick N ── emits fact / submits async request ──► (off-thread I/O)
                                                              │
simulation tick N+k ◄── typed completion applied at a safe boundary ◄┘
```

## Where each piece lives

| Concern | Module |
| --- | --- |
| `AppEvent`, layer dispatch | Engine `Application` |
| `EventId`, `EventMeta`, typed buffers/streams/readers, registry | Engine `Events` |
| Command ingress queues, event production boundaries, phases | Engine `Simulation` |
| Ingress/egress queues between simulation and network, replication extraction/application | Engine `NetworkAdapter` |
| Packet/service/event schemas, message IDs | Platform `Contracts` |
| Transport, channels, serialization, sequencing | Platform `Realtime` |
| gRPC/NATS plumbing, request context, retries, idempotent requests | Platform `ServiceRuntime` |
| Durable replay/deduplication policy | Not owned by Engine `Events` [J.4.3]; generic idempotency store in Platform `Persistence`; domain deduplication semantics in RunicGame services. |
