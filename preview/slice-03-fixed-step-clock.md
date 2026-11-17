# Slice 3 · Fixed-step clock

**Runnable result:** the scheduler runs 60/30/20/10 Hz on the server loop with metrics.
**Packages:** CORE-2, CORE-8 (Runic::Core), SIM-1 (Runic::Simulation, with the RunicServer program).
**Status:** done on the preview branch; 85 new tests and 2 new program tests, 211 in the headless build, all passing.

```sh
cmake --preset headless && cmake --build --preset headless && ctest --preset headless
./build/headless/bin/RunicServer --quit-after 120 --clock-demo --set simulation.workers=2
```

```text
RUNIC: RunicServer started (fixed 60 Hz)
RUNIC: setting simulation.workers = 2 (command line)
RUNIC: setting simulation.max_ticks_per_update = 5 (default)
RUNIC: Simulation layer attached: simulation 1, 2 worker(s), up to 5 step(s) per update
RUNIC: engine:clock_probe ran in hz20 producers (master tick 0, hz20 tick 0, dt 0.0500 s)
RUNIC: engine:clock_probe ran in hz10 producers (master tick 0, hz10 tick 0, dt 0.1000 s)
RUNIC: engine:clock_probe ran in hz30 realtime (master tick 0, hz30 tick 0, dt 0.0333 s)
RUNIC: engine:clock_probe ran in hz20 consumers (master tick 0, hz20 tick 0, dt 0.0500 s)
RUNIC: engine:clock_probe ran in hz10 consumers (master tick 0, hz10 tick 0, dt 0.1000 s)
RUNIC: engine:clock_probe ran in hz60 hook (master tick 0, hz60 tick 0, dt 0.0167 s)
RUNIC: Simulation layer ran 120 master ticks (hz60 120, hz30 60, hz20 40, hz10 20), 0 owed
RUNIC: clock domain hz60: 120 ticks, mean 0.3 us, p99 3.6 us, max 3.6 us
RUNIC: clock domain hz30: 60 ticks, mean 0.6 us, p99 3.8 us, max 3.8 us
RUNIC: clock domain hz20: 40 ticks, mean 1.5 us, p99 25.3 us, max 25.3 us
RUNIC: clock domain hz10: 20 ticks, mean 0.7 us, p99 7.7 us, max 7.7 us
RUNIC: RunicServer stopped cleanly after 120 frames in 2.0 s
```

A bad setting stops the server before its first frame, with every problem and where it came from:

```text
$ ./build/headless/bin/RunicServer --set simulation.workers=0 --set simulation.wokers=2
Error: command line --set simulation.workers=0: simulation.workers: 0 is outside 1 to 64
Error: command line --set simulation.wokers=2: unknown setting 'simulation.wokers'
Error: RunicServer could not start: Configuration failed to attach: 2 problem(s) in the settings, listed above
Error: RunicServer stopped with exit code 1 after 0 frames
```

## What was built

| Package | What | Where |
| --- | --- | --- |
| CORE-2 | The worker pool, per-worker buffers, the per-tick arena, the metrics registry, the debug-stream registry | `Engine/Foundation/Core`: `WorkerPool.hpp`, `PerWorker.hpp`, `TickArena.hpp`, `MetricsRegistry.hpp`, `DebugStreamRegistry.hpp` and `Source/WorkerPool`, `Memory`, `Metrics`, `DebugStreams` |
| CORE-8 | Process configuration | `Engine/Foundation/Core`: `ProcessConfig.hpp`, `Source/Config` |
| SIM-1 | The clock domains, `TickContext`, `DomainTickCount`, the `SimulationRuntime`, the Simulation layer, its settings; RunicServer runs it | `Engine/SimulationCore/Simulation` (a new module), `Engine/Programs/RunicServer` |

## The order to read it in

1. **`Core/Public/WorkerPool.hpp`**, then `Source/WorkerPool/WorkerPool.cpp` (about 200 lines). A pool has N worker
   slots. Slot 0 is the thread that drives the pool (the simulation thread); the pool creates N − 1 threads for the
   others. Work is a `Job` (a function pointer and a `void*`), submitted under a `JobGroup`; `Wait(group)` is the
   barrier, and the waiting thread runs queued jobs itself instead of sleeping. With one worker there is no thread at
   all and jobs run in the order they were submitted, which is how the early slices run. `ParallelFor(count,
   chunkSize, body)` splits a range into chunks whose boundaries depend only on `count` and `chunkSize`, never on the
   worker count: that is what will let slice 10 merge per-chunk output the same way with 1 worker and with 8.
2. **`PerWorker.hpp`** and **`TickArena.hpp`**: the two kinds of memory a parallel phase writes into. `PerWorker<T>` is
   one `T` per slot, 64-byte aligned, so two workers never touch the same cache line. `TickArena` is a bump allocator:
   `Allocate` moves a pointer, `Reset` rewinds it and keeps the blocks.
3. **`MetricsRegistry.hpp`**: counters, gauges and timings. A metric is its name plus a clock-domain label, so
   `simulation.tick_time` exists four times (`hz60`, `hz30`, `hz20`, `hz10`). A timing keeps a count, a total, a
   maximum, the last sample and 20 histogram buckets (16 µs doubling up to 8.4 s), from which `Percentile(0.99)` comes.
   The registry is an object the program owns: the Platform's exporter will walk `Count()` and `Read(i)` (PBND-3).
4. **`DebugStreamRegistry.hpp`**: named streams of byte records for the RunicEditor's live attach (NET-7, slice 39). A
   stream is off until a tool turns it on; each reader keeps its own cursor.
5. **`ProcessConfig.hpp`**, then `Source/Config/ProcessConfig.cpp`. A module declares typed settings
   (`DeclareInt("simulation.workers", 1, {1, 64}, help)`); the program calls `LoadForProcess(arguments)` once. Values
   come from the default, then the file, then the environment, then the command line.
6. **`Simulation/Public/TickDomain.hpp`** and **`TickContext.hpp`**: the four domains and what a system is told about
   its tick.
7. **`SimulationRuntime.hpp`**, then `Source/Scheduler/SimulationRuntime.cpp`: `RunMasterTick` is the whole scheduler,
   about 60 lines.
8. **`SimulationLayer.hpp`**, then `Source/SimulationLayer/SimulationLayer.cpp`: how wall-clock time becomes a number
   of steps.
9. **`Programs/RunicServer/Main.cpp`**: `CreateApplication` wires all of it.

## One master tick, traced

`ApplicationHost` runs RunicServer's fixed 60 Hz loop and calls every layer's `OnUpdate(FrameTime)`. For frame *n* a
fixed loop gives `sinceStart = TimeOfTick(n)`, exact to the nanosecond.

1. `SimulationLayer::OnUpdate` computes how many master ticks are **due**: tick *k* is due at `TimeOfTick(k)` after
   the layer's first update, so the due ticks are `TicksIn(elapsed) + 1`. This is integer arithmetic on nanoseconds;
   no floating-point time is ever accumulated. It runs `due − ticksRun` steps, at most 5 per update.
2. `SimulationRuntime::RunMasterTick` takes the next master tick number *m* and walks the six slots in order:
   Hz20 producers, Hz10 producers, Hz30 realtime, Hz20 consumers, Hz10 consumers, Hz60 hook. A slot runs when its
   domain is due: Hz30 when *m* is a multiple of 2, Hz20 of 3, Hz10 of 6. So master tick 0 runs everything, tick 1
   only the hook, tick 2 realtime and the hook, and the pattern repeats every 6 ticks.
3. For a slot that runs, it builds a `TickContext`: the domain, *m*, the domain tick (*m* / 2, / 3 or / 6), `dt`
   (1/30 s for Hz30) and `time = TimeOfTick(domainTick)` at the domain's rate. That time equals the master tick's
   time exactly, which a test checks for 100,000 ticks.
4. Each system of the slot runs with that context, in the order it was added. With a metrics registry, the clock is
   read after each system: the difference is recorded as `simulation.system_time.<name>` and added to the domain's
   time.
5. After the slots, every due domain's tick counter goes up and its time is recorded (`simulation.tick_time`,
   `simulation.ticks`); the whole step is recorded as `simulation.step_time`, and counted as an overrun if it took
   more than 1/60 s.
6. Back in the layer: what is still owed is the **backlog** (`simulation.backlog_ticks`). If the process stalled for
   a second, 60 ticks are due; this update runs 5, the next ones run 5 each while 1 more comes due, and after 14
   updates the simulation is level again. No tick is ever dropped; a warning says how far behind it is.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `WorkerPool.*`, `PerWorker.*` | 14 | **Jobs run on the pool**: every job once; three jobs that each wait for the other two can only finish on three threads at once; two running jobs never share a slot (4,000 jobs); the barrier holds over 200 rounds; a job may submit and wait; a full queue runs the job inline; `ParallelFor` covers each of 10,007 indices once and its chunks are the same with 1 and 4 workers; no allocation |
| `TickArena.*` | 7 | **A per-tick arena resets without returning memory**: 100 reset-and-refill ticks make 0 allocations and the reserved bytes do not change; storage is aligned and never overlaps; an oversized request gets and keeps its own block |
| `MetricsRegistry.*` | 10 | **Metrics are recorded per tick domain**: one second of a 60 Hz clock gives 60, 30, 20 and 10 samples under the four labels; percentiles; 4 threads × 50,000 records add up; recording allocates nothing; a full registry refuses and says so once |
| `DebugStreamRegistry.*` | 9 | A disabled stream keeps nothing; two readers each get every record; a full ring drops the oldest and tells the reader how many; 4 threads lose no record |
| `ProcessConfig.*`, `ProcessConfigFile.*`, `ProcessConfigDeathTest.*` | 17 | **A setting resolves from each source in precedence order; an unknown key or a bad value is reported with its source**: the exact report lines for the file (`server.cfg:2: unknown setting …`), the environment and the command line, all in one failure |
| `TickDomain.*`, `DomainTickCount.*` | 8 | The rates; a domain tick has its master tick's time; `Covering` never rounds a duration down; a bare number does not compile into a tick count |
| `SimulationRuntime.*` | 10 | **Domain tick counts stay exact over a long run, with no drift**: ten simulated hours (2,160,007 master ticks) give exactly 1,080,004, 720,003 and 360,002 domain ticks, each at its exact time; the slot order over a full six-tick cycle; overruns are counted and the step still runs |
| `SimulationLayer.*`, `SimulationSettings.*` | 10 | One step per update on a fixed loop; 30 Hz, 120 Hz and uneven loops run exactly the due ticks (two simulated hours of random frame times, checked after every update); a stall is caught up with every tick run |
| CTest `programs.runic_server_clock`, `programs.runic_server_bad_setting` | 2 | The runnable result above, on the real server loop |

All of it passes with GCC (Debug and Release), with clang and warnings as errors, and under ThreadSanitizer,
AddressSanitizer and UBSan.

## What it costs

Measured in a Release build on the 2-core machine this was built on, while another build used the second core, so
treat them as orders of magnitude:

| Operation | Cost |
| --- | --- |
| One master step with no systems, without metrics / with metrics | about 20 ns / 170 ns |
| One master step with a trivial system in each of the six slots, with metrics | about 370 ns, so 60 steps a second cost about 22 µs: 0.002% of one core |
| A counter increment / a timing record / reading the clock | about 8 ns / 19 ns / 27 ns |
| Submitting and running one empty job, 1 worker / 2 workers | about 40 ns / 190 ns |
| `ParallelFor` over 16 chunks with an empty body, 1 worker / 2 workers | about 0.2 µs / 0.8 µs |
| An arena allocation | about 3 ns |
| Publishing to a debug stream that is off / on (31 bytes) | about 2 ns / 48 ns |

The scheduler itself is free next to the work it will schedule: a 30 Hz tick has 33.3 ms, and the bookkeeping of a
step is under a microsecond. The pool's queue is one mutex; at a few hundred jobs per tick that is tens of
microseconds, and it only matters if a profile of slice 38 (battle scale) says so.

## Decisions that await the owner

The pipeline would have stopped for each of these in a contract session.

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | The configuration file is flat `key = value` lines with `#` comment lines | The Engine may not add a parser library to Core, and settings are flat (worker count, ports, paths, flags) | TOML or JSON, with a third-party parser the catalog would have to allow |
| D2 | Environment variables are `RUNIC_CFG_<KEY>`, and a variable with that prefix that names no setting is an error | A typo in a pod's environment must not become a silent default; the plain `RUNIC_` prefix would collide with build variables such as `RUNIC_ENGINE_DIR` in a developer's shell | `RUNIC_<KEY>` and ignoring unknown variables |
| D3 | The command line sets values with `--set key=value` and names the file with `--config <path>` | Unknown keys can be told apart from the program's other flags (`--quit-after`) | `--simulation.workers=4`, which cannot report an unknown key |
| D4 | A key set twice in the file is an error; on the command line the last `--set` wins | A repeated line in a file is a mistake; a repeated flag is how a script overrides | Last wins everywhere |
| D5 | `TickContext::dt` is a 32-bit float; the exact time is `TickContext::time` | Gameplay math is 32-bit (GLM, Jolt's step), and `-Werror=float-conversion` would force a cast at every use of a double | A double `dt` |
| D6 | Among producers and among consumers, the 20 Hz slot runs before the 10 Hz slot | RunicDocs fixes "20/10 Hz producers, then realtime, then 20/10 Hz consumers" but not the order inside each pair | 10 Hz first |
| D7 | The Simulation layer owns its runtime, and at most 5 steps run per update (`simulation.max_ticks_per_update`, 1 to 60) | One owner, one lifetime; 5 steps catch up a one-second stall in a quarter of a second without freezing a frame | The program owns the runtime; another limit |
| D8 | The metrics registry and the debug-stream registry are objects the program owns, not globals | Two simulations in one process ("quiet neighbors") and tests need separate ones | Process-wide singletons |
| D9 | A timing keeps 20 fixed buckets (16 µs × 2ⁿ) for percentiles | RunicDocs asks for p50, p95 and p99 per domain and system; fixed buckets cost 19 ns per record and no memory growth | Exact percentiles from stored samples |

One interface note for APP-1 (not changed here): `CreateApplication` cannot report a failure, so RunicServer stops on
a bad configuration by pushing a layer that fails to attach, which gives exit code 1 through the existing path. A
`Result` from `CreateApplication` would say it directly.

## What is finished and what is set up for later

- **Finished:** everything the three packages' "Done when" asks, with the tests above.
- **Set up for slice 4:** `SimulationSpec::id` (events get stable IDs from the `SimulationId` and a sequence, EVT-1).
- **Set up for slice 5 (SIM-2):** the realtime slot is one slot today; SIM-2 divides it into phases with a barrier
  after each, where commands apply and events flush.
- **Set up for slice 10 (SIM-3, PHYS-4):** the pool already runs with any worker count, `ParallelFor`'s chunks do not
  depend on it, and `PerWorker` and `TickArena` are the buffers the merge will read in a fixed order. Jolt's job
  system maps onto `Submit` and `Wait` directly.
- **Not built, by the rule "Stage A only":** changing the worker count while running, a lock-free job queue, more
  setting kinds (decimals, durations, lists), `--help`. Each is listed under Future extensions in its module's
  `ARCHITECTURE.md`.

## Noticed while writing it

- `tests/godot/RunGodotCheck.cmake` accepts only exit code 0, so the bad-setting CTest uses CTest's own
  `PASS_REGULAR_EXPRESSION` over the whole output (which includes "stopped with exit code 1"). An `-DEXIT=<code>`
  option in that script would be cleaner; it is shared build plumbing, so it was left alone.
- The pinned third-party archives come from `codeload.github.com`. Where that host is not reachable, the same files can
  be produced from a tag clone with `git archive --format=tar --prefix=<name>-<version>/ <tag> | gzip -n`, which gives
  the pinned SHA-256 for GLM 1.0.1, GoogleTest 1.17.0, Flecs 4.0.4 and Jolt 5.6.0.
