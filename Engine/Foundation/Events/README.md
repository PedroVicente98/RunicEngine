# Runic::Events

Foundation · Headless: yes · Packages: EVT-1 · Status:
empty until EVT-1 (slice 4) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Typed tick-safe event buffers and readers, multi-consumer delivery, stable event IDs

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Buffers/`](Source/Buffers/README.md) | Typed, tick-safe event buffers that systems write facts into during a phase (HitEvent, DamageEvent, EntityKilled…). | EVT-1 (slice 4) |
| [`Source/Readers/`](Source/Readers/README.md) | Readers and multi-consumer delivery: every consumer receives every fact, after the barrier that flushes it. | EVT-1 (slice 4) |
| [`Source/EventIds/`](Source/EventIds/README.md) | Stable event IDs (SimulationId plus a sequence) that stay the same on replay, so durable consumers can deduplicate. | EVT-1 (slice 4) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. EVT-1 (slice 4) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Repositories, build targets and processes › Three event families
- RunicDocs › Architecture › Simulation core clock and threads (Engine) — facts are flushed at barriers
