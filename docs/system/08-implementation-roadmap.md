# 08 — Implementation roadmap

Dependency-first does **not** mean finishing every foundational module to
production maturity before any vertical slice. Use the pattern:
Stage A proof → dependent Stage A → later Stage B generalization → Stage C
hardening ([02 §Maturity](02-module-contract.md#maturity-stages-j132-j12)). [J.12]

## Phases [J.12]

| Phase | Primary modules | Proof / acceptance outcome | Repositories touched (derived) |
| --- | --- | --- | --- |
| **0 — bootstrap** | Core, Application, basic build graph, headless/client/editor executables. | CMake boundaries compile; headless target has no graphics/UI linkage. | Engine, Platform, Game |
| **1 — authoritative backbone** | Events, Content (basic), ECS, World (basic), Physics, Simulation. | One headless authoritative world, fixed tick, movement/collision, command/event flow. | Engine |
| **2 — local client** | Assets (basic), Renderer, Client, UI shell, Animation/Audio stubs as needed. | Client drives MoveCommand, observes simulation, debug-renders state. | Engine, Game (client shell) |
| **3 — cooker proof** | Cooker + Assets native formats. | GLB/interchange → native mesh + Jolt collision → render and walk on it. | Engine |
| **4 — first multiplayer** | Platform Contracts/Realtime/Identity + Engine NetworkAdapter. | Two clients, movement snapshots, spawn/despawn, prediction/reconciliation. | Platform, Engine, Game |
| **5 — gameplay/combat** | Gameplay, Combat. | Representative melee/projectile/targeted ability, effects/resources/cooldowns, authoritative facts. | Engine, Game (GameCombat) |
| **6 — data-driven content** | Content schemas expanded + GameContent. | Stable namespaced Ability/Item/Effect/Character/NPC definitions. | Engine, Game |
| **7 — streaming world** | World streaming + Assets packages + Cooker world output. | Region/Sector/Cell loading, collision/render streaming, ownership still separate. | Engine, Game (GameWorld) |
| **8 — interest management** | NetworkAdapter/Realtime interest filtering. | Visibility/replication scale by relevant world neighborhood. | Engine, Platform |
| **9 — durable service foundation** | Platform ServiceRuntime/Persistence + Game Character/Inventory services. | Load/reconnect, idempotent item grants, durable character/inventory. | Platform, Game |
| **10 — GameplayFlow + Quest** | GameplayFlow, Quest, GameQuests, private QuestService. | EntityKilled and world interactions advance a durable quest; a scripted sequence executes and cleans up. | Engine, Game |
| **11 — Navigation/Spawn/AI** | Navigation, AI, GameAI, AI Editor subsystem. | NPC navigation/aggro/combat through the intent/command bridge. | Engine, Game |
| **12 — Water/Naval proof** | Water + GameNaval. | Queryable water, buoyancy, swimming, watercraft movement, first ship rule integration. | Engine, Game |
| **13 — distributed simulation** | DistributedSimulation + Platform WorldControl + sim peer transport. | Ghosts, transfer, authority epochs, make-before-break client handoff. | Engine, Platform |
| **14 — hot-area/cross-sim combat** | AuthorityPartition load balancing + foreign combat. | Two simulations inside one city behave as one logical world; cross-boundary attacks remain authoritative. | Engine, Platform, Game |
| **15 — service expansion** | Trade/Auction/Economy/Guild/Mail Game domain services. | Private Game rules over reusable Platform service/persistence APIs. | Game, Platform |
| **16 — production orchestration** | Orchestration + Observability + recovery/hardening. | Agones/K8s fleets/capacity, metrics, failure recovery, load tests. | Platform |

Modules without an explicit phase in J.12 (Editor beyond the bootstrap
executable, GameProgression, GameEconomy beyond phase 15, GameWorldGameplay,
GameInstances, GameEncounters beyond phase 10) are scheduled by the owner when
their prerequisites exist.

## Milestone gates

| Gate | Reached after | Defined in |
| --- | --- | --- |
| First true playable MMO slice | Phases 0–9 (plus AI/Navigation from phase 11 for the wolf) | [09 §J.14.1](09-validation-scenarios.md#j141-first-true-playable-mmo-slice) |
| Quest durability | Phase 10 | [09 §J.14.4](09-validation-scenarios.md#j144-quest-durability-test) |
| Cutscene safety | Phase 10 (handoff part after 13) | [09 §J.14.3](09-validation-scenarios.md#j143-cutscene-safety-test) |
| Hot-city two-simulation | Phases 13–14 | [09 §J.14.2](09-validation-scenarios.md#j142-hot-city-two-simulation-test) |
| Auction secrecy | Phase 15 | [09 §J.14.5](09-validation-scenarios.md#j145-auction-secrecy-and-authority-test) |
| Headless dependency | Every phase, continuously | [09 §J.14.6](09-validation-scenarios.md#j146-headless-dependency-test) |

## Where we are

As of the import of this architecture (October 2026) all three repositories are
in **Phase 0**: build bootstraps exist, no module from the catalogs is
implemented. Exact state and the remaining Phase 0 work are tracked per
repository in `docs/current-state.md`.

## Module docs per phase

Module contracts live in the owning repository:

- Engine modules: `RunicEngine/docs/modules/<Module>.md`
- Platform modules: `RunicPlatform/docs/modules/<Module>.md`
- Game modules: `RunicGame/docs/modules/<Module>.md`
- Game domain services: `RunicGame/docs/services/<Service>.md`
