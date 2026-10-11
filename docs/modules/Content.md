# Content

Contract seed for `Runic::Content`: what a Claude Code session needs to implement the Content work packages. When `Engine/Foundation/Content/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Foundation |
| Directory | `Engine/Foundation/Content/` |
| CMake target | `Runic::Content` |
| Namespace | `Runic::Content` |
| Headless | yes |
| Status | Not started |

## Purpose

Content makes the Engine data-first: skills, items, effects, AI, light values and balance are data with stable namespaced IDs and versioned schemas, reloadable without recompiling (ARCH § Principles and performance targets, principle 4). This module provides the IDs, registries, schemas, validation, loading and live reload that every other module uses for its definitions.

## Owns

- `ContentId` with the `engine:` and `game:` namespaces; registries and lookup (CNT-1).
- Reporting duplicate and unknown IDs together with their source (CNT-1).
- Versioned schemas, validation, loading and live-reload hooks (CNT-2).

## Does not own

- Asset identity, handles and cached binary data (Assets).
- Cooking and cook-time validation (Cooker; COOK-2 validates definitions through this module).
- C++ behavior registries and their hook list (Combat, CMB-4), sequence registries (GameplayFlow), quest registries (Quest), extractor registry (Client). Those modules own their registries and refer to content by `ContentId`.
- Every concrete definition. Game content is split into Shared, Client and Server definitions by RunicGame, and the Cooker emits separate client and server packages (OQ-17).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core |
| Third-party | — |

### Must never depend on

- Any Engine module other than Core.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.
- Any game ID, name or value in code or tests. Tests use neutral IDs such as `engine:test_ability`.

## Public API sketch

Sketch, fixed in the contract session: `ContentId.hpp`, `ContentRegistry.hpp`, `ContentSchema.hpp`, `ContentLoader.hpp`, `ContentReload.hpp`.

Rules the API must keep (ARCH § Engine gameplay APIs, behavior registry contract):

- Data refers to definitions by ID; an unknown ID fails at load, not at use.
- Parameters have a schema checked at load, so the Cooker catches typos.
- Data reloads live; changed C++ needs a restart.

## Threading and tick rules

- Loading and reloading never block a simulation tick (principle 5: nothing blocks the tick).
- A reloaded definition reaches live simulation state only at a safe point; only the simulation's own phases mutate the Flecs world.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| CNT-1 | ContentId with `engine:` and `game:` namespaces, registries, lookup | Duplicate and unknown IDs are reported with their source | CORE-1 | 9 |
| CNT-2 | Versioned schemas, validation, loading, live-reload hooks | A changed definition reloads without a restart | CNT-1 | 20 |

## Acceptance tests

The Done when column is the acceptance list. Slice 20 also requires skills, items and effects as data with live reload, using Engine test data in this repository.

## Open items

- [OQ-17](../system/11-open-questions.md#oq-17) (decided): content is split into Shared, Client and Server definitions; the Cooker emits separate client and server packages.
