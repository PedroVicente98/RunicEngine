# Gameplay

> **Contract seed.** When `Engine/Gameplay/` is created, copy this file to
> `Engine/Gameplay/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, and layout
> come from Appendix J §J.4.17, with restrictions from §J.9.6–J.9.8 and inventory
> from §J.10.2. Other sections are derived and are proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Gameplay/` |
| CMake target | `RunicGameplay` → `Runic::Gameplay` |
| Namespace | `Runic::Gameplay` |
| Roadmap phase | 5 — gameplay/combat (restrictions needed by phase 10) |
| Headless | yes |
| Status | Not started |

## Purpose

Reusable baseline gameplay state and generic world-interaction primitives that
are too small to justify independent modules.

## Owns

- Character, stats, resources.
- Items, the **runtime** inventory model, equipment.
- Interaction.
- Faction (generic relationship primitive).
- Spawn primitives.
- Gameplay restrictions (source-owned).

## Does not own

Specific classes/items/loot tables/economy (Game); durable service transactions
(Platform + Game services); the full combat framework (Combat).

> Gameplay owns generic in-simulation concepts. Durable ownership,
> transactions, and Game policy live elsewhere. [J.4.17]

## Allowed dependencies

- Core, ECS, Events, Content, World, Simulation registration surface.

## Forbidden dependencies

Combat, AI, Quest, GameplayFlow (they depend on Gameplay), Physics/Jolt in
`Public/`, persistence/service clients, RunicPlatform, RunicGame.

## Primary consumers

Combat, AI, Quest, GameplayFlow, Water, Game simulation.

## Public API (conceptual)

```text
Gameplay/
├── Public/
│   ├── Character.hpp
│   ├── Stats.hpp
│   ├── Resources.hpp
│   ├── Item.hpp
│   ├── Inventory.hpp
│   ├── Equipment.hpp
│   ├── Interaction.hpp
│   ├── Faction.hpp
│   ├── Spawn.hpp
│   ├── GameplayRestrictions.hpp
│   └── GameplayEvents.hpp
├── Source/
│   ├── Character/
│   ├── Items/
│   ├── Inventory/
│   ├── Interaction/
│   └── Spawn/
├── Tests/
└── CMakeLists.txt
```

```cpp
// Source-owned restrictions [J.9.6]
RestrictionHandle handle = restrictions.Acquire(
    player,
    RestrictionSet{ .input = Blocked, .movement = Blocked, .abilities = Blocked,
                    .interactions = Blocked, .targeting = Untargetable,
                    .damage = Immune, .external_motion = Immune },
    sequenceInstanceId);

restrictions.ReleaseAllFromSource(sequenceInstanceId);
```

## Threading / tick model (derived)

All mutation on the simulation thread through registered systems and commands.

## Data ownership (derived)

- `ItemId` / `ItemDefinition` / `ItemInstance` / inventory / equipment are the
  simulation's **runtime** representation. Durable item ownership belongs to the
  private Game InventoryService. [J.10.2]
- Restrictions are stored per entity with their source IDs; the effective
  restriction is the union of all active sources.

## Integration points (derived)

- Command validation (Simulation) queries restrictions and rejects blocked
  commands, regardless of what the client UI shows. [J.9.7]
- Combat consults targeting/damage restrictions.
- GameplayFlow acquires/releases restrictions with `SequenceInstanceId` as source.
- Inventory changes that must be durable are submitted as async requests through
  the service bridge; results return at a later safe tick. [J.10.6]

## Acceptance tests (Stage A, derived)

- [ ] Stats/resources modify and emit facts.
- [ ] Runtime inventory add/remove/move in simulation; no persistence calls.
- [ ] Two sources acquire overlapping restrictions; releasing one keeps the
      other's effect; `ReleaseAllFromSource` removes exactly that source.
- [ ] Temporary restriction sources support expiry/watchdog diagnostics. [J.9.8]
- [ ] Blocked commands are rejected by validation even if sent.
- [ ] Builds headless.

## Future extensions

Phase 6 data-driven definitions; generic primitives extracted from Game only
after they prove generic.

## Open questions

- "Resources" appears in both Gameplay (resource state) and Combat (resource
  costs). Proposal: Gameplay owns resource *state*; Combat owns ability
  *costs/consumption*.
