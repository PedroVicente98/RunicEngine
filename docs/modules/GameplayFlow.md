# GameplayFlow

> **Contract seed.** When `Engine/GameplayFlow/` is created, copy this file to
> `Engine/GameplayFlow/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.18 and §J.9. Other sections are
> derived and are proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/GameplayFlow/` |
| CMake target | `RunicGameplayFlow` → `Runic::GameplayFlow` |
| Namespace | `Runic::GameplayFlow` |
| Roadmap phase | 10 — GameplayFlow + Quest |
| Headless | yes |
| Status | Not started |

## Purpose

Reusable authoritative action/condition/sequence runtime for scripted gameplay
that is not specific to quests.

## Owns

- `Sequence`, `SequenceInstance`, steps.
- Action and Condition registries.
- Sequence scheduler.
- Source-owned cleanup.
- Sequence events (`SequenceEnded`, `SequenceAborted`, …).
- Handoff policy metadata.

## Does not own

Concrete quest/boss/cutscene scripts (Game); rendering/camera (client
presentation); domain-service persistence.

## Allowed dependencies

- Core, Events, Gameplay, Simulation, Content.

## Forbidden dependencies

Quest (Quest depends on GameplayFlow), Jolt/bgfx/network backends [J.11.2],
Renderer, RunicPlatform, RunicGame. Combat/AI usage is undecided
([OQ-18](../system/11-open-questions.md#oq-18)).

## Primary consumers

Quest, Game encounters, dungeons, tutorials, world events, cutscene control.

## Public API (conceptual)

```text
GameplayFlow/
├── Public/
│   ├── Sequence.hpp
│   ├── SequenceInstance.hpp
│   ├── SequenceStep.hpp
│   ├── Action.hpp
│   ├── Condition.hpp
│   ├── ActionRegistry.hpp
│   ├── SequenceContext.hpp
│   ├── SequenceScheduler.hpp
│   └── SequenceEvents.hpp
├── Source/
│   ├── Runtime/
│   ├── Conditions/
│   ├── Actions/
│   └── Scheduling/
├── Tests/
└── CMakeLists.txt
```

```cpp
actions.Register("engine:spawn_entity",   SpawnEntityAction{});
actions.Register("engine:start_sequence", StartSequenceAction{});

// Closed-source Game extension:
actions.Register("game:set_trade_route_state", GameSetTradeRouteStateAction{});
```

GameplayFlow is the common scripting mechanism. Quest, bosses, encounters,
cutscenes, escorts, tutorials, and world events must not each invent their own
timer/state-machine infrastructure. Built-in action categories and the boss and
cutscene examples are in [06](../system/06-gameplayflow-quests-cutscenes.md).

```cpp
enum class HandoffPolicy { Transferable, PinAuthority, AbortOnTransfer };
```

## Threading / tick model (derived)

- Sequences advance on the simulation thread inside a registered phase.
- Timers are stored relative to simulation time (needed for transfer).
- "Service" actions submit async operations and wait for an accepted result
  delivered at a later safe tick; they never block.

## Data ownership (derived)

Owns `SequenceInstance` state, timers, and the list of resources the instance
acquired (restrictions, reservations, control), keyed by `SequenceInstanceId`.

## Integration points (derived)

- Events: wait-for-fact conditions; emits sequence facts.
- Gameplay: restriction acquire/release with the instance as source.
- Simulation: actions that act on the world are submitted as commands.
- DistributedSimulation: `Transferable` instances serialized into the
  `FinalTransferSnapshot`.
- Presentation cues to clients through NetworkAdapter (server never renders).

## Acceptance tests (Stage A, derived — subset of [J.14.3](../system/09-validation-scenarios.md#j143-cutscene-safety-test))

- [ ] A sequence runs steps with waits, conditions, and actions to completion.
- [ ] Complete, Abort, Cancel, Timeout, owner destruction, and shutdown all go
      through one termination routine that cancels timers, releases
      source-owned restrictions/reservations, and emits `SequenceEnded` or
      `SequenceAborted`.
- [ ] Watchdog: a lost completion cannot leave restrictions permanent without
      diagnostics/expiry.
- [ ] Game-registered (`game:` test) actions/conditions plug in without modifying
      Engine code.
- [ ] Random branches use an explicit seed and are deterministic.
- [ ] `Transferable` state round-trips through a snapshot with remaining timers.

## Open questions

- [OQ-18](../system/11-open-questions.md#oq-18) — Combat/AI/Movement built-ins.
- Sequence definition format (data vs. code) is not specified.
