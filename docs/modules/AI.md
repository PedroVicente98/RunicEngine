---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/world-systems
  - headless/yes
  - slice/32
aliases:
  - "Runic::AI"
---

# AI

Contract seed for `Runic::AI`: what a Claude Code session needs to implement the AI work packages. When `Engine/WorldSystems/AI/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | World systems |
| Directory | `Engine/WorldSystems/AI/` |
| CMake target | `Runic::AI` |
| Namespace | `Runic::AI` |
| Headless | yes |
| Status | Not started |

## Purpose

AI is reasoning, not authority. The module reads permitted facts, keeps bounded working memory, ticks behavior trees and emits intents; the simulation validates and executes those intents exactly like player commands (ARCH[^arch] § AI architecture). RunicGame supplies the trees, personalities, perception values and game-specific nodes.

**Rule:** the behavior tree decides what an NPC wants to attempt; the authoritative simulation decides whether that action is legal and what actually happens.

## Owns

- Flecs components and the wake, perception, think, target and sleep systems, registered into the simulation-owned Flecs world.
- `BehaviorTreeRuntime` (the BehaviorTree.CPP adapter), the brain pool, per-worker intent buffers and the generic nodes.
- Working memory, AI scheduling (wake, sleep, leash), intents to commands, reactive combat.

| Decision | Direction (locked in ARCH[^arch]) |
| --- | --- |
| AI ownership | AI registers components and systems into the simulation-owned Flecs world; it never owns a separate world |
| Decision runtime | BehaviorTree.CPP, behind a Runic adapter |
| Communication | AI consumes authoritative Flecs state and event buffers and emits intents. Nodes never mutate combat, movement, physics or persistent state |
| Reactive combat | NPCs respond to perceived actions. Imperfection comes from reaction time, action commitment, resources, positioning, knowledge limits and small utility noise |
| Perception[^engine-perception] | Warn and detect radii come from the Perception API (base × light curve × concealment, clamped). No line-of-sight vision simulation. The Aggroable toggle covers non-perception cases (dead, GM, protected) |
| Wake and sleep | Brains are active only when relevant: warn or detect, damage, encounter or script activation, or another explicit wake source. Dormant and unloaded NPCs never tick full trees |
| Scheduler | Decisions and perception at 20 Hz, maintenance at 10 Hz; movement, combat and physics at 30 Hz |
| Editor[^engine-editor] dependency | Runic::AI knows nothing about editor or UI code; the Editor's AI panels depend on AI contracts, never the reverse |

| Stored in Flecs | Stored in the AI runtime |
| --- | --- |
| AIController, AIActive, AIWorkingMemory, CombatPersonality, target IDs, timers, aggro and leash state | BehaviorTree.CPP tree instances, blackboards, node factories, per-brain random stream, tree-debug instrumentation |
| State other systems need or tools inspect | Implementation details needed to execute trees |

| Wake source | Reason |
| --- | --- |
| Perception | A valid hostile Aggroable entity enters the warn radius (alert) or the detect radius (engage) |
| Damage received | An attacker wakes the NPC even from outside its radii |
| Encounter or script activation | Encounters, world events, escorts or scripts wake the AI explicitly |
| Game-specific extension | RunicGame may add wake signals through the public AI API |

Lifecycle: on warn, the NPC wakes Alerted and walks to where it sensed something without attacking; on detect or damage, it wakes engaged with a combat target. A target that dies, becomes non-aggroable, rises in concealment beyond the detect radius or disconnects triggers reevaluation. Leaving the leash boundary moves the NPC to ReturnHome; after any Game reset rules the brain returns to the pool. Unloading a region removes the entity entirely; no offline brain stays alive.

Engine reusable nodes: HasCombatTarget, HasReactableThreat, TargetInRange, HealthBelow, IsAlerted, ChooseDefense, ApproachTarget, InvestigateAlert, ReturnHome, plus generic ability-selection hooks and utility helpers. Encounter logic, skill IDs and other game-specific nodes are RunicGame's.

## Does not own

- Warn and detect radii and the light model (Perception[^engine-perception]).
- Paths (Navigation[^engine-navigation], behind the MoveTo and MoveGoal contract).
- Command validation and execution: movement (Gameplay), abilities, cooldowns, defenses and the input queue (Combat[^engine-combat]). AI never bypasses them.
- The AI editor, inspectors and live debugger (Editor[^engine-editor], EDT-5 and EDT-6).
- Trees, personalities, perception values, NPC definitions, encounters and game-specific nodes (RunicGame).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Gameplay[^engine-gameplay], Combat[^engine-combat], Perception[^engine-perception], Navigation[^engine-navigation] |
| Third-party (PRIVATE) | BehaviorTree.CPP |

Approved public-header exceptions used here (record both in this module's ARCHITECTURE.md; GLM math types from Core are the third exception and need no record here):

1. The Flecs world handle, for registering systems: `AIModule::Register(flecs::world&)`.
2. BehaviorTree.CPP node base types, so RunicGame can write custom AI nodes.

No other library type may appear in `Public/`.

### Must never depend on

- ImGui, bgfx, RmlUi, editor windows or client presentation. Runic::AI builds and runs headless.
- Direct combat, physics or persistence mutation: AI emits intents that become validated commands.
- GameplayFlow[^engine-gameplayflow] or Quest (they drive AI through AI commands), Network, Water[^engine-water], Environment[^engine-environment].
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH[^arch]'s public layout: `AIModule.hpp`, `AIComponents.hpp`, `AIDefinition.hpp`, `AIIntent.hpp`. `Source/` subsystems: `Runtime/` (BehaviorTreeRuntime, BrainPool), `BehaviorTree/` (conditions, actions, utility decision), `Systems/` (aggro, perception, think, target and sleep systems).

```cpp
// Illustrative contract, not a frozen API (ARCH § AI architecture).
class AIModule {
public:
    void Register(flecs::world& world);   // approved Flecs exception
private:
    BehaviorTreeRuntime m_BehaviorTrees;
    AIIntentBuffers     m_Intents;      // one buffer per worker
};

struct AIController {
    AIDefinitionId definition = 0;
    BrainHandle brain = 0;            // 0 while sleeping
};

struct AIDefinition {                 // shared content; many NPCs reference one
    AIDefinitionId id = 0;
    std::string behaviorTreeName;
    CombatPersonality defaultPersonality;
    PerceptionProfileId defaultPerception;
    float defaultLeashRadius = 30.0f;
};

struct AIIntent {
    EntityId actor = 0;
    AIIntentType type {};
    EntityId target = 0;
    AbilityId ability = 0;
    Vec3d position {};
};
```

`AIIntentType` covers MoveTo, FaceTarget, UseAbility, BeginDefense, EndDefense, Evade, ClearTarget, ReturnHome and Investigate. Defensive intents carry a game-registered `DefenseId`; the Game's registered defense chooser fills the choice, and the Game maps each ID to its own defensive rules. The Engine knows no concrete defense.

```cpp
class BehaviorTreeRuntime {
public:
    void        RegisterTree(AIDefinitionId definition, std::string_view xml);
    BrainHandle Wake(EntityId entity, AIDefinitionId definition);   // barrier only
    void        Sleep(BrainHandle brain);                            // barrier only
    void        Tick(BrainHandle brain, double simulationTime, WorkerIndex worker);
};
```

## Threading and tick rules

| Domain and phase | AI responsibility | Result |
| --- | --- | --- |
| 10 Hz maintenance | Wake scans, sleep and leash maintenance, low-priority idle and world AI | Activation changes and coarse intentions; no realtime combat |
| 20 Hz pre-realtime | Validate targets, advance memory, tick active trees, translate output into intents and commands | Commands ready for the next realtime phase |
| 30 Hz realtime | (Other modules) apply commands, movement, combat timelines, defenses, abilities, Jolt queries, hits, damage and effects | Authoritative results and gameplay facts |
| 20 Hz post-realtime | Consume relevant combat facts and update perception and memory | Knowledge ready for a later decision tick |

- Brains tick in parallel by NPC; each worker writes intents into its own buffer.
- Brain creation and release (wake, sleep) happen only at the barrier. Intents are applied at the barrier in a fixed worker order.
- Each brain has its own seeded random stream, so decisions are reproducible.
- Spatial candidate lookup from cells and sectors always comes before perception filtering; never "for every NPC, query every player".
- Trees never subscribe to the global event stream: perception systems filter facts into bounded working memory, and trees read that memory.
- Hot combat facts travel in event buffers, not Flecs observers. AIActive and Aggroable are toggles, not add and remove.
- A tree is never destroyed or replaced while it ticks; live replacement is a development-only safe-point operation.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| AI-1 | AI contracts and runtime shell: components, intents, working memory, AIDefinition, BehaviorTree.CPP adapter, brain pool, wake and sleep | A test tree ticks only while its NPC is awake | GPL-2[^engine-gameplay], SIM-3[^engine-simulation] | 32 |
| AI-2 | Command bridge and data assets: intents become movement and combat commands; tree loading, registries, personality profiles | AI commands pass the same validation as player commands | AI-1, NAV-1[^engine-navigation], CMB-2[^engine-combat] | 32 |
| AI-3 | Perception[^engine-perception] lifecycle: Aggroable, warn and detect wake, target validation, leash, return home, sleep | A mob walks toward a warning, attacks on detection and returns past its leash | AI-1, PRC-3 | 33 |
| AI-4 | Reactive combat: SkillStarted perception, incoming threat, reaction timer, defense choice | A mob answers a telegraphed skill with a defensive action within its reaction time | AI-2, CMB-5 | 34 |
| AI-5 | Instrumentation: per-brain trace, memory, utility and intent diagnostics | The trace explains each decision | AI-2 | 34 |
| AI-6 | Scale tests with thousands of loaded NPCs and controlled awake percentages | The tick budget holds at the target counts | AI-4 | 38 |


ARCH[^arch]'s implementation steps map to these packages: contracts and runtime shell are AI-1; command bridge and data assets are AI-2; the perception lifecycle is AI-3; reactive combat is AI-4; instrumentation is AI-5; scale tests are AI-6. The AI editor steps are EDT-5[^engine-editor] and EDT-6.

## Acceptance tests

| Area | Representative tests |
| --- | --- |
| Perception[^engine-perception] | A hostile entering the detect radius wakes the NPC engaged; entering only the warn radius wakes it alerted; high concealment shrinks the radii; the minimum radius still detects in total darkness; damage wakes from outside both radii |
| Target validity | Death, concealment beyond detect range or a leash break invalidates or changes behavior according to policy |
| Reaction timing | A threat before reactAt fails HasReactableThreat; the same threat after reactAt succeeds |
| Utility | Unavailable actions score invalid; personality biases change ordering; a fixed seed reproduces a choice |
| Intent boundary | A tree action emits the expected intent and never changes combat state directly |
| Command rejection | Choosing a defense while committed is rejected; AI never bypasses combat rules |
| Wake and sleep | A BrainHandle is created on wake and recycled on sleep; dormant entities never tick a tree |

Integration tests (ARCH[^arch] § AI architecture, tests):

1. An NPC detects a player, approaches, attacks, reacts to a telegraphed attack and returns home after a leash break.
2. At night an unlit player passes outside the reduced detect radius unnoticed, while a player carrying a light is detected from farther away.
3. A player raises concealment while engaged; the target is dropped according to policy, and lowering it makes the player detectable again.
4. An NPC chooses a defense after its action became non-cancellable and correctly fails to execute it.
5. A SkillStarted fact produced in the realtime phase reaches AI memory only in the post-realtime phase and is acted on in a later pre-realtime tick.
6. Runic::AI builds and runs headless with no ImGui, bgfx or editor dependency.
7. Selecting a live NPC in the Editor[^engine-editor] shows the same BrainHandle, tree, memory and intents the runtime instrumentation records (with EDT-6).

Tests use Engine test trees, personalities and `engine:` ability IDs, never RunicGame content.

## Open items

- Decided in ARCH[^arch] revision 369: ECS[^engine-ecs] re-exports Flecs to the Engine modules that register systems, so AI uses the Flecs C++ API through ECS (see [ECS.md](ECS.md)).
- Decided in ARCH revision 369: the Engine emits BeginDefense, EndDefense or Evade with a game-registered `DefenseId`, and a defense chooser the Game registers picks the defense from the personality, the incoming threat and the random stream. The AI-4 contract session fixes the registration interface.
- If per-brain BehaviorTree.CPP cost becomes material at very high counts, the public Runic AI contracts stay and a data-oriented Runic executor may be considered later, only after profiling (ARCH). Not part of any current package.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Gameplay|Gameplay]] · [[../modules/Combat|Combat]] · [[../modules/Perception|Perception]] · [[../modules/Navigation|Navigation]]
- **Its packages need:** [[../modules/Simulation|Simulation]]
- **Packages that need it:** [[../modules/Editor|Editor]] · [[../../../RunicGame/docs/game-packages|Game work packages]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-perception]: [[../modules/Perception|Perception]] — Engine module · World systems
[^engine-editor]: [[../modules/Editor|Editor]] — Engine module · Tools
[^engine-navigation]: [[../modules/Navigation|Navigation]] — Engine module · World systems
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-gameplay]: [[../modules/Gameplay|Gameplay]] — Engine module · GameplayAPI
[^engine-gameplayflow]: [[../modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-water]: [[../modules/Water|Water]] — Engine module · World systems
[^engine-environment]: [[../modules/Environment|Environment]] — Engine module · World systems
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-ecs]: [[../modules/ECS|ECS]] — Engine module · Simulation core
