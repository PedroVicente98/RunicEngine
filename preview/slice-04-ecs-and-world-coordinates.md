# Slice 4 · ECS and world coordinates

**Runnable result:** entities with 64-bit positions are created and queried.
**Packages:** ECS-1, ECS-2 (Runic::ECS), EVT-1 (Runic::Events), WORLD-1 (Runic::World).
**Status:** done on the preview branch; 73 new tests and 3 new build tests, 287 in the headless build, all passing.

```sh
cmake --preset headless && cmake --build --preset headless && ctest --preset headless
./build/headless/bin/RunicWorldTests --gtest_filter='WorldComponents.*'     # the runnable result
./build/headless/bin/RunicECSTests --gtest_filter='StateBenchmark.*'        # the ECS-2 benchmark table
```

The runnable result is a test, `WorldComponents.EntitiesWith64BitPositionsAreCreatedAndQueried`: 1,000 entities one
centimetre apart, 6,000 km from the origin and 3 km down, are created, found by a cached query, moved one centimetre
by a system, and read back exactly. A 32-bit float could not tell two of them apart there: its step at 6,000 km is
50 cm. Slice 5 puts such entities on the screen.

## What was built

| Package | What | Where |
| --- | --- | --- |
| ECS-1 | The `EntityWorld` on Flecs 4.2 (entities, registered components, cached queries, held-back changes), the opt-in `Runic::ECSNative` target and its build support | `Engine/SimulationCore/ECS` (a new module); `cmake/` (`runic_add_native_target`, the Flecs 4.2.0 pin); `tests/build/fixtures/native_target*` |
| EVT-1 | Typed event buffers, readers, the hub that flushes them, event IDs | `Engine/Foundation/Events` (a new module) |
| ECS-2 | State declarations with five storage categories, the facts of observable states, the storage benchmark | `Engine/SimulationCore/ECS`: `EntityStates.hpp`, `Source/States`, `Tests/StateBenchmark.cpp` |
| WORLD-1 | Cell and sector keys, a world's coordinate space, region-local coordinates, the world registry, the `WorldTransform` and `Region` components | `Engine/SimulationCore/World` (a new module) |

## The order to read it in

1. **`ECS/Public/Entity.hpp`**: an entity is a 64-bit handle, local to its process.
2. **`ECS/Public/EntityWorld.hpp`**, top to bottom. Register a component type under a name
   (`world.RegisterComponent<Position>("engine:test_position")`), create an entity, `Set`, `Get`, `Remove`,
   `Destroy`. `MakeQuery<Position, const Velocity>("name")` gives a cached query; `Each` visits entity by entity,
   `EachChunk` hands out contiguous arrays. No Flecs type appears: the header's templates call a handful of
   non-template functions that take sizes and `void*`.
3. **`ECS/Source/FlecsWorld/EntityWorld.cpp`** (about 330 lines): the same functions on Flecs. Two things to notice.
   Component types are found by an index (`Detail::TypeIndexOf<T>()`), not by name. And **Flecs 4.2 has no deferred
   mode any more**: an operation given the world happens now, an operation given a *stage* is queued until
   `ecs_merge`. So while a query is being walked, or a `DeferScope` lives, the world sends `Create`, `Set`, `Remove`
   and `Destroy` to its stage and merges when the outermost scope ends.
4. **`ECS/ECSNative/ECSNative.hpp`** (20 lines): the only way to Flecs itself, on its own include folder. In
   `cmake/RunicModule.cmake`, `runic_add_native_target` builds it; in `cmake/RunicChecks.cmake` the end of configure
   prints who links it (`RUNIC: native target Runic::ECSNative (flecs, discouraged) is linked by: no module`).
5. **`Events/Public/EventId.hpp`, `EventHub.hpp`, `EventBuffer.hpp`**. `EventBuffer<T>::Emit` appends to a pending
   list; `EventHub::Flush` gives the pending events their IDs and makes them readable; `EventReader<T>::Take`
   returns what that reader has not seen. The cursor logic that does not depend on `T` is in
   `Source/Readers/ReaderCursors.cpp` (`EndFlush`, 40 lines: the heart of it).
6. **`ECS/Public/EntityStates.hpp`**: the table at the top is the design. Then
   `Source/States/EntityStates.cpp`: `WriteHotFlag`, `ChangeContainer` and `WriteTarget` are the three storages.
7. **`World/Public/CellKeys.hpp`, `CoordinateSpace.hpp`, `RegionFrame.hpp`, `WorldRegistry.hpp`,
   `WorldComponents.hpp`**: small headers, in that order.

## One piece of work, traced

A system that moves entities, as slice 5 will register it:

```cpp
const auto moving = world.MakeQuery<World::WorldTransform, const Velocity>("engine:movement");   // once, at setup
moving.Each([&](ECS::Entity entity, World::WorldTransform &transform, const Velocity &velocity) {
    transform.position = transform.position + velocity.linear * tick.dt;     // 64-bit + 32-bit
    if (states.Has(entity, stunned)) { ... }
    if (hitSomething) { hits.Emit(TestHit{...}); world.Destroy(entity); }
});
```

1. `Each` calls `EntityWorld::IterateQuery`, which raises the world's defer depth and walks Flecs' cached query. For
   each table that holds matching entities it gets the entity array and one array per component, and calls back with
   them (`QueryChunk`); the template turns that into the loop above. Writing `transform.position` writes straight
   into Flecs' storage.
2. `states.Has(entity, stunned)` is one Flecs call that reads a bit (a hot flag is a toggled marker).
3. `hits.Emit` appends to the buffer's pending vector. Nobody can read it yet.
4. `world.Destroy(entity)` is held back: the entity may not leave its table while the table is being walked. It goes
   to the stage.
5. When `Each` returns, the defer depth is back to 0 and the stage is merged: the entity is destroyed.
6. At the phase's barrier the simulation calls `hub.Flush()` (SIM-2, slice 5): the hit gets the ID
   `{simulation 1, sequence n}` and every reader of `hits` will get it on its next `Take`, the 30 Hz consumer this
   tick and the 10 Hz consumer when it next runs.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `EntityWorld.*`, `EntityWorldDeathTest.*` | 19 | **Entities are created, queried and destroyed through Runic types**: a stale handle never comes back to life over 100 reuses; components set, read, removed; a component that owns memory (strings in a vector) is moved between storages 150 times and every instance is destroyed (a live-instance counter ends at 0); a query crosses storages and sees entities made after it; changes inside an iteration apply at its end; 100,000 entities |
| `ECSNative.*` | 2 | **Flecs types appear only in Runic::ECSNative**: the door reaches the same world; with `checks.public_headers`, which fails the build if a `Public/` header includes or names Flecs |
| `build.native_target`, `_public`, `_not_allowed` | 3 | **The configure check lists every module that links it**: a fixture module that links the door is listed; linking it PUBLIC, or from a module whose row does not allow ECS, fails configure |
| `EventBuffer.*`, `EventHub.*` | 12 | **Two consumers read the same fact**, with the same ID; nothing is readable before the flush; each event once, in order; a consumer that reads every third flush gets all six events; read events are let go and 10,000 ticks do not grow the buffer; a full buffer drops its oldest and tells the reader how many |
| `EventId.*`, `EventSequence.*` | 4 | **IDs stay stable on replay**: a scripted run of 23 events gives the same IDs when run again from nothing, and when the second half is replayed from a checkpoint's sequence; another simulation gives other IDs |
| `EntityStates.*`, `EntityStatesDeathTest.*` | 15 | **A state of each category is set, read and cleared; hot and content flags never move the entity** (its archetype is the same over ten rounds of flips, and a few-target relation does change it); **observable states emit StateEntered and StateExited** (ten facts, in order, each once); held-back changes; an entity spawned and given states in one phase; 5,000 content flags |
| `StateBenchmark.*` | 1 | **The benchmark reports ns per operation and ms per tick for 2,000 entities**; the table is below and in `ECS/ARCHITECTURE.md` |
| `CoordinateSpace.*`, `CellKeys.*` | 6 | **A cell key round-trips at any altitude or depth**: from y = −2³¹ to 2³¹ − 1, and 100,000 random 32-bit addresses, through the corner and through the centre |
| `RegionFrame.*` | 5 | **Conversions round-trip exactly far from the origin**: 400,000 region-local positions go up to the world and back bit for bit, at origins up to 8,378 km out |
| `WorldRegistry.*` | 6 | **A WorldId the registry does not know is rejected**, by each of its six lookups, with `world 99 is not registered` |
| `WorldComponents.*` | 3 | The slice's runnable result |

All of it passes with GCC (Debug and Release), with clang and warnings as errors, and under AddressSanitizer and
UBSan; the Events tests also under ThreadSanitizer.

## What it costs

Release build, the same shared 2-core machine as slice 3 (orders of magnitude):

| Operation | Cost |
| --- | --- |
| Create an entity and set two components | about 340 ns |
| `Get<T>(entity)` | about 16 ns |
| A query's `Each` or `EachChunk`, moving a 64-bit position by a velocity | about 2.5 ns per entity: 2,000 entities in 5 µs, 100,000 in 0.25 ms |
| Destroy an entity | about 50 ns |
| An event: emit, flush, read by two readers | about 4 ns |
| `CellOf(position)` | about 21 ns |
| Region-local to world and back | about 11 ns |

ECS-2's benchmark, 2,000 attached entities, three runs:

| One state set or cleared | ns per operation | | A tick: 8 state changes per entity, then a system walks all 2,000 | ms per tick |
| --- | --- | --- | --- | --- |
| Hot flag (toggle) | 43 to 69 | | Hot flags | 0.84 to 1.28 |
| Content flag (a bit) | 14 to 15 | | Content flags | 0.33 to 0.55 |
| Value (a number) | 15 to 17 | | Plain tags | 2.6 to 4.5, in 256 storages |
| Relation, many targets (DontFragment) | 168 to 179 | | The walk alone, one storage | 0.002 to 0.003 |
| Relation, few targets (moves the entity) | 179 to 201 | | | |
| Plain tag (moves the entity) | 144 to 168 | | | |

What it means for the design in RunicDocs: the categories do what they were chosen for. Sixteen thousand state
changes in a tick cost about a millisecond as toggles and a third of that as container bits, against three to four
milliseconds as plain tags, and the tags leave 2,000 entities spread over 256 storages. A DontFragment relation is
not cheaper to write than a fragmenting one (both about 180 ns); it is chosen for keeping the entity where it is.

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | "System registration through Runic types" is a function registered with the `SimulationRuntime` that runs cached queries; there are no Flecs systems and the world is created with Flecs' core only | One scheduler (SIM-1), and RunicDocs says recurring work uses cached queries | Flecs' pipeline as a second scheduler |
| D2 | The world class is `ECS::EntityWorld` | A module and namespace named `World` exist, and `ECS::World` next to `Runic::World` reads badly and is ambiguous under `using` | `ECS::World` |
| D3 | Held-back changes use Flecs 4.2's stage; `Set` inside an iteration is held back like `Remove` and `Destroy` | Flecs 4.2 removed deferred mode (`ecs_defer_begin` no longer exists); a stage queues everything given to it | None with 4.2; with 4.0 a `Set` of an existing component would have applied at once |
| D4 | `Runic::ECSNative` has no catalog row: any module whose row allows ECS may link it, PRIVATE only, and is listed | RunicDocs calls it opt-in and asks only for the listing | A catalog entry per module that may use it |
| D5 | State facts leave ECS through a `StateFactSink`; Simulation will put them into event buffers (SIM-2) | ECS-2 needs EVT-1, but the catalog lets ECS link Core only, and "a package's Needs never grants a link" | Add `Runic::Events` to ECS's row (an owner decision in RunicDocs) |
| D6 | Entities are attached to their state storage by whoever spawns them; an unattached entity moves once at its first write | "Flags never move the entity" can only hold once the storage is there | Every entity gets all state storage at creation |
| D7 | A `Value` is a 64-bit integer; it reports entering when first given and exiting when cleared, not each new number | RunicDocs names reputation and counters; StateEntered and StateExited are the two facts it defines | A decimal; a fact per change |
| D8 | A relation has one target at a time; when the target is destroyed it ends without a fact | Flecs removes the pair with the target; a fact would need an observer | Many targets; an exit fact |
| D9 | One event sequence per simulation, shared by all its buffers; IDs are given at the flush, buffers in creation order | An ID alone must identify an event; a fixed order is what makes replay give the same IDs | A sequence per buffer plus a buffer ID |
| D10 | A buffer is bounded (16,384 events by default); a reader that falls further behind misses the oldest and is told | A reader that never reads must not grow memory without limit; a durable consumer checks `Missed()` | Unbounded |
| D11 | Cell and sector sizes are whole metres; a world's origin is where its cell grid starts | Every boundary is then an exact 64-bit value, which is what makes the cell round trip exact | Fractional sizes |
| D12 | "Round-trip exactly" means: a region-local coordinate of zero or at least 1 cm, up to the world and back, bit for bit, inside 8,388 km | A float below 7.8 mm has a finer step than a double has at 6,000 km, so "exact for every float" is impossible; 1 cm is RunicDocs' smallest unit | A tolerance instead of exactness |
| D13 | `WorldTransform` (64-bit position, rotation) and `Region` are components of Runic::World | World may link ECS, and Simulation, Physics' bridge and the client reach it through Simulation | In Simulation |

Three notes for other packages (nothing was changed outside the packages' modules):

- **Core:** a module other than Core cannot write GLM's identity rotation in a public header, so World builds
  `kNoRotation` from four numbers. A `Core::kIdentityRotation` would serve everyone.
- **RunicDocs:** the Flecs note says "queries on toggled components" and "deferred" in the 4.0 sense; with 4.2 the
  model is stages. Worth a line when the slice is built for real.
- **cmake/README.md** row 15 showed `external/flecs-4.0.4` as its example of a versioned folder; updated to 4.2.0
  on 2026-11-19, with the cached-folder fix (see the README's "After the check" section).

## What is finished and what is set up for later

- **Finished:** everything the four packages' "Done when" asks.
- **Set up for slice 5:** SIM-2 holds a `DeferScope` over each phase, flushes the `EventHub` at each barrier, and
  implements `StateFactSink` on two event buffers; CLI-1's first extractor reads `WorldTransform` through a query.
- **Set up for slice 6:** the editor's ECS inspector lists entities, components (with their text) and queries from
  `EntityWorld`, and states from `EntityStates`.
- **Set up for slice 10:** `EachChunk` hands out the contiguous runs SIM-3 will give to workers.
- **Set up for slice 11:** WORLD-2's index keys on `CellKey`.
- **Not built, by the rule "Stage A only":** queries that filter by a state, entity names and hierarchy, a lookup
  from `GlobalEntityId` to `Entity`, events emitted from several workers.

## Noticed while writing it

- An immediate structural change during a Flecs iteration does not assert in the C API: it silently visits entities
  twice (tried with a scratch program: 8 entities, 16 visits). That is why the world holds changes back itself.
- Flipping a toggle or adding a DontFragment pair during an iteration is safe and is seen at once (same program).
- In a Debug build Flecs checks itself and the state benchmark is about ten times slower; only Release numbers mean
  anything.
