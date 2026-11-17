# Runic::Simulation — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::Simulation` (`runic_simulation`) · Packages: SIM-1 to SIM-8

Source: RunicDocs › Architecture › Simulation core clock and threads (Engine); Repositories, build targets and
processes (Engine modules); Modules, work packages and the implementation contract (SIM-1); Application and LayerStack
(Engine); Tools › Metrics every process exports. Status: **SIM-1 implemented** (slice 3, on the preview branch
`preview/slices-3-11`): the clock domains, `TickContext`, `DomainTickCount`, the `SimulationRuntime`, the Simulation
layer and the Simulation's settings, with their acceptance tests; RunicServer runs it. The folder map is
[README.md](README.md); slice 3's brief and walkthrough are
[preview/slice-03-fixed-step-clock.md](../../../preview/slice-03-fixed-step-clock.md).

## Purpose

The SimulationRuntime owns the tick. A simulation advances in exact fixed steps on a 60 Hz master clock, with 30, 20
and 10 Hz domains that tick on every 2nd, 3rd and 6th master step, and every step runs the systems that are due in a
fixed order. Wall-clock time only decides how many steps are owed: the Simulation layer reads it from the
application's loop, and nothing inside a simulation reads a clock. SIM-1 builds the clock, the scheduler, the layer and
the domain metrics. Later packages add phases, commands and the event flush (SIM-2), phases on the worker pool
(SIM-3), prediction (SIM-4), the service port (SIM-5), the replication registry (SIM-6), behavior counters (SIM-7) and
the physics bridge (SIM-8).

## Public API

Namespace `Runic::Simulation`. Headers are included by their path under `Public/` (`#include "SimulationRuntime.hpp"`).
No library types.

| Header | Holds | Since |
| --- | --- | --- |
| `TickDomain.hpp` | `TickDomain` (`Hz60`, `Hz30`, `Hz20`, `Hz10`), `kTickDomainCount`, `kTickDomains`, `IndexOf`, `MasterTicksPerTick` (1, 2, 3, 6), `TicksPerSecond`, `RateOf`, `StepSeconds`, `TickDomainName` (`hz60` … `hz10`, the metrics label), `IsDue`, `DomainTickAt`; `DomainTickCount` (a tick count tagged with its domain: `Covering`, `ToDuration`, `In`, `+`, `−`, comparisons inside one domain) | SIM-1 |
| `TickContext.hpp` | `TickContext` (domain, master tick, domain tick, `dt`, simulation time) | SIM-1 |
| `SimulationRuntime.hpp` | `ScheduleSlot` (`Hz20Producers`, `Hz10Producers`, `Hz30Realtime`, `Hz20Consumers`, `Hz10Consumers`, `Hz60Hook`), `kScheduleSlotCount`, `DomainOf`, `ScheduleSlotName`, `SystemFunction`, `SystemSpec`, `kStandaloneSimulation`, `SimulationSpec` (id, workers, metrics), `SimulationRuntime` (`AddSystem`, `RunMasterTick`, `Id`, `NextMasterTick`, `TicksRun`, `Time`, `Workers`, `Metrics`), and the metric names `kTickTimeMetric`, `kTicksMetric`, `kStepTimeMetric`, `kOverrunsMetric`, `kSystemTimeMetricPrefix` | SIM-1 |
| `SimulationLayer.hpp` | `SimulationLayer` (an `Application::Layer` that owns a runtime: `Runtime`, `BacklogTicks`), `SimulationLayerSpec`, `kSimulationLayerName`, `kDefaultMaxTicksPerUpdate` (5), `kBacklogMetric` | SIM-1 |
| `SimulationSettings.hpp` | `SimulationSettings`, `DeclareSimulationSettings`, `SimulationSpecFrom`, `SimulationLayerSpecFrom`, the keys `kWorkersSetting` (`simulation.workers`) and `kMaxTicksPerUpdateSetting` (`simulation.max_ticks_per_update`), `kMaxTicksPerUpdateLimit` | SIM-1 |

**The time rule:** a system reads time from its `TickContext` and nowhere else. `TickContext::time` is computed from
the tick's number with integer arithmetic (`Core::TickRate::TimeOfTick`), so a run of any length never drifts, and a
domain tick has the time of its master tick to the nanosecond. `dt` is the domain's fixed step in seconds as a 32-bit
float, for gameplay math; it is never accumulated into a time. A duration in simulation code is a `Core::Duration` or
a `DomainTickCount`; `DomainTickCount` is not constructible from a bare number, and counts of two domains do not mix.

## Owns

- The tick rates: the 60 Hz master clock, the three slower domains and which master ticks each is due on.
- The order of a master step: the 20 Hz producers, the 10 Hz producers, the 30 Hz realtime phase, the 20 Hz
  consumers, the 10 Hz consumers, the 60 Hz hook; inside a slot, the order systems were added in.
- Master and domain tick numbers and simulation time.
- The simulation's worker pool (a `Core::WorkerPool`, one per runtime).
- The rule for falling behind: every due step runs, late if need be; the backlog is a metric and a warning, and no
  step is skipped.
- The Simulation's metrics (names below) and its two settings.

## Does not own

- The loop and wall-clock time: Runic::Application's host paces the frames and gives the layer `FrameTime`.
- The pool itself, the metrics registry and the configuration (Runic::Core: CORE-2, CORE-8); the program owns the
  registry and the configuration and passes them in.
- What systems do: each module registers its own. The world's data is Runic::ECS (ECS-1), events are Runic::Events
  (EVT-1), physics is Runic::Physics.
- Phases inside the realtime slot, commands, deferred mutation and the event flush at barriers (SIM-2, slice 5);
  phases on the worker pool and the deterministic merge (SIM-3, slice 10). SIM-1's systems may already split their
  work with `Workers().ParallelFor`.
- Publishing metrics (the Platform's exporter, PBND-3) and the `SimulationId` values of a deployed world (the
  Platform's services); a process on its own uses `kStandaloneSimulation`.

## Allowed dependencies

Catalog row: Core, Application, ECS, Events, World, Physics, Content. SIM-1 links Core and Application (PUBLIC: the
layer is an `Application::Layer`, and the public headers use Core's time, ID, pool, metrics and configuration types).

## Forbidden dependencies

Everything outside the row: GameplayAPI, WorldSystems, Networking, Presentation and Tools modules, Godot, any audio or
editor code (the module is headless), and RunicPlatform and RunicGame. No third-party library.

## Threading and tick model

- One thread drives a runtime: the thread that calls `RunMasterTick` (in a process, the main thread through the
  Simulation layer). It is the worker pool's driving thread, worker 0.
- A system runs on the driving thread and may split its work across the pool; it has finished every job it submitted
  when it returns, so the end of a system is a barrier.
- `AddSystem` is called between steps only (asserted). `NextMasterTick`, `TicksRun` and `Time` are read by the driving
  thread; other threads read the metrics registry instead.
- Nothing here blocks or reads a file or a socket. `RunMasterTick` allocates nothing itself.
- The layer runs at most `maxTicksPerUpdate` steps per update. What is still due stays owed and runs in later
  updates.

## Data ownership

A `SimulationRuntime` owns its worker pool, its systems (their names and functions) and its tick counters. It holds a
pointer to the metrics registry of its spec, which must outlive it. A `SimulationLayer` owns its runtime; the
application's layer stack owns the layer.

## Integration points

- A program (RunicServer; later the RunicEditor's local world and RunicGame's GameServer) declares the settings with
  `DeclareSimulationSettings`, loads its configuration, builds a `SimulationRuntime` from `SimulationSpecFrom`, lets
  modules add their systems and pushes a `SimulationLayer` that owns the runtime.
- Modules add systems with `AddSystem` and take time from `TickContext`.
- The metrics, in the registry of the spec (the domain label is `TickDomainName`):

| Metric | Kind | Domain label | Meaning |
| --- | --- | --- | --- |
| `simulation.tick_time` | Timing | each domain | Time the domain's systems took in one of its ticks |
| `simulation.ticks` | Counter | each domain | Ticks the domain has run |
| `simulation.step_time` | Timing | none | Time one master step took, all domains |
| `simulation.overruns` | Counter | none | Master steps that took longer than their 1/60 s |
| `simulation.system_time.<system>` | Timing | the system's domain | Time one system took in a tick |
| `simulation.backlog_ticks` | Gauge | `hz60` | Master steps owed after the layer's last update |

## Where the code lives

- `TickDomain.hpp` and `TickContext.hpp` are header-only except `DomainTickCount`'s conversions
  (`Source/Scheduler/TickDomain.cpp`).
- `Source/Scheduler/`: `SimulationRuntime.cpp` (the master step, the slots, the domain metrics) and
  `SimulationSettings.cpp`.
- `Source/SimulationLayer/SimulationLayer.cpp`: the layer.
- The step: for each slot in order whose domain is due on this master tick, build the `TickContext` and run the slot's
  systems; then count the tick of every due domain; then advance the master tick.
- Owed steps: master tick k is due at `TimeOfTick(k)` after the layer's first update, so the ticks due at an update
  are `TicksIn(elapsed) + 1`, an integer computation; the layer runs the difference to the ticks it has run, up to
  its limit per update.
- Formats the tests and RunicServer's CTest rely on: the layer logs `RUNIC: Simulation layer attached: simulation
  <id>, <n> worker(s), up to <m> step(s) per update` and, when it detaches, `RUNIC: Simulation layer ran <n> master
  ticks (hz60 <a>, hz30 <b>, hz20 <c>, hz10 <d>), <e> owed`. A backlog is warned about on the first update that ends
  with one, then once every 300 updates in a row.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| SIM-1 | SimulationRuntime: 60 Hz master clock, 30/20/10 Hz domains, TickContext, the Simulation layer, domain metrics; RunicServer runs it with its settings from configuration | Domain tick counts stay exact over a long run, with no drift | **Implemented** (slice 3, preview branch): `RunicSimulationTests` passes with the 28 tests below (26 where asserts are compiled out); RunicServer's CTests `programs.runic_server_clock` and `programs.runic_server_bad_setting` pass; awaiting the owner's review |
| SIM-2 | Phases, commands and the command queue, deferred mutation, event flush at barriers | Commands apply in phase order; facts are visible to the next phase | Not started (slice 5) |
| SIM-3 | Phases on the worker pool, per-worker buffers, deterministic merge | The same scene with one worker and with many gives identical results | Not started (slice 10) |
| SIM-4 to SIM-7 | Prediction subset, service port, replication registry, behavior counters | See RunicDocs › Modules, work packages | Not started (slices 14, 27, 13, 39) |
| SIM-8 | The physics bridge | The capsule's Flecs transform matches its CharacterVirtual after every step | Not started (slice 7) |

SIM-1's acceptance tests (`RunicSimulationTests`): `TickDomain.*` (the rates; when a domain is due; a domain tick has
its master tick's time), `DomainTickCount.*` (exact durations; `Covering` never shorter; conversion between domains;
not constructible from a bare number, a `static_assert`) and `DomainTickCountDeathTest.*`; `SimulationRuntime.*` (the
slot order over a six-tick cycle; ten simulated hours of ticks with exact counts and times; the `TickContext`; the
metrics per domain and per system; overruns counted, never skipped; a system on the worker pool) and
`SimulationRuntimeDeathTest.*`; `SimulationLayer.*` (one step per update on a fixed 60 Hz loop; slower, faster and
uneven loops; a stall caught up over later updates with every step run; the backlog gauge and warning; the detach
report) and `SimulationSettings.*`.

Decisions taken in SIM-1 that await the owner (also in the slice brief): `dt` is a 32-bit float (the exact time is
`TickContext::time`); the 20 Hz slots run before the 10 Hz slots among producers and among consumers; the Simulation
layer owns its runtime; the default catch-up limit is 5 steps per update.

## Future extensions

- Phases inside the realtime slot and the barrier work between them (SIM-2), then phases on the pool (SIM-3).
- Removing a system, when a module is unloaded at run time (the editor's reloaded library, slice 6, re-creates the
  runtime instead).
- Several simulations in one process ("quiet neighbors"), each with its own runtime and layer; nothing here is
  global, so it needs a program that builds two.
- Changing the worker count between ticks, with Core's pool (CORE-2's future extension).
