# Quest

Contract seed for `Runic::Quest`: what a Claude Code session needs to implement the Quest work packages. When `Engine/GameplayAPI/Quest/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | GameplayAPI (linked through the umbrella target `Runic::GameplayAPI`) |
| Directory | `Engine/GameplayAPI/Quest/` |
| CMake target | `Runic::Quest` |
| Namespace | `Runic::Quest` |
| Headless | yes |
| Status | Not started |

## Purpose

Quest is the quest runtime and the quest-creation API. The runtime runs inside the simulation on top of GameplayFlow, so a kill or an interaction advances a quest in the same tick that produced the fact. Durable progress lives in a Game service built on the Platform Go SDK. The Engine owns the runtime and the API; the Game owns every quest (ARCH § Engine gameplay APIs, quest runtime and the quest-creation API).

## Owns

- The quest runtime and the quest-creation API: definitions, objectives, triggers from facts, instances (QST-1).
- Definitions as data. A quest is an ID, its states and objectives, and for each transition a trigger, conditions, actions and the next state. Designers write them without C++, and the Cooker validates them.
- Registries that extend the vocabulary. The Engine ships generic entries under `engine:` IDs; the Game registers its own `game:` triggers, conditions and actions in C++.

| Registry | Engine built-ins |
| --- | --- |
| Triggers | Entity killed, item acquired, interaction completed, area entered, sequence ended |
| Conditions | Quest state, has item, entity alive, time window |
| Actions | Set objective, start sequence, request an item grant or removal through a service, spawn |

- A builder for code: the same definitions can be built in C++ for tests and generated content; data and code end in the same registry.
- The durable bridge (QST-2): outgoing quest facts with stable IDs leave through the Simulation's service port; accepted results are applied at safe ticks.
- Quest instances registered as a transfer-snapshot contributor (QST-3), so a quest in progress moves with its owner.

The durable path:

1. A module emits a fact once, with a stable event ID (for example EntityKilled from Combat).
2. The quest runtime consumes the fact, advances its local state and may start a sequence.
3. The fact leaves through the Simulation's service port, and the Platform's services-bridge layer carries it to the Game's Quest service, asynchronously.
4. The service validates the source, deduplicates by event ID, updates progress and commits rewards in one transaction.
5. The accepted result returns to the simulation and is applied at a later safe tick; a service callback never touches Flecs.
6. Replaying the same fact changes nothing.

On login the character's quest state loads from the service into the runtime. On handoff, quest instances travel in the transfer snapshot with the rest of the player's state.

## Does not own

- Sequences, timers and their termination path (GameplayFlow). Quest builds on GameplayFlow and never becomes a second scripting runtime.
- Durable progress, deduplication and rewards: the Game's Quest service, on the Platform Go SDK's transactions and idempotency store (OQ-16).
- The service port (Simulation, SIM-5) and the bridge that implements it (Platform, PBND-1 and PBND-4).
- The transfer protocol and the snapshot-contributor registry (DistributedSimulation, DSIM-3).
- The facts themselves: each emitting module defines its own fact types.
- Every concrete quest, dialogue and `game:` registry entry (RunicGame).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | GameplayFlow, DistributedSimulation (handoff snapshot) |
| Third-party | — |

DistributedSimulation is needed only for the handoff snapshot (QST-3); a package links only what it uses.

### Must never depend on

- Combat or AI internals: quests act through Simulation commands and Gameplay APIs, the same doors players use (ARCH § Modules, work packages and the implementation contract, dependency rules).
- gRPC, NATS, databases or any service client. Durable work goes only through the service port.
- Presentation modules (Client, Renderer, UI, Audio).
- Game quests, names or values in code or tests. Tests use neutral IDs such as `engine:test_quest`.
- RunicPlatform or RunicGame code; Agones or Kubernetes.

## Public API sketch

ARCH's illustrative builder, with neutral test IDs in place of ARCH's example content:

```cpp
// Illustrative
QuestDefinition quest = QuestBuilder("engine:test_quest")
    .State("OpenDoor")
        .On(Trigger::InteractionCompleted("engine:test_door"))
        .If(Condition::HasItem("engine:test_key"))
        .If(Condition::EntityAlive("engine:test_npc"))
        .Do(Action::StartSequence("engine:test_sequence"))
        .Do(Action::SetObjective("Leave"))
        .GoTo("Leave")
    .Build();

quests.Register(std::move(quest));   // the same path loaded data takes
conditions.Register("game:<condition>", GameCondition{});   // RunicGame, server only
```

Headers (sketch, fixed in the contract session): `QuestDefinition.hpp`, `QuestBuilder.hpp`, `QuestRegistries.hpp`, `QuestRuntime.hpp`, `QuestInstance.hpp`, `QuestEvents.hpp`, `QuestDurableBridge.hpp`.

## Threading and tick rules

- The runtime runs on the simulation thread at phase boundaries and consumes facts after they are flushed, in the same tick that produced them.
- Quest consumers are typical 10 Hz domain work (ARCH § Simulation core: clock and threads); the QST-1 contract session fixes the phase.
- Outgoing facts carry their stable event IDs; the runtime never re-numbers them.
- Accepted results are drained from the service port inbox at safe ticks. No I/O or service callback touches Flecs.
- A replayed fact never advances progress twice.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| QST-1 | Quest runtime and quest-creation API: definitions, objectives, triggers from facts, instances | A quest defined only in data advances from kill and interaction facts | FLOW-1 | 30 |
| QST-2 | Durable bridge: outgoing quest facts with stable IDs through the service port; accepted results applied at safe ticks | A replayed fact never doubles progress; no service callback touches Flecs | QST-1, SIM-5 | 30 |
| QST-3 | Quest instances registered as a transfer-snapshot contributor | A quest in progress crosses a simulation border with its objectives and timers intact | QST-1, DSIM-3 | 37 |

## Acceptance tests

The Done when column is the acceptance list. Slice 30 proves a persistent quest driven by gameplay facts; slice 37 proves that a quest survives handoff. In this repository both use Engine test quests and a fake service port, never RunicGame content.

## Open items

- The "entity killed" trigger consumes a kill fact that Combat emits, but Quest does not list Combat as a dependency. The QST-1 contract session must ask the owner where the kill fact type lives (for example in Gameplay or Events) so Quest can read it without a Quest → Combat edge.
- [OQ-16](../system/11-open-questions.md#oq-16) (decided): the Platform Go SDK owns transactions and the idempotency store; the Game Quest service stores progress.
