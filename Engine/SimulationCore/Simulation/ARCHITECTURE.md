# Runic::Simulation — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::Simulation` (`runic_simulation`) · Packages: SIM-1 to SIM-8

Source: RunicDocs › Architecture › Simulation core clock and threads (Engine); Repositories, build targets and
processes (Engine modules); Modules, work packages and the implementation contract (SIM-1); Application and LayerStack
(Engine); Tools › Metrics every process exports; Simulation core Flecs and Jolt (Engine) › Realtime 30 Hz phase,
Deferred mutation and Events. Status: **SIM-1 implemented** (slice 3, on the preview branch `preview/slices-3-11`):
the clock domains, `TickContext`, `DomainTickCount`, the `SimulationRuntime`, the Simulation layer and the
Simulation's settings; RunicServer runs it. **SIM-2 implemented** (slice 5, same branch): the realtime phases and
their barriers, command queues, and the simulation's entities, states and event hub. **SIM-8 implemented** (slice 7,
same branch): the physics bridge. Each with its acceptance tests. The folder map is [README.md](README.md); the slice briefs and walkthroughs are
[preview/slice-03-fixed-step-clock.md](../../../preview/slice-03-fixed-step-clock.md) and
[preview/slice-05-moving-entities.md](../../../preview/slice-05-moving-entities.md) and
[preview/slice-07-physics.md](../../../preview/slice-07-physics.md).

## Purpose

The SimulationRuntime owns the tick. A simulation advances in exact fixed steps on a 60 Hz master clock, with 30, 20
and 10 Hz domains that tick on every 2nd, 3rd and 6th master step, and every step runs the systems that are due in a
fixed order. Wall-clock time only decides how many steps are owed: the Simulation layer reads it from the
application's loop, and nothing inside a simulation reads a clock. SIM-1 builds the clock, the scheduler, the layer and
the domain metrics. SIM-2 gives the runtime its world (entities, their states and an event hub) and splits the 30 Hz
realtime slot into seven phases with a barrier after each, where held-back world changes apply and emitted facts
become readable; input enters the world only through command queues drained in the first phase. SIM-8 adds the physics bridge: an
opt-in physics world whose bodies and characters follow their entities, synced on each side of the step, with
contacts as facts. Later packages add
phases on the worker pool (SIM-3), prediction (SIM-4), the service port (SIM-5), the replication registry (SIM-6) and
behavior counters (SIM-7).

## Public API

Namespace `Runic::Simulation`. Headers are included by their path under `Public/` (`#include "SimulationRuntime.hpp"`).
No library types.

| Header | Holds | Since |
| --- | --- | --- |
| `TickDomain.hpp` | `TickDomain` (`Hz60`, `Hz30`, `Hz20`, `Hz10`), `kTickDomainCount`, `kTickDomains`, `IndexOf`, `MasterTicksPerTick` (1, 2, 3, 6), `TicksPerSecond`, `RateOf`, `StepSeconds`, `TickDomainName` (`hz60` … `hz10`, the metrics label), `IsDue`, `DomainTickAt`; `DomainTickCount` (a tick count tagged with its domain: `Covering`, `ToDuration`, `In`, `+`, `−`, comparisons inside one domain) | SIM-1 |
| `TickContext.hpp` | `TickContext` (domain, master tick, domain tick, `dt`, simulation time) | SIM-1 |
| `SimulationRuntime.hpp` | `ScheduleSlot` (`Hz20Producers`, `Hz10Producers`, `Hz30Realtime`, `Hz20Consumers`, `Hz10Consumers`, `Hz60Hook`), `kScheduleSlotCount`, `DomainOf`, `ScheduleSlotName`, `SystemFunction`, `SystemSpec`, `kStandaloneSimulation`, `SimulationSpec` (id, workers, metrics), `SimulationRuntime` (`AddSystem`, `RunMasterTick`, `Id`, `NextMasterTick`, `TicksRun`, `Time`, `Workers`, `Metrics`), and the metric names `kTickTimeMetric`, `kTicksMetric`, `kStepTimeMetric`, `kOverrunsMetric`, `kSystemTimeMetricPrefix` | SIM-1 |
| `SimulationRuntime.hpp` | `RealtimePhase` (`Commands`, `Movement`, `PhysicsStructure`, `PhysicsStep`, `PostPhysics`, `Queries`, `Commit`), `kRealtimePhaseCount`, `RealtimePhaseName`, `SystemSpec::phase`; `SimulationRuntime::Entities`, `States`, `EventHub`, `StateEnteredFacts`, `StateExitedFacts`, `IsRunning`, `CommandQueueCount`, `DescribeCommandQueue`; `CommandQueueInfo`; `kCommandsAppliedMetricPrefix`, `kStateEnteredBufferName` (`engine:state_entered`), `kStateExitedBufferName` (`engine:state_exited`) | SIM-2 |
| `CommandQueue.hpp` | `CommandQueue<T>` (`Push` from any thread, `Drain(apply)` in the Commands phase, `Name`, `Describe`) | SIM-2 |
| `SimulationRuntime.hpp` | `SimulationRuntime::EnablePhysics`, `FindPhysicsBridge` | SIM-8 |
| `PhysicsBridge.hpp` | `PhysicsBody` (body handle, motion), `PhysicsCharacter` (character handle, next move), `PhysicsContact` (the fact: phase, entities, point, normal), `PhysicsBridgeStats`, `PhysicsBridge` (`AddBody`, `AddCharacter`, `Remove`, `World`, `Contacts`, `Stats`); the names `kPhysicsBodyName` (`engine:physics_body`), `kPhysicsCharacterName` (`engine:physics_character`), `kPhysicsContactsBufferName` (`engine:physics_contacts`) and the systems' `engine:physics_structure`, `engine:physics_step`, `engine:physics_write_back` | SIM-8 |
| `SimulationLayer.hpp` | `SimulationLayer` (an `Application::Layer` that owns a runtime: `Runtime`, `BacklogTicks`), `SimulationLayerSpec`, `kSimulationLayerName`, `kDefaultMaxTicksPerUpdate` (5), `kBacklogMetric` | SIM-1 |
| `SimulationSettings.hpp` | `SimulationSettings`, `DeclareSimulationSettings`, `SimulationSpecFrom`, `SimulationLayerSpecFrom`, the keys `kWorkersSetting` (`simulation.workers`) and `kMaxTicksPerUpdateSetting` (`simulation.max_ticks_per_update`), `kMaxTicksPerUpdateLimit` | SIM-1 |

**The time rule:** a system reads time from its `TickContext` and nowhere else. `TickContext::time` is computed from
the tick's number with integer arithmetic (`Core::TickRate::TimeOfTick`), so a run of any length never drifts, and a
domain tick has the time of its master tick to the nanosecond. `dt` is the domain's fixed step in seconds as a 32-bit
float, for gameplay math; it is never accumulated into a time. A duration in simulation code is a `Core::Duration` or
a `DomainTickCount`; `DomainTickCount` is not constructible from a bare number, and counts of two domains do not mix.

**The phase rule** (SIM-2): a phase is a run of systems with a barrier after it. Every slot other than the realtime
one is one phase; the realtime slot is seven, always in the order of `RealtimePhase`, whatever order their systems
were added in. While a phase runs, the world's shape holds still: `Set` of a new component, `Remove`, `Destroy` and
state changes that would move an entity are held back (Runic::ECS's `DeferScope`), and values written into existing
components change in place. Facts emitted into the runtime's event hub are not readable in the phase that emits them.
At the barrier the held-back changes apply in the order they were asked for and the hub is flushed, so every system of
the next phase sees the world and the facts the previous phase left, with event IDs given in a fixed order (Runic::
Events). A barrier also opens every master step, so a fact emitted between steps (a service result applied at a safe
point) is readable in the step's first phase.

**The command rule** (SIM-2): player and AI input changes the world only through a `CommandQueue<T>`. `Push` is safe
from any thread at any time and touches nothing but the queue. The queue's owner drains it in a system of the
`Commands` phase, the first of the realtime slot, which applies every command queued so far in the order it was
queued; a command queued during the drain waits for the next one. `Drain` anywhere else is a fault (asserted). A
command is applied, not just accepted: the system that drains it validates it and changes the world, and a rejected
command changes nothing.

**The physics rule** (SIM-8): physics is opt-in (`EnablePhysics`, once, between steps); the runtime then owns a
`Physics::PhysicsWorld` and the bridge, which registers two components and adds one system to each of the
PhysicsStructure, PhysicsStep and PostPhysics phases. The entities hold the truth: an entity's `WorldTransform` is
where it is, and its body or character is an opaque handle in `PhysicsBody` or `PhysicsCharacter`, which only the
bridge adds and removes.

- *Bodies follow their entities.* `AddBody`, `AddCharacter` and `Remove` are requests; the PhysicsStructure phase
  applies them, removals first, bodies in one batch (Jolt's prepare and finalize), each at its entity's transform and
  owned by it. The component arrives at the phase's barrier, so the entity has its body from PhysicsStep on.
  Destroying an entity (or taking its component) is enough: at every barrier, once held-back changes have applied, the
  bridge compares how many entities have its components with how many it keeps, and only when fewer remain looks for
  the ones that went and takes their bodies and characters. The entities and Physics never disagree inside a phase.
- *A sync on each side of the step.* PhysicsStep moves every kinematic body to its entity's transform, hands every
  character its `move` (and clears it, so a character nobody moves stands), then steps the world once with the
  realtime `dt`. PostPhysics writes every dynamic body's pose and every character's position (its feet) into their
  transforms; a character's rotation stays the entity's own. Static bodies never move.
- *Contacts are facts.* Physics only buffers what Jolt reports inside the step; PostPhysics emits each as a
  `PhysicsContact` with the entities of its bodies into `engine:physics_contacts`, readable from the Queries phase on.
  No Jolt callback touches the entities.

## Owns

- The tick rates: the 60 Hz master clock, the three slower domains and which master ticks each is due on.
- The order of a master step: the 20 Hz producers, the 10 Hz producers, the 30 Hz realtime phase, the 20 Hz
  consumers, the 10 Hz consumers, the 60 Hz hook; inside a slot, the order systems were added in.
- Master and domain tick numbers and simulation time.
- The simulation's worker pool (a `Core::WorkerPool`, one per runtime).
- The simulation's world (SIM-2): its `ECS::EntityWorld` with World's components registered, its `ECS::EntityStates`,
  its `Events::EventHub` (its event IDs carry the simulation's ID) and the two buffers that carry the facts of
  observable states.
- The phases of the realtime slot and the barrier after every phase; the command queues' list, their counts and
  their metric (SIM-2).
- The physics bridge, once enabled (SIM-8): the physics world, the two components, the requests, which entity has
  which body or character, the sync and the contact facts' buffer.
- The rule for falling behind: every due step runs, late if need be; the backlog is a metric and a warning, and no
  step is skipped.
- The Simulation's metrics (names below) and its two settings.

## Does not own

- The loop and wall-clock time: Runic::Application's host paces the frames and gives the layer `FrameTime`.
- The pool itself, the metrics registry and the configuration (Runic::Core: CORE-2, CORE-8); the program owns the
  registry and the configuration and passes them in.
- What systems do: each module registers its own. The world's data is Runic::ECS (ECS-1), events are Runic::Events
  (EVT-1), physics is Runic::Physics (the bridge only connects it to the entities).
- Phases on the worker pool and the deterministic merge (SIM-3, slice 10). Until then a phase's systems run one
  after another on the driving thread, and each may split its own work with `Workers().ParallelFor`.
- How entities, states and events work (Runic::ECS, ECS-1 and ECS-2; Runic::Events, EVT-1): the runtime owns one of
  each and decides when held-back changes apply and when the hub flushes.
- What a command means: each module defines its command types and the system that applies them.
- Publishing metrics (the Platform's exporter, PBND-3) and the `SimulationId` values of a deployed world (the
  Platform's services); a process on its own uses `kStandaloneSimulation`.

## Allowed dependencies

Catalog row: Core, Application, ECS, Events, World, Physics, Content. SIM-1 links Core and Application (PUBLIC: the
layer is an `Application::Layer`, and the public headers use Core's time, ID, pool, metrics and configuration types).
SIM-2 adds ECS and Events (PUBLIC: `SimulationRuntime.hpp` hands out the `EntityWorld`, `EntityStates`, `EventHub` and
two `EventBuffer`s) and World (PUBLIC: the runtime registers World's components on its entities, and every user of
those entities places them with `World::WorldTransform` and `World::Region`). SIM-8 adds Physics (PUBLIC:
`PhysicsBridge.hpp` hands out the `PhysicsWorld` and puts its handles in components). The ECS native target is not
linked; Content is not linked yet.

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
- The world (`Entities`, `States`, `EventHub`) belongs to the driving thread: systems change it in their phases, and
  setup code and safe-point code change it between steps, when changes apply at once. No other thread touches it.
- `CommandQueue::Push` and `Describe` are safe from any thread: a mutex guards the incoming list, held only to append
  or to swap lists. `Drain` runs on the driving thread in the Commands phase; it applies the commands outside the
  lock, so a producer never waits for the world. A queue is made and destroyed between steps, before its runtime
  is destroyed (asserted).
- Nothing here blocks or reads a file or a socket. `RunMasterTick` allocates nothing itself.
- The physics bridge belongs to the driving thread too: requests are made between steps or from systems, and its
  systems run in their phases. Jolt runs on the driving thread (Physics, until PHYS-4).
- The layer runs at most `maxTicksPerUpdate` steps per update. What is still due stays owed and runs in later
  updates.

## Data ownership

A `SimulationRuntime` owns its worker pool, its systems (their names and functions), its tick counters and, from
SIM-2, its entity world, the states declared on it, its event hub and the two state-fact buffers. Event buffers that
modules make on the hub and command queues belong to whoever made them and must go before the runtime (a queue
checks it). A command queue owns the commands queued on it until they are applied. It holds a
pointer to the metrics registry of its spec, which must outlive it. A `SimulationLayer` owns its runtime; the
application's layer stack owns the layer.

## Integration points

- A program (RunicServer; later the RunicEditor's local world and RunicGame's GameServer) declares the settings with
  `DeclareSimulationSettings`, loads its configuration, builds a `SimulationRuntime` from `SimulationSpecFrom`, lets
  modules add their systems and pushes a `SimulationLayer` that owns the runtime.
- Modules add systems with `AddSystem` and take time from `TickContext`; a realtime system names its phase in
  `SystemSpec::phase`.
- Modules register their components, declare their states and make their queries on `Entities()` and `States()`, and
  make their event buffers on `EventHub()`, all at setup.
- A module that takes input makes a `CommandQueue<T>` and adds one system to the `Commands` phase that drains it.
  Producers (a network receive thread, a 20 Hz AI system, a tool) only push.
- The metrics, in the registry of the spec (the domain label is `TickDomainName`):

| Metric | Kind | Domain label | Meaning |
| --- | --- | --- | --- |
| `simulation.tick_time` | Timing | each domain | Time the domain's systems took in one of its ticks |
| `simulation.ticks` | Counter | each domain | Ticks the domain has run |
| `simulation.step_time` | Timing | none | Time one master step took, all domains |
| `simulation.overruns` | Counter | none | Master steps that took longer than their 1/60 s |
| `simulation.system_time.<system>` | Timing | the system's domain | Time one system took in a tick |
| `simulation.backlog_ticks` | Gauge | `hz60` | Master steps owed after the layer's last update |
| `simulation.commands_applied.<queue>` | Counter | `hz30` | Commands of one queue applied to the world (SIM-2) |

## Where the code lives

- `TickDomain.hpp` and `TickContext.hpp` are header-only except `DomainTickCount`'s conversions
  (`Source/Scheduler/TickDomain.cpp`).
- `Source/Scheduler/`: `SimulationRuntime.cpp` (the master step, the slots, the domain metrics) and
  `SimulationSettings.cpp`.
- `Source/SimulationLayer/SimulationLayer.cpp`: the layer.
- `Source/Scheduler/RuntimeState.hpp`: the runtime's state, shared by the scheduler, the phases and the commands.
- `Source/Phases/Phases.cpp`: `RunPhase` (the phase's systems inside one `ECS::EntityWorld::DeferScope`, then the
  physics bridge's look at destroyed entities, then `EventHub::Flush`) and the barrier that opens a step.
- `Source/PhysicsBridge/PhysicsBridge.cpp`: the bridge (its requests, systems, queries and the barrier check) and
  `SimulationRuntime::EnablePhysics`.
- `Source/Commands/CommandQueueBase.cpp`: a queue's registration, counts and metric; the typed part is in
  `Public/CommandQueue.hpp` because it is a template. A drain swaps the incoming list with the list being applied
  under the lock, so once both have grown to the largest burst, queuing and applying allocate nothing.
- The state facts: the runtime is its `EntityStates`' fact sink and emits each fact into `engine:state_entered` or
  `engine:state_exited`, so they flush at the barrier like any other fact.
- The step: a barrier; then, for each slot in order whose domain is due on this master tick, build the `TickContext`
  and run the slot's systems as one phase or, in the realtime slot, phase by phase; then count the tick of every due
  domain; then advance the master tick. A phase with no systems is skipped and costs nothing.
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
| SIM-2 | Phases, commands and the command queue, deferred mutation, event flush at barriers | Commands apply in phase order; facts are visible to the next phase | **Implemented** (slice 5, preview branch): `RunicSimulationTests` passes with SIM-1's 28 tests and the 22 below (18 where asserts are compiled out); awaiting the owner's review |
| SIM-3 | Phases on the worker pool, per-worker buffers, deterministic merge | The same scene with one worker and with many gives identical results | Not started (slice 10) |
| SIM-4 to SIM-7 | Prediction subset, service port, replication registry, behavior counters | See RunicDocs › Modules, work packages | Not started (slices 14, 27, 13, 39) |
| SIM-8 | The physics bridge | The capsule's Flecs transform matches its CharacterVirtual after every step; destroying an entity removes its body at the next barrier; a contact reaches the next phase as a fact and no callback touches Flecs | **Implemented** (slice 7, preview branch): `RunicSimulationTests` passes with the 8 tests below added (7 where asserts are compiled out); awaiting the owner's review |

SIM-1's acceptance tests (`RunicSimulationTests`): `TickDomain.*` (the rates; when a domain is due; a domain tick has
its master tick's time), `DomainTickCount.*` (exact durations; `Covering` never shorter; conversion between domains;
not constructible from a bare number, a `static_assert`) and `DomainTickCountDeathTest.*`; `SimulationRuntime.*` (the
slot order over a six-tick cycle; ten simulated hours of ticks with exact counts and times; the `TickContext`; the
metrics per domain and per system; overruns counted, never skipped; a system on the worker pool) and
`SimulationRuntimeDeathTest.*`; `SimulationLayer.*` (one step per update on a fixed 60 Hz loop; slower, faster and
uneven loops; a stall caught up over later updates with every step run; the backlog gauge and warning; the detach
report) and `SimulationSettings.*`.

SIM-2's acceptance tests (`RunicSimulationTests`): `RealtimePhases.*` (the seven phases run in their fixed order
whatever order their systems were added in; a phase's systems in the order they were added; the realtime slot still
between producers and consumers; `IsRunning` names the running phase and no other), `Barriers.*` (a fact emitted in a
phase is readable from the next phase on and not in its own; every other slot is one phase; a fact emitted between
steps is readable in the next step's first phase; a component added and an entity destroyed in a phase take effect at
its barrier; observable state facts arrive as events after the barrier; event IDs are the same on every run),
`SimulationRuntime.OwnsAnEntityWorldWithTheWorldsComponentsRegistered`, `CommandQueue.*` (commands change the world in
the Commands phase in the order they were queued, before Movement reads it; a command queued after a realtime tick
waits for the next; a command queued during a drain waits for the next drain; 20,000 commands pushed from four
threads while the simulation ticks all apply, each thread's in its order; tools list every queue with its counts; the
applied metric) and `CommandQueueDeathTest.*` (no drain outside the Commands phase, between steps or from another
slot; the runtime outlives its queues).
SIM-8's acceptance tests (`RunicSimulationTests`): `PhysicsBridge.*` (**a walking capsule's transform equals its
character's feet exactly after every step, 8,600 km from the origin**; bodies arrive in the PhysicsStructure phase,
visible from PhysicsStep on; **destroying an entity in a phase takes its body at that phase's barrier, and between
steps at the barrier that opens the next step**; kinematic bodies follow their transforms and dynamic poses come back
exactly; **a contact is read once, in the phase after the one that emitted it, with its two entities**; remove
requests take bodies and drop waiting requests; an entity without a transform gets one from its description) and
`PhysicsBridgeDeathTest.*`.

Decisions taken in SIM-8 that await the owner (also in the slice brief): physics is opt-in per runtime, owned by it;
the bridge's components are added and removed only by the bridge; a destroyed entity is noticed by comparing counts
at every barrier (Runic::ECS has no removal hook); a character's rotation is the entity's own; static bodies never
follow their entity; the contact owner is the entity's local ID, inside the process only.

Decisions taken in SIM-2 that await the owner (also in the slice brief): every slot other than the realtime one is a
single phase with a barrier after it; a barrier opens every master step; values written into existing components
change in place during a phase (only changes that move an entity are held back), as Runic::ECS decided; World is a
PUBLIC dependency.

Decisions taken in SIM-1 that await the owner (also in the slice brief): `dt` is a 32-bit float (the exact time is
`TickContext::time`); the 20 Hz slots run before the 10 Hz slots among producers and among consumers; the Simulation
layer owns its runtime; the default catch-up limit is 5 steps per update.

## Future extensions

- Writing back only the bodies Jolt moved (its active bodies) instead of every dynamic one, and a removal hook in
  Runic::ECS so a destroyed entity is found without a scan, when the numbers ask for it.
- Teleports and facing through the bridge (today `World().SetCharacterPose`).

- Phases on the pool (SIM-3): the systems of one phase that touch different data run at once, with per-worker
  command and event buffers merged in a fixed order at the barrier.
- Phases for the other slots, if a 20 Hz or 10 Hz slot needs a barrier inside it.
- A bound on a command queue (a full queue refuses the command and counts it), when the network's per-connection
  limits need one.
- Removing a system, when a module is unloaded at run time (the editor's reloaded library, slice 6, re-creates the
  runtime instead).
- Several simulations in one process ("quiet neighbors"), each with its own runtime and layer; nothing here is
  global, so it needs a program that builds two.
- Changing the worker count between ticks, with Core's pool (CORE-2's future extension).
