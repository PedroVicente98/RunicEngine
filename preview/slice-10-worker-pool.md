# Slice 10 · Worker pool

**Runnable result:** the same scene with several workers gives identical results.
**Packages:** PHYS-4 (Runic::Physics) and SIM-3 (Runic::Simulation), and the RunicServer demo that shows them. One
follow-up rides along as its own commit: PHYS-4's files in the repository's format.
**Status:** done on the preview branch. New tests: 5 in `RunicPhysicsTests` and 7 in `RunicSimulationTests`, plus the
CTest `programs.runic_server_parallel`. The Engine's debug build runs 510 tests and the headless build 399, all
passing. (Correction to slice 9's message: the headless build had 386 tests then, not 366.)

```sh
cmake --preset headless && cmake --build --preset headless && ctest --preset headless
ctest --preset headless -R "PhysicsJobSystem|Parallel|runic_server_parallel"
# By hand: the same scene on one worker and on four
./build/headless/bin/RunicServer --quit-after 150 --parallel-demo --set simulation.workers=1
./build/headless/bin/RunicServer --quit-after 150 --parallel-demo --set simulation.workers=4
```

Both runs print, among their other lines, the same hash:

```text
RUNIC: Simulation layer attached: simulation 1, 1 worker(s), up to 5 step(s) per update
RUNIC: parallel demo at realtime tick 60 on 1 worker(s): state 5d057ab9ec7bde8c; 2159 movers, 23342 hits, 1029 died, 188 spawned, 100 crates and 6 walkers in physics
RUNIC: Simulation layer attached: simulation 1, 4 worker(s), up to 5 step(s) per update
RUNIC: parallel demo at realtime tick 60 on 4 worker(s): state 5d057ab9ec7bde8c; 2159 movers, 23342 hits, 1029 died, 188 spawned, 100 crates and 6 walkers in physics
```

The hash covers every mover's entity handle, transform, velocity, health and hit count, every physics entity's
transform and every hit fact with its event ID. One bit of difference anywhere changes it. (The value itself depends
on the compiler and its flags. What the slice proves is that the worker count does not change it.)

## What was built

| Package | What | Where |
| --- | --- | --- |
| PHYS-4 | Jolt's job system on the Core worker pool. `PhysicsWorldSpec::workers` names the pool. Jolt's jobs sit in a fixed free list, and each job that becomes ready goes to the pool as a Core job under the world's group. `Step` waits for the group before it returns. Without a pool, or with one worker, nothing is queued and Jolt's barrier runs every job on the stepping thread. Contact facts are sorted at the end of each step, so they come out the same on any number of workers | `Engine/SimulationCore/Physics`: `Source/JobSystem/PoolJobSystem.hpp`/`.cpp`, `Public/PhysicsWorld.hpp`, `Source/PhysicsWorld/` |
| SIM-3 | `ParallelEach` splits a system's per-entity work into chunks across the runtime's pool. The chunks depend only on the entity count and the chunk size. `ParallelOutput<T>` gives each worker its own buffer and merges them by loop, then chunk, then push order. `EnablePhysics` puts the physics world on the runtime's pool | `Engine/SimulationCore/Simulation`: `Public/ParallelWork.hpp`, `Source/Workers/ParallelWork.cpp`, `Source/PhysicsBridge/PhysicsBridge.cpp` |
| (program) | `RunicServer --parallel-demo` runs 3,000 movers and a physics pile, then reports a hash at realtime tick 60. The CTest runs it on 1, 2 and 4 workers and compares the hashes | `Engine/Programs/RunicServer/ParallelDemo.cpp`, `SameParallelResult.cmake`, `Main.cpp`, `CMakeLists.txt` |

## The order to read it in

1. **`Simulation/Public/ParallelWork.hpp`**: read the opening comment first, which states the whole rule. Then read
   `ParallelEach`, about 40 lines: gather the runs, `ParallelFor`, and find each chunk's run. Then
   `ParallelOutput::Merge`: list the chunks, sort them, apply.
2. **`Programs/RunicServer/ParallelDemo.cpp`**'s `Move` shows what a system does with these. Each chunk writes only
   its own entity and pushes everything else; the merges then change the world on the driving thread.
3. **`Physics/Source/JobSystem/PoolJobSystem.cpp`**: the adapter, about 70 lines. `CreateJob`, `QueueJob` and `Run`
   are the whole bridge between Jolt and the pool.
4. **`Physics/Source/PhysicsWorld/PhysicsWorld.cpp`**: in `ContactBuffer::Settle`, two `sort`s make the contacts
   deterministic. `PhysicsWorld::Step` brackets the step with `BeginStep` and `Drain`.

## One realtime tick on four workers, traced

1. **Movement.** The demo's system calls `ParallelEach` over the movers (2,159 by tick 60) with 256 per chunk, which makes
   9 chunks.
   The loop opens a `DeferScope` and takes loop number *n*. It gathers the query's runs of entities: one run, since
   every mover has the same components. It then calls `ParallelFor(2159, 256)`.
   - The driving thread and three pool threads take chunks as they come. A worker might run chunks 0, 4 and 7. Each
     chunk moves its own movers.
   - Each hit, spawn or death goes into that worker's slot of a `ParallelOutput`, tagged (n, chunk).
   - `ParallelFor` returns when all 9 chunks have run. That return is the barrier.
2. **The merges, on the driving thread.** `hits.Merge` lists each worker's records chunk by chunk, sorts that list by
   (n, chunk) and applies the hits in that order. Applying a hit means health × 0.97 − amount, then emitting a fact.
   So the target's health and the event IDs come out the same as they would on one worker.
   - The spawns ask for `Create` and `Set`, and the deaths ask for `Destroy`. Inside the phase these wait for its
     barrier, in the order they were asked.
   - The walkers' moves come from a second loop, number *n + 1*.
3. **PhysicsStructure, PhysicsStep.** The bridge steps the world. `PhysicsSystem::Update` asks `PoolJobSystem` for
   its step's jobs (61 a step for 1,000 bodies on four workers), and each one goes to the runtime's pool as soon as
   it is ready.
   - The driving thread waits in Jolt's barrier and runs jobs there too. A job runs once, on whichever thread gets
     to it first.
   - Contact callbacks come from every thread and only record, under a lock.
   - After the update, `Drain` waits for the pool's last Core jobs. These are wrappers whose Jolt job a barrier
     already ran. Then `Settle` decides the lost pairs in pair order and sorts the step's facts by (a, b, phase).
4. **PostPhysics → Queries.** The bridge writes the poses back and emits the contact facts. The Queries phase reads
   the hits and contacts, the same facts with the same IDs on any number of workers.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `PhysicsJobSystem.*` (PHYS-4) | 5 | **A physics step on the pool matches a single-thread step.** A 144-body pile of boxes and spheres falls on an 800-triangle bumpy mesh 3,600 km out, swept by a kinematic paddle, with four characters walking through. Every body, every character and every contact fact is the same to the bit at each of 60 steps: without a pool, and with 1, 2, 4 and 8 workers. Three runs on four workers agree, and a step's facts are in order. **No thread is created outside the pool**: the process has as many threads afterwards as before, and the pool ran the step's jobs. A one-worker pool queues nothing |
| `ParallelEach.*`, `ParallelOutput.*`, `ParallelPhases.*`, `PhysicsBridge.ThePhysicsStepRunsOnTheRuntimesPool`, `ParallelWorkDeathTest.*` (SIM-3) | 7 | **The same scene with one worker and with many gives identical results.** The scene: 3,000 movers that bounce, hit each other in an order-sensitive way, die and spawn, plus 100 crates and 6 characters in physics, 4,500 km out, for 60 realtime ticks. Every entity, every fact with its ID, and every count is the same on 1, 2, 4 and 8 workers. Also tested: chunks depend only on the count, including when they cross runs stored apart; the merge order; physics runs on the runtime's pool; nested loops, merges during a loop and a foreign pool are faults |
| `programs.runic_server_parallel` | 1 | The runnable result, in a program: one scene hash on 1, 2 and 4 workers |

Both test suites pass under ThreadSanitizer (the Jolt jobs and the chunks really run on 2 to 8 threads there),
AddressSanitizer with UBSan, clang and Release. Every new source file compiles cleanly with GCC under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion`.

Each determinism test was also checked against a deliberately broken version:
- Without the end-of-step sort, the physics runs differ from step 16 on 2, 4 and 8 workers.
- Merging in worker order instead of chunk order makes the scene differ on 2, 4 and 8 workers.

## What it costs

Release, on the 2-vCPU build machine. Four workers on two vCPUs is oversubscribed, so its numbers show overhead, not
gain.

| Operation | Cost |
| --- | --- |
| A physics step, 1,000 active boxes and spheres | no pool 7.2–7.6 ms; 2 workers 5.8–6.8 ms (43 Core jobs per step); 4 workers 6.2–7.5 ms (61 jobs). Jolt's own thread pool in the same place: 5.9–9.4 ms on 2 threads. The adapter costs nothing measurable; Jolt itself gains ~1.2× here |
| A physics step, 4,000 active bodies | no pool 42 ms; 2 workers 38 ms |
| `ParallelEach`, 100,000 entities, ~480 ns of work each | `Each` 47–50 ms; 1 worker 47–48 ms; **2 workers 24–26 ms (1.9–2.0×)**; 4 workers 25 ms |
| `ParallelEach`, 100,000 entities, two additions each (memory-bound) | `Each` 0.25–0.30 ms; `ParallelEach` 0.23–0.30 ms on any worker count |
| An empty `ParallelEach` (gather plus 391 chunks) | 3 µs on 1 worker, 15–28 µs on 2 to 4 |
| `ParallelOutput` push plus merge, per record | 3.7–4.4 ns on 1 worker, 5.4–6.2 ns on 2, 7.7–7.9 ns on 4 (the first fill grows the buffers once: 0.7–2.2 ms for 100,000) |

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | One job system per physics world, on the pool the world is given (`PhysicsWorldSpec::workers`); none by default | A world stays usable without a simulation (tools, tests) | A process-wide job system |
| D2 | Jolt's own limits per step: 2,048 jobs and 8 barriers | Jolt's samples and its thread pool use them | Limits from settings |
| D3 | A pool of one worker is the stepping thread: nothing is queued, as Jolt's own pool does with no threads | No overhead in the early slices' configuration | Queue and drain anyway |
| D4 | `Step` waits for every Core job it queued before it returns | Nothing of the world runs between steps | Let wrappers finish later |
| D5 | Contact facts sorted at the end of each step by (a, b, phase); lost pairs decided in pair order | Jolt's callbacks come from any thread; what it finds is deterministic, the order is not | Per-worker contact buffers merged by worker (not deterministic: which worker finds a pair varies) |
| D6 | The parallel unit is a system's loop over a query's entities, not the systems of a phase at once | RunicDocs: "the work inside a phase is split across workers"; running systems at once needs declared reads and writes | A scheduler over declared component access |
| D7 | Chunks are positions in the query's order, 256 entities by default; the system picks the size | Chunks that never depend on the worker count are what makes the merge deterministic | Chunks per archetype table; work stealing by entity |
| D8 | A chunk writes only its own entity's components and pushes everything else into a `ParallelOutput` (a rule the system keeps; not checked at run time) | RunicDocs: "each worker writes only to the entities it owns" | Locks on shared data |
| D9 | The system merges its outputs after its loops, by loop, chunk and push order. The runtime does not merge at the phase barrier | The merge needs the system's meaning (apply a hit, emit a fact); the loop's end is already a barrier | Outputs registered with the runtime and merged at the phase's barrier |
| D10 | One loop at a time, on the driving thread | Keeps the world's rule (one thread uses it) simple | Nested loops |
| D11 | The physics world always runs on the runtime's pool; a spec naming another is a fault | RunicDocs: "Jolt runs on the same pool" | A separate physics pool |
| D12 | The bridge's kinematic sync and write-back stay on the driving thread | Physics' world is used by one thread at a time; at these sizes they cost little | Parallel write-back through a bulk pose read |

## Notes for other packages and for the plan

- **No interface change was needed.** Physics uses Core's `WorkerPool` through its existing Core link. Simulation
  uses ECS's `EachChunk`, Core's `PerWorker` and `ParallelFor`. The chunks touch only the component arrays the query
  handed over, never the world, so ECS's "one thread uses a world at a time" rule holds.
- **PHYS-5 (slice 15) builds on this.** Its "per-worker query queues" are a `ParallelOutput` of queries. Its "results
  identical for any worker count" is the same chunk-order merge.
- **The build machine has two vCPUs**, so the speedups measured here are capped near 2×. Jolt's own gain is smaller
  (~1.2× at 1,000 bodies with its own thread pool too). Worth measuring again on the owner's machine with
  `simulation.workers` at its core count.
- **One full `ctest -j2` run in the debug tree had three failures that did not come back.** They passed on a rerun
  and in three more full runs (510 of 510 each time). The failing names were not kept. The likely suspects are the
  timing-sensitive program tests ("stopped cleanly after 120 frames in 2.0 s") running beside the new multi-threaded
  tests on two vCPUs.
