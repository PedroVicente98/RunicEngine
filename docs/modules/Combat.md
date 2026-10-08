# Combat

> **Contract seed.** When `Engine/Combat/` is created, copy this file to
> `Engine/Combat/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.20 and §J.8.5. Other sections are
> derived and are proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Combat/` |
| CMake target | `RunicCombat` → `Runic::Combat` |
| Namespace | `Runic::Combat` |
| Roadmap phase | 5 — gameplay/combat (foreign combat in 14) |
| Headless | yes |
| Status | Not started |

## Purpose

Server-authoritative, reusable spatial action-combat framework.

## Owns

- Abilities and ability timelines.
- Targeting (spatial first, explicit entity targets where needed).
- Combat volumes and hit-detection integration.
- Damage pipeline hooks.
- Effects and status effects.
- Cooldowns, resource costs, crowd control.
- Combat events.
- The foreign-combat contract.

CooldownSystem, DamageSystem, targeting, effects, and hit detection are
**subsystems** of Combat, not top-level modules. [J.4.20]

## Does not own

Concrete RunicGame skill formulas, classes, weapon policy, PvP tuning; transport
implementation.

## Allowed dependencies

- Core, Events, Gameplay, Physics, World, Simulation, Content.
- DistributedSimulation, for foreign interactions.

## Forbidden dependencies

Renderer, Animation (presentation), NetworkAdapter/transport, AI (AI emits
intents that Combat consumes), RunicPlatform, RunicGame.

## Primary consumers

Game combat, AI intents, bosses, Client presentation state.

## Public API (conceptual)

```text
Combat/
├── Public/
│   ├── CombatModule.hpp
│   ├── Ability.hpp
│   ├── Targeting.hpp
│   ├── CombatVolume.hpp
│   ├── Damage.hpp
│   ├── Effect.hpp
│   ├── StatusEffect.hpp
│   ├── Cooldown.hpp
│   ├── CrowdControl.hpp
│   ├── ForeignCombat.hpp
│   └── CombatEvents.hpp
├── Source/
│   ├── Ability/
│   ├── Targeting/
│   ├── HitDetection/
│   ├── Damage/
│   ├── Effects/
│   ├── Cooldowns/
│   └── CrowdControl/
├── Tests/
└── CMakeLists.txt
```

```cpp
struct ForeignCombatRequest {
    GlobalEntityId     source;
    AbilityExecutionId execution;
    CombatVolume       volume;
    TickId             source_tick;
    AuthorityEpoch     source_epoch;
};
```

## Threading / tick model (derived)

Registered into Simulation phases/tick domains; all mutation on the simulation
thread. Ability requests arrive as commands (players, AI, GameplayFlow).

## Data ownership (derived)

- Combat state (cooldowns, active effects, CC) lives in Flecs components of the
  *owning* simulation.
- Only the owner of the target applies consequences. A ghost is never damaged
  locally. [J.8.5]

## Integration points (derived)

- Physics casts/overlaps → hit candidates; Combat interprets them.
- Damage pipeline hooks → Game supplies formulas, eligibility, Guard/Resolve
  policy, and tuning.
- Events → `HitEvent`, `DamageApplied`, `EntityKilled`.
- Gameplay → stats/resources/restrictions (untargetable, immune).
- DistributedSimulation → `ForeignCombatRequest` / `RemoteHitResult` for volumes
  crossing an authority boundary.

## Acceptance tests (Stage A, derived — phase 5 proof)

- [ ] A representative melee, projectile, and targeted ability each resolve
      authoritatively with test-only data.
- [ ] Effects/status, resource costs, and cooldowns apply and expire on the
      correct ticks.
- [ ] `EntityKilled` is emitted once with a stable identity.
- [ ] Damage formulas come from a registered hook (test hook), not Engine code.
- [ ] Untargetable/immune restrictions prevent targeting/damage.
- [ ] Phase 14: a volume crossing into another simulation's authority produces a
      `ForeignCombatRequest`; the target is damaged exactly once by its owner.

## Future extensions

Phase 14 foreign combat and world-boss flows ([05](../system/05-distributed-simulation.md)).

## Open questions

- Combat technical contract details (v2.1 §E and the Combat System Context
  document) are not imported ([OQ-12](../system/11-open-questions.md#oq-12)).
- Resource *state* vs. *cost* split with Gameplay (see [Gameplay](Gameplay.md)).
