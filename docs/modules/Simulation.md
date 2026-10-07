# Simulation

> **Contract seed.** When `Engine/Simulation/` is created, copy this file to
> `Engine/Simulation/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.8. Other sections are derived and are
> proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Simulation/` |
| CMake target | `RunicSimulation` → `Runic::Simulation` |
| Namespace | `Runic::Simulation` |
| Roadmap phase | 1 — authoritative backbone |
| Headless | yes (mandatory) |
| Status | Not started |

## Purpose

Authoritative fixed-step runtime that owns the Flecs world, the simulation
clock, phases, commands, and event production boundaries.

## Owns

- `SimulationRuntime`.
- The 60 Hz master clock and the 30/20/10 Hz domains.
- Explicit phases.
- Command ingress.
- Event/fact buffers (integration with Events).
- Deferred mutation.
- Physics synchronization timing.

## Does not own

Combat/AI/Quest/Game rules as hard-coded built-ins; sockets; DB calls;
UI/rendering.

## Allowed dependencies

- Core, ECS, Events, World, Physics, Content.

## Forbidden dependencies

Gameplay, Combat, AI, Quest, GameplayFlow, Water, Navigation (they register into
Simulation, not the reverse); DistributedSimulation; NetworkAdapter; Renderer;
RunicPlatform; **any concrete RunicGame code**. [J.11.2]

## Primary consumers

Gameplay modules, server runtime, client prediction facade, NetworkAdapter.

## Public API (conceptual)

```text
Simulation/
├── Public/
│   ├── Simulation.hpp
│   ├── SimulationRuntime.hpp
│   ├── TickScheduler.hpp
│   ├── TickDomain.hpp
│   ├── SimulationCommand.hpp
│   ├── CommandQueue.hpp
│   ├── SimulationEvent.hpp
│   └── SimulationPhase.hpp
├── Source/
│   ├── Scheduler/
│   ├── Commands/
│   ├── Events/
│   └── Pipeline/
├── Tests/
└── CMakeLists.txt
```

```cpp
enum class TickDomain { Hz60, Hz30, Hz20, Hz10 };

// Modules register systems into named simulation phases.
CombatModule::Register(sim);
AIModule::Register(sim);
QuestModule::Register(sim);
```

Simulation owns orchestration, not every gameplay feature. Gameplay modules
depend on the registration surface; Simulation never depends back on them.

## Threading / tick model (derived)

- One simulation thread mutates authoritative state.
- Master tick = 60 Hz. Domain cadence: 30 Hz every 2nd master tick, 20 Hz every
  3rd, 10 Hz every 6th.
- Commands may be enqueued from any thread (network, AI bridge, tests); they are
  applied only at a defined point of the tick.
- Off-thread I/O completions are enqueued as typed work and applied only at
  explicit safe tick boundaries; nothing blocks a tick. [J.7.2]

## Data ownership (derived)

Owns the Flecs world instance, the clock, and `TickId`. Commands and completions
are values copied into the queues.

## Integration points (derived)

- Registration API: feature modules add systems to named phases and tick domains.
- `CommandQueue`: ingress for client commands (via NetworkAdapter or a local
  client), AI intents, GameplayFlow actions.
- Events: tick stamping and buffer rotation.
- Physics: step and transform synchronization.
- Command validation hook that consults Gameplay restrictions
  ([06 §J.9.7](../system/06-gameplayflow-quests-cutscenes.md#client-control-suppression-is-not-security-j97)).

## Acceptance tests (Stage A, derived)

- [ ] The clock advances a fixed step independent of wall-clock time; running N
      ticks twice from the same inputs gives the same state.
- [ ] Systems registered in 30/20/10 Hz domains run at exactly the expected
      master ticks.
- [ ] Phase order is explicit and stable.
- [ ] Commands enqueued from another thread are applied only at the tick's
      ingress point.
- [ ] A test-only module registers systems without Simulation depending on it.
- [ ] Phase 1 proof: one headless authoritative world with movement/collision and
      command → fact flow.

## Future extensions

Client simulation surface for prediction ([OQ-15](../system/11-open-questions.md#oq-15)).

## Open questions

- Phase names and which systems run in which tick domain
  ([OQ-12](../system/11-open-questions.md#oq-12)).
- Whether Flecs is exposed to registering modules (see [ECS](ECS.md)).
