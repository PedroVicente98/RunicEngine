# Runic::ECS — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::ECS` (`runic_ecs`), and the opt-in `Runic::ECSNative` ·
Packages: ECS-1, ECS-2

Source: RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › The Runic API over Flecs and Jolt, State
declarations, Flecs rules; Repositories, build targets and processes (Engine modules); Modules, work packages and the
implementation contract (ECS-1, ECS-2). Status: **ECS-1 implemented** (slice 4, on the preview branch
`preview/slices-3-11`): the `EntityWorld`, component and query registration through Runic types, and the opt-in
`Runic::ECSNative` target, with their acceptance tests. **ECS-2 implemented** (slice 4, same branch): state
declarations with their five storage categories, the facts of observable states and the storage benchmark. The folder
map is [README.md](README.md); slice 4's brief and walkthrough are
[preview/slice-04-ecs-and-world-coordinates.md](../../../preview/slice-04-ecs-and-world-coordinates.md).

## Purpose

The Engine stores a simulation's entities and their data in Flecs, and every module reaches them through the Runic
API of this module, never through Flecs itself. That keeps the Engine's conventions in one place (namespaced,
registered components; cached queries; changes held back while entities are walked) and leaves the storage free to
change. ECS-1 builds the `EntityWorld`: entities, registered components, cached queries, and the one opt-in door to
native Flecs. ECS-2 adds state declarations on top of it: a state of an entity is declared once, and its category
picks how it is stored, so flipping the states that change all the time never moves an entity out of its storage.

## Public API

Namespace `Runic::ECS`. Headers are included by their path under `Public/` (`#include "EntityWorld.hpp"`). No Flecs
type appears in them, and the public-header scan fails the build if one does.

| Header | Holds | Since |
| --- | --- | --- |
| `Entity.hpp` | `Entity`: a local entity handle (`IsNull`, `Raw`, `FromRaw`, comparisons, `std::hash`) | ECS-1 |
| `ComponentInfo.hpp` | `ComponentOptions` (description, `ComponentFormatter` for tools), `ComponentInfo`; `Detail::ComponentLifecycle`, `Detail::TypeIndexOf` | ECS-1 |
| `EntityQuery.hpp` | `EntityQuery<Ts...>` (`Each`, `EachChunk`, `Count`, `IsValid`), `QueryInfo`, `kMaxQueryTerms` (8); `Detail::QueryChunk` | ECS-1 |
| `EntityWorld.hpp` | `EntityWorld` (`RegisterComponent`, `IsRegistered`, `ComponentCount`, `DescribeComponent`; `Create`, `Destroy`, `IsAlive`, `EntityCount`; `Set`, `Get`, `TryGet`, `Has`, `Remove`; `MakeQuery`, `QueryCount`, `DescribeQuery`; `DeferScope`, `IsDeferred`; `ForEachEntity`, `ForEachComponentOf`, `ArchetypeOf`), `ArchetypeId` | ECS-1 |

| `EntityStates.hpp` | `StateCategory` (`HotFlag`, `Relation`, `ContentFlag`, `Value`, `Derived`), `StateVisibility`, `StateLifetime`, `TargetCardinality`, `DerivedCondition`, `StateDeclaration`, `StateId`, `StateInfo`; the facts `StateEntered` and `StateExited` and the `StateFactSink` they are reported to; `EntityStates` (`Declare`, `Find`, `Count`, `Describe`, `SetFactSink`, `Attach`, `Has`, `TargetOf`, `ValueOf`, `Set`, `SetTarget`, `SetValue`, `Clear`) | ECS-2 |

The opt-in target `Runic::ECSNative` has its own include root, [`ECSNative/`](ECSNative/README.md), outside `Public/`:

| Header | Holds | Since |
| --- | --- | --- |
| `ECSNative.hpp` | `Native::WorldOf` (the `ecs_world_t*` of an `EntityWorld`), `Native::ToNative`, `Native::FromNative`, `Native::ComponentOf<T>`; includes `flecs.h` | ECS-1 |

**The conventions** (RunicDocs › Flecs rules), which the API enforces where it can:

- A component is plain data: default-constructible, copy- and move-assignable (checked at compile time). It is
  registered once per world under a name `namespace:name` in lowercase letters, digits and underscores
  (`engine:test_position`, `game:…`; asserted). Hot state is dense and owned by its entity.
- Recurring work uses a cached query made once at setup (`MakeQuery`), never a search per tick. A `const` component
  type in a query is read, any other may be written.
- While entities are walked (`Each`, `EachChunk`, `ForEachEntity`) or a `DeferScope` lives, `Set`, `Remove`, `Destroy`
  and the bookkeeping of `Create` are held back and apply, in order, when the outermost one ends. Reads see the world
  as it was; a value written through `Get` or a query's reference changes at once.
- Only the simulation's own phases change a world, and one thread uses a world at a time.
- An `Entity` is local to its process. It never goes into a message, a file or a service call: across processes an
  entity is a `Core::GlobalEntityId`.
- Module code does not link `Runic::ECSNative`. A module that must links it PRIVATE, and the end of configure lists
  it: `RUNIC: native target Runic::ECSNative (flecs, discouraged) is linked by: <modules>`.

**State declarations** (ECS-2; RunicDocs › State declarations). One declaration per state, with its category, its
owner (the one system that writes it), whether it is observable, its visibility and its lifetime:

| Category | Example | Storage | A change |
| --- | --- | --- | --- |
| `HotFlag` | Stunned, Mounted, InCombat | A marker with the toggle trait; set means enabled | One bit; the entity stays in its table |
| `Relation`, many targets | MountedOn(horse) | An exclusive relationship kept beside the table (DontFragment) | The entity stays in its table |
| `Relation`, few targets | a team, a faction | An exclusive relationship; entities with one target are stored together | Moves the entity, like adding a component |
| `ContentFlag` | MetTheHarbormaster | One bit in the entity's state container, a component | In place |
| `Value` | a reputation, a counter | One 64-bit number in the same container | In place |
| `Derived` | IsNight, IsHealthLow | Nothing: a condition computed when asked | Cannot be written |

- `Attach(entity)` gives an entity the storage of every hot flag declared so far and its container, in one move;
  whoever spawns an entity attaches it. An entity that was not attached gets a state's storage at that state's first
  write, which moves it once.
- A change that does not move the entity applies at once, also inside a query's iteration. A change that moves it (a
  relation with few targets; a first write on an entity that was not attached) follows the world's rule for held-back
  changes, and until it applies reads show the state as it was.
- An observable state reports `StateEntered` and `StateExited` to the `StateFactSink` when the change is asked for,
  once (asking twice reports once, also while the change is held back). A relation that changes target exits the old
  one, then enters the new one. A value reports entering when it is first given and exiting when it is cleared, not
  each new number. A `Derived` state cannot be observable.
- When a relation's target is destroyed the relation ends, without a fact.
- Readers ask by `StateId` or by name (`Find`) and never depend on the storage.

## Owns

- The Flecs world of a simulation and everything in it: entities, component storage, queries.
- The registry of components and queries of a world, which tools read (`DescribeComponent`, `DescribeQuery`,
  `ForEachEntity`, `ForEachComponentOf`).
- The rule for changes during iteration, and the mapping of that rule onto Flecs.
- `Runic::ECSNative`, the only way to Flecs from outside this module.
- The states of a world's entities: the declarations, which storage each category uses, the entity's state
  container, and when a state's fact is reported.

## Does not own

- Which components exist: each module registers its own (the world's transform is Runic::World's, WORLD-1).
- When systems run: the scheduler is Runic::Simulation's (SIM-1); a system is a function that runs cached queries,
  registered with the `SimulationRuntime`. Flecs' own pipeline, systems and timers are never imported.
- Events: Runic::Events (EVT-1). Global IDs and their mapping to entities: Core's ID types and, later, Network.
- Which states exist: each module declares its own (the Game its own, in the `game:` namespace).
- Carrying the state facts: the catalog lets this module link Core only, so the facts leave through `StateFactSink`
  and Runic::Simulation, which may link both, emits them into Runic::Events buffers (SIM-2).
- Using a state's visibility and lifetime: replication (Network) and persistence read them later; here they are
  recorded.

## Allowed dependencies

Catalog row: Core. Third party: Flecs 4.2.0 (PRIVATE; `cmake/Dependencies.cmake`, pinned by SHA-256).

## Forbidden dependencies

Every other Runic module; Godot, audio and editor code (the module is headless); RunicPlatform and RunicGame. Flecs
types in `Public/`.

## Threading and tick model

- An `EntityWorld` is used by one thread at a time: the thread that drives its simulation. Nothing in it locks.
- No I/O callback touches a world (CLAUDE.md: "Never mutate Flecs from an I/O callback").
- Registration (`RegisterComponent`, `MakeQuery`) happens at setup and allocates. `Create`, `Set` of a new component,
  `Remove` and `Destroy` may allocate inside Flecs when storage grows; reading and writing values in place and
  iterating a query do not.
- Parallel iteration of a query's chunks on the worker pool arrives with SIM-3 (slice 10); `EachChunk` already hands
  out the contiguous runs it will split.

## Data ownership

An `EntityWorld` owns its Flecs world, its component records and its queries; an `EntityQuery` is a handle that is
valid while the world lives. Component values belong to their entity and are destroyed with it or with the world. A
pointer or reference to a component is valid until the next change that moves entities.

## Integration points

- A simulation owns one `EntityWorld`. Modules register their components and make their queries in it at setup, and
  their systems (functions registered with the `SimulationRuntime`) run those queries.
- Tools (the RunicEditor's ECS inspector, GDT-2) list entities, components and queries from the world's registry, and
  show values through each component's `ComponentFormatter`.
- `Runic::ECSNative` for the rare module that needs Flecs itself.

## Where the code lives

- `Source/FlecsWorld/EntityWorld.cpp`: the world on Flecs. The world is created with the core only (`ecs_mini`).
  Every entity made through `Create` carries one tag, which is what `EntityCount` and `ForEachEntity` read; an entity
  made through the native door does not have it.
- `Source/Registration/`: the type indices and the naming convention (`ComponentTypes.cpp`, `ComponentNames.hpp`).
- Component types are found without a lookup by name: `Detail::TypeIndexOf<T>()` gives each type one index for the
  whole process, and each world keeps the Flecs id of its registered types in an array by that index. The Engine's
  modules are static libraries of one binary, so there is one index per type.
- A component that is not trivial (it owns memory) is registered with four lifecycle functions built from the type
  (construct, destroy, copy-assign, move-assign); Flecs composes the rest from them.
- **Flecs 4.2 has no deferred mode.** An operation given the world happens at once; an operation given a stage is
  queued until the stage is merged. While changes are held back, the world sends `Create`, `Set`, `Remove` and
  `Destroy` to its main stage and merges it when the outermost scope ends.
- `Source/States/EntityStates.cpp`: the declarations and each category's storage; `Source/States/StateContainer.hpp`:
  the compact container (a bit array of content flags, a bit array of given values and the values), which grows to
  the highest slot its entity uses. `Source/FlecsWorld/WorldAccess.hpp` is how this part of the module reaches the
  Flecs world behind an `EntityWorld`.
- A change that had to be held back is also remembered per entity (`HeldBack`) until the world applies its changes,
  so a second request for it is recognised and its fact is reported once; the world counts how often it has applied
  held-back changes, and the memory is dropped when that count moves.
- A query is a Flecs cached query with one term per component (`EcsIn` for a `const` type, `EcsInOut` otherwise).
  `EachChunk` visits one run per table (or per run of enabled rows); `Each` is a loop over it.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| ECS-1 | Flecs world wrapper on Flecs 4.2; component, system and query registration through Runic types; conventions; the opt-in Runic::ECSNative target | Entities are created, queried and destroyed through Runic types; Flecs types appear only in Runic::ECSNative, and the configure check lists every module that links it | **Implemented** (slice 4, preview branch): `RunicECSTests` passes with the 21 tests below (17 where asserts are compiled out); the build tests `build.native_target*` pass; awaiting the owner's review |
| ECS-2 | State declarations by category, owner, observable, visibility and lifetime; the benchmark harness | A state of each category is set, read and cleared through the API, and hot and content flags never move the entity to another table; observable states emit StateEntered and StateExited facts; the benchmark reports nanoseconds per operation and milliseconds per tick for 2,000 entities, recorded in the package docs | **Implemented** (slice 4, preview branch): `RunicECSTests` passes with the 16 ECS-2 tests below (14 where asserts are compiled out); the benchmark's numbers are recorded below; awaiting the owner's review |

ECS-1's acceptance tests: `EntityWorld.*` (entities created and destroyed, a stale handle never alive again;
components set, read, removed; a component that owns memory built, moved and destroyed properly; queries across
storages, made before their entities exist; contiguous chunks; changes inside an iteration and inside a `DeferScope`
applied at its end; two independent worlds; the tools' listings; the archetype only changes with the set of
components; 100,000 entities), `EntityWorldDeathTest.*` (an unregistered type, a name that is not namespaced, two
types under one name, a dead entity) and `ECSNative.*` (the door reaches the same world; an entity made through it is
not one of the world's). The build's side: `build.native_target`, `build.native_target_public` and
`build.native_target_not_allowed` (cmake/README.md), and `checks.public_headers`, which keeps Flecs out of `Public/`.

ECS-2's acceptance tests: `EntityStates.*` (a state of each category set, read and cleared; hot flags, content flags,
values and a relation with many targets never change the entity's archetype over ten rounds, and a relation with few
targets does; an entity that was not attached moves once; the facts of observable states, in order, each once; a
state that is not observable; a relation ends with its target; a derived state; changes at once inside an iteration;
changes held back to the end of a scope, with reads and facts; an entity spawned and given its states in one phase;
`Attach` with 20 more hot flags; 5,000 content flags; the listing), `EntityStatesDeathTest.*` and `StateBenchmark.*`.

**The benchmark** (`Tests/StateBenchmark.cpp`, the test `StateBenchmark.ReportsTheCostOfEachStorageFor2000Entities`,
which prints its table). 2,000 attached entities; a Release build with GCC 13 at `x86-64-v3` on a shared 2-core
machine, three runs:

| One state set or cleared | Nanoseconds per operation |
| --- | --- |
| Hot flag (toggle) | 43 to 69 |
| Content flag (a bit in the container) | 14 to 15 |
| Value (a number in the container) | 15 to 17 |
| Relation with many targets (DontFragment) | 168 to 179 |
| Relation with few targets (moves the entity) | 179 to 201 |
| Plain tag, through the native door (moves the entity) | 144 to 168 |

| A tick: every entity changes 8 states, then one system walks all 2,000 | Milliseconds per tick |
| --- | --- |
| Hot flags | 0.84 to 1.28 |
| Content flags | 0.33 to 0.55 |
| Plain tags | 2.6 to 4.5, and the entities end up in 256 storages |
| The walk alone, entities in one storage | 0.002 to 0.003 |

What the numbers say: a content flag or a value costs about 15 ns; a hot flag about 55 ns, which is three calls into
Flecs; a relation costs the same whether it fragments or not, so DontFragment is chosen for what it keeps (one
storage to walk), not for the write. With plain tags the same eight states scatter 2,000 entities over 256 storages
and the tick costs three to four times as much. In a Debug build every figure is about ten times larger, because
Flecs checks itself.

Decisions taken in ECS-2 that await the owner (also in the slice brief): the facts leave through `StateFactSink`
because the catalog does not let ECS link Events; a `Value` is a 64-bit integer and reports entering and exiting,
not each change; a relation has one target at a time; a relation whose target is destroyed ends without a fact;
entities are attached explicitly by whoever spawns them; no query filters by a state yet (a system asks `Has` per
entity).

Decisions taken in ECS-1 that await the owner (also in the slice brief): "system registration" is a function
registered with the `SimulationRuntime` that runs cached queries, not a Flecs system; the world class is named
`EntityWorld` (a module named World exists); `Set` inside an iteration is held back like every other change;
`Runic::ECSNative` needs no catalog row.

## Future extensions

- Queries that filter by a hot flag or a relation, so a system walks only the entities in a state.
- An exit fact when a relation's target is destroyed.
- Other value types (a decimal, a small set), and a fact for a value that changes, if a consumer needs them.
- Splitting a query's chunks across the worker pool, with per-worker buffers for what a chunk wants to change
  elsewhere (SIM-3, slice 10).
- Entity names and a parent relation, when an authored hierarchy needs them.
- Routing the library's own log and abort messages into Core's log.
- A lookup from `Core::GlobalEntityId` to `Entity` (Network, slice 13).
