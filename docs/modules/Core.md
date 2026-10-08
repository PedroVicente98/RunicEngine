# Core

> **Contract seed.** When `Engine/Core/` is created, copy this file to
> `Engine/Core/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, and the
> layout come from Appendix J §J.4.1. The other sections are derived from the
> cited sections and are proposals until the owner confirms them.

| Field | Value |
| --- | --- |
| Directory | `Engine/Core/` |
| CMake target | `RunicCore` → `Runic::Core` |
| Namespace | `Runic::Core` (proposal: the most basic vocabulary types may live directly in `Runic`) |
| Roadmap phase | 0 — bootstrap |
| Headless | yes |
| Status | Not started. A bootstrap `Runic::Core` exists only as an INTERFACE target that sets C++20 and the `include/` root. |

## Purpose

Foundation types and utilities shared by all Engine targets.

## Owns

- Stable IDs.
- Result/error utilities.
- Assertions.
- Logging hooks.
- Time primitives.
- Small containers.
- Math-facing common types.

## Does not own

Flecs world ownership, Jolt objects, renderer state, gameplay rules, networking,
persistence.

> Keep Core deliberately boring. Moving high-level concepts here to avoid
> dependencies is an architectural failure. [J.4.1]

## Allowed dependencies

- C++ standard library.
- GLM, only where accepted as the foundational math dependency.

## Forbidden dependencies

Every other Engine module; every third-party library except GLM; RunicPlatform;
RunicGame.

## Primary consumers

Almost every Engine module.

## Public API (conceptual)

```text
Core/
├── Public/
│   ├── CoreTypes.hpp
│   ├── Result.hpp
│   ├── Assert.hpp
│   ├── IDs.hpp
│   ├── Time.hpp
│   ├── Math.hpp
│   └── Logging.hpp
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

- Value types are freely copyable across threads.
- Logging hooks are callable from any thread (simulation, I/O, render).
- No global mutable state other than the installation of logging sinks at
  startup.

## Data ownership (derived)

Core owns no gameplay or runtime state. ID types are strongly typed value types;
their *meaning* belongs to the module that issues them (e.g. authority semantics
of `GlobalEntityId` belong to DistributedSimulation). See
[OQ-2](../system/11-open-questions.md#oq-2) for which IDs are declared here.

## Integration points (derived)

- Logging hooks: the executable installs sinks (console, file, or a Platform
  Observability bridge). Core never depends on Observability.
- Time: tick/duration primitives used by Simulation's 60 Hz clock and its
  30/20/10 Hz domains.

## Acceptance tests (Stage A, derived)

- [ ] `Result`/error propagation, including errors carrying a message/code.
- [ ] Assertions fire in Debug and follow the chosen Release policy.
- [ ] ID types are distinct: no implicit conversion between, e.g.,
      `GlobalEntityId` and `SimulationId`.
- [ ] Tick ↔ duration conversions are exact at 60 Hz and for the 30/20/10 Hz
      domains (integer arithmetic, no float drift).
- [ ] Logging with no sink installed is a no-op, not a crash.
- [ ] Builds headless with no third-party dependency other than GLM.

## Future extensions

Allocator/profiling hooks if a real consumer needs them. Anything gameplay-,
ECS-, or backend-shaped stays out.

## Open questions

- [OQ-2](../system/11-open-questions.md#oq-2) — which ID/time types live here.
- [OQ-8](../system/11-open-questions.md#oq-8) — replacing the bootstrap INTERFACE target.
