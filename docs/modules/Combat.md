---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/gameplay-api
  - headless/yes
  - slice/16
aliases:
  - "Runic::Combat"
---

# Combat

Contract seed for `Runic::Combat`: what a Claude Code session needs to implement the Combat work packages. When `Engine/GameplayAPI/Combat/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | GameplayAPI (linked through the umbrella target `Runic::GameplayAPI`) |
| Directory | `Engine/GameplayAPI/Combat/` |
| CMake target | `Runic::Combat` |
| Namespace | `Runic::Combat` |
| Headless | yes |
| Status | Not started |

## Purpose

Combat is the Engine's server-authoritative action-combat toolkit: abilities and their timelines, hit timing, the one-slot input queue, targeting, hit detection, statuses, cooldowns, the rule pipeline, behavior registries, damage interception, threat and crowd control, and foreign combat across simulation borders (ARCH[^arch] § Engine gameplay APIs). The Engine knows timelines, tags, statuses, rules and threat, but no concrete status, defense or formula.

## Owns

| API | What the Engine provides | What a game supplies |
| --- | --- | --- |
| Ability toolkit | Timeline runner in DomainTickCount (startup, active, recovery, cancel windows); hit volumes and projectile queries; cooldowns; resource pools; one-slot input queue | Ability definitions, timings, costs, queue window |
| Hit timing | ServerTime and CappedRewind (with a per-character position history), chosen per ability | The choice per ability and the cap |
| Status container | Timed statuses with stacking policy, duration, periodic tick and immunity windows | Every concrete status |
| Rule pipeline | Registration of typed conditions and modifiers; evaluation in named stages; outputs to per-worker buffers; compilation into a resolved loadout per character; value variants chosen by a per-hit context (for example PvE and PvP): a value has a base and optional contextual variants, and the context of each hit picks one | Stage order, every condition and modifier, the contexts and how a hit's context is decided |
| Damage interception | Hooks before and after damage is applied; invulnerability windows | Defenses and the damage formula |
| Threat table | Per-NPC threat storage read by AI target selection | What generates threat; taunt |
| Behavior registries | ID-to-C++ registration, parameter schemas, a fixed hook list, read-only context and output buffers | The behaviors |

Also owned: targeting modes (free aim, FriendlyLock, light aim-assist hooks), combat volumes and hit detection over Physics[^engine-physics] queries (CMB-2); crowd control with a target cap and immunity windows (CMB-6); the foreign-combat path where volumes crossing an authority border become requests to the owner (CMB-8).

### Behavior registry contract

| Rule | Why |
| --- | --- |
| Behaviors are registered by ID; data refers to the ID | Content stays data; an unknown ID fails at load, not mid-fight |
| Parameters have a schema checked at load | Typos are caught by the Cooker[^engine-cooker], not by players |
| Hooks are a fixed list: OnActivate, OnPhaseChange, OnHitCandidate, OnHit, OnEffectTick, OnEffectExpire, OnKill, OnInteract | The engine knows exactly when custom code runs |
| A behavior subscribes only to the hooks it uses | Abilities without custom code cost nothing |
| Behaviors read a context and write an output buffer | Safe in parallel phases; outputs merge in a fixed order |
| Behaviors are stateless; per-instance state lives in components | Inspection and data reload stay simple |
| The same registry serves abilities, item effects, AI nodes and interactions | One extension model for any game |
| Data reloads live; changed C++ needs a restart | A scripting language can be added later behind the same registry |

### Input queue (decided)

- The queue lives in Combat, which knows each ability's recovery and cancel windows; Network only carries the inputs. The client may send the next action before the current one ends, inside a window the game sets.
- The server keeps at most one queued action per character and starts it on the first tick after recovery ends, so at most one 30 Hz tick late.
- A queued action is validated when it starts, not when it arrives; a stun in between cancels it.
- Priority inputs defined by the game skip the queue when the current ability's cancel window allows.

## Does not own

- Attributes, modifiers, tags, resources and restrictions (Gameplay); Combat reads and applies them.
- Geometry (Physics[^engine-physics]): Combat interprets Physics query results.
- Authority, epochs and the foreign-interaction contract (DistributedSimulation[^engine-distributedsimulation]); Combat produces and consumes its requests.
- AI decisions: AI emits intents that become the same validated commands players use.
- Every concrete ability, status, defense, damage type, formula, resolver order and threat rule (RunicGame).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Gameplay[^engine-gameplay], Physics[^engine-physics], Environment[^engine-environment] (projectile wind), DistributedSimulation[^engine-distributedsimulation] |
| Third-party | — |

The list is a ceiling: a package links only what it uses. DistributedSimulation is linked from CMB-8 (foreign combat) on.

```cmake
# Illustrative (ARCH § Modules, work packages and the implementation contract)
runic_add_module(Combat
    GROUP        GameplayAPI
    PUBLIC_DEPS  Runic::Gameplay Runic::Physics
    PRIVATE_DEPS Runic::DistributedSimulation
    HEADLESS)
```

### Must never depend on

- AI, GameplayFlow[^engine-gameplayflow], Quest, Network, Renderer[^engine-renderer], Animation[^engine-animation], UI or Audio.
- Any game rule, name or number, in code or tests. Tests use neutral IDs such as `engine:test_ability`.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH[^arch] names three public headers for this module: `CombatModule.hpp`, `Ability.hpp`, `CombatEvents.hpp`, with `Source/` subsystems such as `Abilities/`, `Cooldowns/` and `Damage/`. Further headers (sketch, fixed in the contract session): `Targeting.hpp`, `CombatVolume.hpp`, `HitTiming.hpp`, `InputQueue.hpp`, `Status.hpp`, `Cooldown.hpp`, `RulePipeline.hpp`, `BehaviorRegistry.hpp`, `DamagePipeline.hpp`, `ThreatTable.hpp`, `CrowdControl.hpp`, `ForeignCombat.hpp`.

Foreign combat (ARCH § Networking: replication, interest and handoff): the target's owner resolves the hit, through a ForeignCombatVolume and a RemoteHitResult.

## Threading and tick rules

- Ability starts, timelines and projectiles advance in the 30 Hz realtime phase (steps 1 and 2); combat, projectile and AoE queries run in step 6 and pass hit candidates to the resolver.
- Combat queries run in parallel by attack or projectile; they are read-only and their results are buffered.
- Hit resolution (damage, effects) is grouped by target, so two attacks on one target apply in a fixed order.
- Behaviors and rules write per-worker output buffers merged at the barrier in a fixed order.
- Durations are tagged with their domain (`DomainTickCount`).
- Combat emits facts such as EntityKilled once, with a stable event ID; quests and services rely on it.
- Only the owner of the target applies consequences; a ghost is never damaged locally.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| CMB-1 | Ability toolkit: timelines and hit timing per skill (ServerTime, CappedRewind) | Timeline phases fire on exact ticks | GPL-2[^engine-gameplay], CNT-1[^engine-content] | 16 |
| CMB-2 | Targeting modes (free aim, FriendlyLock, light aim-assist hooks), combat volumes and hit detection over Physics[^engine-physics] queries | A melee arc and a projectile each hit a dummy exactly once; FriendlyLock never selects a hostile | CMB-1, PHYS-5 | 16 |
| CMB-3 | Statuses and effects, cooldowns, resource costs | Durations, stacks and cooldowns expire on the right tick | CMB-1 | 17 |
| CMB-4 | Rule pipeline and behavior registries (fixed hooks for Game code); value variants selected by a per-hit context the Game defines | A test rule and a test behavior run in order; a value with a base and a contextual variant resolves by the hit context; the Engine holds no game rule | CMB-3 | 17 |
| CMB-5 | Damage pipeline, interception hooks, invulnerability windows | Interception follows hook order; invulnerability blocks hits | CMB-4, GPL-3 | 18 |
| CMB-6 | Threat table and crowd control (target cap, immunity windows) | A CC chain and its immunity follow the test data | CMB-5 | 19 |
| CMB-7 | Input queue: one queued action per character inside the game-set window, started on the first legal tick; game-named priority inputs skip it when the cancel window allows | An early input fires on the first legal tick; a stun in between cancels it | CMB-1, SIM-4[^engine-simulation] | 19 |
| CMB-8 | Foreign combat: volumes crossing an authority border become requests to the owner | The target is damaged exactly once, by its owner | CMB-5, DSIM-4[^engine-distributedsimulation] | 37 |


## Acceptance tests

The Done when column is the acceptance list. All test data is Engine test data (`engine:` IDs). The Editor[^engine-editor]'s resolver debugger (EDT-3) reads the rule pipeline's trace, so CMB-4 must expose which rules applied to a hit and why.

## Open items

- Decided in ARCH[^arch] revision 369: projectiles that opt into wind drift (a per-ability wind flag) read wind from Environment[^engine-environment]; Combat → Environment is an allowed edge, linked by the package that adds the flag.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Gameplay|Gameplay]] · [[../modules/Physics|Physics]] · [[../modules/Environment|Environment]] · [[../modules/DistributedSimulation|DistributedSimulation]]
- **Depended on by:** [[../modules/AI|AI]]
- **Its packages need:** [[../modules/Content|Content]] · [[../modules/Simulation|Simulation]]
- **Packages that need it:** [[../modules/Editor|Editor]] · [[../../../RunicGame/docs/game-packages|Game work packages]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-physics]: [[../modules/Physics|Physics]] — Engine module · Simulation core
[^engine-cooker]: [[../modules/Cooker|Cooker]] — Engine module · Tools
[^engine-distributedsimulation]: [[../modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-gameplay]: [[../modules/Gameplay|Gameplay]] — Engine module · GameplayAPI
[^engine-environment]: [[../modules/Environment|Environment]] — Engine module · World systems
[^engine-gameplayflow]: [[../modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-renderer]: [[../modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-animation]: [[../modules/Animation|Animation]] — Engine module · Presentation
[^engine-content]: [[../modules/Content|Content]] — Engine module · Foundation
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-editor]: [[../modules/Editor|Editor]] — Engine module · Tools
