# Runic::ECS — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::ECS` (`runic_ecs`), and the opt-in `Runic::ECSNative` ·
Packages: ECS-1, ECS-2

Source: RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › The Runic API over Flecs and Jolt, Flecs
rules; Repositories, build targets and processes (Engine modules); Modules, work packages and the implementation
contract (ECS-1). Status: **ECS-1 implemented** (slice 4, on the preview branch `preview/slices-3-11`): the
`EntityWorld`, component and query registration through Runic types, and the opt-in `Runic::ECSNative` target, with
their acceptance tests. The folder map is [README.md](README.md); slice 4's brief and walkthrough are
[preview/slice-04-ecs-and-world-coordinates.md](../../../preview/slice-04-ecs-and-world-coordinates.md).

## Purpose

The Engine stores a simulation's entities and their data in Flecs, and every module reaches them through the Runic
API of this module, never through Flecs itself. That keeps the Engine's conventions in one place (namespaced,
registered components; cached queries; changes held back while entities are walked) and leaves the storage free to
change. ECS-1 builds the `EntityWorld`: entities, registered components, cached queries, and the one opt-in door to
native Flecs. ECS-2 adds state declarations on top of it.

## Public API

Namespace `Runic::ECS`. Headers are included by their path under `Public/` (`#include "EntityWorld.hpp"`). No Flecs
type appears in them, and the public-header scan fails the build if one does.

| Header | Holds | Since |
| --- | --- | --- |
| `Entity.hpp` | `Entity`: a local entity handle (`IsNull`, `Raw`, `FromRaw`, comparisons, `std::hash`) | ECS-1 |
| `ComponentInfo.hpp` | `ComponentOptions` (description, `ComponentFormatter` for tools), `ComponentInfo`; `Detail::ComponentLifecycle`, `Detail::TypeIndexOf` | ECS-1 |
| `EntityQuery.hpp` | `EntityQuery<Ts...>` (`Each`, `EachChunk`, `Count`, `IsValid`), `QueryInfo`, `kMaxQueryTerms` (8); `Detail::QueryChunk` | ECS-1 |
| `EntityWorld.hpp` | `EntityWorld` (`RegisterComponent`, `IsRegistered`, `ComponentCount`, `DescribeComponent`; `Create`, `Destroy`, `IsAlive`, `EntityCount`; `Set`, `Get`, `TryGet`, `Has`, `Remove`; `MakeQuery`, `QueryCount`, `DescribeQuery`; `DeferScope`, `IsDeferred`; `ForEachEntity`, `ForEachComponentOf`, `ArchetypeOf`), `ArchetypeId` | ECS-1 |

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

## Owns

- The Flecs world of a simulation and everything in it: entities, component storage, queries.
- The registry of components and queries of a world, which tools read (`DescribeComponent`, `DescribeQuery`,
  `ForEachEntity`, `ForEachComponentOf`).
- The rule for changes during iteration, and the mapping of that rule onto Flecs.
- `Runic::ECSNative`, the only way to Flecs from outside this module.

## Does not own

- Which components exist: each module registers its own (the world's transform is Runic::World's, WORLD-1).
- When systems run: the scheduler is Runic::Simulation's (SIM-1); a system is a function that runs cached queries,
  registered with the `SimulationRuntime`. Flecs' own pipeline, systems and timers are never imported.
- Events: Runic::Events (EVT-1). Global IDs and their mapping to entities: Core's ID types and, later, Network.
- State declarations and their storage (ECS-2, this module's next package).

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
- A query is a Flecs cached query with one term per component (`EcsIn` for a `const` type, `EcsInOut` otherwise).
  `EachChunk` visits one run per table (or per run of enabled rows); `Each` is a loop over it.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| ECS-1 | Flecs world wrapper on Flecs 4.2; component, system and query registration through Runic types; conventions; the opt-in Runic::ECSNative target | Entities are created, queried and destroyed through Runic types; Flecs types appear only in Runic::ECSNative, and the configure check lists every module that links it | **Implemented** (slice 4, preview branch): `RunicECSTests` passes with the 21 tests below (17 where asserts are compiled out); the build tests `build.native_target*` pass; awaiting the owner's review |
| ECS-2 | State declarations by category, owner, observable, visibility and lifetime; the benchmark harness | A state of each category is set, read and cleared through the API, and hot and content flags never move the entity to another table; observable states emit StateEntered and StateExited facts; the benchmark reports its numbers | Not started (slice 4) |

ECS-1's acceptance tests: `EntityWorld.*` (entities created and destroyed, a stale handle never alive again;
components set, read, removed; a component that owns memory built, moved and destroyed properly; queries across
storages, made before their entities exist; contiguous chunks; changes inside an iteration and inside a `DeferScope`
applied at its end; two independent worlds; the tools' listings; the archetype only changes with the set of
components; 100,000 entities), `EntityWorldDeathTest.*` (an unregistered type, a name that is not namespaced, two
types under one name, a dead entity) and `ECSNative.*` (the door reaches the same world; an entity made through it is
not one of the world's). The build's side: `build.native_target`, `build.native_target_public` and
`build.native_target_not_allowed` (cmake/README.md), and `checks.public_headers`, which keeps Flecs out of `Public/`.

Decisions taken in ECS-1 that await the owner (also in the slice brief): "system registration" is a function
registered with the `SimulationRuntime` that runs cached queries, not a Flecs system; the world class is named
`EntityWorld` (a module named World exists); `Set` inside an iteration is held back like every other change;
`Runic::ECSNative` needs no catalog row.

## Future extensions

- State declarations (ECS-2).
- Splitting a query's chunks across the worker pool, with per-worker buffers for what a chunk wants to change
  elsewhere (SIM-3, slice 10).
- Entity names and a parent relation, when an authored hierarchy needs them.
- Routing the library's own log and abort messages into Core's log.
- A lookup from `Core::GlobalEntityId` to `Entity` (Network, slice 13).
