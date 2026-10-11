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

Combat is the Engine's server-authoritative action-combat toolkit: abilities and their timelines, hit timing, the one-slot input queue, targeting, hit detection, statuses, cooldowns, the rule pipeline, behavior registries, damage interception, threat and crowd control, and foreign combat across simulation borders (ARCH § Engine gameplay APIs). The Engine knows timelines, tags, statuses, rules and threat, but no concrete status, defense or formula.

## Owns

| API | What the Engine provides | What a game supplies |
| --- | --- | --- |
| Ability toolkit | Timeline runner in DomainTickCount (startup, active, recovery, cancel windows); hit volumes and projectile queries; cooldowns; resource pools; one-slot input queue | Ability definitions, timings, costs, queue window |
| Hit timing | ServerTime and CappedRewind (with a per-character position history), chosen per ability | The choice per ability and the cap |
| Status container | Timed statuses with stacking policy, duration, periodic tick and immunity windows | Every concrete status |
| Rule pipeline | Registration of typed conditions and modifiers; evaluation in named stages; outputs to per-worker buffers; compilation into a resolved loadout per character | Stage order and every condition and modifier |
| Damage interception | Hooks before and after damage is applied; invulnerability windows | Defenses and the damage formula |
| Threat table | Per-NPC threat storage read by AI target selection | What generates threat; taunt |
| Behavior registries | ID-to-C++ registration, parameter schemas, a fixed hook list, read-only context and output buffers | The behaviors |

Also owned: targeting modes (free aim, FriendlyLock, light aim-assist hooks), combat volumes and hit detection over Physics queries (CMB-2); crowd control with a target cap and immunity windows (CMB-6); the foreign-combat path where volumes crossing an authority border become requests to the owner (CMB-8).

### Behavior registry contract

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

### Input queue (decided)

- The queue lives in Combat, which knows each ability's recovery and cancel windows; Network only carries the inputs. The client may send the next action before the current one ends, inside a window the game sets.
- The server keeps at most one queued action per character and starts it on the first tick after recovery ends, so at most one 30 Hz tick late.
- A queued action is validated when it starts, not when it arrives; a stun in between cancels it.
- Priority inputs defined by the game skip the queue when the current ability's cancel window allows.

## Does not own

- Attributes, modifiers, tags, resources and restrictions (Gameplay); Combat reads and applies them.
- Geometry (Physics): Combat interprets Physics query results.
- Authority, epochs and the foreign-interaction contract (DistributedSimulation); Combat produces and consumes its requests.
- AI decisions: AI emits intents that become the same validated commands players use.
- Every concrete ability, status, defense, damage type, formula, resolver order and threat rule (RunicGame).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Gameplay, Physics, DistributedSimulation |
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

- AI, GameplayFlow, Quest, Network, Renderer, Animation, UI or Audio.
- Any game rule, name or number, in code or tests. Tests use neutral IDs such as `engine:test_ability`.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH names three public headers for this module: `CombatModule.hpp`, `Ability.hpp`, `CombatEvents.hpp`, with `Source/` subsystems such as `Abilities/`, `Cooldowns/` and `Damage/`. Further headers (sketch, fixed in the contract session): `Targeting.hpp`, `CombatVolume.hpp`, `HitTiming.hpp`, `InputQueue.hpp`, `Status.hpp`, `Cooldown.hpp`, `RulePipeline.hpp`, `BehaviorRegistry.hpp`, `DamagePipeline.hpp`, `ThreatTable.hpp`, `CrowdControl.hpp`, `ForeignCombat.hpp`.

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
| CMB-1 | Ability toolkit: timelines and hit timing per skill (ServerTime, CappedRewind) | Timeline phases fire on exact ticks | GPL-2, CNT-1 | 16 |
| CMB-2 | Targeting modes (free aim, FriendlyLock, light aim-assist hooks), combat volumes and hit detection over Physics queries | A melee arc and a projectile each hit a dummy exactly once; FriendlyLock never selects a hostile | CMB-1, PHYS-5 | 16 |
| CMB-3 | Statuses and effects, cooldowns, resource costs | Durations, stacks and cooldowns expire on the right tick | CMB-1 | 17 |
| CMB-4 | Rule pipeline and behavior registries (fixed hooks for Game code) | A test rule and a test behavior run in order; the Engine holds no game rule | CMB-3 | 17 |
| CMB-5 | Damage pipeline, interception hooks, invulnerability windows | Interception follows hook order; invulnerability blocks hits | CMB-4, GPL-3 | 18 |
| CMB-6 | Threat table and crowd control (target cap, immunity windows) | A CC chain and its immunity follow the test data | CMB-5 | 19 |
| CMB-7 | Input queue: one queued action per character inside the game-set window, started on the first legal tick; priority defensive inputs skip it when the cancel window allows † | An early input fires on the first legal tick; a stun in between cancels it | CMB-1, SIM-4 | 19 |
| CMB-8 | Foreign combat: volumes crossing an authority border become requests to the owner | The target is damaged exactly once, by its owner | CMB-5, DSIM-4 | 37 |

† CMB-7 is worded generically for this public repository, as in [12-work-packages.md](../system/12-work-packages.md); ARCH names the game's own defense inputs there.

## Acceptance tests

The Done when column is the acceptance list. All test data is Engine test data (`engine:` IDs). The Editor's resolver debugger (EDT-3) reads the rule pipeline's trace, so CMB-4 must expose which rules applied to a hit and why.

## Open items

- Projectiles may opt into wind drift through a per-ability wind flag (ARCH § Environment), but Combat does not list Environment as a dependency. The contract session that adds the flag must ask the owner how wind reaches projectiles.
