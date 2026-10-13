# 06 — GameplayFlow, quests and cutscenes

This file defines the Engine's runtime for scripted gameplay (GameplayFlow), source-owned restrictions, cutscenes, and the quest runtime with its quest-creation API and durable bridge.

Source: ARCH § Engine gameplay APIs (Behavior registry contract, Scripted sequences (GameplayFlow) and restrictions,
Quest runtime and the quest-creation API), § Repositories, build targets and processes (Engine modules).

All examples below are neutral format examples. Concrete sequences, quests, encounters and cues are RunicGame content
and never appear in this folder.

## Package map

| Package | What | Needs | Slice |
| --- | --- | --- | --- |
| GPL-3 | Restrictions: source-owned restriction sets, acquire and release by source, command-validation hooks | GPL-2 | 18 |
| GPL-5 | Interaction framework: interactables, reservation, interaction facts | GPL-2, EVT-1 | 29 |
| FLOW-1 | Sequence runtime: trigger, condition and action registries, scheduler, timers on simulation time | GPL-5 | 29 |
| FLOW-2 | Termination path, restriction ownership, watchdog, presentation cues | FLOW-1, GPL-3 | 29 |
| FLOW-3 | Handoff policies and sequence state in the transfer snapshot | FLOW-2, DSIM-3 | 37 |
| QST-1 | Quest runtime and quest-creation API: definitions, objectives, triggers from facts, instances | FLOW-1 | 30 |
| QST-2 | Durable bridge through the service port; accepted results applied at safe ticks | QST-1, SIM-5 | 30 |
| QST-3 | Quest instances registered as a transfer-snapshot contributor | QST-1, DSIM-3 | 37 |

## GameplayAPI group at a glance

GameplayFlow and Quest belong to the GameplayAPI group with Gameplay and Combat. Each is its own module with its own
boundary and tests. The Engine knows timelines, tags, statuses, rules, threat and perception, but no concrete rule.

| API | Module | What the Engine provides | What a game supplies |
| --- | --- | --- | --- |
| Ability toolkit | Combat | Timeline runner in DomainTickCount (startup, active, recovery, cancel windows); hit volumes and projectile queries; cooldowns; resource pools; one-slot input queue | Ability definitions, timings, costs, queue window |
| Hit timing | Combat | ServerTime and CappedRewind (with a per-character position history), chosen per ability | The choice per ability and the cap |
| Tags | Gameplay | Hierarchical, interned tags on abilities, statuses and entities | The tag vocabulary |
| Status container | Combat | Timed statuses with stacking policy, duration, periodic tick and immunity windows | Every concrete status |
| Attributes and modifiers | Gameplay | Base values and modifier stacks (add, multiply, override) with caps | Stats and formulas |
| Rule pipeline | Combat | Registration of typed conditions and modifiers; evaluation in named stages; outputs to per-worker buffers; compilation into a resolved loadout per character; value variants chosen by a per-hit context (e.g., PvE and PvP) | Stage order and every condition and modifier |
| Damage interception | Combat | Hooks before and after damage is applied; invulnerability windows | Defense rules, armor, the damage formula |
| Threat table | Combat | Per-NPC threat storage read by AI target selection | What generates threat and how it is overridden |
| Behavior registries | Combat | ID-to-C++ registration, parameter schemas, a fixed hook list, read-only context and output buffers | The behaviors |
| Restrictions | Gameplay | Source-owned restriction sets (input, movement, abilities, interactions, targeting, damage, external motion), release by source, command validation | Which skills, effects and sequences acquire which restrictions |
| Interactions | Gameplay | Interaction volumes, reach and permission checks, reservation, an interaction registry | The concrete interactions |
| Movement | Gameplay | CharacterVirtual integration; walk, swim, glide (reads wind), mount and vehicle modes; prediction hooks; server validation | Speeds, dashes, mount and glider stats |
| Item runtime | Gameplay | Runtime item instances and containers mirrored from services | Item definitions and rules |
| Scripted sequences | GameplayFlow | Sequence runtime, trigger, condition and action registries, timers, termination path, handoff policies | Boss phases, cutscenes, dungeon and world-event scripts |
| Quests | Quest | Quest runtime and quest-creation API, generic triggers, conditions and actions, durable-fact bridge | Every quest; Game-specific triggers, conditions and actions |

## GameplayFlow model

GameplayFlow is **the one authoritative runtime for scripted gameplay**. Quests, boss phases, dungeons, cutscenes,
escorts, tutorials and world events all run on it instead of each inventing its own timers and state machines.

A sequence is a graph of steps. Each step waits for a trigger, checks conditions, runs actions and moves to the next
state:

```text
TRIGGER → CONDITIONS → ACTIONS → STATE TRANSITION
```

- **Triggers and conditions** read authoritative facts and state.
- **Actions change the world only through Simulation commands and Gameplay APIs**, the same doors players and AI use.
  Starting an ability is a UseAbility command; changing an NPC's behavior is an AI profile command.
- **GameplayFlow never calls Combat or AI internals** (decided, [11 § OQ-18](11-open-questions.md#oq-18)). Its
  allowed dependencies are Gameplay and DistributedSimulation (handoff snapshot) only.
- Sequences run on the simulation thread at phase boundaries.
- **Timers use simulation time, never wall-clock time.** A wait is a time or a domain-tagged tick count.

### Built-in action categories

The Engine provides these under `engine:` IDs.

| Category | Generic actions |
| --- | --- |
| Entity | Spawn, despawn, enable, disable |
| State | Set a tag, state or value; set an objective or sequence variable |
| Movement | Move an entity, follow a path, teleport where explicitly legal |
| Interaction | Enable or disable an interaction; reserve or release a target |
| Combat | Request an ability through a command; apply an approved effect; end an encounter state |
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

## Registries and namespaces

- Every extension is registered by ID into a registry; data refers to the ID.
- **`engine:` IDs** are Engine built-ins. **`game:` IDs** are registered by RunicGame (server only) for mechanics that
  are not reusable.
- An unknown ID fails at load, not mid-encounter. Parameters have schemas checked at load and by the Cooker.

```cpp
// Illustrative
actions.Register("engine:spawn_entity", SpawnEntityAction{});
actions.Register("engine:start_sequence", StartSequenceAction{});

// RunicGame, server only (format example)
actions.Register("game:<action_name>", GameActionType{});
```

Neutral example of a sequence (format only; the Game writes its own):

```text
Sequence: game:<encounter>_phase_2
  Wait:     Boss.Health <= <threshold>
  Actions:  SetEncounterPhase(2)
            CloseDoors(<arena doors>)
            AcquireRestriction(Boss, DamageImmune)
            PresentationCue("game:<cue>")
            Spawn(<adds>)
            WaitFor(AllSpawnedDead)
            ReleaseRestriction(Boss)
            RequestAbility("game:<ability>")
```

The phase, the spawned entities, the cue and the ability are Game content. The waits, the cleanup, the fact handling
and the calls into Engine APIs are Engine behavior.

### Behavior registry contract

The same extension model serves abilities, item effects, AI nodes and interactions (ARCH § Engine gameplay APIs).

| Rule | Why |
| --- | --- |
| Behaviors are registered by ID; data refers to the ID | Content stays data; an unknown ID fails at load, not mid-fight |
| Parameters have a schema checked at load | Typos are caught by the Cooker, not by players |
| Hooks are a fixed list: OnActivate, OnPhaseChange, OnHitCandidate, OnHit, OnEffectTick, OnEffectExpire, OnKill, OnInteract | The engine knows exactly when custom code runs |
| A behavior subscribes only to the hooks it uses | Abilities without custom code cost nothing |
| Behaviors read a context and write an output buffer | Safe in parallel phases; outputs merge in a fixed order |
| Behaviors are stateless; per-instance state lives in components | Inspection and data reload stay simple |
| The same registry serves abilities, item effects, AI nodes and interactions | One extension model for any game |
| Data reloads live; changed C++ needs a restart | A scripting language can be added later behind the same registry |

## Source-owned restrictions

No scattered flags such as `invulnerable = true`. A sequence, a skill or an effect acquires a restriction set under its
own source ID, and every exit path releases everything held by that source in one call. Restrictions live in
Runic::Gameplay (GPL-3); invulnerability windows use the same mechanism.

```cpp
// Illustrative
RestrictionHandle handle = restrictions.Acquire(player, RestrictionSet{
    .input = Blocked, .movement = Blocked, .abilities = Blocked,
    .interactions = Blocked, .targeting = Untargetable,
    .damage = Immune, .externalMotion = Immune }, sequenceInstanceId);

restrictions.ReleaseAllFromSource(sequenceInstanceId);   // on every terminal path
```

- "Intangible during a cutscene" is several separate policies (input, movement, abilities, interactions, targeting,
  damage, external motion, optional collision), so other systems can hold their own restrictions at the same time.
- Blocked commands are rejected by server command validation, whatever the client does.

## One termination path

Complete, abort, cancel, timeout, disconnect, owner destruction, quest abandon, shutdown and transfer failure all
converge on **one routine** (FLOW-2). It:

1. cancels the sequence's timers;
2. releases the sequence's restrictions and reservations;
3. clears sequence-owned control;
4. emits `SequenceEnded` or `SequenceAborted`.

Temporary restrictions also carry an expiry and a watchdog diagnostic, so no fault can leave a player permanently
invulnerable.

## Handoff policies

Each sequence declares what happens when its owner's authority moves to another simulation (FLOW-3).

```cpp
enum class HandoffPolicy { Transferable, PinAuthority, AbortOnTransfer };
```

| Policy | Behavior |
| --- | --- |
| Transferable | Travels in the final transfer snapshot with its step, its timers relative to simulation time, its restriction source IDs and its quest or encounter context; timers are kept |
| PinAuthority | Delays the transfer until a short, critical sequence ends |
| AbortOnTransfer | Runs the termination path first, then the transfer proceeds |

GameplayFlow registers its sequences as a transfer-snapshot contributor
([05 § Transfer protocol](05-distributed-simulation.md#transfer-protocol)).

## Cutscenes

**Cutscenes are server sequences with client presentation.**

- The server runs the sequence, acquires restrictions and emits presentation cues.
- The client plays camera, animation, dialogue, music and UI from the Game's presentation definition and disables
  local controls for feel.
- The server keeps validating and rejecting blocked commands, so a modified client gains nothing.
- Music that players cannot override during a cue is an explicit, rare Game exception.

## Interactions

The interaction framework (GPL-5) provides interactables, reach and permission checks, reservation and interaction
facts. Two players cannot reserve the same target.

Flow of a world interaction:

1. The client sends an intent.
2. The simulation checks reach, state and permissions, executes and emits an interaction fact.
3. The quest runtime advances any quest in the same tick.
4. The fact travels with its stable ID to the durable service (see the durable bridge below).

## Quest runtime and the quest-creation API

The quest runtime (Runic::Quest) runs inside the simulation on top of GameplayFlow, so a kill or an interaction
advances a quest **in the same tick** that produced the fact. Durable progress lives in the Game's Go Quest service.
The Engine owns the runtime and the API; the Game owns every quest.

- **Definitions are data.** A quest is an ID, its states and objectives, and for each transition a trigger,
  conditions, actions and the next state. Designers write them without C++; the Cooker validates them.
- **Registries extend the vocabulary.** The Engine ships generic triggers (entity killed, item acquired, interaction
  completed, area entered, sequence ended), conditions (quest state, has item, entity alive, time window) and actions
  (set objective, start sequence, request an item grant or removal through a service, spawn). The Game registers
  `game:` triggers, conditions and actions in C++.
- **A builder for code.** The same definitions can be built in C++ for tests and generated content; data and code end
  in the same registry.

Neutral data example (format only):

```text
Quest: game:example_quest
  State OpenDoor
    Trigger:     InteractionCompleted(game:example_door)
    Conditions:  HasItem(game:example_key), EntityAlive(game:example_npc)
    Actions:     OpenDoor(game:example_door)
                 RemoveItem(game:example_key)
                 StartSequence(game:example_sequence)
                 SetObjective(Escape)
    Next:        Escape
```

```cpp
// Illustrative; Engine tests use engine: test IDs instead of game: IDs
QuestDefinition quest = QuestBuilder("engine:test_quest")
    .State("OpenDoor")
        .On(Trigger::InteractionCompleted("engine:test_door"))
        .If(Condition::HasItem("engine:test_key"))
        .If(Condition::EntityAlive("engine:test_npc"))
        .Do(Action::StartSequence("engine:test_sequence"))
        .Do(Action::SetObjective("Escape"))
        .GoTo("Escape")
    .Build();

quests.Register(std::move(quest));   // the same path loaded data takes
conditions.Register("game:<condition_name>", GameConditionType{});   // RunicGame only
```

## Durable quest bridge

Quest facts reach durable storage only through the Simulation's service port (QST-2, SIM-5).

1. Combat emits `EntityKilled` once, with a stable event ID.
2. The quest runtime consumes the fact, advances its local state and may start a sequence.
3. The fact leaves through the Simulation's service port; the Platform's services-bridge layer carries it to the Quest
   service, asynchronously (gRPC calls from PBND-1; NATS publishing and subscriptions from PBND-4, which slice 30
   delivers together with QST-2).
4. The service validates the source, deduplicates by event ID, updates progress and commits rewards in one
   transaction.
5. The accepted result returns to the simulation and is applied at a later safe tick; a service callback never touches
   Flecs directly.
6. Replaying the same fact changes nothing.

- **On login** the character's quest state loads from the service into the runtime.
- **On handoff** quest instances travel in the transfer snapshot with the rest of the player's state (QST-3).
- Platform owns transactions and the idempotency store; the Game Quest service owns quest-shaped storage
  (decided, [11 § OQ-16](11-open-questions.md#oq-16)).

## Rules for any session touching this area

- Do act only through Simulation commands and Gameplay APIs from GameplayFlow and Quest.
- Do route every exit of a sequence through the one termination path.
- Do acquire restrictions under a source ID and release them by source.
- Never read the wall clock in a sequence or quest; use simulation time.
- Never call Combat or AI internals from GameplayFlow or Quest.
- Never wait synchronously for a service; submit through the service port and wait for the accepted result.
- Never put a concrete quest, encounter, cue or status in Engine code, tests or this folder; use `engine:test_*` IDs.
