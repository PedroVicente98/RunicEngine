# Runic::Physics — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::Physics` (`runic_physics`) · Packages: PHYS-1 to PHYS-5

Source: RunicDocs › Architecture › Simulation core Flecs and Jolt (Engine) › Jolt rules, Flecs and Jolt together;
Repositories, build targets and processes (Engine modules); Modules, work packages and the implementation contract
(PHYS-1, PHYS-2, PHYS-3); Debug drawing (Engine) › Physics debug view. Status: **PHYS-1 and PHYS-2 implemented**
(slice 7, on the preview branch `preview/slices-3-11`): the PhysicsWorld, bodies, shapes and collision layers on Jolt
v5.6 in double precision (PHYS-1), and characters on Jolt's CharacterVirtual with ground movement (PHYS-2). **PHYS-3
implemented** (slice 8, same branch): the debug view through Jolt's debug interface, cached meshes for large shapes,
and the descriptions tools read. Each with its acceptance tests. The folder map is [README.md](README.md); slice 7's brief and walkthrough are
[preview/slice-07-physics.md](../../../preview/slice-07-physics.md); slice 8's,
[preview/slice-08-physics-view.md](../../../preview/slice-08-physics-view.md).

## Purpose

Jolt behind Runic types. Bodies live at 64-bit world positions (Jolt built with `JPH_DOUBLE_PRECISION`), so physics
far from the world's origin behaves as it does at it; shapes are immutable and shared through a registry; collision
layers decide what meets what; contacts come out as facts, never as callbacks into the caller. PHYS-1 builds the
world, its bodies and shapes, the layers and the contacts; PHYS-2 adds characters, upright capsules that walk, climb
steps, stop at walls, fall and ride what they stand on; PHYS-3 draws the bodies and characters near a point into Core
debug geometry through Jolt's own debug interface, and describes bodies and shapes for tools; later packages add Jolt
on the Core worker pool (PHYS-4) and gameplay queries (PHYS-5).

## Public API

Namespace `Runic::Physics`. No Jolt type appears in a public header (the `checks.public_headers` scan enforces it).

| Header | Holds | Since |
| --- | --- | --- |
| `PhysicsHandles.hpp` | `BodyHandle`, `CharacterHandle`, `ShapeId`, `CollisionLayer` (opaque `Core::Id`s), `MotionType` (`Static`, `Kinematic`, `Dynamic`) | PHYS-1 |
| `CollisionLayers.hpp` | `CollisionLayers` (`Add`, `SetCollides`, `Count`, `Find`, `Describe`, `Collides`), `BroadPhaseGroup` (`Static`, `Moving`), `CollisionLayerInfo`, `kMaxCollisionLayers` (64), `DefaultCollisionLayers()` and its names `engine:static`, `engine:moving`, `engine:character`, `engine:sensor` | PHYS-1 |
| `PhysicsShapes.hpp` | `ShapeDesc` (`Box`, `Sphere`, `Capsule`, `Mesh`) and `BoxShape`, `SphereShape`, `CapsuleShape`, `MeshShape`; `kMinShapeSizeM` | PHYS-1 |
| `PhysicsWorld.hpp` | `PhysicsWorldSpec` (capacities, scratch memory, gravity, layers), `Pose`, `BodyDesc`, `ContactPhase`, `ContactFact`, `PhysicsStats`, `PhysicsWorld` (`AddShape`, `FindShape`, `CreateBodies`, `CreateBody`, `RemoveBodies`, `IsBody`, `BodyPose`, `LinearVelocity`, `IsActive`, `MoveKinematic`, `SetLinearVelocity`, `Step`, `TakeContacts`, `Stats`); the defaults `kDefaultMaxBodies`, `kStandardGravityMps2` and the others | PHYS-1 |
| `PhysicsWorld.hpp` | `PhysicsDebugSettings` (radius, bodies, characters, faces, bounding boxes, velocities, batch, the large-shape threshold, the mesh ID base), `PhysicsDebugStats`, `ShapeInfo`, `BodyInfo`; `PhysicsWorld::ExtractDebugGeometry`, `FindBodies`, `DescribeBody`, `DescribeShape`; the defaults `kDefaultDebugRadiusM` (150 m), `kDefaultLargeShapeTriangles` (256), `kDefaultPhysicsMeshIdBase` (2^40) and the colors `kStaticBodyColor`, `kMovingBodyColor`, `kSleepingBodyColor`, `kSensorBodyColor`, `kCharacterColor`, `kVelocityColor`, `kDebugFaceAlpha` | PHYS-3 |
| `PhysicsWorld.hpp` | `CharacterDesc` (radius, height, layer, pose of the feet, steepest walkable slope, step height, floor distance, owner), `GroundState` (`OnGround`, `OnSteepGround`, `NotSupported`, `InAir`), `CharacterMove` (walk velocity, jump speed), `CharacterState`; `PhysicsWorld::CreateCharacter`, `RemoveCharacter`, `IsCharacter`, `MoveCharacter`, `SetCharacterPose`, `Character`; `PhysicsStats::characters`; the defaults `kDefaultCharacterRadiusM`, `kDefaultCharacterHeightM`, `kDefaultMaxSlopeDeg`, `kDefaultStepHeightM`, `kDefaultStickToFloorM` and the limit `kMaxSlopeLimitDeg` | PHYS-2 |

**The precision rule:** a body's place is a `Core::WorldPosition` (64-bit), its rotation a 32-bit quaternion, and a
shape's geometry 32-bit relative to its body. Jolt runs in double precision, so two bodies 8,600 km from the origin
touch, settle and rest exactly as they do at it.

**The character rule:** a character is a Jolt `CharacterVirtual`, not a body: an upright capsule (+Y up) whose
position is its feet, moved by collision checks. Each `Step` updates every character, in slot order, before the
bodies step, whether anyone moved it or not, so a kinematic platform carries whoever stands on it. A `CharacterMove`
lasts one step: on walkable ground the character takes the ground's velocity plus the horizontal part of the move,
and may leave the ground with the move's jump speed; on steep ground or touching only a wall it keeps its vertical
speed and slides or falls; in the air it keeps the horizontal velocity it had; gravity (the world's) always pulls.
Jolt's extended update then walks it up steps no higher than its `stepHeight`, keeps it on the floor down to its
`stickToFloor` and stops it at walls, keeping the part of its motion along them. A new or teleported character finds
what it touches at once, so one made standing on the ground walks in its first step.

**Characters meet each other only when their layers collide** (the Engine's `engine:character` meets no character
layer, so by default characters pass through each other). They are found near each other through a grid of 4 m
columns over the 64-bit world position, kept as they move, so a test covers the characters around one character, not
all of them. Characters block each other; neither pushes the other (Jolt would let a walking character push a
standing one along).

**The debug view rule** (PHYS-3; RunicDocs › Debug drawing › Path B): Physics implements Jolt's debug interface
(a `DebugRendererSimple`) and draws, through each shape's own `Draw`, the bodies whose bounds overlap a box around an
origin and the characters inside it, as camera-relative 32-bit lines (and translucent faces when asked) in a Core
`RenderData::DebugGeometry`; every 64-bit position is brought down relative to the origin one at a time, so a body
8,600 km out is drawn to the millimetre. Colors follow RunicDocs' palette: static gray, moving blue, sleeping dark
blue, sensors cyan, characters green. A registered shape of at least `largeShapeTriangles` triangles, when a
`DebugMeshLibrary` is given, is captured once in its own space (wireframe and faces) under `DebugMeshId(meshIdBase +
ShapeId)` and drawn as an instance at its body's pose. The view needs Jolt's debug renderer: builds with
`RUNIC_PHYSICS_DEBUG_VIEW` off report it unavailable. Physics links no GodotBridge; GodotBridge draws the plain
geometry.

**The contact rule:** Jolt reports contacts from inside the step, on whichever thread runs its collision job. Physics
only records them, under a lock: one fact when a pair of bodies begins to touch (its first sub-shape contact, with a
point and the normal from a to b) and one when it stops (its last), each with the owners the bodies were given.
`TakeContacts` hands them over after the step. Nothing the caller owns is touched from a callback. Jolt drops the
contacts of a body that falls asleep though it still touches; Physics decides after the step: a pair whose moving
bodies all sleep stays touching (dormant), wakes silently when Jolt finds it again, and ends when one of its bodies
woke a step ago and Jolt did not find it, or when one of its bodies is removed. A crate asleep on a pressure plate
never "ends" its contact.

## Owns

- Jolt: its process-wide setup (allocator, factory, types; the first world sets it up, the last tears it down), the
  `PhysicsSystem`, its scratch allocator and its job system. Until PHYS-4, Jolt runs on a single-threaded job system:
  it creates no thread.
- The shape registry: names, the immutable Jolt shapes, their sharing.
- The collision layers handed to the world, as Jolt's three layer interfaces (a layer's broad-phase mask is worked out
  once).
- Bodies: created and added in one batch (`AddBodiesPrepare`, `AddBodiesFinalize`), removed and destroyed in one
  batch; their handles; their owners, kept until the step after a body's removal so its last contacts still name it.
- The step: the characters' update, then one `PhysicsSystem::Update` per call, with a collision step every 1/60 s
  (two at 30 Hz), as Jolt advises.
- The debug view (PHYS-3): its Jolt renderer (made at the first extraction, one per world) and which shapes are
  cached meshes; each registered shape carries its `ShapeId` in Jolt's user data, so a body's shape names its entry.
- Characters: their `CharacterVirtual`s in slots (a handle is the slot's generation above its index, so a freed and
  reused slot never answers to an old handle), one capsule shape per size shared by every character of that size, the
  per-layer column grid that finds characters near each other, and the contact listener that keeps them from
  pushing each other.

## Does not own

- Which entity a body belongs to, when bodies are made and removed, and writing transforms back: the Simulation's
  physics bridge (SIM-8). Physics knows an owner only as a number it hands back.
- What a character's movement values are (walk speed, jump speed, step height, slope): the Engine's movement
  attributes (GPL-2) and movement modes (GPL-1), filled from Game data. Physics takes them as given.
- Contacts of characters as facts, and characters seen by bodies or sensors (an inner body): not yet (see Future
  extensions).
- Drawing: GodotBridge's physics geometry layer (GDB-4) uploads the geometry and the cached meshes; which frame
  stream they go into and when is the extractor's (a program's, or a later Client package's).
- Jolt on the worker pool (PHYS-4), gameplay queries (PHYS-5).
- What a contact means: Runic decides (RunicDocs: "Jolt results are facts").

## Allowed dependencies

Catalog row: Core, World; Jolt private. PHYS-1 links Core (PUBLIC: the public headers use its world-space, math, ID
and Result types) and Jolt (PRIVATE). PHYS-3 uses Core's RenderData (debug geometry and cached meshes) through the
same link. World is not linked: nothing needs it yet.

## Forbidden dependencies

Everything outside the row: ECS, Simulation and every module above (the bridge is Simulation's), GodotBridge and
Godot, RunicPlatform and RunicGame. Jolt's types in a public header.

## Threading and tick model

A world is used by one thread at a time: in a simulation, the driving thread in the physics phases (SIM-8). Contact
callbacks may run on Jolt's job threads once PHYS-4 gives it the pool; the contact buffer is already locked for it.
Jolt's process-wide setup is guarded by a mutex.

## Data ownership

A `PhysicsWorld` owns its Jolt system, its bodies, its shapes, its characters and its copy of the layers; destroying
it destroys every character and removes and destroys every body first. Handles are plain numbers, meaningless outside their world.

## Where the code lives

- `Source/PhysicsWorld/`: `PhysicsWorld.cpp`, `WorldState.hpp` (the state, `PhysicsWorld::State` and the Runic↔Jolt
  conversions the module's files share), `JoltRuntime.hpp`/`.cpp`.
- `Source/Characters/`: `CharacterSet.hpp` (slots, handles, capsules, the column grid, the contact listener) and
  `Characters.cpp` (the PhysicsWorld's character methods and the update).
- `Source/CollisionLayers/`: `CollisionLayers.cpp`, `JoltLayers.hpp`.
- `Source/DebugExtraction/`: `DebugCapture.hpp` (the capture's deleter, so the world state needs no Jolt renderer
  header) and `DebugExtraction.cpp` (the capture, `ExtractDebugGeometry`, `FindBodies`, `DescribeBody`,
  `DescribeShape`). Bodies are found with a broad-phase box query and drawn in handle order.
- `cmake/Dependencies.cmake`: `RUNIC_PHYSICS_DEBUG_VIEW` (on by default) builds Jolt with `JPH_DEBUG_RENDERER` in every
  configuration.
- A body handle is Jolt's index-and-sequence number plus one, so 0 stays "none" and a reused index is a new handle.
- `cmake/Dependencies.cmake`: the Jolt 5.6.0 recipe (double precision; asserts, floating-point exceptions, object
  stream, profiler and debug renderer off).

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| PHYS-1 | PhysicsWorld, bodies, shapes and collision layers on Jolt v5.6 in double precision, kept private | Bodies collide without jitter far from the origin | **Implemented** (slice 7, preview branch): `RunicPhysicsTests` passes with the 12 tests below (10 where asserts are compiled out); awaiting the owner's review |
| PHYS-2 | CharacterVirtual integration and ground movement | A capsule walks, climbs steps and stops at walls | **Implemented** (slice 7, preview branch): `RunicPhysicsTests` passes with the 14 tests below added (13 where asserts are compiled out); awaiting the owner's review |
| PHYS-3 | Debug-geometry extraction: Jolt's debug interface implemented inside Physics, written into Core DebugGeometry buffers | Bodies and shapes come out as plain lines and triangles; Physics links no GodotBridge | **Implemented** (slice 8, preview branch): `RunicPhysicsTests` passes with the 7 tests below added (skipped where the debug view is built out); awaiting the owner's review |
| PHYS-4, PHYS-5 | Jolt on the worker pool, gameplay queries | See RunicDocs › Modules, work packages | Not started (slices 10, 15) |

PHYS-1's acceptance tests (`RunicPhysicsTests`): `CollisionLayers.*` (the Engine's layers and their table; a game's
own layer) and `CollisionLayersDeathTest.*`; `PhysicsShapes.*` (registered once by name; degenerate shapes refused);
`PhysicsWorld.*` (a crate falls and rests; **the same drop 8,600 km from the origin rests at the same height, still to
a tenth of a millimetre**; a batch of 1,000 bodies made and 400 removed; a kinematic body reaches its target; contacts
arrive as one fact per pair with their owners, began and ended; a contact lasts while its bodies sleep and ends when a
sleeping body is removed; Jolt creates no thread) and `PhysicsWorldDeathTest.*`.

PHYS-2's acceptance tests (`RunicPhysicsTests`): `Characters.*` (walk on flat ground and stand when nobody moves
them; **climb a step lower than their step height; stop at a higher one; stop at walls and slide along them**; walk up
a 20° ramp but not a 65° one; fall off an edge and land below; jump only from the ground; ride a kinematic platform
nobody else moves; **walk the same course over a step and along a wall at the origin and 8,600 km from it, within a
millimetre at every step**; meet each other only when their layers collide, and then stand shoulder to shoulder;
find each other across the grid's columns and after teleports, without pushing; handles; degenerate descriptions
refused) and `CharactersDeathTest.*`.

PHYS-3's acceptance tests (`RunicPhysicsTests`): `PhysicsDebugView.*` (**a box 8,600 km out comes out as plain lines
spanning it to the millimetre, relative to the origin**; only what is near the origin; colors follow motion, sleep
and faces are translucent; characters drawn from their feet; **a 512-triangle mesh becomes one cached mesh put once
and drawn by an instance, or lines without a library**; bounds and velocity arrows in the overlay batch) and
`PhysicsInspection.*` (bodies near a point in handle order; a body's motion, layer, owner, activity, pose; a shape's
name, kind, size and triangles).

Decisions taken in PHYS-3 that await the owner (also in the slice brief): Jolt's debug renderer is in every build
unless `RUNIC_PHYSICS_DEBUG_VIEW` is off; the view draws each shape through Jolt's `Shape::Draw` (wireframe, faces on
request), not Path A's own primitives; the colors above; bodies are found with a box query around the origin
(150 m by default); a large shape is one of 256 triangles or more, cached with its wireframe and faces in the color
it had when first drawn; the capture paints the color it was told (Jolt's multiply by white rounds 255 to 254).

Decisions taken in PHYS-2 that await the owner (also in the slice brief): a move lasts one step; in the air a
character keeps its horizontal velocity (no air control until the movement attributes bring it); a jump is a speed
the caller gives, taken only from walkable ground; characters update before the bodies; characters block but never
push each other; by default characters pass through each other (`engine:character` does not meet itself); a
character is a capsule given by radius and height (not a registry shape); no inner body, so bodies and sensors do
not see characters, and character contacts are not facts yet; Jolt's own values for padding, predictive distance,
mass and strength.

Decisions taken in PHYS-1 that await the owner (also in the slice brief): Jolt runs single-threaded until PHYS-4;
two collision steps per 30 Hz step; a contact fact per body pair (not per sub-shape), no "persisted" fact; four Engine
layers in two broad-phase groups; owners are a 64-bit number the caller chooses; Jolt's asserts are off.

## Future extensions

- Constraints, contact points and the broad phase in the debug view, when a slice needs them; text (a body's name
  beside it) through Core's debug text.

- Characters seen by sensors and bodies (Jolt's inner body), and their contacts as facts, when a slice needs a
  trigger that detects characters; another shape or a shape change (crouching, mounts); air control and the other
  movement attributes (GPL-1, GPL-2).

- Jolt on the Core worker pool (PHYS-4); queries (PHYS-5); the debug view (PHYS-3).
- Height-field terrain, compound shapes, constraints, when a slice needs them.
- `OptimizeBroadPhase` after a large static load (a cell streamed in).
