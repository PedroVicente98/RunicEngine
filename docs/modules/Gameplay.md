# Gameplay

Contract seed for `Runic::Gameplay`: what a Claude Code session needs to implement the Gameplay work packages. When `Engine/GameplayAPI/Gameplay/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | GameplayAPI (linked through the umbrella target `Runic::GameplayAPI`) |
| Directory | `Engine/GameplayAPI/Gameplay/` |
| CMake target | `Runic::Gameplay` |
| Namespace | `Runic::Gameplay` |
| Headless | yes |
| Status | Not started |

## Purpose

Gameplay provides the generic, thread-safe character-state building blocks a game composes: attributes, modifiers, resources, tags, movement modes, restrictions, the interaction framework, the item and equipment runtime, factions and spawning (ARCH § Engine gameplay APIs). The Engine provides mechanisms; the game supplies every value and rule.

## Owns

| API | What the Engine provides | What a game supplies |
| --- | --- | --- |
| Tags | Hierarchical, interned tags on abilities, statuses and entities | The vocabulary |
| Attributes and modifiers | Base values and modifier stacks (add, multiply, override) with caps | Stats and formulas |
| Restrictions | Source-owned restriction sets (input, movement, abilities, interactions, targeting, damage, external motion), release by source, command validation | Which skills, effects and sequences acquire which restrictions |
| Interactions | Interaction volumes, reach and permission checks, reservation, an interaction registry | The concrete interactions |
| Movement | CharacterVirtual integration; a movement-mode registry with walk, glide (reads Environment wind), mount and vehicle modes; Water registers swimming (CharacterWaterMovement); prediction hooks; server validation | Speeds, dashes, mount and glider stats |
| Item runtime | Runtime item instances and containers mirrored from services | Item definitions and rules |

Also owned: resources and the death and kill facts published when a vital resource reaches zero, with the source filled in by Combat (GPL-2); factions and spawn primitives (GPL-6); flight and climbing modes (GPL-7, expansion).

Source-owned restrictions: no scattered flags such as `invulnerable = true`. A sequence, a skill or an effect acquires a restriction set under its own source ID, and every exit path releases everything held by that source in one call. Dodge invulnerability windows use the same mechanism. Temporary restrictions carry an expiry and a watchdog diagnostic, so no fault can leave a player permanently invulnerable.

## Does not own

- Abilities, statuses, cooldowns, damage and threat (Combat).
- Sequences and their termination path (GameplayFlow); quests (Quest).
- Swimming physics (Water's CharacterWaterMovement), wind (Environment), collision (Physics).
- Durable item state: the Platform item ledger owns it; the Game's Inventory service applies the rules.
- Every concrete value: speeds, stats, formulas, factions, item definitions.

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Simulation, Environment (wind for gliding) |
| Third-party | — |

### Must never depend on

- Combat, GameplayFlow, Quest, AI or any World-systems module other than Environment. They build on Gameplay.
- Renderer, UI, Audio or any presentation library (the module is headless).
- Game rules, names or numbers, in code or tests. Tests use neutral IDs such as `engine:test_ability`.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH's illustrative restriction use (ARCH § Engine gameplay APIs, scripted sequences and restrictions):

```cpp
// Illustrative
RestrictionHandle handle = restrictions.Acquire(player, RestrictionSet{
    .input = Blocked, .movement = Blocked, .abilities = Blocked,
    .interactions = Blocked, .targeting = Untargetable,
    .damage = Immune, .externalMotion = Immune }, sequenceInstanceId);
restrictions.ReleaseAllFromSource(sequenceInstanceId);   // on every terminal path
```

Intangible during a cutscene is several separate policies (input, movement, abilities, interactions, targeting, damage, external motion, optional collision), so other systems can hold their own restrictions at the same time.

Headers (sketch, fixed in the contract session): `MovementModes.hpp`, `Attributes.hpp`, `Modifiers.hpp`, `Resources.hpp`, `GameplayTags.hpp`, `Restrictions.hpp`, `ItemRuntime.hpp`, `Interactions.hpp`, `Factions.hpp`, `Spawning.hpp`.

## Threading and tick rules

- Gameplay systems register into Simulation phases; only those phases mutate state.
- Blocked commands are rejected on the server, whatever the client shows (GPL-3).
- Modifier order is deterministic; outputs written by parallel workers merge in a fixed order.
- Movement runs in the 30 Hz realtime phase, steps 2 and 5; the server validates every movement independently of prediction.
- The item mirror is updated from accepted service results applied at safe ticks, never from an I/O callback.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| GPL-1 | Movement-mode registry and modes (walk, sprint, dash hook, glide, mount; Water registers swimming) with movement parameters from data; gliding reads wind once ENV-2 provides it | Modes switch without teleporting; parameters come from Game data | PHYS-2 | 14 |
| GPL-2 | Attributes, modifiers, resources and tags; death and kill facts when a vital resource reaches zero (Combat fills in the source) | Modifier order is deterministic; resources clamp | SIM-2, CNT-1 | 15 |
| GPL-3 | Restrictions: source-owned restriction sets, acquire and release by source, command-validation hooks | Blocked commands are rejected on the server; releasing a source clears everything it held | GPL-2 | 18 |
| GPL-4 | Item and equipment runtime that mirrors ledger containers | Equipping changes modifiers; the mirror matches the ledger after a reload | GPL-2 | 28 |
| GPL-5 | Interaction framework: interactables, reservation, interaction facts | Two players cannot reserve the same target | GPL-2, EVT-1 | 29 |
| GPL-6 | Factions and spawn primitives | A spawner keeps its population; faction checks drive hostility | GPL-2 | 31 |
| GPL-7 | Flight and climbing movement modes: sustained flight where a layer allows it, flying vehicles, climbing mounts | Flight stops below a game-set altitude and gliding takes over; a mount climbs a marked wall | GPL-1, WORLD-3 | 44+ |

"Parameters come from Game data" means parameters come from data; tests in this repository use Engine test data.

## Acceptance tests

The Done when column is the acceptance list.

## Open items

- GPL-1 uses CharacterVirtual (PHYS-2), GPL-2 uses ContentId (CNT-1) and GPL-5 emits facts (EVT-1), while the catalog row lists Simulation and Environment. The GPL-1, GPL-2 and GPL-5 contract sessions must confirm that Physics, Content and Events arrive through Simulation's public headers, or report the link they need. (Glide wind is decided: Gameplay → Environment, ARCH revision 369.)
- Decided in ARCH revision 369: Gameplay owns a movement-mode registry, and Water registers swimming (CharacterWaterMovement) into it. Gameplay never links Water.
- Decided in ARCH revision 369: Gameplay publishes death and kill facts when a vital resource reaches zero (GPL-2); Combat fills in the source. Quest reads them without a Quest → Combat edge.
