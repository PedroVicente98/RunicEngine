# Content

> **Contract seed.** When `Engine/Content/` is created, copy this file to
> `Engine/Content/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.4. Other sections are derived and are
> proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Content/` |
| CMake target | `RunicContent` → `Runic::Content` |
| Namespace | `Runic::Content` |
| Roadmap phase | 1 (basic) — expanded in 6 (data-driven content) |
| Headless | yes |
| Status | Not started |

## Purpose

Stable data-driven definition identity, schemas, registration, lookup, and
validation.

## Owns

- Namespaced `ContentId`.
- Versioned schemas.
- `ContentRegistry`.
- Definition lookup/loading contracts.

## Does not own

Concrete RunicGame item/ability/NPC/quest data; asset bytes; persistence.

## Allowed dependencies

- Core. Assets may consume Content IDs but must avoid cycles.

## Forbidden dependencies

Assets (Content must not depend back on it), ECS, Simulation, every gameplay
module, RunicPlatform, RunicGame.

## Primary consumers

Gameplay, Combat, AI, Quest, Game content.

## Public API (conceptual)

```text
Content/
├── Public/
│   ├── ContentId.hpp
│   ├── ContentRegistry.hpp
│   ├── ContentSchema.hpp
│   ├── ContentDefinition.hpp
│   └── ContentLoader.hpp
├── Source/
├── Tests/
└── CMakeLists.txt
```

```cpp
ContentId engine_id = "engine:default_interaction";
ContentId game_id   = "game:infernal_slash";
```

Engine knows how definitions are named, versioned, validated, and looked up;
Game supplies the actual definitions. [J.4.4]

## Threading / tick model (derived)

- Registration happens at load time or at safe boundaries (e.g. during
  streaming), never concurrently with simulation reads of the same entry.
- During ticks the registry is read-only, so lookups need no locks.

## Data ownership (derived)

Definitions are immutable after registration. Runtime instances refer to a
definition by `ContentId` (or an interned handle derived from it).

## Integration points (derived)

- Feature modules (Gameplay, Combat, AI, Quest, GameplayFlow) register their
  schemas; Game registers concrete definitions under the `game:` namespace.
- Cooker validates content against the same schemas at build time.

## Acceptance tests (Stage A, derived)

- [ ] Parse/validate namespaced IDs; reject malformed IDs.
- [ ] Reject duplicate registration of the same ID.
- [ ] Detect schema version mismatch.
- [ ] Lookup of an unknown ID returns an error `Result`.
- [ ] Engine tests use only `engine:`/test namespaces — no Game content.

## Future extensions

Phase 6: expanded Ability/Item/Effect/Character/NPC schemas. Editor-time hot
reload.

## Open questions

- Definition file format is not chosen ([OQ-12](../system/11-open-questions.md#oq-12)).
- Client-safe vs. server-only definitions ([OQ-17](../system/11-open-questions.md#oq-17)).
