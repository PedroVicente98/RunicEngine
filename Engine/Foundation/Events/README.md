# Runic::Events

Foundation · Headless: yes · Packages: EVT-1 · Status:
EVT-1 implemented (slice 4); contract in [ARCHITECTURE.md](ARCHITECTURE.md)

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
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | exists (EVT-1 contract) |
| [`CMakeLists.txt`](CMakeLists.txt) | One `runic_add_module(...)` call. | exists (EVT-1) |


## How the folders fill

EVT-1 (slice 4) wrote `ARCHITECTURE.md`, `CMakeLists.txt`, three public headers (`EventId.hpp`, `EventHub.hpp`, `EventBuffer.hpp`), `Source/Buffers`, `Source/EventIds`, `Source/Readers` and the tests in `Tests/`. A package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Repositories, build targets and processes › Three event families
- RunicDocs › Architecture › Simulation core clock and threads (Engine) — facts are flushed at barriers
