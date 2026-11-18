# Runic::World — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::World` (`runic_world`) · Packages: WORLD-1, WORLD-2,
WORLD-3

Source: RunicDocs › Architecture › World space coordinates, cells and activation (Engine); Repositories, build targets
and processes (Engine modules); Modules, work packages and the implementation contract (WORLD-1, WORLD-2); Simulation
core Flecs and Jolt (Engine) › Flecs and Jolt together; Debug drawing (Engine) › What to visualize. Status: **WORLD-1
implemented** (slice 4, on the preview branch `preview/slices-3-11`): region-local coordinates, the world registry,
cell and sector keys and the components that place things in the world. **WORLD-2 implemented** (slice 11, same
branch): the sparse 3D spatial index with exact queries and membership updates, authored volumes with exact point
tests and their backends, and both in the debug view. Each with its acceptance tests. The folder map is
[README.md](README.md); the slice briefs and walkthroughs are
[preview/slice-04-ecs-and-world-coordinates.md](../../../preview/slice-04-ecs-and-world-coordinates.md) and
[preview/slice-11-spatial-index.md](../../../preview/slice-11-spatial-index.md).

## Purpose

The world is one continuous 64-bit space, thousands of kilometres wide and with real height and depth. Runic::World
says how that space is addressed: which worlds exist and how each divides its space into cells and sectors, how a
region's authored 32-bit coordinates become places in the world and back, and which components put an entity or a
region there. WORLD-1 builds those. WORLD-2 builds on them the spatial index (which entities are near a place, in
3D, exactly) and authored volumes (caves, rule areas, indivisible volumes: whether a place is inside them), and draws
both in the debug view. Activation (WORLD-3, slice 21) is built on the index.

## Public API

Namespace `Runic::World`. Headers are included by their path under `Public/` (`#include "WorldRegistry.hpp"`). No
library types.

| Header | Holds | Since |
| --- | --- | --- |
| `CellKeys.hpp` | `CellCoord` (x, y, z, 32-bit), `SectorCoord` (x, z), `CellKey` and `SectorKey` (a `Core::WorldId` and the address), their `std::hash` | WORLD-1 |
| `CoordinateSpace.hpp` | `CoordinateSpace` (origin, `cellSizeM`, `cellsPerSector`; `CellOf`, `CellMin`, `CellCenter`, `OffsetInCell`, `SectorOf`, `SectorSizeM`), `kDefaultCellSizeM` (64), `kDefaultCellsPerSector` (8) | WORLD-1 |
| `RegionFrame.hpp` | `RegionFrame` (a 64-bit origin; `ToWorld`, `ToRegionLocal`, `Reaches`), `kExactRegionRangeM` | WORLD-1 |
| `WorldRegistry.hpp` | `WorldRegistry` (`Register`, `Knows`, `SpaceOf`, `CellAt`, `SectorAt`, `CellMin`, `CellCenter`, `SectorOf`, `Count`, `Describe`), `WorldDescription`, `WorldInfo`, `WorldInstance`, `kSharedInstance` | WORLD-1 |
| `SpatialIndex.hpp` | `SpatialBounds` (a 64-bit center and 32-bit half sizes), `CellRange` (`Contains`, `Count`), `CellsTouched`, `QueryShape` (`Sphere`, `Box`, `Ellipsoid`), `SpatialQuery` (`Sphere`, `Box`, `Ellipsoid`, `Overlaps`, `Contains`, `Bounds`), `SpatialIndexStats`, `SpatialCellInfo`, `SpatialIndex` (`Insert`, `Update`, `Remove`, `Contains`, `BoundsOf`, `CellsOf`, `Query`, `MembersIn`, `Cells`, `MembersOf`, `Stats`) | WORLD-2 |
| `AuthoredVolumes.hpp` | `VolumeId` (slot and generation), `VolumeKindId`, `VolumeBackend` (`SpatialIndex`, `SensorBody`, `StaticBody`), `VolumeKindDesc`, `VolumeKindInfo`, `VolumePartShape` (`Box`, `Sphere`, `Cylinder`), `VolumePart` and `BoxPart`, `SpherePart`, `CylinderPart`, `kMinVolumePartSizeM` (1 cm), `VolumeDesc`, `VolumeInfo`, `VolumeSetStats`, `VolumeSet` (`AddKind`, `FindKind`, `DescribeKind`, `Add`, `Remove`, `IsVolume`, `SetPose`, `Describe`, `PartsOf`, `CellsOf`, `Volumes`, `IsInside`, `VolumesAt`, `PhysicsVolumes`, `Stats`) | WORLD-2 |
| `SpatialDebug.hpp` | `DrawSpatialIndex`, `DrawSpatialQuery`, `DrawVolumes`, `SpatialDebugSettings`, `SpatialDebugStats`, `kDefaultSpatialDebugRadiusM` (200 m), `kPointMarkerHalfM`, and the colors `kSpatialCellColor`, `kSpatialMemberColor`, `kSpatialQueryColor`, `kSpatialFoundColor`, `kRuleAreaColor`, `kSensorVolumeColor`, `kSolidVolumeColor`, `kIndivisibleBoundsColor` | WORLD-2 |
| `WorldComponents.hpp` | The components `WorldTransform` (a `Core::WorldPosition` and a rotation) and `Region` (a world and a `RegionFrame`), `kNoRotation`, `RegisterWorldComponents`, `kWorldTransformName`, `kRegionName` | WORLD-1 |

**What is exact** (the tests hold the module to it):

- A position falls in exactly one cell: `floor((position − origin) / cell size)` on each axis, y included. Cell and
  sector sizes are whole metres, so every boundary is an exact 64-bit value, and `CellOf(CellMin(c)) == c` and
  `CellOf(CellCenter(c)) == c` for every 32-bit `c`: any altitude, any depth, any distance.
- A region-local coordinate that is zero or at least 1 cm goes up to the world (`ToWorld`) and back
  (`ToRegionLocal`) unchanged, bit for bit, wherever origin plus offset stays inside 2²³ m (8,388 km) on each axis:
  `kExactRegionRangeM` is that bound minus the 10 km a local offset may span. Further out the round trip is off by at
  most the 64-bit step there (2 nm up to 16,777 km).
- The other way loses what a float cannot hold: a world position that goes down to region-local and up again is
  within half a millimetre inside the 10 km a local offset may span, and within 4 µm inside a 64 m cell
  (`OffsetInCell`).
- A `WorldId` the registry does not know is refused by every lookup, with `world <id> is not registered`.

**The index rule** (WORLD-2; RunicDocs › Sparse 3D cells and exact queries):

- *Sparse by column.* A member is an entity with a box (a 64-bit center, 32-bit half sizes; a point has none). It is
  in every cell its box touches, from the cell of its lowest corner to the cell of its highest, so a ship as large as
  many cells is in all of them. The grid is a map of XZ columns. Each column holds only its occupied Y cells, sorted
  by y. A cell left empty is freed, and a column left empty is erased. Empty air and solid rock cost nothing.
- *Membership follows moves.* `Update` changes only the cells a member left and the cells it entered. A move that
  stays in the same cells changes none (it costs a bounds write), and in the cells it stays in a member keeps its
  place.
- *Queries are exact in 3D.* A query gathers the cells its volume's box touches and tests every member there with the
  same exact test it exposes (`SpatialQuery::Overlaps`). The test is in 64-bit, from the offset between the two
  centers, boundaries included: the sphere and the ellipsoid against a box's nearest point (for an axis-aligned
  ellipsoid that point is the box clamped axis by axis, in the space where the ellipsoid is a unit sphere), and two
  boxes by their overlap on each axis. So a query returns exactly what testing every member returns, each member once.
  A member in several cells of the range is reported from the first of them, so no query keeps state, and several
  threads may query at once while nothing changes the index.
- *A fixed order.* Results come in cell order (x, then z, then y, ascending) and, in a cell, in the order members
  entered it. A query whose range covers more columns than the grid has walks the grid's columns (sorting the ones
  inside the range) instead of looking up every column of the range, and gives the same order.

**The volume rule** (WORLD-2; RunicDocs › Volumes for caves and rules; Flecs and Jolt together: "the volume kind
picks the backend"):

- *A volume is the union of its parts* (boxes, spheres and upright cylinders, each moved and turned in the volume's
  own space), placed at a 64-bit origin with a rotation. A point is inside when it is in or on one of its parts,
  tested in 64-bit from the volume's origin. A cave is a volume beneath the surface in the same coordinates; nothing
  here knows surface, sky or underground.
- *The kind picks where it lives.* A kind (a namespaced name, its meaning the Game's) is a static rule area by
  default: its volumes go in a sparse grid of their own and `VolumesAt` answers for a point from its cell, each
  candidate tested exactly. They never become a Physics body. A kind that follows an entity makes sensor volumes
  (`SetPose` moves them), and a solid kind makes static-body volumes. Both are listed by `PhysicsVolumes` for whoever
  owns the physics world, and neither is in the rule areas' grid. A kind that is both solid and moving is refused.
- *Indivisible volumes* (an island, a castle, a docked ship) are flagged. `CellsOf` says which cells a volume touches,
  so ownership can keep them with one simulation.
- *IDs* are a slot and its generation, so a removed volume's ID never names the volume that takes its slot.

**The debug view rule** (WORLD-2; RunicDocs › Debug drawing: "Cells and interest: AABBs, grid cells, colored
boundaries"): everything whose box comes within a radius of an origin is drawn as camera-relative 32-bit lines in a
Core `RenderData::DebugGeometry`, each 64-bit position brought down relative to the origin one at a time.
- The occupied cells are dim gray boxes. A member is an orange box (a point is a box 0.5 m across).
- A query is magenta (a sphere, a box, or an ellipsoid as three ellipses), with the members it found, in the overlay
  batch.
- Volume parts take their backend's color: rule areas orange, sensors cyan, solids blue. An indivisible volume's
  bounds are also drawn, in white.
- Shapes that reach further than Core's 10 km from the origin are counted, not drawn.

World links no GodotBridge.

## Owns

- The partition's keys: what a cell and a sector are and how a position maps to them.
- Each world's coordinate space (where its grid starts, its cell and sector sizes) and the registry of worlds.
- What an instance is: an `InstanceId` of a world, sharing that world's space; `kSharedInstance` (0) is the world's
  own copy.
- Region-local coordinates and the rule for when they are exact.
- The `WorldTransform` and `Region` components.
- The spatial index (WORLD-2): its grid, its members' boxes and cells, the exact query tests and their order.
- Authored volumes (WORLD-2): their kinds and which backend each picks, the volumes' parts and poses, the rule areas'
  grid, the point test, and the list of volumes that need a Physics body.
- Drawing the index, its queries and the volumes into Core debug geometry.

## Does not own

- The 64-bit types themselves and the one narrowing conversion (`Core::WorldPosition`, `Core::ToLocal`).
- Which worlds exist and their sizes: the Game registers them (the sizes in RunicDocs are examples); allocating
  instances is the Platform's.
- Which simulation runs which cells (the Platform's allocation), the cells' files (the Cooker), quantized positions
  on the wire (Network) and the presenter's floating origin (GodotBridge).
- Who is a member and when membership updates: the system that moves entities (RunicDocs: the PostPhysics phase
  writes positions to spatial membership; the Simulation's or a module's system, not yet wired in the runtime).
- What a volume kind means (a quest area, a flight ceiling, a light profile): the Game. Volume flags that weather or
  light read are not here yet.
- Activation volumes and streaming (WORLD-3).
- Physics: a cell's or a volume's collision is Runic::Physics'. Making the sensor and solid bodies `PhysicsVolumes`
  lists is the physics world's owner's (the Simulation's bridge, in a later package); World cannot link Physics.
- Drawing: GodotBridge draws the geometry; which frame stream it goes into is the extractor's (a program's).

## Allowed dependencies

Catalog row: Core, ECS (both PUBLIC: the public headers use Core's world-space, math, ID and result types, and
`WorldComponents.hpp` registers components in an `ECS::EntityWorld`). No third party. WORLD-2 uses Core's RenderData
(debug geometry and drawing), its GLM math (double-precision rotations in `Source/Volumes`) and ECS's `Entity`
through the same links.

## Forbidden dependencies

Every other Runic module; Godot, audio and editor code (the module is headless); RunicPlatform and RunicGame.

## Threading and tick model

- `CoordinateSpace`, `RegionFrame` and the keys are plain values: any thread.
- `WorldRegistry`: worlds are registered at start, on one thread; after that every thread may read it.
- Nothing here blocks, and the conversions allocate nothing. A failed lookup builds its error text.
- `SpatialIndex` and `VolumeSet`: changed by one thread (in a simulation, the driving thread in its phases); queries,
  point tests and drawing only read, so any number of threads may run them while nothing changes the index or the set.
  A query allocates only when its range is wider than the grid (the list of the grid's columns inside it) and when its
  output grows.

## Data ownership

The program owns the `WorldRegistry` and hands it to what needs it. A `CoordinateSpace` is copied freely. The
components belong to their entities.

## Integration points

- A program registers its worlds, and a simulation knows its own `WorldInstance`.
- Modules place entities with `WorldTransform`; systems move them by adding `Core::WorldDelta` or 32-bit vectors to
  the 64-bit position.
- The Cooker and the editor read and write region-local coordinates through `RegionFrame`.
- WORLD-2's spatial index, Network's interest grid and the streaming of cells key on `CellKey` and `SectorKey`.

## Where the code lives

- `Source/Coordinates/RegionFrame.cpp`: `Reaches`; the two conversions are inline, on Core's operators.
- `Source/Partition/CoordinateSpace.cpp`: cells and sectors (floor on each axis; floor division for sectors).
- `Source/Partition/WorldRegistry.cpp`: the registry.
- `Source/SpatialIndex/SparseCells.hpp`/`.cpp`: the grid. Columns in an `unordered_map` keyed by (x, z) packed in 64
  bits; a column's cells a vector sorted by y; a cell's slots a vector reused from freed cells. Removal keeps the
  others' order.
- `Source/SpatialIndex/SpatialIndex.cpp`: members in slots (a free list, an `unordered_map` from entity to slot), the
  exact tests, membership and queries. `SpatialDebug.cpp`: drawing the index and a query.
- `Source/Volumes/VolumeShapes.hpp`: the parts' geometry in 64-bit (GLM double quaternions). `AuthoredVolumes.cpp`:
  kinds, volumes (each part's inverse rotation worked out once), the rule areas' grid and the point test.
  `VolumeDebug.cpp`: drawing the volumes.
- A volume's box is worked out in 64-bit around its parts and rounded up to the next float only where a float cannot
  hold it, so its cells are exact for parts that end on a cell boundary.
- `Source/Partition/WorldComponents.cpp`: the components' registration and their formatters for tools
  (`(6000000.12, -42.50, 7.00) m`; `world 3, origin (100.00, 0.00, -100.00) m`).
- Why the round trip is exact: a float of magnitude at least 2⁻⁷ m (7.8 mm) has a step of at least 2⁻³⁰ m, and a
  64-bit value below 2²³ m has a step of at most 2⁻³⁰ m, so the sum of the origin and the offset is a 64-bit value
  with no rounding.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| WORLD-1 | Region-local origins and conversions; the world registry (WorldId, coordinate space, origin, partition; instances as InstanceIds); Region, Sector and sparse 3D Cell keys | Conversions round-trip exactly far from the origin; a cell key round-trips at any altitude or depth; a WorldId the registry does not know is rejected | **Implemented** (slice 4, preview branch): `RunicWorldTests` passes with the 20 tests below; awaiting the owner's review |
| WORLD-2 | Sparse 3D spatial index, exact radius, box and ellipsoid queries, membership updates, authored volumes with point-in-volume queries; the volume kind picks the backend | Query results match brute force in 3D and draw in the debug view; a point inside a cave volume is reported inside it; a static rule area creates no Physics body | **Implemented** (slice 11, preview branch): `RunicWorldTests` passes with the 21 tests below added (19 where asserts are compiled out); RunicClient's CTest `godot.runic_client_spatial` passes; awaiting the owner's review |
| WORLD-3 | Activation volumes and streaming state | See RunicDocs › Modules, work packages | Not started (slice 21) |

WORLD-1's acceptance tests: `CoordinateSpace.*` (one cell per position, with floor on negatives and exact
boundaries; a cell round-trips from the ends of the 32-bit range, at the surface and 100,000 random addresses; every
point of a far cell maps to it; the offset in a cell; sectors), `CellKeys.*`, `RegionFrame.*` (400,000 local
positions go up and back bit for bit at four origins up to the end of the exact range; the way down is within half a
millimetre; `Reaches`; nanometres beyond the range), `WorldRegistry.*` (each world's space; an unknown `WorldId`
refused by every lookup; bad registrations; two worlds, two cells; a cell through the registry and back; instances)
and `WorldComponents.*` (the slice's runnable result: 1,000 entities one centimetre apart, 6,000 km out, created,
queried and moved; a region as an entity; the tools' text).

WORLD-2's acceptance tests (`RunicWorldTests`):
- `SpatialIndex.*`: a column holds only its occupied cells, from a cave 500 m down to an island 2 km up. **5,000
  points and boxes (a fifth of them in many cells) 7,800 km from the origin, 1,800 random spheres, boxes and
  ellipsoids, before and after the members move, are removed and come back: every result equals testing every member,
  each member once.** Also: a 500 m ship in 81 cells found once; a move inside its cells changes none; results in cell
  order, also for queries wider than the grid; four threads querying at once get what one gets; the lists for tools.
- `SpatialQuery.*`: the exact tests at their boundaries, to the millimetre.
- `AuthoredVolumes.*`: **a point inside a cave (a tunnel turned 30°, a chamber, a shaft) 200 m under the surface is
  reported inside, and the rock beside, above and between its parts is not, to the millimetre at its wall. A static
  rule area creates no Physics body**: quest areas and caves are in World's index and `PhysicsVolumes` is empty, while
  a deck that follows its ship and a wall are listed and not indexed. Also: 5,000 random points against 428 random rule
  areas give what testing each gives; an indivisible island names its 50 cells; a deck moves with its ship; kinds and
  volumes are checked; IDs are never reused.
- `SpatialDebug.*`: **cells and members come out as lines spanning them to the millimetre relative to an origin
  7,800 km out**; only what is near is drawn; a query's sphere, box or ellipsoid and what it found; volumes in their
  backend's color with an indivisible volume's bounds.
- Death tests: a member is in the index once; a static volume never moves.

Decisions taken in WORLD-2 that await the owner (also in the slice brief):
- A member is an `ECS::Entity` with a box; a member is in every cell its box touches.
- Queries are sphere, box and axis-aligned ellipsoid, exact against members' boxes, boundaries included. Results come
  in cell order, then entry order.
- A query wider than the grid walks the grid's columns.
- Volume parts are boxes, spheres and upright cylinders, a volume their union.
- A kind is a rule area unless it is solid (a static body) or follows an entity (a sensor); both at once is refused.
- World lists the volumes that need a Physics body and makes none.
- `VolumesAt` answers for rule areas only.
- The debug colors above.

Decisions taken in WORLD-1 that await the owner (also in the slice brief): cell and sector sizes are whole metres;
a world's "origin" is where its cell grid starts; "exact" is defined for local coordinates of zero or at least 1 cm
inside 8,388 km; `WorldTransform` and `Region` live here; `InstanceId` 0 means the world's own copy.

One interface note for Core (not changed here): a module other than Core cannot write GLM's identity rotation in a
public header (the public-header scan allows GLM names in Core only), so `kNoRotation` is built from four numbers. A
`Core::kIdentityRotation` would serve every module.

## Future extensions

- Activation (WORLD-3) on the index: cells near a player's ellipsoid, not members.
- Members' boxes stored beside the cells' slots (one memory read per candidate), when a query covering thousands of
  members needs it: today such a query costs about twice a scan of every member.
- Membership kept by a system (the PostPhysics phase) from `WorldTransform` and a member component, once a consumer
  (combat queries, interest) needs the runtime's own index.
- The Simulation's bridge turning sensor and solid volumes into Physics bodies; volume flags (sheltered, dark) for
  weather and light; volumes cooked with their cells (the Cooker and the RunicVolume3D node).
- More part shapes (capsules, convex hulls) and oriented query boxes, when a slice needs them.
- Scale in `WorldTransform`, when a consumer needs it.
- Region bounds and the region a position belongs to, with the authored volumes.
