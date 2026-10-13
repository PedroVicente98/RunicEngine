---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/gameplay-api
  - headless/yes
  - slice/29
aliases:
  - "Runic::GameplayFlow"
---

# GameplayFlow

Contract seed for `Runic::GameplayFlow`: what a Claude Code session needs to implement the GameplayFlow work packages. When `Engine/GameplayAPI/GameplayFlow/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | GameplayAPI (linked through the umbrella target `Runic::GameplayAPI`) |
| Directory | `Engine/GameplayAPI/GameplayFlow/` |
| CMake target | `Runic::GameplayFlow` |
| Namespace | `Runic::GameplayFlow` |
| Headless | yes |
| Status | Not started |

## Purpose

GameplayFlow is the one authoritative runtime for scripted gameplay. Quests, boss phases, dungeons, cutscenes, escorts, tutorials and world events all run on it instead of each inventing its own timers and state machines (ARCH[^arch] § Engine gameplay APIs, scripted sequences and restrictions). The Game supplies every concrete sequence.

## Owns

- The sequence runtime: trigger, condition and action registries, a scheduler, timers on simulation time (FLOW-1).
- The termination path, restriction ownership for sequences, a watchdog, and presentation cues (FLOW-2).
- Handoff policies and sequence state in the transfer snapshot (FLOW-3).
- The Engine's built-in actions and conditions under `engine:` IDs.

A sequence is a graph of steps. Each step waits for a trigger, checks conditions, runs actions and moves to the next state:

```text
TRIGGER → CONDITIONS → ACTIONS → STATE TRANSITION
```

| Category | Generic actions |
| --- | --- |
| Entity | Spawn, despawn, enable, disable |
| State | Set a tag, state or value; set an objective or sequence variable |
| Movement | Move an entity, follow a path, teleport where explicitly legal |
| Interaction | Enable or disable an interaction; reserve or release a target |
| Combat[^engine-combat] | Request an ability through a command; apply an approved effect; end an encounter state |
| AI | Change a behavior profile or state through AI commands |
| World | Open or close a door, activate an object, set a world-state value |
| Timing | Delay, timeout, wait until a simulation time |
| Sequence | Start, stop or wait for a nested sequence |
| Event | Wait for or emit an authoritative fact |
| Restriction | Acquire or release input, movement, ability, interaction, targeting or damage restrictions |
| Presentation | Start dialogue or emit a presentation cue for clients |
| Service | Submit an asynchronous durable operation and wait for its accepted result |
| Branching | If, switch, random choice with an explicit seed |
| Synchronization | Wait for an entity, party, event or encounter condition |

**One termination path.** Complete, abort, cancel, timeout, disconnect, owner destruction, quest abandon, shutdown and transfer failure all converge on one routine. It cancels timers, releases the sequence's restrictions and reservations, clears sequence-owned control and emits SequenceEnded or SequenceAborted.

**Cutscenes are server sequences with client presentation.** The server runs the sequence, acquires restrictions and emits presentation cues. The client plays the presentation and disables local controls for feel; the server keeps validating and rejecting blocked commands.

## Does not own

- Restrictions as a mechanism (Gameplay); GameplayFlow acquires and releases them under the sequence's source ID.
- Combat[^engine-combat] and AI internals: sequences act through Simulation commands and Gameplay APIs, the same doors players and AI use.
- Quests (Quest builds on this module), the transfer protocol (DistributedSimulation[^engine-distributedsimulation]), durable services (reached through the Simulation's service port).
- Every concrete sequence, cue and `game:` action (RunicGame, server only).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Gameplay[^engine-gameplay], DistributedSimulation[^engine-distributedsimulation] (handoff snapshot) |
| Third-party | — |

DistributedSimulation is needed only for the handoff snapshot (FLOW-3); a package links only what it uses.

### Must never depend on

- Combat[^engine-combat] or AI internals (decided in OQ-18[^oq-18]: GameplayFlow acts only through Simulation commands and Gameplay APIs).
- Quest (Quest depends on GameplayFlow).
- Presentation modules: cues are data for clients, never calls into Client, UI or Audio.
- Game content in code or tests; RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

Registration (ARCH[^arch], illustrative):

```cpp
actions.Register("engine:spawn_entity", SpawnEntityAction{});
actions.Register("engine:start_sequence", StartSequenceAction{});
// RunicGame registers its own actions under game: IDs, server only.
```

Handoff policy (ARCH, illustrative):

```cpp
enum class HandoffPolicy { Transferable, PinAuthority, AbortOnTransfer };
```

A Transferable sequence travels in the final transfer snapshot with its step, its timers relative to simulation time, its restriction source IDs and its quest or encounter context. PinAuthority delays the transfer until a short, critical sequence ends. AbortOnTransfer runs the termination path first.

Headers (sketch, fixed in the contract session): `Sequence.hpp`, `SequenceRuntime.hpp`, `FlowRegistries.hpp`, `FlowActions.hpp`, `PresentationCue.hpp`, `HandoffPolicy.hpp`, `FlowEvents.hpp`.

## Threading and tick rules

- Sequences run on the simulation thread at phase boundaries.
- Timers use simulation time, never wall-clock time.
- Triggers and conditions read authoritative facts and state; actions change the world only through commands and Gameplay APIs.
- A service action submits through the service port and resumes when the accepted result is applied at a later safe tick.
- Random choices use an explicit seed.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| FLOW-1 | Sequence runtime: trigger, condition and action registries, scheduler, timers on simulation time | A sequence waits, branches and acts only through commands and Gameplay APIs | GPL-5[^engine-gameplay] | 29 |
| FLOW-2 | Termination path, restriction ownership, watchdog, presentation cues | Complete, abort, timeout, disconnect and shutdown all release everything the sequence held | FLOW-1, GPL-3 | 29 |
| FLOW-3 | Handoff policies and sequence state in the transfer snapshot | Transferable keeps its timers; PinAuthority blocks transfer; AbortOnTransfer cleans up | FLOW-2, DSIM-3[^engine-distributedsimulation] | 37 |

## Acceptance tests

The Done when column is the acceptance list. Slice 29's runnable result (a scripted phase locks an area, a cutscene protects the player, a disconnect releases everything) is reproduced with Engine test sequences.

## Open items

- [OQ-18](../system/11-open-questions.md#oq-18) (decided, option 1): GameplayFlow acts only through Simulation commands and Gameplay APIs (FLOW-1).
- FLOW-3 registers sequence state into DistributedSimulation[^engine-distributedsimulation]'s snapshot-contributor registry (DSIM-3).

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Gameplay|Gameplay]] · [[../modules/DistributedSimulation|DistributedSimulation]]
- **Depended on by:** [[../modules/Quest|Quest]]
- **Packages that need it:** [[../../../RunicGame/docs/game-packages|Game work packages]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-distributedsimulation]: [[../modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-gameplay]: [[../modules/Gameplay|Gameplay]] — Engine module · GameplayAPI
[^oq-18]: [[../system/11-open-questions#OQ-18 — GameplayFlow built-in actions versus its dependency list (decided 2026-10-09)|OQ-18]] — open question or decision
