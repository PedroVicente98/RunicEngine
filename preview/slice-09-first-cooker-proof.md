# Slice 9 · First cooker proof

**Runnable result:** a mesh made in Blender is imported by Godot, its collision is cooked by the Runic Cooker, and the
mesh is drawn and walked on.
**Packages:** CNT-1 (Runic::Content, new), ASSET-1 (Runic::Assets, new), COOK-1 (Runic::Cooker, new), GDT-4
(Runic::GodotTools), and the RunicEditor and RunicServer changes that show them. One follow-up rides along as its
own commit: GDT-4's menu cooks the edited scene while a project has no world scene.
**Status:** done on the preview branch. New tests: 9 in `RunicContentTests`, 11 in `RunicAssetsTests`, 7 in
`RunicCookerTests` and 4 in `RunicGodotToolsTests`, plus three new CTests. The Engine's debug build runs 497 tests and
the headless build 386, all passing.

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
ctest --preset debug -R "editor_cook|server_walks_cooked|EditorCook|Cooker|Assets|Content"
# By hand, on the project the CTests made (godot/NewProjectCheck, with the test region in Content/TestRegion):
./build/debug/bin/RunicEditor --editor --path build/debug/godot/NewProjectCheck   # open test_region.tscn, cook it
./build/debug/bin/RunicServer --quit-after 480 --cooked build/debug/godot/NewProjectCheck/Build/Cooked --cooked-spawn -12,0,0
python3 tests/godot/fixtures/test_region/make_test_region.py out.glb             # the "Blender" export
```

The editor's cook without a window, then the server's walk, print among their other lines:

```text
RUNIC: cook succeeded in 3 ms: 4 meshes, 4 collisions, 1 cells, 4 placements, 1 colliders, 44 collision triangles (60 welded vertices, 0 dropped triangles) into .../NewProjectCheck/Build/Cooked
RUNIC: the local world loaded the cook: 1 cells, 5 bodies (0 replaced)
RUNIC: RunicEditor exited cleanly after 253 frames
RUNIC: cooked walk: loaded 1 cells and 5 bodies from .../NewProjectCheck/Build/Cooked
RUNIC: cooked walk: 1 cells, 5 bodies from .../Build/Cooked; the character walked 15.6 m on cell game:test_region/test_region/cell_0_0_0 and ends at (3.58, 1.00, 0.00) from its origin, on the ground
```

**What you see in the editor:** open `Content/TestRegion/test_region.tscn`. Godot draws the imported glTF: a 30 m
ground, a ramp climbing 1 m over 4 m, a 10 m platform and a rock on it. The wall (a RunicCollider3D) shows as a
light-blue wireframe box standing on the platform. The rock's collision box, `Rock-rcol`, is in the file but hidden.
**Project › Tools › Runic: cook the project** cooks the scene and loads the result into the editor's local world.
The Runic Physics dock's "see what the server sees" layer then draws the cooked collision over the meshes: the
ground, ramp and platform as their own triangles, the rock as its simple box, and the wall. The Import dock of
`test_region.glb` has **Runic › Collision**, which sets the file's preset.

## What was built

| Package | What | Where |
| --- | --- | --- |
| CNT-1 | Content IDs (`engine:` and `game:`, names of lowercase parts between `/`), parsed with the first fault and its position, hashed with 64-bit FNV-1a of their text; registries that keep each definition's source and report every duplicate, unknown and malformed ID with it | `Engine/Foundation/Content`: `Public/ContentId.hpp`, `ContentRegistry.hpp`, `Source/ContentIds`, `Source/Registries` |
| ASSET-1 | The native cooked formats (a 24-byte header with a version, a kind and a checksum; an asset's mesh, an asset's collision shapes, a cell's placements and colliders around a 64-bit origin), readers that check every byte, a text catalog with its problems' lines, handles, a cache that loads once and collects | `Engine/Foundation/Assets`: `Public/AssetHandle.hpp`, `CookedFormats.hpp`, `AssetCatalog.hpp`, `AssetCache.hpp`, `Source/Handles`, `Source/Cache`, `Source/CookedFormats`, `Tests/Data/golden_square.rmesh` |
| COOK-1 | The Cooker: scenes as plain data (assets, placements, colliders, an origin) into meshes, collisions per placement scale (welded, cleaned, built by Jolt through Physics), a cell per scene, a catalog; every problem named by scene, asset and node, and a wrong input writes nothing | `Engine/Tools/Cooker`: `Public/CookInput.hpp`, `Cooker.hpp`, `Source/Orchestration`, `Source/Inputs`, `Source/Collision` |
| GDT-4 | The editor's side: the scene reader, the import plugin's collision preset, the `-rcol` naming convention, RunicCollider3D and its gizmo, the cook from the menu, before each export and with `--runic-cook`, the cook loaded into the local world | `Engine/Tools/GodotTools`: `Public/EditorCook.hpp`, `Source/Export`, `Source/MarkerNodes`, the plugin; the template's `Content/README.md` |
| (programs, fixture) | RunicEditor `--runic-cook` and `--runic-cook-scene=`, the cook's exit code; RunicServer `--cooked` and `--cooked-spawn`; the test region (a glTF standing in for a Blender export, its generator, a scene placing it); three CTests | `Engine/Programs/RunicEditor/Main.cpp`, `CMakeLists.txt`; `Engine/Programs/RunicServer/CookedWalk.cpp`, `Main.cpp`; `tests/godot/fixtures/test_region`, `tests/godot/CopyIntoFolder.cmake` |

## The order to read it in

1. **`Content/Public/ContentRegistry.hpp`**: the opening comment states the rule (report everything, with sources);
   the registry is one template of about 80 lines.
2. **`Assets/Public/CookedFormats.hpp`**: what a cooked file holds. Then `Source/CookedFormats/CookedFormats.cpp`'s
   `Open` (the header's checks) and `ReadShape` (what "checks every byte" means).
3. **`Cooker/Public/CookInput.hpp`** and **`Cooker.hpp`**: what the Cooker takes and gives. Then
   `Source/Orchestration/Cooker.cpp`'s `Cook`, which checks, builds in memory, then writes, and
   `Source/Collision/CollisionCooking.cpp`'s `CleanCollision`.
4. **`GodotTools/Public/EditorCook.hpp`**: the convention and the presets in its opening comment. Then
   `Source/Export/SceneReader.cpp`'s `SceneWalk` (assets, placements, the `-rcol` owner) and `ReadMesh` (the winding).
5. **`Programs/RunicServer/CookedWalk.cpp`**: what a runtime does with a cooked folder, in one function.

## One rock, traced

The artist exports `Rock` and its simple collision `Rock-rcol` from Blender. The test's generator writes the same
glTF.

1. Godot imports the glTF. The import plugin's `_post_process` puts the file's preset on every mesh node
   (`runic_collision`), and marks and hides `Rock-rcol` (`runic_collision_only`). Godot saves the imported scene.
2. The cook (the menu, an export or `--runic-cook`) loads `test_region.tscn` afresh and walks it.
   `Geometry/Rock` is a MeshInstance3D from the instanced glTF. Its asset is that file plus the path `Rock`, so its
   AssetId is FNV-1a of the file's Godot UID and `Rock`, and its name is `game:test_region/test_region/rock`.
   Its triangles are read and turned counter-clockwise. Its child `Rock-rcol` is collision only, and its owner is
   `Rock`, so its triangles join the rock's collision meshes, in the rock's space. The asset becomes
   `CollisionMeshes`. The placement is `Geometry/Rock` at (2, 1, 3) relative to the scene's root.
3. The Cooker checks everything first. It merges the rock's surfaces into its mesh file. The placement's scale is
   1, so the rock's collision is `<rock>/collision`: the `-rcol` box, welded (24 corners into 8) and handed to Jolt,
   which takes it. The cell gets the placement: a stable ID from `game:test_region/test_region` and `Geometry/Rock`,
   the rock's mesh and collision IDs, and its pose. The catalog lists all nine files. All of it is written to
   `Build/Cooked` with Assets' writers.
4. The editor loads the cook into its local world: a static body for the rock's box at the cell's origin plus
   (2, 1, 3). The server view draws the box over the rock. The RunicServer does the same in its own world, and a
   character walking past the rock collides with the box, not with the visual mesh.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `ContentId.*`, `ContentRegistry.*`, `ContentSource.*` (CNT-1) | 9 | **Duplicate and unknown IDs are reported with their source**: a duplicate with both sources (the first kept), an unknown ID with where it was referenced, a malformed one with where it was written and the fault's position; the hash's fixed values; definitions keep their address |
| `CookedFormats.*`, `AssetCatalog.*`, `AssetCache.*` (ASSET-1) | 11 | **A cooked test mesh loads on client and server**: the same tests pass in the headless build (the server's) and the debug build (the client's). The golden mesh file still reads and is written byte for byte. Every kind round-trips, a cell's origin to the last bit. A damaged file says what is wrong. The catalog's problems come with their lines. The cache loads once, shares, collects, says why a load failed, and is safe from four threads |
| `Cooker.*` (COOK-1) | 7 | **Walkable**: a character walks on cooked collision 8,600 km out and stops at the collider. The cooked files read back through Assets. The naming convention, welding, scales, and stable IDs and bytes. A wrong scene cooks nothing and says what and where |
| `EditorCook.*` (GDT-4, Godot in the test process) | 4 | **A Blender mesh imported by Godot cooks into collision and is walkable**: the reader takes what Godot imported (the convention, the marker, counter-clockwise triangles, stable IDs, the hidden collision mesh). The cook loads into the local world, where a character walks up the ramp to the wall. The server view's lines span the imported ground mesh to the millimetre. The import preset decides; content names are in snake case |
| `godot.editor_cooks_test_region`, `godot.server_walks_cooked_region` (with `godot.editor_cook_fixture`) | 3 | **Renders through Godot, cooks, walkable on the server**: the RunicEditor imports and cooks the test region without a window. The RunicServer loads the cook and walks a character from the ground up the ramp and onto the platform, stopping 0.32 m from the wall |

Content and Assets pass under AddressSanitizer with UBSan, ThreadSanitizer, clang and Release. So do the Cooker's
tests, built against the sanitizer libraries, and with clang. Every new source file compiles cleanly with GCC under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion`.

## What it costs

Release unless said, one thread, on the 2-core build machine.

| Operation | Cost |
| --- | --- |
| A content ID: parse / lookup by text / lookup by ID / registry add | 46 ns / 51 ns / 26 ns / 241 ns (10,000 definitions) |
| A cooked mesh of 130,050 triangles with normals (3.1 MB) | write 7.4 ms, read and check 4.2 ms (750 MB/s), first load through the cache 4.9 ms, a cache hit 7 ns |
| Cooking that mesh placed 100 times (welding, Jolt's build, the files) | 108 ms |
| The editor's cook of the test region, read from Godot (debug build) | 3 ms |

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | Two namespaces, `engine` and `game`; names of lowercase letters, digits and `_`, with `/` between parts, at most 128 characters | Readable in files, safe in paths, one rule everywhere | More namespaces; mixed case |
| D2 | 64-bit FNV-1a of the text as a content ID's hash, written into cooked files; placement, cell and collision IDs from it too | Stable across processes and machines without a table | Interned IDs per process; random IDs stored in the scenes |
| D3 | A duplicate keeps the first definition; `Find` reports nothing and `Resolve` reports | A reference is where a typo is noticed | The last wins |
| D4 | Loose files and a text catalog until ASSET-2's packages; one header with a version, a kind and a checksum | The proof needs no packages; a person can read the catalog and a diff shows a cook | Packages now |
| D5 | Collision stored as shapes and triangles, not Jolt's binary shapes; Physics builds the mesh shape at load | Physics cannot read Jolt's binary state (an interface change); for these sizes the build is fast | Jolt's binary shapes, with a Physics package that reads them |
| D6 | One collision per asset and scale (scales equal to a thousandth share), so a runtime never scales a shape; negative scales refused | Jolt's scaled shapes and mirrored triangles are not in Physics | Scaled shapes at runtime |
| D7 | One cell per scene, one file for client and server, until COOK-3's 64 m cells and client/server split | The first proof is one test cell | Cells now |
| D8 | GodotTools hands the Cooker plain data; the Cooker links no Godot | No second importer, and the Cooker runs on build machines | The Cooker reads Godot resources |
| D9 | Collision is the mesh itself or the convention's meshes; convex hulls and simplified meshes are reported as not cooked yet | Physics has no convex shape and meshoptimizer is not fetched yet | Fall back silently |
| D10 | Welding to a 0.1 mm grid, dropping triangles under 10⁻⁸ m² | Exporters split vertices at seams; Jolt would drop the slivers silently | Exact welding only |
| D11 | A full cook replaces the previous one; a wrong input writes nothing | A cooked folder is always one consistent cook | Write what is valid |
| D12 | The collision preset is per file (the Import dock's global options), not per mesh node | One option a file is enough for the proof; per node needs the Advanced Import Settings | Per node now |
| D13 | The naming convention is `-rcol` (and `-rcol` followed by anything), for a parent or a sibling, and wins over the file's preset | Godot's `-col` makes Godot physics bodies, which are not in the build; an artist's collision mesh is explicit | `UCX_` prefixes; Godot's suffixes |
| D14 | A mesh's AssetId hashes its file's Godot UID and its node path; its name comes from the path under the content folder, in snake case | Godot gives UIDs to files, not to the meshes inside them | Extract every mesh to its own file |
| D15 | The menu and the export cook the world scene (or the edited scene while there is none); `--runic-cook-scene=` names others; `--runic-cook` waits for the imports and a second more | Matches the plan's World.tscn; build machines name what they cook | Cook every scene in Content |
| D16 | The editor loads each cook into its local world | The server view then shows the cooked collision, as RunicDocs' editor loop does | A separate "preview" world |
| D17 | The RunicClient does not draw cooked meshes yet; the mesh renders through Godot in the editor | Drawing cooked meshes in the client is GDB-5's (slice 22) | A debug-mesh path in the client now |

## Notes for other packages and for the plan

- **Two interface changes reported, not made.** First, GodotBridge's GodotLayer stops the application as a clean exit
  when Godot quits, whatever Godot's exit code. The RunicEditor passes the cook's code itself (`cookFinished` and a
  layer of its own); a GodotLayer that hands Godot's quit code to the application would make that unneeded. Second,
  Physics has no way to read Jolt's binary shapes, so collision is rebuilt from triangles at load (D5).
- **No runtime module may link both Assets and Physics.** Simulation's catalog row has no Assets. So loading a cooked
  cell into a world is written three times in slice 9: the editor's `LoadCookedCells`, the RunicServer's cooked walk,
  and the Cooker's tests. WORLD-3 and COOK-3 (slice 21) should give it one owner, likely a world-streaming module or
  an Assets row for Simulation.
- **meshoptimizer, Recast and Zstandard are not fetched yet.** This session has no network for downloads, and none
  of the three is needed before simplification (later), navigation (COOK-6) and packages (ASSET-2). Their pins go in
  `cmake/Dependencies.cmake` with the packages that first link them.
- **The editor tests print "Can't open dynamic library"** for scratch projects without a built editor library, and
  Godot's headless editor prints `Index p_which = 0 is out of bounds` at every start. Both are harmless and were
  already there.
- **The Blender export is generated.** No Blender runs here, so `make_test_region.py` writes the glTF that Blender's
  exporter writes for the same objects. A real Blender file is the owner's next check: export a mesh with a `-rcol`
  child, drop it in `Content/`, and cook.
