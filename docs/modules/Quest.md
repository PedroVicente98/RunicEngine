# Quest

> **Contract seed.** When `Engine/Quest/` is created, copy this file to
> `Engine/Quest/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and example come from Appendix J §J.4.19, §J.9, and §J.10.5. Other sections are
> derived and are proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Quest/` |
| CMake target | `RunicQuest` → `Runic::Quest` |
| Namespace | `Runic::Quest` |
| Roadmap phase | 10 — GameplayFlow + Quest |
| Headless | yes |
| Status | Not started |

## Purpose

Reusable quest/objective/trigger/condition/action runtime and world-interaction
bridge.

## Owns

- `QuestId` and quest instances.
- Objective, trigger, and condition abstractions.
- `QuestAction` integration (with GameplayFlow).
- `QuestContext`.
- Runtime consumption of gameplay facts.

## Does not own

Actual RunicGame quests, story, rewards, and the private durable QuestService
policy.

## Allowed dependencies

- Core, Events, Gameplay, GameplayFlow, Content, Simulation.

## Forbidden dependencies

Persistence/service clients, NetworkAdapter, Jolt/bgfx, RunicPlatform,
RunicGame. Quest must not become a second scripting engine — sequences belong
to GameplayFlow.

## Primary consumers

RunicGame quests, the Platform adapter/service bridge, Editor quest tooling
(later).

## Public API (conceptual)

```text
Quest/
├── Public/
│   ├── QuestId.hpp
│   ├── QuestInstance.hpp
│   ├── QuestObjective.hpp
│   ├── QuestTrigger.hpp
│   ├── QuestCondition.hpp
│   ├── QuestAction.hpp
│   ├── QuestContext.hpp
│   ├── QuestRuntime.hpp
│   └── QuestEvents.hpp
├── Source/
│   ├── Objectives/
│   ├── Conditions/
│   ├── Triggers/
│   └── Runtime/
├── Tests/
└── CMakeLists.txt
```

```text
Trigger:     PlayerInteracted(PrisonDoor)
Conditions:  QuestState == RescuePrincess.OpenCell
             PlayerHasItem(CellKey)
             PrincessAlive
Actions:     OpenDoor
             RemoveItem(CellKey)
             StartSequence(PrincessRescue)
             SetObjective(EscapeCastle)
Transition:  OpenCell -> EscapeCastle
```

Engine defines extensible action/condition/trigger registries, not a hard-coded
enum of every possible RunicGame quest action. [J.4.19]

## Threading / tick model (derived)

Runs on the simulation thread; consumes facts from Events readers each tick.
Durable progression is asynchronous (below).

## Data ownership (derived)

- The simulation holds the *runtime* quest state needed for local world behavior.
- Durable progression and rewards are owned by the private Game QuestService;
  the accepted durable state is applied back at a later safe tick. [J.10.5, J.14.4]

## Integration points (derived)

- Events: `EntityKilled`, `InteractionCompleted`, etc. drive triggers.
- GameplayFlow: `StartSequence` and shared action/condition registries.
- Durable bridge: Quest emits durable quest facts/operations with stable
  identity; NetworkAdapter/Platform deliver them to QuestService. Quest itself
  never calls a service.

## Acceptance tests (Stage A, derived — subset of [J.14.4](../system/09-validation-scenarios.md#j144-quest-durability-test))

- [ ] A test quest advances an objective on a matching fact and ignores
      non-matching facts.
- [ ] Trigger → conditions → actions → transition executes atomically within a
      tick.
- [ ] The same fact (same `EventId`) delivered twice does not advance progress
      twice locally.
- [ ] Durable operations are emitted with stable identity; an accepted result
      is applied at a later tick, never from a callback.
- [ ] Game-specific (`game:` test) triggers/conditions/actions register without
      Engine changes.

## Open questions

- [OQ-16](../system/11-open-questions.md#oq-16) — `QuestStore` placement.
