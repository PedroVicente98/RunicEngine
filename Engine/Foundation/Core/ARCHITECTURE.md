# Runic::Core — module contract

Group: Foundation · Headless: **yes** · Target: `Runic::Core` (`runic_core`) · Packages: CORE-1 to CORE-8

Source: RunicDocs › Architecture › Repositories, build targets and processes (Engine modules); Modules, work packages
and the implementation contract (CORE-1); World space coordinates, cells and activation (Which values are 64-bit);
Application and LayerStack (Implementation rules); Simulation core clock and threads; Render data, extraction and world
UI (RenderData in Core); Debug drawing (Engine). Status: **CORE-1 implemented** (slice 1); **CORE-3 contract** (slice 2):
the RenderData headers and their acceptance tests, which fail to link until `Source/RenderData/` exists. The folder map
is [README.md](README.md); the slice briefs are [RunicEngine#144](https://github.com/PedroVicente98/RunicEngine/issues/144)
(slice 1) and [RunicEngine#145](https://github.com/PedroVicente98/RunicEngine/issues/145) (slice 2), the plans
[RunicEngine#9](https://github.com/PedroVicente98/RunicEngine/issues/9) (CORE-1) and
[RunicEngine#18](https://github.com/PedroVicente98/RunicEngine/issues/18) (CORE-3).

## Purpose

The Engine's vocabulary: the types every module and every process shares. Plain IDs that cross processes, `Result`
and asserts for failure, logging hooks a host plugs into, one monotonic clock with exact tick arithmetic, the 32-bit
math types on GLM, the 64-bit world-space types and their type rule, fixed-capacity containers for paths that must
not allocate, and RenderData: the plain frame the client extracts and only GodotBridge reads, starting with debug
geometry and the debug-draw API (CORE-3). Later packages add the worker pool, metrics and debug streams (CORE-2), more
RenderData kinds (CORE-4 to CORE-7) and process configuration (CORE-8).

## Public API

Namespace `Runic::Core` (slice 1 decision D2, 2026-11-14): other modules write `Core::WorldPosition`. RenderData is the
one subfolder and namespace inside Core that RunicDocs defines (`Public/RenderData/`, `Runic::RenderData`). Headers are
included by their path under `Public/` (`#include "WorldSpace.hpp"`, `#include "RenderData/RenderFrame.hpp"`).

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

RenderData (`Public/RenderData/`, namespace `Runic::RenderData`): plain data, no Flecs, Jolt or Godot types, and
camera-relative floats under one 64-bit origin per frame.

| Header | Holds | Since |
| --- | --- | --- |
| `RenderData/Color.hpp` | `PackedColor` (a 32-bit color whose bytes in memory are R, G, B, A), `PackABGR`, `RedOf`, `GreenOf`, `BlueOf`, `AlphaOf`, `WithAlpha`, `kOpaqueAlpha`, `kHalfAlpha`, and the debug palette `kDebugPredicted`, `kDebugAuthoritative`, `kDebugHarmful`, `kDebugBeneficial`, `kDebugPhysics`, `kDebugQuery`, `kDebugInactive`, `kDebugText` | CORE-3 |
| `RenderData/Camera.hpp` | `Camera` (position relative to the frame's origin, rotation, vertical field of view, near and far planes; Godot's defaults), `LookAt` | CORE-3 |
| `RenderData/DebugGeometry.hpp` | `DebugBatch` (`WorldDepth`, `Overlay`), `DebugPrimitive` (`Lines`, `Triangles`), `DebugGeometryCapacity`, `DebugText`, `DebugGeometry` (per batch and primitive a positions array and a colors array, texts and their bytes; a fixed capacity, adds past it dropped and counted) | CORE-3 |
| `RenderData/DebugDraw.hpp` | `DebugDraw` (line, ray, arrow, AABB, OBB, circle, sphere, cylinder, capsule, cone, arc sector, arc volume, XZ grid, text, filled triangle) and each shape's cost: the `kMin*Segments`, the part counts and the `*Lines` functions | CORE-3 |
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
- Everything of the later Core packages: the worker pool, metrics, debug streams and per-tick arenas (CORE-2), the
  RenderData kinds of CORE-4 to CORE-7 and cached debug meshes (slice 8), process configuration (CORE-8).

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
- `RenderFrameExchange`: one writer thread (`WriteFrame`, `Publish`) and one reader thread (`ReadNewest`), which may
  be the same thread; each call is one lock-free atomic exchange, so neither side waits. `PublishedFrames` is
  thread-safe. `DebugGeometry` and `DebugDraw` are not thread-safe: one writer per geometry.
- Nothing in Core blocks, and nothing allocates on a hot path: a log line is formatted into a stack buffer of
  `kMaxLogLineBytes`, the fixed containers store their elements inline, and `DebugGeometry` allocates its arrays once,
  at construction.

## Data ownership

Core keeps three pieces of process-wide state: the log sink with its user pointer, the minimum log level, and the
assert handler. Everything else is a value owned by whoever holds it. A `RenderFrameExchange` owns its three frames;
the writer owns the frame `WriteFrame` returned until it publishes it, and the reader the frame `ReadNewest` returned
until its next call.

## Integration points

- Hosts install `SetLogSink` (APP-1's entry point writes to stdout and stderr), `SetMinimumLogLevel` and
  `SetAssertHandler`.
- Lines from another system (Godot's output, from GDB-1) enter through `WriteLog`.
- The client writes frames and GodotBridge reads them through one `RenderFrameExchange` the program owns (in slice 2
  the RunicClient program writes and GDB-2 reads); GodotBridge uploads `DebugGeometry`'s arrays as they are.
- Every other module uses the types directly.

## Where the code lives

- Templates and `constexpr` functions are defined in their public headers: `Id`, `TickId`, `AuthorityEpoch`,
  `Result`, the world-space operators, `TickRate::Hz`, and the members of `FixedRingBuffer` and `FixedVector`, which the
  implementation writes below each class.
- Everything else is defined in `Source/<Subsystem>/`: `Errors/` (`FailAssert`, `SetAssertHandler`), `Logging/` (the
  sink, the level, `WriteLog`, `Detail::WriteFormattedLog`), `Time/` (`MonotonicNow`, `ToSeconds`, `FromSeconds`,
  `TickRate::TimeOfTick`, `TickRate::TicksIn`), `Math/` (`ToLocal`) and `RenderData/` (`DebugGeometry`'s construction
  and adds, `DebugDraw`'s shapes, `RenderFrameExchange`). `Ids/` and `Memory/` have no folder yet: those subsystems are
  header-only so far. In RenderData, `Color`, `Camera`, `RenderFrame` and the line-count functions are header-only.
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
| CORE-2 | Worker pool, metrics registry, debug-stream registry, per-tick arenas and per-worker buffers | Jobs run on the pool; metrics are recorded per tick domain; a per-tick arena resets without returning memory to the system | Not started (slice 3) |
| CORE-3 | RenderData foundation: the double-buffered RenderFrame, DebugGeometry and the debug-draw API | A frame is written on one thread and read on another with no lock; debug vertices need no conversion before upload; every debug shape becomes lines and triangles | **Contract** (slice 2): the RenderData headers and the tests below; decisions D1–D3, D5 and D6 in RunicEngine#145 |
| CORE-4 to CORE-7 | RenderData kinds: scene, world state, water and fields, effects | See RunicDocs › Modules, work packages | Not started (slices 22, 25a, 42a, 44+) |
| CORE-8 | Process configuration | A setting resolves from each source in precedence order; an unknown key or a bad value is reported with its source | Not started (slice 3) |

CORE-1's acceptance tests (`RunicCoreTests`): `PlainIds.*`, `Result.*`, `Assert.*`, `Log.*`, `LogDefaultSink.*`,
`Time.*`, `WorldSpace.*` and `FixedContainers.*`, plus the `*DeathTest.*` suites that run where asserts are on; the
type-rule checks are `static_assert`s in the test sources, so a violation stops the build.

CORE-3's acceptance tests: `RenderFrameExchange.*` (a frame written on one thread is read whole on another; one
lock-free atomic; no allocation), `DebugGeometry.*` and `Color.*` (the vertex layout Godot uploads; the budget; text
copied in), `DebugDraw.*` (every shape's lines against its line-count function; filled triangles keep their opacity;
the batch; degenerate shapes; no allocation for a frame of every shape) and `Camera.*`.

## Future extensions

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
