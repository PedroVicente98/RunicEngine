# Slice 11 · Spatial index

**Runnable result:** 3D cells, volumes and membership drawn; queries visualized.
**Packages:** WORLD-2 (Runic::World), and the RunicClient scene that shows it.
**Status:** done on the preview branch. New tests: 21 in `RunicWorldTests`, plus the CTest
`godot.runic_client_spatial`. The Engine's debug build runs 532 tests and the headless build 420, all passing.

```sh
cmake --preset headless && cmake --build --preset headless && ctest --preset headless -R RunicWorld
cmake --preset debug && cmake --build --preset debug && ctest --preset debug -R "runic_client_spatial|World"
./build/debug/bin/RunicClient --spatial-scene                              # the scene in a window
./build/debug/bin/RunicClient --headless --quit-after 240 --spatial-scene  # what the CTest runs
```

The CTest's run prints, among its other lines:

```text
RUNIC: debug view stopped at frame 240: 5312 line vertices, 0 triangle vertices, 1 texts in 2 surfaces
RUNIC: spatial scene: 150 walkers in 46 cells of 16 columns (56 cell changes), 50 queries matched testing every walker (53 found, 0 differed), 50 walker positions inside the cave, volumes 4 (3 rule areas, 1 need a Physics body, 1 indivisible)
```

**What you see in the window:** the camera circles a 200 m box, 6,400 km from the world's origin.
- **Cells.** The occupied 64 m cells are dim gray boxes, stacked in columns from a cave below the ground to an
  island above it. Only cells that hold a walker exist.
- **Walkers.** 150 small orange boxes drift, climb and dive. As they cross a boundary, their old cell disappears and
  the new one appears.
- **The query.** A magenta sphere, box or ellipsoid (the shape changes every 2 s) sweeps a circle while rising from
  the cave to the island. The walkers it found are drawn in magenta over it, with a label (`ellipsoid: 3 found`).
- **The volumes:**
  - the cave (a tunnel turned 25° and a round chamber) and a quest area, as orange outlines: rule areas;
  - the island, a cylinder in the sky with its bounds in white: it is indivisible;
  - a ship's deck circling the scene in cyan: a sensor that follows its ship.

## What was built

| Package | What | Where |
| --- | --- | --- |
| WORLD-2 | **The spatial index.** Entities with boxes sit in a sparse grid of XZ columns, each holding only its occupied Y cells; a member is in every cell its box touches. Membership updates change only the cells left and entered. Sphere, box and ellipsoid queries are exact in 64-bit, return each member once, in cell order, and only read the index | `Engine/SimulationCore/World`: `Public/SpatialIndex.hpp`, `Source/SpatialIndex/SparseCells.hpp`/`.cpp`, `SpatialIndex.cpp` |
| WORLD-2 | **Authored volumes.** Unions of boxes, spheres and cylinders at a 64-bit origin, with an exact point test. The kind picks the backend: a rule area is in World's own grid; a sensor or solid volume is listed for the physics world's owner. Indivisible volumes name their cells | `Public/AuthoredVolumes.hpp`, `Source/Volumes/VolumeShapes.hpp`, `AuthoredVolumes.cpp` |
| WORLD-2 | **The debug view.** Cells, members, a query with what it found, and volumes, as camera-relative lines in Core debug geometry | `Public/SpatialDebug.hpp`, `Source/SpatialIndex/SpatialDebug.cpp`, `Source/Volumes/VolumeDebug.cpp` |
| (program) | `RunicClient --spatial-scene` and its CTest | `Engine/Programs/RunicClient/SpatialScene.cpp`, `Main.cpp`, `CMakeLists.txt` |

## The order to read it in

1. **`World/Public/SpatialIndex.hpp`**: the opening comment and `SpatialQuery::Overlaps`'s comment state what
   "exact" means.
2. **`Source/SpatialIndex/SparseCells.hpp`**: the grid. `ForEachIn` is the whole query walk, including the switch to
   the grid's own columns for a range wider than the grid.
3. **`Source/SpatialIndex/SpatialIndex.cpp`**: `SpatialQuery::Overlaps`, about 30 lines. Then `SpatialIndex::Query`,
   about 15 lines; its dedup line is why no query keeps state. Then `Update`: only the cells left and entered.
4. **`World/Public/AuthoredVolumes.hpp`**: the opening comment, the three backends, then
   `Source/Volumes/AuthoredVolumes.cpp`'s `InsideLocked` and `VolumesAt`.
5. **`Programs/RunicClient/SpatialScene.cpp`**: `UpdateMembership` and `RunQuery` show a runtime using both, in the
   phases RunicDocs gives them.

## One query, traced

The scene's Queries phase runs an ellipsoid 40 m wide and 12 m tall, centered 70 m above the ground.

1. `CellsTouched` turns the ellipsoid's box (±40, ±12, ±40 m around its center) into a range of cells: 2 or 3 along
   x and z, and 1 or 2 along y, depending on where the box falls on the 64 m grid.
2. `SparseCells::ForEachIn` sees that the range has fewer columns than the grid (16), so it looks up each of the
   range's columns. In each column it binary-searches to the first Y cell at or above the range's bottom, then walks
   up while y is inside the range.
3. For each slot in a cell, `Query` finds the member's cell box. If this cell is not the member's first cell inside
   the range (a big member sits in several), it skips it. Otherwise it applies `Overlaps`: the offset from the
   ellipsoid's center to the walker, in 64-bit, each axis divided by its radius, summed squared, against 1.
4. The members found come in cell order. The scene's check then tests all 150 walkers with the same `Overlaps` and
   compares the two sorted lists: they have been equal in every query run so far.
5. The extractor draws the ellipsoid (three ellipses of 24 segments) and each walker found (a 0.5 m box) in the
   overlay batch, relative to the camera.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `SpatialIndex.*`, `SpatialQuery.*` | 9 | **Query results match brute force in 3D.** The setup: 5,000 points and boxes 7,800 km out, spread from 800 m deep to 1,500 m high, a fifth of them in many cells. The queries: 1,800 random spheres, boxes and ellipsoids, before and after the members move, leave and come back. Every result equals testing every member, each member once. Also tested: columns hold only occupied cells; a 500 m ship in 81 cells is found once; a move inside its cells changes none; results come in cell order, also for queries wider than the grid; four threads querying at once agree with one; the exact tests at their boundaries |
| `AuthoredVolumes.*` | 6 | **A point inside a cave volume is reported inside it.** The cave is a turned tunnel, a chamber and a shaft 200 m down. The rock beside, between and above them is outside, to the millimetre at the tunnel's wall. **A static rule area creates no Physics body.** Caves and quest areas are in World's index and nothing is listed for physics; a ship's deck and a wall are listed and not indexed. Also tested: 5,000 random points against 428 rule areas match brute force; an island names its 50 cells; a deck follows its ship; bad kinds and volumes are refused; IDs are never reused |
| `SpatialDebug.*` | 4 | **...and draw in the debug view.** Cells and members come out as lines spanning them to the millimetre relative to an origin 7,800 km out; only what is near is drawn; a query's shape and what it found; volumes in their backend's color |
| `SpatialIndexDeathTest.*`, `AuthoredVolumesDeathTest.*` | 2 | A member is in the index once; a static volume never moves |
| `godot.runic_client_spatial` | 1 | The runnable result in the client: membership updates as walkers move, every query matches testing every walker, walkers pass through the cave, four volumes in their backends |

`RunicWorldTests` passes under ThreadSanitizer (the four-thread query test is the one that matters there),
AddressSanitizer with UBSan, clang and Release. Every new source file compiles cleanly with GCC under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion`.

## What it costs

Release, one thread, on the 2-vCPU build machine. The setup: 100,000 members over 4 km × 4 km × 200 m (a fiftieth of
them 40 m boxes), in 15,920 cells of 4,096 columns.

| Operation | Cost |
| --- | --- |
| Insert / remove a member | 250–540 ns / 170–240 ns |
| Update a member that moved 10 cm (a tick's walk; 0.2 % crossed a cell) | 33–43 ns |
| Update a member across a cell boundary | 350–650 ns |
| A 5 m sphere (a melee check) | 0.6–1.1 µs |
| A 20 m sphere (1 found) | 1.1–2.3 µs |
| A 100 m sphere (106 found) | 12–24 µs |
| A 1 km sphere (15,200 found) / a 1,000 × 200 m ellipsoid (13,600 found) | 590–920 µs / 620–970 µs |
| Testing every one of the 100,000 members (brute force) | 330–400 µs |
| The debug view within 200 m (155 cells, 657 members, 9,744 lines) | 400–640 µs |
| A point against 10,000 volumes (`VolumesAt`, 0.8 inside on average) | 0.6–0.9 µs |

The index beats brute force 150 to 350 times over for the small queries combat and AI make. For a query that covers a large
part of the members (the 1 km sphere finds 15 % of them), it costs about twice a plain scan, because each candidate
is a memory read through its slot. That is the shape activation and interest queries will have. WORLD-3 (activation)
will ask for cells, not members, and the members' boxes could be stored beside the cells' slots when a consumer needs
it (in the contract's future extensions).

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | A member is an `ECS::Entity` with a box (a 64-bit center, 32-bit half sizes); it is in every cell its box touches | RunicDocs: "large objects register in every cell their bounds touch, deduplicated on query" | Members by a plain number; large objects in one cell with an overflow list |
| D2 | Queries are a sphere, a box and an axis-aligned ellipsoid (radii on x, y, z), exact against members' boxes, boundaries included | The Done when's three shapes; an activation ellipsoid is wider than tall, not turned | Oriented boxes and turned ellipsoids now |
| D3 | Results in cell order (x, z, y), then entry order; a member in several cells reported from the first one inside the query's range | A fixed order with no per-query state, so queries run on many threads | Sorted by entity; a visited stamp per member |
| D4 | A query wider than the grid walks the grid's columns instead of the range's | A 3,000 km query would otherwise look up billions of columns | Cap the query size |
| D5 | Membership is the caller's to update; the scene does it in PostPhysics, the runtime has no index of its own yet | Who needs the runtime's index (combat, interest) is a later package's | A Simulation-owned index updated from every `WorldTransform` |
| D6 | Volume parts are boxes, spheres and upright cylinders; a volume is their union, placed at a 64-bit origin with a rotation | Enough for caves (tunnels and chambers), areas and islands | Convex hulls or meshes |
| D7 | A kind is a rule area unless it is solid (a static body) or follows an entity (a sensor); both at once is refused | RunicDocs' table names three backends and no moving solid | A kinematic body for moving solids |
| D8 | World lists the volumes that need a Physics body and makes none; the Simulation's bridge will make them | World cannot link Physics (Physics may depend on World) | Physics reads World's volumes |
| D9 | `VolumesAt` answers for rule areas only; a sensor's or a solid's point test is `IsInside` | Sensors and solids are found by Physics contacts and queries | All backends in `VolumesAt` |
| D10 | Volume IDs are a slot and its generation; kinds are numbered in the order they were added | Streaming adds and removes volumes for ever | Monotonic IDs |
| D11 | The debug colors: cells dim gray, members orange, queries magenta, rule areas orange, sensors cyan, solids blue, indivisible bounds white | RunicDocs' palette | Colors by kind (the Game's) |

## Notes for other packages and for the plan

- **No interface change was needed.** World uses Core's RenderData, GLM and ECS's `Entity` through its catalog row.
- **Volumes are not cooked yet.** RunicDocs puts them in the RunicEditor as RunicVolume3D nodes, cooked with their
  cells. The node, its gizmo and the Cooker's support belong to GodotTools and the Cooker (COOK-3 or a GDT package);
  `VolumeDesc` is what they will produce.
- **The physics side of sensor and solid volumes** needs a Simulation package: the bridge turns `PhysicsVolumes()`
  into a sensor or static body and moves sensors with their entity's transform. Physics' sensor bodies already report
  contacts.
- **In headless runs the client's frames outpace the clock.** The 240 frames of the CTest ran 99 master ticks, so 50
  realtime queries. The test only asks for "at least one", so it does not depend on the pace.
