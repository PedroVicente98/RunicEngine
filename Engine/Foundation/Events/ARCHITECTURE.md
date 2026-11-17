# Runic::Events — module contract

Group: Foundation · Headless: **yes** · Target: `Runic::Events` (`runic_events`) · Packages: EVT-1

Source: RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › Flecs rules (event buffers, stable event
IDs); Simulation core clock and threads (events flush at barriers); Modules, work packages and the implementation
contract (EVT-1). Status: **EVT-1 implemented** (slice 4, on the preview branch `preview/slices-3-11`): typed event
buffers and readers, the hub that flushes them, and event IDs, with their acceptance tests. The folder map is
[README.md](README.md); slice 4's brief and walkthrough are
[preview/slice-04-ecs-and-world-coordinates.md](../../../preview/slice-04-ecs-and-world-coordinates.md).

## Purpose

A simulation's systems tell each other what happened through facts (a hit landed, an entity died, a state was
entered), not by calling each other. Runic::Events carries those facts: a system emits a fact into a typed buffer
during a phase, the simulation flushes the buffers at the barrier, and from then on each consumer reads every fact
once, at its own pace. Each fact gets an ID that is the same when the simulation is replayed, so a consumer that must
not handle a fact twice (a quest, a loot grant) can tell.

## Public API

Namespace `Runic::Events`. Headers are included by their path under `Public/` (`#include "EventBuffer.hpp"`). No
library types.

| Header | Holds | Since |
| --- | --- | --- |
| `EventId.hpp` | `EventId` (a `Core::SimulationId` and a sequence; `IsValid`, comparisons), `ToText`, `EventSequence` (`Next`, `Take`), `kNoEventSequence` (0), `kFirstEventSequence` (1) | EVT-1 |
| `EventHub.hpp` | `EventHub` (`Flush`, `Simulation`, `NextSequence`, `BufferCount`, `Describe`), `EventBufferInfo`; `Detail::EventBufferBase` | EVT-1 |
| `EventBuffer.hpp` | `Event<T>` (an ID and its fact), `EventBuffer<T>` (`Emit`, `MakeReader`, `Name`, `Describe`), `EventReader<T>` (`Take`, `Missed`, `IsValid`; move-only), `kDefaultEventCapacity` (16,384) | EVT-1 |

**The rules:**

- **Tick-safe.** A fact emitted in a phase is not readable until the hub's next `Flush`, so what a reader sees never
  changes while a phase runs, and a fact is visible to the next phase.
- **Multi-consumer.** Each reader of a buffer gets every event flushed after it was made, exactly once and in order,
  whatever the other readers do. A buffer keeps an event until every reader has read it.
- **Stable IDs.** `Flush` gives IDs in a fixed order: the buffers in the order they were made, each buffer's facts in
  the order they were emitted. A simulation replayed from the same state, with its hub started at the sequence a
  checkpoint saved (`NextSequence`), gives every event the ID it had before.
- **Bounded.** A buffer keeps at most its capacity. If a reader falls further behind, the oldest events go, the
  reader's `Missed()` says how many, and an error is logged when it begins.

## Owns

- The form of a fact in transit (`Event<T>`), of its ID, and the order IDs are given in.
- When a fact becomes readable and when it is let go.

## Does not own

- Which facts exist: each module defines its own fact types and owns its buffers (the state facts of ECS-2 reach a
  buffer through Runic::Simulation, because the catalog lets ECS link Core only).
- When the barrier is: Runic::Simulation flushes its hub between phases (SIM-2).
- Durable handling of a fact: a consumer that grants something records the `EventId` it handled (the Platform's
  idempotency store); this module only makes the ID stable.
- Facts that leave the process: published facts are Protobuf contracts of the Platform.
- The order of facts emitted by several workers in one phase (SIM-3 merges them in a fixed order before they reach a
  buffer).

## Allowed dependencies

Catalog row: Core (PUBLIC: `EventId` holds a `Core::SimulationId`). No third party.

## Forbidden dependencies

Every other Runic module; Godot, audio and editor code (the module is headless); RunicPlatform and RunicGame.

## Threading and tick model

- One thread emits into a buffer at a time: the simulation's driving thread, in a phase.
- `EventHub::Flush` runs at a barrier, on the driving thread, while no system runs.
- Between two flushes any number of readers may `Take` at the same time, each used by one thread; the span a reader
  gets is valid until the next flush.
- `Emit` appends to a vector, which allocates only while the buffer warms up; `Take` allocates nothing.

## Data ownership

A module owns its buffers; the simulation owns the hub, which must outlive them. A reader belongs to its consumer and
must not outlive its buffer; destroying it releases the events it was holding. Facts are copied or moved into the
buffer and destroyed when let go.

## Integration points

- A simulation builds one `EventHub` from its `SimulationId` (and, when it resumes, from a checkpoint's sequence) and
  flushes it at every barrier (SIM-2).
- A module makes an `EventBuffer<Fact>` on the simulation's hub at setup, emits into it from its systems, and hands
  `MakeReader()` to consumers.
- Tools list a hub's buffers with `Describe` (pending, readable, readers, flushed, dropped).

## Where the code lives

- `EventBuffer<T>` and `EventReader<T>` are templates in `Public/EventBuffer.hpp`; what does not depend on the fact's
  type is `Detail::EventBufferBase`.
- `Source/EventIds/EventSequence.cpp`: `EventSequence` and `ToText`.
- `Source/Buffers/EventHub.cpp`: the hub, and a buffer's registration and description.
- `Source/Readers/ReaderCursors.cpp`: the readers' cursors and `EndFlush`, which decides what a flush lets go.
- A buffer numbers its events from 0 as they are flushed; a reader's cursor is such a number. A flush appends the
  emitted facts with their IDs, then lets go of everything before the oldest unread cursor (or before `end −
  capacity`, when that is later), and erases the freed front of the array once it is most of it.
- Formats the tests rely on: `ToText` gives `<simulation>:<sequence>`; the overflow error is `RUNIC: event buffer
  '<name>' is over its capacity of <n> events: a reader fell behind and missed <m> event(s)`.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| EVT-1 | Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs (SimulationId plus sequence) | Two consumers read the same fact; IDs stay stable on replay | **Implemented** (slice 4, preview branch): `RunicEventsTests` passes with the 16 tests below; awaiting the owner's review |

EVT-1's acceptance tests: `EventBuffer.*` (a fact is not readable before the flush; two consumers read the same fact
with the same ID; each event once, in order; a slower consumer keeps its events across flushes; read events are let
go and a long run does not grow; a reader made later; no reader, nothing kept; a reader that leaves; a full buffer and
`Missed`; facts that own memory, copied and move-only), `EventHub.*` (IDs in buffer then emission order; the buffer
listing), `EventId.*` and `EventSequence.*` (the same IDs on a full replay and on a resume from a checkpoint; another
simulation, other IDs).

Decisions taken in EVT-1 that await the owner (also in the slice brief): the sequence is one per simulation, shared
by all its buffers; a reader sees only events flushed after it was made; a buffer is bounded and a reader that falls
behind misses events (reported), instead of growing without limit.

## Future extensions

- Emitting from several workers of one phase, merged in a fixed order at the barrier (SIM-3, slice 10).
- A metric per buffer (flushed, kept, dropped), registered by the simulation.
