# Runic::Core — module contract

Group: Foundation · Headless: **yes** · Target: `Runic::Core` (`runic_core`) · Packages: CORE-1 to CORE-8

Source: RunicDocs › Architecture › Repositories, build targets and processes (Engine modules); Modules, work packages
and the implementation contract (CORE-1); World space coordinates, cells and activation (Which values are 64-bit);
Application and LayerStack (Implementation rules); Simulation core clock and threads. Status: **CORE-1 contract**
(slice 1): the public headers and the acceptance tests, which fail until the implementation. The folder map is
[README.md](README.md); the slice brief is [RunicEngine#144](https://github.com/PedroVicente98/RunicEngine/issues/144)
and the approved plan [RunicEngine#9](https://github.com/PedroVicente98/RunicEngine/issues/9).

## Purpose

The Engine's vocabulary: the types every module and every process shares. Plain IDs that cross processes, `Result`
and asserts for failure, logging hooks a host plugs into, one monotonic clock with exact tick arithmetic, the 32-bit
math types on GLM, the 64-bit world-space types and their type rule, and fixed-capacity containers for paths that must
not allocate. Core does no work of its own: later packages add the worker pool, metrics and debug streams (CORE-2),
RenderData (CORE-3 to CORE-7) and process configuration (CORE-8).

## Public API

Namespace `Runic::Core`, with no exception (slice 1 decision D2, 2026-11-14): other modules write `Core::WorldPosition`.
Headers are included by their path under `Public/` (`#include "WorldSpace.hpp"`).

| Header | Holds | Since |
| --- | --- | --- |
| `PlainIds.hpp` | `Id<Tag, Rep>` (a distinct type per kind, explicit from its integer, 0 invalid, ordered, hashable); 64-bit `GlobalEntityId`, `GlobalPlayerId`, `AssetId`; 32-bit `SimulationId`, `WorldId`, `InstanceId`, `WaterBodyId`, `FieldId`, `WidgetId`; the counters `TickId` (64-bit) and `AuthorityEpoch` (32-bit) | CORE-1 |
| `Result.hpp` | `Error` (a message), `Fail`, `Result<T>`, `Result<void>` | CORE-1 |
| `Assert.hpp` | `RUNIC_ASSERT`, `RUNIC_ASSERT_MSG`, `RUNIC_ASSERTS_ENABLED`; `AssertFailure`, `AssertHandler`, `SetAssertHandler`, `FailAssert` | CORE-1 |
| `Log.hpp` | `LogLevel`, `LogRecord`, `LogSink`, `SetLogSink`, `SetMinimumLogLevel`, `MinimumLogLevel`, `IsLogLevelEnabled`, `WriteLog`, `Log`, `kMaxLogLineBytes`, the `RUNIC_LOG_*` macros | CORE-1 |
| `Time.hpp` | `Duration` (`std::chrono::nanoseconds`), `MonotonicClock`, `TimePoint`, `MonotonicNow`, `ToSeconds`, `FromSeconds`, `TickRate` | CORE-1 |
| `Math.hpp` | `Vec2`, `Vec3`, `Vec4`, `Quat`, `Mat3`, `Mat4` (GLM) | CORE-1 |
| `WorldSpace.hpp` | `WorldPosition`, `WorldDelta`, their operators, `DistanceSq`, `ToLocal`, `kMaxLocalOffsetM` | CORE-1 |
| `FixedRingBuffer.hpp` | `FixedRingBuffer<T, N>` | CORE-1 |
| `FixedVector.hpp` | `FixedVector<T, N>` | CORE-1 |

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

## Does not own

- ID values: the Game's data, the Cooker and the services give them (the accounts service issues `GlobalPlayerId`),
  and World's registry says what each `WorldId` is (WORLD-1). Content IDs are Runic::Content (CNT-1).
- Where log lines go (the host: APP-1's entry point) and what happens after an assert beyond the default abort.
- Simulation and frame time: `TickContext`, the clock domains and `DomainTickCount` are Runic::Simulation (SIM-1);
  `FrameTime` is Runic::Application (APP-1). Core gives them `Duration`, `TimePoint` and `TickRate`.
- Region-local origins and cell keys (Runic::World, WORLD-1); camera-relative conversion (Runic::Client).
- Everything of the later Core packages: the worker pool, metrics, debug streams and per-tick arenas (CORE-2),
  RenderData (CORE-3 to CORE-7), process configuration (CORE-8).

## Allowed dependencies

Catalog row: no Runic module. Third parties: GLM (PUBLIC, the approved exception) and the C++ standard library.

## Forbidden dependencies

Every Runic module (Core is the bottom of the Engine); every other third-party library; Godot and platform SDKs; any
I/O except the default log sink's writes to stderr.

## Threading and tick model

- The value types (IDs, `Result`, time types, math and world-space types) have no thread rules of their own.
- `FixedRingBuffer` and `FixedVector` are not thread-safe: one owner at a time.
- `SetLogSink` and `SetAssertHandler` are called at start, before other threads exist. The sink runs on the thread
  that logs, so a sink several threads use must be thread-safe. `SetMinimumLogLevel` and `IsLogLevelEnabled` are
  thread-safe. `MonotonicNow` is thread-safe.
- Nothing in Core blocks, and nothing allocates on a hot path: a log line is formatted into a stack buffer of
  `kMaxLogLineBytes`, and the fixed containers store their elements inline.

## Data ownership

Core keeps three pieces of process-wide state: the log sink with its user pointer, the minimum log level, and the
assert handler. Everything else is a value owned by whoever holds it.

## Integration points

- Hosts install `SetLogSink` (APP-1's entry point writes to stdout and stderr), `SetMinimumLogLevel` and
  `SetAssertHandler`.
- Lines from another system (Godot's output, from GDB-1) enter through `WriteLog`.
- Every other module uses the types directly.

## Where the code lives

- Templates and `constexpr` functions are defined in their public headers: `Id`, `TickId`, `AuthorityEpoch`,
  `Result`, the world-space operators, `TickRate::Hz`, and the members of `FixedRingBuffer` and `FixedVector`, which the
  implementation writes below each class.
- Everything else is defined in `Source/<Subsystem>/`: `Errors/` (`FailAssert`, `SetAssertHandler`), `Logging/` (the
  sink, the level, `WriteLog`, `Detail::WriteFormattedLog`), `Time/` (`MonotonicNow`, `ToSeconds`, `FromSeconds`,
  `TickRate::TimeOfTick`, `TickRate::TicksIn`) and `Math/` (`ToLocal`). `Ids/` and `Memory/` stay empty: those
  subsystems are header-only.
- Formats the tests rely on: the default sink writes `<Level>: <message>` and a newline to stderr; `FailAssert` logs
  `assert failed: <condition>[ - <message>] (<file>:<line>)` at Fatal.
- `TickRate`: `TimeOfTick(k)` = ⌈k × 10⁹ / rate⌉ ns and `TicksIn(t)` = ⌊t × rate / 10⁹⌋, computed without overflow by
  splitting k = q × rate + r and t = s × 10⁹ + n. `FromSeconds` rounds half away from zero.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| CORE-1 | Plain ID types, Result, asserts, logging hooks, time types, GLM math, the world-space types, fixed-capacity containers | Unit tests cover IDs, Result, time conversions and world-space arithmetic; adding two WorldPositions or converting one to a float vector does not compile; fixed-capacity containers never allocate after construction | **Contract** (slice 1): headers and `RunicCoreTests`; the compile-time checks hold, and the tests fail to link until the implementation |
| CORE-2 | Worker pool, metrics registry, debug-stream registry, per-tick arenas and per-worker buffers | Jobs run on the pool; metrics are recorded per tick domain; a per-tick arena resets without returning memory to the system | Not started (slice 3) |
| CORE-3 | RenderData foundation: the double-buffered RenderFrame, DebugGeometry and the debug-draw API | A frame is written on one thread and read on another with no lock; debug vertices need no conversion before upload; every debug shape becomes lines and triangles | Not started (slice 2) |
| CORE-4 to CORE-7 | RenderData kinds: scene, world state, water and fields, effects | See RunicDocs › Modules, work packages | Not started (slices 22, 25a, 42a, 44+) |
| CORE-8 | Process configuration | A setting resolves from each source in precedence order; an unknown key or a bad value is reported with its source | Not started (slice 3) |

CORE-1's acceptance tests (`RunicCoreTests`): `PlainIds.*`, `Result.*`, `Assert.*`, `Log.*`, `LogDefaultSink.*`,
`Time.*`, `WorldSpace.*` and `FixedContainers.*`, plus the `*DeathTest.*` suites that run where asserts are on; the
type-rule checks are `static_assert`s in the test sources, so a violation stops the build.

## Future extensions

- A container whose capacity is chosen at construction and allocated once, when its first consumer needs one
  (Network, slice 12).
- An error code in `Error`, when a caller must branch on the kind of failure.
- Copying and moving the fixed containers, when a consumer needs it.
- More plain ID kinds, added to `PlainIds.hpp` by the package that first needs each.

None of these, and none of CORE-2 to CORE-8, belongs in CORE-1.
