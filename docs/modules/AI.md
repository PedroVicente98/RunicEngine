# AI

> **Contract seed.** When `Engine/AI/` is created, copy this file to
> `Engine/AI/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, and layout
> come from Appendix J §J.4.21. Other sections are derived and are proposals
> until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/AI/` |
| CMake target | `RunicAI` → `Runic::AI` |
| Namespace | `Runic::AI` |
| Roadmap phase | 11 — Navigation/Spawn/AI |
| Headless | yes |
| Status | Not started — BehaviorTree.CPP not yet added |

## Purpose

Reusable authoritative AI runtime: perception, aggro/memory, a BehaviorTree
adapter, and intent emission.

## Owns

- AI definitions and runtime state.
- Working memory.
- Perception.
- Aggro lifecycle.
- Generic nodes.
- The BehaviorTree.CPP adapter.
- The command/intent bridge.

## Does not own

Concrete NPC/boss behavior trees or personalities (Game `GameAI`); direct
mutation of combat/physics/persistence; editor ownership (the AI Editor is an
Editor subsystem).

## Allowed dependencies

- Core, Events, Gameplay, Navigation, World, Simulation, Content.
- BehaviorTree.CPP (private).

## Forbidden dependencies

Direct calls that mutate Combat, Physics, or persistence state [J.11.2]; Editor;
Renderer; RunicPlatform; RunicGame.

## Primary consumers

RunicGame AI, Spawn, encounter logic, the AI Editor subsystem.

## Public API (conceptual)

```text
AI/
├── Public/
│   ├── AIModule.hpp
│   ├── AIDefinition.hpp
│   ├── AIIntent.hpp
│   ├── AIWorkingMemory.hpp
│   ├── AIPerception.hpp
│   └── AIEvents.hpp
├── Source/
│   ├── BehaviorTree/
│   ├── Perception/
│   ├── Aggro/
│   ├── Decision/
│   └── Intents/
├── Tests/
└── CMakeLists.txt
```

AI consumes authoritative state/events and emits **intents**. Nodes never bypass
Combat, movement, physics, or persistence APIs to mutate authoritative state.
[J.4.21]

## Threading / tick model (derived)

Ticks in a registered simulation phase, typically a lower-rate domain (exact
domain not specified — [OQ-12](../system/11-open-questions.md#oq-12)). Intents
enter Simulation's command queue and are validated like any other command.

## Data ownership (derived)

Owns per-agent AI state (memory, perception results, aggro table, tree state).

## Integration points (derived)

- Events: perception of facts (damage taken, deaths, interactions).
- Navigation: path queries.
- Simulation: intents → commands (e.g. move, use ability) consumed by
  movement/Combat.
- Game `GameAI`: registers concrete trees and Game-specific nodes.
- Editor: AI Editor subsystem inspects/authors trees.

## Acceptance tests (Stage A, derived — phase 11 proof)

- [ ] An NPC perceives a target, gains aggro, paths to it, and requests an
      ability through intents only.
- [ ] Aggro decays/clears according to the lifecycle.
- [ ] A test asserts AI code has no path that mutates combat/physics state
      directly (e.g. AI target does not link Combat).
- [ ] No BehaviorTree.CPP type in `Public/`; builds headless.

## Future extensions

Spawn integration; encounter-specific behaviors registered by Game.
