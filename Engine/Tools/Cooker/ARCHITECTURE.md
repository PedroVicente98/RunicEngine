# Runic::Cooker — module contract

Group: Tools · Headless: **tool** (built with the tools, never shipped to players) · Target: `Runic::Cooker`
(`runic_cooker`) · Packages: COOK-1 to COOK-8

Source: RunicDocs › Architecture › World authoring and cooking (Engine, Game) (Ownership; Godot and Blender;
Interchange; Cooker; Cooked world files and packages; Marker nodes and what they cook into; Authoring work per
roadmap slice: the first cooker proof); Tools (Engine) › Cooker; Repositories, build targets and processes (Engine
modules); Modules, work packages and the implementation contract (COOK-1). Status: **COOK-1 implemented** (slice 9,
on the preview branch `preview/slices-3-11`): the first cooker proof, with its acceptance tests. The folder map is
[README.md](README.md); slice 9's brief and walkthrough are
[preview/slice-09-first-cooker-proof.md](../../../preview/slice-09-first-cooker-proof.md).

## Purpose

One authored truth, many runtime products. The Cooker turns what was authored in the RunicEditor into Runic's native
formats, which client and server load without ever understanding an editor scene: no scene survives cooking, and no
glTF is read twice. It runs inside the RunicEditor (GodotTools' cook reads the scenes Godot imported and hands them
over as plain data) and on build machines as the same editor without a window. COOK-1 is the first proof: an
imported mesh's visual geometry and its collision, its placement in a test cell, a collider marker; a character walks
on the result.

## Public API

Namespace `Runic::Cooker`. Headers are included by their path under `Public/`. No library types.

| Header | Holds | Since |
| --- | --- | --- |
| `CookInput.hpp` | `CookScene` (name, source, 64-bit origin, assets, placements, colliders), `InputAsset` (AssetId, content name, source, surfaces, `CollisionSource`, collision meshes), `InputMesh`, `InputPlacement` (node path, asset, pose, scale), `InputCollider` (node path, a primitive `Assets::CollisionShape`), `CollisionSource` (`None`, `VisualMesh`, `CollisionMeshes`, `ConvexHull`, `SimplifiedMesh`) | COOK-1 |
| `Cooker.hpp` | `Cook(scenes, settings)`, `CookSettings` (the output folder, the weld tolerance), `CookReport` (succeeded, problems, counts, triangles in and out, welded vertices, dropped triangles, files written, seconds); `PlacementIdOf`, `CellIdOf`, `CollisionIdOf`; `kDefaultWeldToleranceM` (0.1 mm), `kMinTriangleAreaM2`, the folder names `Meshes`, `Collision`, `Cells` | COOK-1 |

**The cook rule:** a cook checks everything it was handed first and reports every problem, each a sentence naming the
scene or the asset's source and the node or the asset (`res://Content/TestRegion/test_region.tscn: node Stray places
asset 000000000000dead, which is not among the scene's imported assets`); a wrong input cooks nothing and leaves the
previous cook as it was. A cook that can go builds every file in memory, then replaces the previous cook's `Meshes/`,
`Collision/`, `Cells/` and `catalog.txt` in the output folder, the catalog written last. Two cooks of the same input
write the same bytes.

**What each input becomes:**

- An asset: its mesh (`Meshes/<id>.rmesh`), its surfaces merged into one, normals kept, or computed area-weighted when
  any surface has none; its catalog name is its content name. Every imported asset cooks a mesh, placed or not.
- A placement of an asset with collision: the asset's collision for that scale (`Collision/<id>.rcol`), cooked once
  per asset and scale (scales equal to a thousandth share one, cooked at that thousandth), so a runtime never scales a
  shape. Its triangles: the visual mesh (`VisualMesh`) or the collision-only meshes that came with it
  (`CollisionMeshes`), scaled, welded to a 0.1 mm grid, cleaned of triangles with two corners welded or less than
  10⁻⁸ m² of area, cleared of unused vertices, then built into a Jolt mesh shape through Runic::Physics to prove Jolt
  takes them. Its name: `<asset>/collision`, or `<asset>/collision_<x>_<y>_<z>` in thousandths for another scale.
  `ConvexHull` and `SimplifiedMesh` are reported as not cooked yet.
- A scene: one cell around its origin (`Cells/<id>.rcell`, named `<scene>/cell_0_0_0`): each placement with its stable
  ID, mesh, collision and pose relative to the origin, each collider with its stable ID and shape.

**The IDs:** an asset's AssetId is the one it came with (Godot's UID, mapped by GodotTools). What has none of its own
gets 64-bit FNV-1a of a text, never 0: a placement or a collider from its scene and node path (the same rock, the same
ID, cook after cook, on server and client), a cell from its scene, a collision from its asset and scale.

## Owns

- Cook orchestration (COOK-1: full cooks; incremental cooking is COOK-8's), the cook's checks and report.
- Collision cooking: welding, cleaning, Jolt's acceptance (meshoptimizer's simplification when it comes).
- Later: validation of data-driven definitions (COOK-2), world cooking into regions, sectors, sparse 3D cells,
  volumes, spawn areas and packages (COOK-3), client packages (COOK-4), skinned meshes (COOK-5), navigation tiles
  (COOK-6), water fragments (COOK-7).

## Does not own

- Reading Godot: GodotTools' cook (GDT-4) walks the imported scenes, the import settings' collision presets, the
  collision naming convention and the RunicCollider3D markers, and fills `CookScene`.
- The formats and their writers (Runic::Assets, ASSET-1): the Cooker links them.
- Loading cooked data at runtime (Runic::Assets loads; turning a cell into bodies is WORLD-3's and COOK-3's, slice 21;
  slice 9's programs do it themselves).

## Allowed dependencies

Catalog row (ceiling): Core, Content, Assets, World, Physics, Navigation, Water; third parties Jolt, Recast,
meshoptimizer, Zstandard. COOK-1 links Runic::Core, Runic::Content and Runic::Assets (public: their types are in the
input and the report) and Runic::Physics (private: Jolt's check goes through it, so the Cooker includes no Jolt header).
meshoptimizer, Recast and Zstandard wait for their packages.

## Forbidden dependencies

Godot and godot-cpp (the Cooker is plain C++: GodotTools reads Godot); any shipped executable linking the Cooker; the
Platform; game code.

## Threading and tick model

A cook runs on the caller's thread (the editor's main thread in GDT-4, or a build machine's) and never touches a
simulation. It is not reentrant into the same output folder.

## Data ownership

The cook owns its report and, while it runs, the files it builds; the output folder's cooked files are replaced by
each full cook. The input belongs to the caller.

## Integration points

- GodotTools' cook (GDT-4) fills `CookScene`s from the scenes Godot imported and calls `Cook`; the RunicEditor's
  `--runic-cook` and the export plugin are its entry points.
- Client and server read the output with `Assets::AssetCache` (the catalog, then cells, then collisions).

## Where the code lives

- `Public/CookInput.hpp`, `Public/Cooker.hpp`.
- `Source/Orchestration/Cooker.cpp`; `Source/Inputs/InputChecks.hpp`/`.cpp`; `Source/Collision/CollisionCooking.hpp`/
  `.cpp`.
- `Tests/CookerTests.cpp`: the tests link Runic::Physics to walk on what they cooked.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| COOK-1 | First cooker proof: geometry read from a scene Godot imported (no second glTF importer) into Jolt collision and a cell visual list | A Blender mesh renders through Godot and is walkable | **Implemented** (slice 9, preview branch): `RunicCookerTests` passes with the 7 tests below; with GDT-4, the RunicEditor cooks a glTF Godot imported and a character walks on it (see GodotTools and the slice brief); awaiting the owner's review |
| COOK-2 to COOK-8 | Validation of definitions, world cooking, client export, skinned meshes, navigation tiles, water, incremental cooking at scale | See RunicDocs | Not started |

COOK-1's acceptance tests (`RunicCookerTests`): `Cooker.*` (**a scene cooks into a mesh, a collision and a cell that
Runic::Assets reads back**, with the cell's 64-bit origin and the placement's stable ID; **a character walks on the
cooked collision 8,600 km out and stops at the collider**; collision meshes under the convention replace the visual
mesh; welding and degenerate triangles; a scaled placement gets its own collision and equal scales share one; IDs and
bytes stable from cook to cook; **a wrong scene cooks nothing and says what and where**: an unknown asset, a flat
scale, a position that is not finite, a collider without size, a convex preset, a second asset with the same ID, an
asset without triangles). "Renders through Godot": GDT-4's tests and CTests, where Godot imports the glTF and draws it.

Decisions taken in COOK-1 that await the owner (also in the slice brief): GodotTools hands the scenes over as plain
data (the Cooker links no Godot); collision is the visual mesh or the convention's collision-only meshes, convex hulls
and simplified meshes reported as not yet; collision per asset and scale, scales sharing to a thousandth; welding to a
0.1 mm grid and dropping triangles under 10⁻⁸ m²; Jolt's acceptance through Physics; one cell per scene until COOK-3's
64 m cells; a full cook replaces the previous one and a wrong input writes nothing; IDs of placements, cells and
collisions from FNV-1a of their names; mirrored (negative) scales refused.

Costs, measured (release, `-O2`): cooking a 130,050-triangle asset placed 100 times (its mesh, one collision welded,
cleaned and built by Jolt, the cell, the catalog) 108 ms.

## Future extensions

- COOK-3: regions, sectors, sparse 3D cells of 64 m, volumes, spawn areas, packages with Zstandard, client cell
  visual lists apart from server cells, collision near the player for client prediction.
- Convex hulls (when Physics has a convex shape) and simplified collision meshes with meshoptimizer.
- Jolt's own binary shapes in the collision files, once Physics can read them.
- COOK-8: incremental cooking of only the cells an edit touches.
