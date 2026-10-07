# 06 — GameplayFlow, quests, scripted actions, bosses, and cutscenes

## Quest is a consumer of GameplayFlow [J.9.1]

Quest must not own the only scripting engine in the project. Bosses, dungeons,
world events, escorts, tutorials, scripted interactions, and cinematics need the
same authoritative sequence/action/condition machinery, so **GameplayFlow sits
below Quest** and is reusable by all of them.

```text
Gameplay Events
      |
      +------> Quest -------------------+
      |                                |
      +------> Encounter --------------+
      |                                v
      +------> World Event ------> GameplayFlow
      |                                |
      +------> Boss Script ------------+
                                       |
                                       v
                              Gameplay / Combat / AI / World
```

| Piece | Repository / module |
| --- | --- |
| Sequence runtime, steps, waits, timers, cleanup, action/condition registries | Engine `GameplayFlow` |
| Quest instance/objective/trigger/condition/action runtime | Engine `Quest` |
| Gameplay restrictions (source-owned) | Engine `Gameplay` (`GameplayRestrictions`) |
| Concrete quests, encounters, boss phases, cutscene definitions, private actions | RunicGame (`GameQuests`, `GameEncounters`, `GameClientPresentation`) |
| Durable quest progression and rewards | RunicGame private `QuestService` on Platform `ServiceRuntime`/`Persistence` |

## Trigger → conditions → actions → transition [J.9.2]

```text
TRIGGER
  |
CONDITIONS
  |
ACTIONS
  |
STATE TRANSITION
```

Quest and encounter definitions may be data-driven graphs over **registered**
triggers, conditions, and actions. Engine provides built-ins; Game registers
private actions/conditions for mechanics that are not generally reusable. Engine
defines extensible registries, **not** a giant hard-coded enum of every possible
RunicGame action. [J.4.19]

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

```cpp
// Conceptual registration
actions.Register("engine:spawn_entity",   SpawnEntityAction{});
actions.Register("engine:start_sequence", StartSequenceAction{});

// Closed-source Game extension:
actions.Register("game:set_trade_route_state", GameSetTradeRouteStateAction{});
```

IDs are namespaced: `engine:` for built-ins, `game:` for RunicGame content
(see Engine `Content`).

## Built-in action categories [J.9.3]

| Category | Representative generic actions |
| --- | --- |
| Entity | Spawn, despawn, enable, disable. |
| State | Set tag/state/value; set objective or generic state variable. |
| Movement | Move entity, teleport when explicitly legal, follow path, scripted locomotion. |
| Interaction | Enable/disable an interaction; reserve/release an interaction target. |
| Combat | Start combat, request ability, apply approved effect through Combat, end encounter state. |
| AI | Change behavior/profile/state through AI APIs. |
| World | Open/close door, activate object, set world-state primitive. |
| Timing | Delay, timeout, schedule, wait until simulation time. |
| Sequence | Start/stop/wait for nested sequence. |
| Event | Wait for or emit an authoritative gameplay fact where semantically valid. |
| Player restriction | Acquire/release movement/input/ability/interaction/targeting/damage restrictions. |
| Dialogue/presentation cue | Start dialogue or emit a client presentation cue; the server still owns gameplay state. |
| Service | Submit an asynchronous durable operation and wait for an accepted result when required. |
| Branching | If/else, switch, deterministic/random-with-explicit-seed selection. |
| Synchronization | Wait for entity/party/event/encounter condition. |

Every action goes through the owning Engine API (Combat, AI, World, Gameplay…);
actions never touch Jolt, bgfx, or network backends. [J.11.2]

## Boss sequence example [J.9.4]

```text
Sequence: BossPhase2

Wait:
    Boss.Health <= 70%

Actions:
    SetBossPhase(2)
    LockArenaDoors
    AcquireBossInvulnerability
    StartPresentationCue("game:dragon_phase_2")
    SpawnAdds
    WaitFor(AllAddsDead)
    ReleaseBossInvulnerability
    RequestAbility("game:dragon_firestorm")
```

The phase, add set, presentation cue, and ability are Game content. The sequence
runtime, waits, cleanup semantics, event integration, and calls into Engine APIs
are reusable Engine behavior.

## Server-authoritative cutscenes [J.9.5]

The server does not render a cinematic. It runs an authoritative gameplay
sequence and emits presentation cues; the client renders camera, animation,
dialogue, letterboxing, VFX, music, and UI from the Game presentation definition.

```text
Server:
    Start Sequence #12841
    acquire gameplay restrictions
    execute authoritative scripted actions
    emit presentation cue

Client:
    play camera / animation / dialogue / audio
    disable local controls for UX

Server:
    continue validating/rejecting blocked commands
    complete or abort sequence
    release sequence-owned restrictions
    emit SequenceEnded
```

## Source-owned gameplay restrictions [J.9.6]

Never implement a cutscene with scattered booleans (`invulnerable = true` …
later `invulnerable = false`). That breaks under aborts, disconnects, handoff,
exceptions, quest cancellation, or script failure. Restrictions are acquired with
a stable **source** and released centrally.

```cpp
// Conceptual
RestrictionHandle handle = restrictions.Acquire(
    player,
    RestrictionSet{
        .input           = Blocked,
        .movement        = Blocked,
        .abilities       = Blocked,
        .interactions    = Blocked,
        .targeting       = Untargetable,
        .damage          = Immune,
        .external_motion = Immune
    },
    sequenceInstanceId);

// Every terminal path eventually calls:
restrictions.ReleaseAllFromSource(sequenceInstanceId);
```

"Intangible during cutscene" is decomposed into independent authoritative
policies: input suppression, movement lock, ability lock, interaction lock,
untargetable state, damage immunity, external-motion immunity, optional collision
policy, and sequence-controlled movement. There is no single overloaded
`IsInCutscene` flag, and other systems may acquire restrictions independently.

## Client control suppression is not security [J.9.7]

```text
Client UI/input:
    controls disabled

Hacked client still sends MoveCommand / UseAbility / Interact
    |
    v
Server CommandValidation
    |
    restriction present?
          yes -> reject
```

The client-side lock exists for presentation and feel only. The authoritative
simulation independently rejects movement, ability, interaction, targeting, or
any other command forbidden by the active restriction set.

## Termination and watchdog rules [J.9.8]

All terminal paths — Complete, Abort, Cancel, Timeout, disconnect handling, owner
destruction, quest abandon, simulation shutdown, transfer failure — converge on
**one** sequence-termination routine. It:

1. cancels timers;
2. releases sequence-owned restrictions and reservations;
3. clears sequence-owned control;
4. emits `SequenceEnded` or `SequenceAborted`.

Temporary restriction sources also support expiry/watchdog diagnostics so that
corruption cannot leave a player permanently invulnerable.

## Handoff policy for active sequences [J.9.9]

```cpp
enum class HandoffPolicy { Transferable, PinAuthority, AbortOnTransfer };
```

| Policy | Behavior |
| --- | --- |
| `Transferable` | The active instance is serialized into the final transfer snapshot and resumes on the new owner. |
| `PinAuthority` | Normal authority migration is blocked until the critical sequence finishes. |
| `AbortOnTransfer` | A local ephemeral sequence aborts and runs cleanup before transfer. |

```text
FinalTransferSnapshot
├── Player authoritative gameplay state
├── active effects
├── active restriction sources
├── GameplayFlow SequenceInstance
├── current step/state
├── relative timers
├── quest/encounter context
└── authority epoch / last processed input sequence
```

Timers are stored **relative to simulation time** so they survive the move.
