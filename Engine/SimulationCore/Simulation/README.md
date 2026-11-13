# Runic::Simulation

Simulation core · Headless: yes · Packages: SIM-1, SIM-2, SIM-3, SIM-4, SIM-5, SIM-6, SIM-7, SIM-8 · Status:
empty until SIM-1 (slice 3) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): SimulationRuntime: multi-rate scheduler, phases, barriers, commands, event flush, worker integration, client prediction subset, the replication registry, the service port (outgoing durable operations, accepted results applied at safe ticks; header-only Runic::SimulationPort target); the physics bridge (transforms and Physics handles in Flecs, sync around the step); the Simulation layer

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, Application, ECS, Events, World, Physics, Content

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`SimulationPort/`](SimulationPort/README.md) | Headers of the header-only `Runic::SimulationPort` interface target: the service port the Platform's services-bridge layer implements. | SIM-5 (slice 27) |
| [`Source/Scheduler/`](Source/Scheduler/README.md) | The SimulationRuntime's multi-rate scheduler: the 60 Hz master clock, the 30, 20 and 10 Hz domains, TickContext, DomainTickCount and domain metrics. | SIM-1 (slice 3) |
| [`Source/SimulationLayer/`](Source/SimulationLayer/README.md) | The Simulation layer: the Application layer that drives the SimulationRuntime inside a process's LayerStack (GameServer, the RunicEditor's local world). | SIM-1 (slice 3) |
| [`Source/Phases/`](Source/Phases/README.md) | Phases in a fixed order, barriers, and the event flush at each barrier. | SIM-2 (slice 5) |
| [`Source/Commands/`](Source/Commands/README.md) | Commands and the command queue, deferred mutation: the only door player and AI input has into the world. | SIM-2 (slice 5) |
| [`Source/Workers/`](Source/Workers/README.md) | Phases on the worker pool, per-worker buffers and the deterministic merge; the physics step runs through PHYS-4. | SIM-3 (slice 10) |
| [`Source/PhysicsBridge/`](Source/PhysicsBridge/README.md) | The physics bridge: Flecs holds transforms and opaque Physics handles; a sync phase writes kinematic targets before the step and transforms back after it; contacts become facts for the next phase. | SIM-8 (slice 7) |
| [`Source/Prediction/`](Source/Prediction/README.md) | The client prediction subset: selected systems run on the client with RunicGame::Shared. | SIM-4 (slice 14) |
| [`Source/Replication/`](Source/Replication/README.md) | The replication registry: modules register replicated components and world-state values; Network encodes only what is registered. | SIM-6 (slice 13) |
| [`Source/ServicePort/`](Source/ServicePort/README.md) | The service port's implementation: an outbox of durable operations with stable IDs and an inbox of accepted results drained at safe ticks. | SIM-5 (slice 27) |
| [`Source/BehaviorCounters/`](Source/BehaviorCounters/README.md) | Behavior counters: small per-player counters kept in fixed memory and flushed as one summary per player per interval through the service port. | SIM-7 (slice 39) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. SIM-1 (slice 3) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Simulation core clock and threads (Engine)
- RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine)
- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
