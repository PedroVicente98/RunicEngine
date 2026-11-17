# Runic::Core — module contract

Group: Foundation · Headless: **yes** · Target: `Runic::Core` (`runic_core`) · Packages: CORE-1 to CORE-8

Source: RunicDocs › Architecture › Repositories, build targets and processes (Engine modules); Modules, work packages
and the implementation contract (CORE-1); World space coordinates, cells and activation (Which values are 64-bit);
Application and LayerStack (Implementation rules); Simulation core clock and threads; Render data, extraction and world
UI (RenderData in Core); Debug drawing (Engine). Status: **CORE-1 implemented** (slice 1); **CORE-3 implemented** (slice 2):
the RenderData headers, `Source/RenderData/` and their acceptance tests, which pass with GCC (and under
ThreadSanitizer for the frame exchange); awaiting the verify step. **CORE-2 implemented** (slice 3, on the preview
branch `preview/slices-3-11`): the worker pool, the metrics and debug-stream registries, the per-tick arena and the
per-worker buffers, with their acceptance tests. **CORE-8 implemented** (slice 3, same branch): process
configuration, with its acceptance tests. The folder map
is [README.md](README.md); the slice briefs are [RunicEngine#144](https://github.com/PedroVicente98/RunicEngine/issues/144)
(slice 1) and [RunicEngine#145](https://github.com/PedroVicente98/RunicEngine/issues/145) (slice 2), the plans
[RunicEngine#9](https://github.com/PedroVicente98/RunicEngine/issues/9) (CORE-1) and
[RunicEngine#18](https://github.com/PedroVicente98/RunicEngine/issues/18) (CORE-3). Slice 3's brief and walkthrough
are [preview/slice-03-fixed-step-clock.md](../../../preview/slice-03-fixed-step-clock.md).

## Purpose

The Engine's vocabulary: the types every module and every process shares. Plain IDs that cross processes, `Result`
and asserts for failure, logging hooks a host plugs into, one monotonic clock with exact tick arithmetic, the 32-bit
math types on GLM, the 64-bit world-space types and their type rule, fixed-capacity containers for paths that must
not allocate, and RenderData: the plain frame the client extracts and only GodotBridge reads, starting with debug
geometry and the debug-draw API (CORE-3). CORE-2 adds what a simulation runs on: the worker pool that owns every
simulation thread, the metrics registry and the debug-stream registry a process records into, and the per-tick arenas
and per-worker buffers its phases write into. CORE-8 adds process configuration: the typed settings a process reads
once at start from its defaults, a file, the environment and the command line. Later packages add more RenderData
kinds (CORE-4 to CORE-7).

## Public API

Namespace `Runic::Core` (slice 1 decision D2, 2026-11-14): other modules write `Core::WorldPosition`. RenderData is the
one subfolder and namespace inside Core that RunicDocs defines (`Public/RenderData/`, `Runic::RenderData`). Headers are
included by their path under `Public/` (`#include "WorldSpace.hpp"`, `#include "RenderData/RenderFrame.hpp"`).

| Header | Holds | Since |
| --- | --- | --- |
| `PlainIds.hpp` | `Id<Tag, Rep>` (a distinct type per kind, explicit from its integer, `kInvalidValue` (0) invalid, ordered, hashable); 64-bit `GlobalEntityId`, `GlobalPlayerId`, `AssetId`; 32-bit `SimulationId`, `WorldId`, `InstanceId`, `WaterBodyId`, `FieldId`, `WidgetId`; the counters `TickId` (64-bit, from `kFirstTick`) and `AuthorityEpoch` (32-bit, from `kFirstEpoch`) | CORE-1 |
| `Result.hpp` | `Error` (a message), `Fail`, `Result<T>`, `Result<void>` | CORE-1 |
| `Assert.hpp` | `RUNIC_ASSERT`, `RUNIC_ASSERT_MSG`, `RUNIC_ASSERTS_ENABLED`; `AssertFailure`, `AssertHandler`, `SetAssertHandler`, `FailAssert` | CORE-1 |
| `Log.hpp` | `LogLevel`, `LogRecord`, `LogSink`, `SetLogSink`, `SetMinimumLogLevel`, `MinimumLogLevel`, `IsLogLevelEnabled`, `WriteLog`, `Log`, `kMaxLogLineBytes`, `LogLevelName` and the names in `LogLevelNames`, the `RUNIC_LOG_*` macros | CORE-1 |
| `Time.hpp` | `Duration` (`std::chrono::nanoseconds`), `MonotonicClock`, `TimePoint`, `MonotonicNow`, `ToSeconds`, `FromSeconds`, `TickRate`, `kMasterTicksPerSecond` (60) | CORE-1 |
| `Math.hpp` | `Vec2`, `Vec3`, `Vec4`, `Quat`, `Mat3`, `Mat4` (GLM) | CORE-1 |
| `WorldSpace.hpp` | `WorldPosition`, `WorldDelta`, their operators, `DistanceSq`, `ToLocal`, `kMaxLocalOffsetM` | CORE-1 |
| `FixedRingBuffer.hpp` | `FixedRingBuffer<T, N>` | CORE-1 |
| `FixedVector.hpp` | `FixedVector<T, N>` | CORE-1 |
| `WorkerPool.hpp` | `WorkerIndex`, `kDrivingWorker`, `kSingleWorker`, `kMaxWorkers`, `kDefaultJobQueueCapacity`, `kMaxParallelChunks`, `Job`, `JobGroup` (the barrier's counter), `WorkerPoolSpec`, `ChunkRange`, `WorkerPool` (`Submit`, `Wait`, `ParallelFor`, `CurrentWorker`, `WorkerCount`, `JobsRunBy`, `JobsRunInline`) | CORE-2 |
| `PerWorker.hpp` | `PerWorker<T>` (one `T` per worker slot, each on its own cache lines), `kCacheLineBytes` | CORE-2 |
| `TickArena.hpp` | `TickArena` (`Allocate`, `AllocateArray`, `Create`, `Reset`, `UsedBytes`, `ReservedBytes`, `BlockCount`), `kDefaultArenaBlockBytes` | CORE-2 |
| `MetricsRegistry.hpp` | `MetricKind`, `MetricDescription` (name, clock-domain label, help), `kNoMetricDomain`, `CounterId`, `GaugeId`, `TimingId`, `TimingSnapshot` (`Mean`, `Percentile`), `MetricSnapshot`, `MetricsRegistry` (`AddCounter`, `AddGauge`, `AddTiming`, `Increment`, `Add`, `Set`, `Record`, `Count`, `Capacity`, `Read`), `ScopedTiming`, `kDefaultMetricCapacity`, `kTimingBuckets`, `kFirstTimingBucket`, `TimingBucketLimit` | CORE-2 |
| `ProcessConfig.hpp` | `ConfigSource`, `ConfigSourceName`, `SettingKind`, `BoolSetting`, `IntSetting`, `TextSetting`, `IntRange`, `ConfigProblem`, `ConfigInputs`, `SettingInfo`, `ProcessConfig` (`DeclareBool`, `DeclareInt`, `DeclareText`, `Load`, `LoadForProcess`, `IsLoaded`, `Problems`, `Get`, `SourceOf`, `Count`, `Describe`, `EnvironmentName`), `kConfigFileFlag` (`--config`), `kConfigSetFlag` (`--set`), `kConfigEnvironmentPrefix` (`RUNIC_CFG_`), `kConfigTrue`, `kConfigFalse` | CORE-8 |
| `DebugStreamRegistry.hpp` | `DebugStreamSpec`, `DebugStreamId`, `DebugRecordRead`, `DebugStreamInfo`, `DebugStreamRegistry` (`Add`, `SetEnabled`, `IsEnabled`, `Publish`, `Read`, `Count`, `IdAt`, `Find`, `Describe`), `kFirstDebugRecord`, `kDefaultDebugStreamCapacity`, `kDefaultDebugStreamBytes` | CORE-2 |

RenderData (`Public/RenderData/`, namespace `Runic::RenderData`): plain data, no Flecs, Jolt or Godot types, and
camera-relative floats under one 64-bit origin per frame.

| Header | Holds | Since |
| --- | --- | --- |
| `RenderData/Color.hpp` | `PackedColor` (a 32-bit color whose bytes in memory are R, G, B, A), `PackABGR`, `RedOf`, `GreenOf`, `BlueOf`, `AlphaOf`, `WithAlpha`, `kOpaqueAlpha`, `kHalfAlpha`, and the debug palette `kDebugPredicted`, `kDebugAuthoritative`, `kDebugHarmful`, `kDebugBeneficial`, `kDebugPhysics`, `kDebugQuery`, `kDebugInactive`, `kDebugText` | CORE-3 |
| `RenderData/Camera.hpp` | `Camera` (position relative to the frame's origin, rotation, vertical field of view, near and far planes; Godot's defaults), `LookAt` | CORE-3 |
| `RenderData/DebugGeometry.hpp` | `DebugBatch` (`WorldDepth`, `Overlay`), `DebugPrimitive` (`Lines`, `Triangles`), `DebugGeometryCapacity`, `DebugText`, `DebugGeometry` (per batch and primitive a positions array and a colors array, texts and their bytes; a fixed capacity, adds past it dropped and counted) | CORE-3 |
| `RenderData/DebugDraw.hpp` | `DebugDraw` (line, ray, arrow, AABB, OBB, circle, sphere, cylinder, capsule, cone, arc sector, arc volume, XZ grid, text, filled triangle) and each shape's cost: the `kMin*Segments`, the part counts (`kNoLines` for a shape with no size) and the `*Lines` functions; `kNoInnerRadius` and `kGridHeightM` | CORE-3 |
| `RenderData/RenderFrame.hpp` | `RenderFrame` (number, 64-bit origin, camera, debug geometry) | CORE-3 |
| `RenderData/RenderFrameExchange.hpp` | `RenderFrameExchange` (`WriteFrame`, `Publish`, `ReadNewest`, `PublishedFrames`) | CORE-3 |

**Public exception (approved):** GLM types appear in `Math.hpp` and `WorldSpace.hpp` as the Engine's math vocabulary
(RunicDocs › Modules, work packages and the implementation contract › Module contract). GLM is a PUBLIC dependency, and
`GLM_FORCE_EXPLICIT_CTOR` is a PUBLIC definition of `Runic::Core`, so a 64-bit GLM vector never becomes a 32-bit one
implicitly; `Math.hpp` refuses to compile without it.

**The type rule:** a place in the world is a `WorldPosition` and a difference between places a `WorldDelta`, both
64-bit; directions, velocities, sizes, rotations, local offsets and shapes are 32-bit. Defined: P − P → D, P ± D → P,
D ± D → D, D × double → D, P + Vec3 → P. Not defined, so they do not compile: P + P, P × double, and any conversion of
a `WorldPosition` or a `WorldDelta` to `Vec3`. `ToLocal` is the only way down to 32 bits and asserts that the offset is
under `kMaxLocalOffsetM` (10 km, where the float step is about 1 mm).

## Owns

- The meaning of the plain ID types: their widths, the invalid value, comparison and hashing; not their values.
- The world-space types and the type rule, with `ToLocal` as the only way to 32 bits.
- The process's one monotonic clock and the exact arithmetic of a fixed tick rate.
- Three process-wide hooks the host sets at start: the log sink, the minimum log level and the assert handler.
- The RenderData types and their rules: plain data, camera-relative floats under one 64-bit origin, debug vertices in
  the layout Godot uploads (3 floats per position, colors as the bytes R, G, B, A), and the hand-over of frames from
  one thread to another without a lock.
- The debug-draw API: how each shape becomes lines and triangles, and what it costs.
- The worker pool: the only threads the Engine creates for simulation work, the worker slots (two jobs that run at the
  same time never share one) and the barrier. A pool of one worker creates no thread.
- The form of a metric (a counter, a gauge or a timing with a histogram, labelled with a clock domain) and of a debug
  stream (records of bytes with a sequence number, kept in a ring), and that recording never allocates.
- The per-tick arena's promise: `Reset` returns nothing to the system.
- How a process is configured: the order of precedence (default, file, environment, command line), the file's format
  (`key = value` lines, `#` comment lines), the environment's names (`RUNIC_CFG_` and the key in upper case with `_`
  for `.`), the command line's flags (`--set key=value`, `--config <path>`), how a value of each kind is written, and
  that every problem is reported with its source, all at once.

## Does not own

- ID values: the Game's data, the Cooker and the services give them (the accounts service issues `GlobalPlayerId`),
  and World's registry says what each `WorldId` is (WORLD-1). Content IDs are Runic::Content (CNT-1).
- Where log lines go (the host: APP-1's entry point) and what happens after an assert beyond the default abort.
- Simulation and frame time: `TickContext`, the clock domains and `DomainTickCount` are Runic::Simulation (SIM-1);
  `FrameTime` is Runic::Application (APP-1). Core gives them `Duration`, `TimePoint` and `TickRate`.
- Region-local origins and cell keys (Runic::World, WORLD-1); camera-relative conversion and what goes into a frame
  (Runic::Client, CLI-1).
- Drawing render data: uploads, materials, toggles and the development overlay are Runic::GodotBridge (GDB-2); which
  system draws what, beyond the palette's meanings, is each system's.
- Which metrics and streams exist and what they mean: each module registers its own (the Simulation its clock
  domains, SIM-1). The clock-domain labels are the Simulation's; Core only stores them.
- Publishing metrics (the Platform's exporter, PBND-3) and sending debug streams to the RunicEditor (the Diagnostics
  layer, NET-7): Core only keeps what was recorded and lets them read it.
- What runs on the pool and in which order: phases and their merges are Runic::Simulation (SIM-2, SIM-3); Jolt's job
  system is adapted in Runic::Physics (PHYS-4). The worker count is a setting the program reads (CORE-8, SIM-1).
- Which settings exist: each module declares its own through a function the program calls before it loads the
  configuration (the Simulation's worker count, SIM-1). What a program does when the configuration has a problem (it
  logs the error and stops) is the program's.
- The rest of the command line: flags such as `--quit-after` are Runic::Application's `CommandLine`; the configuration
  reads only `--set` and `--config` and leaves every other argument alone.
- Everything of the later Core packages: the RenderData kinds of CORE-4 to CORE-7 and cached debug meshes (slice 8).

## Allowed dependencies

Catalog row: no Runic module. Third parties: GLM (PUBLIC, the approved exception) and the C++ standard library, with
the platform's thread library where it is a separate one (`Threads::Threads`, PUBLIC, for the worker pool).

## Forbidden dependencies

Every Runic module (Core is the bottom of the Engine); every other third-party library; Godot and platform SDKs; any
I/O except the default log sink's writes to stderr and `ProcessConfig::LoadForProcess`, which reads the configuration
file and the process's environment once, at start.

## Threading and tick model

- The value types (IDs, `Result`, time types, math and world-space types) have no thread rules of their own.
- `FixedRingBuffer` and `FixedVector` are not thread-safe: one owner at a time.
- `SetLogSink` and `SetAssertHandler` are called at start, before other threads exist. The sink runs on the thread
  that logs, so a sink several threads use must be thread-safe. `SetMinimumLogLevel` and `IsLogLevelEnabled` are
  thread-safe. `MonotonicNow` is thread-safe.
- `RenderFrameExchange`: one writer thread (`WriteFrame`, `Publish`) and one reader thread (`ReadNewest`), which may
  be the same thread; each call is one lock-free atomic exchange, so neither side waits. `PublishedFrames` is
  thread-safe. `DebugGeometry` and `DebugDraw` are not thread-safe: one writer per geometry.
- `WorkerPool`: one thread at a time drives a pool from outside (the simulation thread: `Submit`, `Wait`,
  `ParallelFor`), and running jobs may submit and wait too. The driving thread is worker 0 and runs jobs while it
  waits; the pool's threads are workers 1 to N − 1. `Wait` is the barrier: it returns when the group's last job has
  finished. A job never blocks on anything but a nested `Wait`. `Submit` and `Wait` never allocate; a full queue runs
  the job on the submitting thread.
- `PerWorker<T>` gives each worker slot its own `T`, so a parallel phase writes without a lock; the owner reads the
  slots in order after the barrier. `TickArena` is not thread-safe: one user at a time, one arena per worker in a
  parallel phase.
- `MetricsRegistry` and `DebugStreamRegistry`: registration is thread-safe and happens at start (it allocates).
  Recording a metric is lock-free from any thread. Publishing to a debug stream takes that stream's short lock, and
  only when the stream is enabled: a disabled stream costs one atomic load. Reading is thread-safe.
- `ProcessConfig`: settings are declared and loaded at start, on one thread, before anything reads them; after
  `Load` it never changes, so every thread may read it. `LoadForProcess` reads a file: it runs at start, never in a
  tick.
- Nothing in Core blocks outside a barrier, and nothing allocates on a hot path: a log line is formatted into a stack
  buffer of `kMaxLogLineBytes`, the fixed containers store their elements inline, `DebugGeometry` allocates its arrays
  once, at construction, the pool's queue and the registries' slots are allocated at construction, and an arena keeps
  its blocks across ticks.

## Data ownership

Core keeps three pieces of process-wide state: the log sink with its user pointer, the minimum log level, and the
assert handler. Everything else is a value owned by whoever holds it. A `RenderFrameExchange` owns its three frames;
the writer owns the frame `WriteFrame` returned until it publishes it, and the reader the frame `ReadNewest` returned
until its next call. A `WorkerPool` owns its threads and its queue; a job's `user` data and its `JobGroup` belong to
whoever submitted it, and stay alive until the group has been waited for. A registry owns its slots and a copy of
every name and help text; the program owns the registries and hands them to what records into them (they are objects,
not globals). A `ProcessConfig` owns its settings' keys, help texts and values; the program owns it and passes it,
or the values read from it, to what it builds. An arena owns its blocks; what it hands out is valid until its next `Reset`, and no destructor runs.

## Integration points

- Hosts install `SetLogSink` (APP-1's entry point writes to stdout and stderr), `SetMinimumLogLevel` and
  `SetAssertHandler`.
- Lines from another system (Godot's output, from GDB-1) enter through `WriteLog`.
- The client writes frames and GodotBridge reads them through one `RenderFrameExchange` the program owns (in slice 2
  the RunicClient program writes and GDB-2 reads); GodotBridge uploads `DebugGeometry`'s arrays as they are.
- A simulation builds one `WorkerPool` (SIM-1) and runs its phases on it; Physics adapts Jolt's job system onto the
  same pool (PHYS-4), so no library owns threads.
- Modules register their metrics and debug streams in the registries the program gives them. The Platform's exporter
  walks `MetricsRegistry::Count` and `Read` (PBND-3); the Diagnostics layer enables streams and reads them with a
  cursor per reader (NET-7).
- A program builds one `ProcessConfig`, lets each module it composes declare its settings, calls `LoadForProcess`
  with its command line and stops with the error if it fails (RunicServer, SIM-1). The Platform's server bindings and
  the Game's programs declare theirs the same way.
- Every other module uses the types directly.

## Where the code lives

- Templates and `constexpr` functions are defined in their public headers: `Id`, `TickId`, `AuthorityEpoch`,
  `Result`, the world-space operators, `TickRate::Hz`, and the members of `FixedRingBuffer` and `FixedVector`, which the
  implementation writes below each class.
- Everything else is defined in `Source/<Subsystem>/`: `Errors/` (`FailAssert`, `SetAssertHandler`), `Logging/` (the
  sink, the level, `LogLevelName`, `WriteLog`, `Detail::WriteFormattedLog`), `Time/` (`MonotonicNow`, `ToSeconds`,
  `FromSeconds`, `TickRate::TimeOfTick`, `TickRate::TicksIn`), `Math/` (`ToLocal`) and `RenderData/` (`DebugGeometry`'s
  construction and adds, `DebugDraw`'s shapes, `RenderFrameExchange`). `Ids/` and `Memory/` have no folder yet: those
  subsystems are header-only so far. In RenderData, `Color`, `Camera`, `RenderFrame` and the line-count functions are
  header-only.
- CORE-2's code: `Source/WorkerPool/` (the pool), `Source/Memory/` (`TickArena`), `Source/Metrics/` and
  `Source/DebugStreams/` (the registries). `PerWorker` and the template part of `ParallelFor` are header-only.
- CORE-8's code: `Source/Config/ProcessConfig.cpp`. `Load` is pure (it reads only its inputs), which is what the
  tests use; `LoadForProcess` gathers the file's text and the `RUNIC_CFG_` variables and calls it.
- Formats the tests rely on: a problem is `<source>: <message>`, where the source is `<file>:<line>`,
  `environment <NAME>` or `command line --set <key>=<value>`; `Load`'s error is `<n> configuration problem(s)` followed
  by one indented problem per line. A Bool is `true` or `false`; an Int is decimal with an optional sign; a Text is
  the rest of the line without the blanks around it (in the file) or the value as given (elsewhere). A key set twice
  in the file is a problem; on the command line the last `--set` wins.
- The pool: one fixed ring of jobs under one mutex and one condition variable, signalled when a job is queued, when a
  group finishes and when the pool stops. `ParallelFor` submits one runner per worker (at most one per chunk); the
  runners claim chunk numbers from one atomic counter, so the chunks depend on the count and the chunk size only.
- A timing's histogram has `kTimingBuckets` (20) buckets whose limits double from `kFirstTimingBucket` (16 µs) to about
  8.4 s; `Percentile` returns the limit of the bucket that holds the rank asked for, never above the maximum.
- A debug stream stores each record as a 32-bit size and its bytes in a ring; a full ring drops its oldest records,
  and a reader whose cursor is older learns how many it missed.
- The frame exchange: three slots; the writer's and the reader's are their own, and one atomic byte holds the third
  slot's index and whether it holds a frame the reader has not taken.
- Formats the tests rely on: the default sink writes `<Level>: <message>` and a newline to stderr; `FailAssert` logs
  `assert failed: <condition>[ - <message>] (<file>:<line>)` at Fatal.
- `TickRate`: `TimeOfTick(k)` = ⌈k × 10⁹ / rate⌉ ns and `TicksIn(t)` = ⌊t × rate / 10⁹⌋, computed without overflow by
  splitting k = q × rate + r and t = s × 10⁹ + n. `FromSeconds` rounds half away from zero.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| CORE-1 | Plain ID types, Result, asserts, logging hooks, time types, GLM math, the world-space types, fixed-capacity containers | Unit tests cover IDs, Result, time conversions and world-space arithmetic; adding two WorldPositions or converting one to a float vector does not compile; fixed-capacity containers never allocate after construction | **Implemented** (slice 1): `RunicCoreTests` passes, 39 tests in Debug, 31 in Release (where asserts are compiled out); awaiting the verify step |
| CORE-2 | Worker pool, metrics registry, debug-stream registry, per-tick arenas and per-worker buffers | Jobs run on the pool; metrics are recorded per tick domain; a per-tick arena resets without returning memory to the system | **Implemented** (slice 3, preview branch): `RunicCoreTests` passes with the 40 CORE-2 tests below, with GCC, with clang and warnings as errors, and under ThreadSanitizer, AddressSanitizer and UBSan; awaiting the owner's review |
| CORE-3 | RenderData foundation: the double-buffered RenderFrame, DebugGeometry and the debug-draw API | A frame is written on one thread and read on another with no lock; debug vertices need no conversion before upload; every debug shape becomes lines and triangles | **Implemented** (slice 2): `RunicCoreTests` passes with the 23 CORE-3 tests below; decisions D1–D3, D5 and D6 in RunicEngine#145; awaiting the verify step |
| CORE-4 to CORE-7 | RenderData kinds: scene, world state, water and fields, effects | See RunicDocs › Modules, work packages | Not started (slices 22, 25a, 42a, 44+) |
| CORE-8 | Process configuration | A setting resolves from each source in precedence order; an unknown key or a bad value is reported with its source | **Implemented** (slice 3, preview branch): `RunicCoreTests` passes with the 17 CORE-8 tests below (13 where asserts are compiled out); three decisions await the owner (file format, the `RUNIC_CFG_` prefix, `--set`), listed in the slice brief; awaiting the owner's review |

CORE-1's acceptance tests (`RunicCoreTests`): `PlainIds.*`, `Result.*`, `Assert.*`, `Log.*`, `LogDefaultSink.*`,
`Time.*`, `WorldSpace.*` and `FixedContainers.*`, plus the `*DeathTest.*` suites that run where asserts are on; the
type-rule checks are `static_assert`s in the test sources, so a violation stops the build.

CORE-3's acceptance tests: `RenderFrameExchange.*` (a frame written on one thread is read whole on another; one
lock-free atomic; no allocation), `DebugGeometry.*` and `Color.*` (the vertex layout Godot uploads; the budget; text
copied in), `DebugDraw.*` (every shape's lines against its line-count function; filled triangles keep their opacity;
the batch; degenerate shapes; no allocation for a frame of every shape) and `Camera.*`.

CORE-2's acceptance tests: `WorkerPool.*` (every job runs once; jobs run on the pool's threads at the same time and
never share a slot; the barrier; nested submits and waits; a full queue; `ParallelFor` covers every index once with
chunks that do not depend on the worker count; no allocation), `PerWorker.*`, `TickArena.*` (aligned, non-overlapping
storage; a reset tick allocates nothing and returns nothing; an oversized request), `MetricsRegistry.*` (one metric per
clock domain; percentiles; several threads; no allocation; a full registry) and `DebugStreamRegistry.*` (a disabled
stream keeps nothing; two readers; a full ring; several publishing threads; no allocation).

CORE-8's acceptance tests: `ProcessConfig.*` (defaults; each source wins over the ones before it; every kind from
every source; the file's format; an unknown key and a bad value reported with their source, all at once; malformed
lines and flags; `Describe`), `ProcessConfigFile.*` (the running process: the file `--config` names, the environment,
an unreadable file) and `ProcessConfigDeathTest.*` (declaring twice, after the load, a bad key, a default outside its
range).

## Future extensions

- More setting kinds (a decimal number, a duration, a list), each with the package that first needs it.
- Reloading a development flag while the process runs, if a tool needs it; today the configuration is read once.
- A listing of every setting with its help (`--help`), built on `Count` and `Describe`.
- Changing a pool's worker count at a safe point between ticks (RunicDocs › Threading model), when a process resizes
  its CPU allotment; today the count is fixed when the pool is built.
- A lock-free or work-stealing queue, if the queue's mutex shows in a profile (slice 38, battle scale).
- Worker-busy time per slot, for the worker-utilization metric, next to the job counts the pool keeps today.

- A container whose capacity is chosen at construction and allocated once, when its first consumer needs one
  (Network, slice 12).
- An error code in `Error`, when a caller must branch on the kind of failure.
- Copying and moving the fixed containers, when a consumer needs it.
- More plain ID kinds, added to `PlainIds.hpp` by the package that first needs each.
- Cached debug meshes, with their first user, the physics geometry layer (PHYS-3 and GDB-4, slice 8; slice 2 decision
  D2).
- Several writers into one frame (per-worker debug geometries merged in a fixed order), when extraction runs on the
  worker pool (CLI-1 and later).

None of these belongs in CORE-1 or CORE-3.
