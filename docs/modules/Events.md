# Events

> **Contract seed.** When `Engine/Events/` is created, copy this file to
> `Engine/Events/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.3 and §J.7. Other sections are derived
> and are proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Events/` |
| CMake target | `RunicEvents` → `Runic::Events` |
| Namespace | `Runic::Events` |
| Roadmap phase | 1 — authoritative backbone |
| Headless | yes |
| Status | Not started |

## Purpose

Typed, tick-safe, authoritative gameplay fact/event streams inside a simulation.
Application events and distributed messages are separate families
([04](../system/04-events-and-messages.md)).

## Owns

- `EventId` and event metadata.
- Typed event buffers, streams, and readers.
- Event registration.
- Multi-consumer delivery semantics.

## Does not own

Window/input propagation; network serialization; service message transport;
durable replay policy.

## Allowed dependencies

- Core. (Simulation integrates ownership and tick boundaries.)

## Forbidden dependencies

ECS, Simulation (Simulation depends on Events, not the reverse), every gameplay
module, NetworkAdapter, any serialization/transport library, RunicPlatform,
RunicGame.

## Primary consumers

Combat, AI, Quest, GameplayFlow, Spawn, Game simulation modules, NetworkAdapter.

## Public API (conceptual)

```text
Events/
├── Public/
│   ├── EventId.hpp
│   ├── EventMeta.hpp
│   ├── GameplayEvent.hpp
│   ├── EventBuffer.hpp
│   ├── EventStream.hpp
│   ├── EventReader.hpp
│   └── EventRegistry.hpp
├── Source/
├── Tests/
└── CMakeLists.txt
```

```cpp
struct EntityKilled {
    EventId        id;
    TickId         tick;
    GlobalEntityId killer;
    GlobalEntityId victim;
};

events.Publish(EntityKilled{ .id = events.NextId(), .tick = tick,
                             .killer = attacker, .victim = target });
```

**Rule.** Gameplay facts are not handled-away like UI events. Multiple legitimate
consumers must be able to observe the same authoritative fact. [J.4.3]

## Threading / tick model (derived)

- Facts are published on the simulation thread during simulation phases.
- Each reader has its own cursor; reading never removes a fact for others.
- Buffer rotation/retention happens at tick boundaries decided by Simulation.

## Data ownership (derived)

- Events owns the buffers. Payloads are plain values — no pointers into Flecs
  storage — so a fact stays valid after the entity changes or dies.
- Payloads reference entities by `GlobalEntityId` so NetworkAdapter can forward
  them without translation.

## Integration points (derived)

- Simulation: tick stamping, buffer rotation.
- Quest / GameplayFlow: "wait for fact" conditions.
- NetworkAdapter: reads facts that must leave the process (e.g. durable quest
  facts) and turns them into distributed messages.

## Acceptance tests (Stage A, derived)

- [ ] Two independent readers each observe every published fact exactly once.
- [ ] Publication order is preserved within a tick.
- [ ] Every fact carries an `EventId` and `TickId`; IDs are unique within a
      simulation.
- [ ] No API exists to mark a fact as handled/consumed.
- [ ] A reader that falls behind the retention window gets an explicit
      diagnostic, not silent loss.
- [ ] Builds headless with Core only.

## Future extensions

Typed fact registration for Game-defined facts; tooling to inspect streams in
the Editor. Durable replay stays out of this module.

## Open questions

- [OQ-2](../system/11-open-questions.md#oq-2) — where `TickId`/`GlobalEntityId`
  are declared, and how `EventId` stays stable for deduplication.
- Retention length (ticks) per stream is not specified.
