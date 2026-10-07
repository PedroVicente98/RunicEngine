# Cooker

> **Contract seed.** When `Engine/Cooker/` is created, copy this file to
> `Engine/Cooker/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, and Primary consumers come
> from Appendix J §J.4.24. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Cooker/` (+ `RunicCooker` executable) |
| CMake target | `RunicCooker` library → `Runic::Cooker` (executable name to be chosen so it does not clash) |
| Namespace | `Runic::Cooker` |
| Roadmap phase | 3 — cooker proof (world output in 7) |
| Headless | tool — runs without a window; never linked by server or client |
| Status | Not started — tool libraries not yet added |

## Purpose

Tool-side orchestration that converts source/interchange content into native
runtime assets and world packages.

## Owns

- Cooker pipeline and context.
- Mesh, texture, physics, navigation, world, and package processing.
- Deterministic native writers.

## Does not own

Runtime asset streaming (Assets); Game art creation; renderer runtime; server
authority.

## Allowed dependencies

- Core, Content, asset-format definitions, World data definitions.
- Tool-only: fastgltf, meshoptimizer, MikkTSpace, KTX, Jolt (collision cooking),
  Recast, Zstd — as needed.

## Forbidden dependencies

Simulation, Renderer runtime, Client, gameplay modules, RunicPlatform,
RunicGame. No runtime target may link Cooker or its tool-only libraries.

## Primary consumers

The `RunicCooker` executable; the Game content pipeline.

## Public API (conceptual)

```text
Cooker/
├── Public/
│   └── Cooker.hpp           # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

Offline tool; may parallelize jobs freely. Not part of any runtime tick.

## Data ownership (derived)

Reads source/interchange files, writes native assets/packages. Output must be
deterministic: the same inputs produce byte-identical outputs.

## Integration points (derived)

- Assets reads what Cooker writes.
- Physics loads cooked collision; Navigation loads cooked navmesh.
- Game content pipeline runs the Cooker over RunicGame content; client and
  server packages are written separately
  ([OQ-17](../system/11-open-questions.md#oq-17)).

## Acceptance tests (Stage A, derived — phase 3 proof)

- [ ] GLB/interchange → native mesh + Jolt collision → rendered by the client
      and walkable by the simulation.
- [ ] Re-cooking identical inputs yields identical output bytes.
- [ ] Invalid input produces actionable errors.

## Future extensions

Phase 7 world output (Region/Sector/Cell packages); navmesh building with Recast
(phase 11); water data (phase 12).

## Open questions

- Where native asset-format definitions live so that Cooker (writer) and Assets
  (reader) share them without Cooker depending on the runtime loader.
  Proposal: a header-only format-definition part owned by Assets that Cooker may
  include.
- World Authoring/Cooking reference not imported ([OQ-12](../system/11-open-questions.md#oq-12)).
