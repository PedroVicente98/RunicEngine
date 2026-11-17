# Runic::Physics — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::Physics` (`runic_physics`) · Packages: PHYS-1 to PHYS-5

Source: RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › Jolt rules, Flecs and Jolt together;
Repositories, build targets and processes (Engine modules); Modules, work packages and the implementation contract
(PHYS-1, PHYS-2). Status: **PHYS-1 implemented** (slice 7, on the preview branch `preview/slices-3-11`): the
PhysicsWorld, bodies, shapes and collision layers on Jolt v5.6 in double precision, with their acceptance tests. The
folder map is [README.md](README.md); slice 7's brief and walkthrough are
[preview/slice-07-physics.md](../../../preview/slice-07-physics.md).

## Purpose

Jolt behind Runic types. Bodies live at 64-bit world positions (Jolt built with `JPH_DOUBLE_PRECISION`), so physics
far from the world's origin behaves as it does at it; shapes are immutable and shared through a registry; collision
layers decide what meets what; contacts come out as facts, never as callbacks into the caller. PHYS-1 builds the
world, its bodies and shapes, the layers and the contacts; later packages add characters (PHYS-2), the debug view
(PHYS-3), Jolt on the Core worker pool (PHYS-4) and gameplay queries (PHYS-5).

## Public API

Namespace `Runic::Physics`. No Jolt type appears in a public header (the `checks.public_headers` scan enforces it).

| Header | Holds | Since |
| --- | --- | --- |
| `PhysicsHandles.hpp` | `BodyHandle`, `CharacterHandle`, `ShapeId`, `CollisionLayer` (opaque `Core::Id`s), `MotionType` (`Static`, `Kinematic`, `Dynamic`) | PHYS-1 |
| `CollisionLayers.hpp` | `CollisionLayers` (`Add`, `SetCollides`, `Count`, `Find`, `Describe`, `Collides`), `BroadPhaseGroup` (`Static`, `Moving`), `CollisionLayerInfo`, `kMaxCollisionLayers` (64), `DefaultCollisionLayers()` and its names `engine:static`, `engine:moving`, `engine:character`, `engine:sensor` | PHYS-1 |
| `PhysicsShapes.hpp` | `ShapeDesc` (`Box`, `Sphere`, `Capsule`, `Mesh`) and `BoxShape`, `SphereShape`, `CapsuleShape`, `MeshShape`; `kMinShapeSizeM` | PHYS-1 |
| `PhysicsWorld.hpp` | `PhysicsWorldSpec` (capacities, scratch memory, gravity, layers), `Pose`, `BodyDesc`, `ContactPhase`, `ContactFact`, `PhysicsStats`, `PhysicsWorld` (`AddShape`, `FindShape`, `CreateBodies`, `CreateBody`, `RemoveBodies`, `IsBody`, `BodyPose`, `LinearVelocity`, `IsActive`, `MoveKinematic`, `SetLinearVelocity`, `Step`, `TakeContacts`, `Stats`); the defaults `kDefaultMaxBodies`, `kStandardGravityMps2` and the others | PHYS-1 |

**The precision rule:** a body's place is a `Core::WorldPosition` (64-bit), its rotation a 32-bit quaternion, and a
shape's geometry 32-bit relative to its body. Jolt runs in double precision, so two bodies 8,600 km from the origin
touch, settle and rest exactly as they do at it.

**The contact rule:** Jolt reports contacts from inside the step, on whichever thread runs its collision job. Physics
only records them, under a lock: one fact when a pair of bodies begins to touch (its first sub-shape contact, with a
point and the normal from a to b) and one when it stops (its last), each with the owners the bodies were given.
`TakeContacts` hands them over after the step. Nothing the caller owns is touched from a callback.

## Owns

- Jolt: its process-wide setup (allocator, factory, types; the first world sets it up, the last tears it down), the
  `PhysicsSystem`, its scratch allocator and its job system. Until PHYS-4, Jolt runs on a single-threaded job system:
  it creates no thread.
- The shape registry: names, the immutable Jolt shapes, their sharing.
- The collision layers handed to the world, as Jolt's three layer interfaces (a layer's broad-phase mask is worked out
  once).
- Bodies: created and added in one batch (`AddBodiesPrepare`, `AddBodiesFinalize`), removed and destroyed in one
  batch; their handles; their owners, kept until the step after a body's removal so its last contacts still name it.
- The step: one `PhysicsSystem::Update` per call, with a collision step every 1/60 s (two at 30 Hz), as Jolt advises.

## Does not own

- Which entity a body belongs to, when bodies are made and removed, and writing transforms back: the Simulation's
  physics bridge (SIM-8). Physics knows an owner only as a number it hands back.
- Characters (PHYS-2), the debug view (PHYS-3), Jolt on the worker pool (PHYS-4), gameplay queries (PHYS-5).
- What a contact means: Runic decides (RunicDocs: "Jolt results are facts").

## Allowed dependencies

Catalog row: Core, World; Jolt private. PHYS-1 links Core (PUBLIC: the public headers use its world-space, math, ID
and Result types) and Jolt (PRIVATE). World is not linked: nothing in PHYS-1 needs it.

## Forbidden dependencies

Everything outside the row: ECS, Simulation and every module above (the bridge is Simulation's), GodotBridge and
Godot, RunicPlatform and RunicGame. Jolt's types in a public header.

## Threading and tick model

A world is used by one thread at a time: in a simulation, the driving thread in the physics phases (SIM-8). Contact
callbacks may run on Jolt's job threads once PHYS-4 gives it the pool; the contact buffer is already locked for it.
Jolt's process-wide setup is guarded by a mutex.

## Data ownership

A `PhysicsWorld` owns its Jolt system, its bodies, its shapes and its copy of the layers; destroying it removes and
destroys every body first. Handles are plain numbers, meaningless outside their world.

## Where the code lives

- `Source/PhysicsWorld/`: `PhysicsWorld.cpp`, `WorldState.hpp` (the state and the Runic↔Jolt conversions the module's
  files share), `JoltRuntime.hpp`/`.cpp`.
- `Source/CollisionLayers/`: `CollisionLayers.cpp`, `JoltLayers.hpp`.
- A body handle is Jolt's index-and-sequence number plus one, so 0 stays "none" and a reused index is a new handle.
- `cmake/Dependencies.cmake`: the Jolt 5.6.0 recipe (double precision; asserts, floating-point exceptions, object
  stream, profiler and debug renderer off).

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| PHYS-1 | PhysicsWorld, bodies, shapes and collision layers on Jolt v5.6 in double precision, kept private | Bodies collide without jitter far from the origin | **Implemented** (slice 7, preview branch): `RunicPhysicsTests` passes with the 11 tests below (9 where asserts are compiled out); awaiting the owner's review |
| PHYS-2 | CharacterVirtual integration and ground movement | A capsule walks, climbs steps and stops at walls | Not started (slice 7) |
| PHYS-3 to PHYS-5 | Debug-geometry extraction, Jolt on the worker pool, gameplay queries | See RunicDocs › Modules, work packages | Not started (slices 8, 10, 15) |

PHYS-1's acceptance tests (`RunicPhysicsTests`): `CollisionLayers.*` (the Engine's layers and their table; a game's
own layer) and `CollisionLayersDeathTest.*`; `PhysicsShapes.*` (registered once by name; degenerate shapes refused);
`PhysicsWorld.*` (a crate falls and rests; **the same drop 8,600 km from the origin rests at the same height, still to
a tenth of a millimetre**; a batch of 1,000 bodies made and 400 removed; a kinematic body reaches its target; contacts
arrive as one fact per pair with their owners, began and ended; Jolt creates no thread) and `PhysicsWorldDeathTest.*`.

Decisions taken in PHYS-1 that await the owner (also in the slice brief): Jolt runs single-threaded until PHYS-4;
two collision steps per 30 Hz step; a contact fact per body pair (not per sub-shape), no "persisted" fact; four Engine
layers in two broad-phase groups; owners are a 64-bit number the caller chooses; Jolt's asserts are off.

## Future extensions

- Jolt on the Core worker pool (PHYS-4); queries (PHYS-5); the debug view (PHYS-3).
- Height-field terrain, compound shapes, constraints, when a slice needs them.
- `OptimizeBroadPhase` after a large static load (a cell streamed in).
