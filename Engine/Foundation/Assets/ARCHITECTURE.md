# Runic::Assets — module contract

Group: Foundation · Headless: **yes** · Target: `Runic::Assets` (`runic_assets`) · Packages: ASSET-1, ASSET-2

Source: RunicDocs › Architecture › Repositories, build targets and processes (Engine modules: "AssetId, handles, cache,
native cooked formats, random-access packages with Zstandard decompression, manifests, client and server package
selection, streaming"); World authoring and cooking (Godot and Blender: identity; Cooked world files and packages);
Render data (stable references); Modules, work packages and the implementation contract (ASSET-1, ASSET-2). Status:
**ASSET-1 implemented** (slice 9, on the preview branch `preview/slices-3-11`): handles, the catalog, the cache and the
native cooked mesh, collision and cell formats, with their acceptance tests. The folder map is
[README.md](README.md); slice 9's brief and walkthrough are
[preview/slice-09-first-cooker-proof.md](../../../preview/slice-09-first-cooker-proof.md).

## Purpose

What the Cooker writes, client and server read, with the same code. No scene and no glTF survives cooking: a runtime
loads Runic's own files by their `Core::AssetId` (the Cooker maps Godot's stable UID to it) or by their content name,
checks every byte before it trusts it, and shares one copy among everything that holds it. ASSET-1 is the first proof:
an asset's mesh, an asset's collision and a cell that places them, in loose files with a text catalog; packages,
compression and streaming are ASSET-2's (slice 21).

## Public API

Namespace `Runic::Assets`. Headers are included by their path under `Public/`. No library types.

| Header | Holds | Since |
| --- | --- | --- |
| `AssetHandle.hpp` | `AssetKind` (`Mesh`, `Collision`, `Cell`), `kAssetKindCount`, `AssetKindName`, `ParseAssetKind`; `AssetHandle<T>` (`Id`, `IsLoaded`, `Get`, `->`, `*`; equal when they share a load) | ASSET-1 |
| `CookedFormats.hpp` | `CookedMesh` (positions, normals, indices, bounds), `CookedCollision` (`CollisionMesh`es and `CollisionShape`s: box, sphere, capsule, mesh, at a pose), `CookedCell` (a 64-bit origin, `CellPlacement`s with a stable ID, a mesh, a collision and a pose and scale, `CellCollider`s); `WriteCookedMesh`/`Collision`/`Cell`, `ReadCookedMesh`/`Collision`/`Cell`, `PeekCookedKind`, `WriteBytesToFile`, `ReadBytesFromFile`; `kCookedFormatVersion` (1), `kCookedHeaderBytes` (24), `kCookedMagic`, the extensions `.rmesh`, `.rcol`, `.rcell`, `kUnitQuaternionTolerance` | ASSET-1 |
| `AssetCatalog.hpp` | `AssetCatalog` (`Add`, `Find` by ID or name, `Count`, `Entries`, `Problems`, `Write`, `Read`), `AssetEntry` (ID, kind, content name, file), `AssetIdText`, `ParseAssetId`; `kCatalogFileName`, `kCatalogHeader` | ASSET-1 |
| `AssetCache.hpp` | `AssetCache` (`Open`, `Catalog`, `Root`, `LoadMesh`/`LoadCollision`/`LoadCell` by ID or name, `Collect`, `Stats`), `AssetCacheStats` | ASSET-1 |

**The file rule:** a 24-byte header (the magic `RUNA`, the format version, the kind, the payload's size and its 64-bit
FNV-1a checksum), then the payload; every number little-endian, floats IEEE-754, so a file cooked on one machine reads
on any other. A reader checks the header, the size, the checksum, then the content: counts fit the payload, indices
stay inside their vertices and are whole triangles, numbers are finite, rotations are unit quaternions, sizes are
positive, a mesh shape names a mesh that exists, a cell's colliders are not meshes, nothing follows the payload. Its
error says what is wrong and where (`triangle index 9 at 4 is past the 4 vertices`). Files are written to a temporary
name and renamed, so a reader never sees half of one.

**The formats (version 1):** a mesh is its counts, bounds, positions, normals (one a vertex, or none) and triangle
indices, in the asset's own space. A collision is its meshes (positions and indices) and its shapes (kind, half
extents, radius, half height, mesh, position, rotation), in the asset's own space: the same file is the server's
authority and the client's prediction. A cell is a world origin in doubles, its placements (a stable 64-bit ID, a mesh
AssetId, a collision AssetId cooked for this placement's scale or none, a position, rotation and scale relative to the
origin) and its colliders (an ID and a primitive shape relative to the origin). A runtime never scales a shape: the
Cooker cooks a collision per scale.

**The catalog rule:** `catalog.txt` in the cooked folder, text, a header line `runic-assets 1` and one asset a line
(kind, 16-digit hex ID, content name, file relative to the folder, separated by tabs; `#` starts a comment). Reading it
collects every problem with its line: a duplicate name or ID (the first stays, both lines named), an unknown kind, a
malformed ID or name, a line with too few columns.

**The cache rule:** `Open` reads the catalog (an error lists its problems; what read cleanly is held). A load by ID or
name reads the file the first time and shares that copy afterwards; asking for another kind than the catalog says, an
asset not in the catalog, a missing or damaged file fail with the asset's ID, name and file. `Collect` frees what no
handle holds. One lock guards the cache and a load reads under it.

## Owns

- The native cooked formats, their readers and their writers (the Cooker links the writers: "the Cooker links each
  runtime module's writer").
- The catalog of a cooked folder; handles; the cache.
- Later (ASSET-2, slice 21): random-access packages and manifests with Zstandard, client and server package
  selection, asynchronous streaming.

## Does not own

- What is cooked and how (Runic::Cooker); turning a cooked collision into bodies (Runic::Physics builds shapes from
  triangles and primitives; who loads a cell into a world is WORLD-3's and COOK-3's, slice 21; slice 9's programs do
  it themselves); drawing a mesh (GDB-5, slice 22).
- Godot's resources: the client's textures, materials and final meshes come from Godot's .pck (COOK-4).

## Allowed dependencies

Catalog row (ceiling): Core, Content; third party Zstandard. ASSET-1 links Runic::Core (public: AssetId, Result,
math) and Runic::Content (public: content names and sources in the catalog). Zstandard waits for ASSET-2's packages.

## Forbidden dependencies

Any module above Foundation (a format never knows Physics, World or Godot); Jolt (collision is stored as shapes and
triangles, which Physics builds).

## Threading and tick model

The formats are pure functions. The cache is safe from any thread under its one lock; it is for I/O threads and
loaders, never a simulation tick: a load reads a file.

## Data ownership

The cache owns what it loaded until it collects it; a handle shares it, and loaded data never changes. A catalog owns
its entries and problems.

## Integration points

- The Cooker writes the files and the catalog with this module's writers (`WriteCooked...`, `WriteBytesToFile`,
  `AssetCatalog::Write`).
- Client and server open the same cooked folder with an `AssetCache`; slice 9's programs load a cell, then its
  placements' collisions, and build Physics bodies from the shapes.

## Where the code lives

- `Public/AssetHandle.hpp` (the handle is a template), `Public/CookedFormats.hpp`, `Public/AssetCatalog.hpp`,
  `Public/AssetCache.hpp`.
- `Source/Handles/AssetKind.cpp`; `Source/CookedFormats/CookedFormats.cpp`, `ByteStream.hpp`;
  `Source/Cache/AssetCatalog.cpp`, `AssetCache.cpp`.
- `Tests/AssetsTests.cpp`; `Tests/Data/golden_square.rmesh` (the golden file, see its README).

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| ASSET-1 | AssetId, handles, cache, loading of native cooked mesh and collision formats | A cooked test mesh loads on client and server | **Implemented** (slice 9, preview branch): `RunicAssetsTests` passes with the 11 tests below in the headless build (the server's) and the debug build (the client's); awaiting the owner's review |
| ASSET-2 | Random-access packages and manifests with Zstandard decompression, client and server package selection, asynchronous streaming | Cells load from a package without stalling a tick or a frame | Not started (slice 21) |

ASSET-1's acceptance tests (`RunicAssetsTests`): `CookedFormats.*` (a mesh, a collision with every shape kind and a
cell round-trip, the cell's 64-bit origin to the last bit; **the golden mesh file reads and the writer still writes it
byte for byte**; a damaged file says what is wrong: the magic, a newer version, another kind, the checksum, a short
payload, a short header, an index past the vertices, a value that is not finite, a partial triangle, a shape naming a
missing mesh, a rotation that is not a unit), `AssetCatalog.*` (written and read back, found by ID or name; every
problem with its line: a name twice, an ID twice, an unknown kind, a malformed name, too few columns, a wrong header),
`AssetCache.*` (**a cooked test mesh loads once and is shared**; collect frees what no handle holds and a later load
reads again; a load that cannot happen says why: not in the catalog, an unknown name, another kind, a missing file, a
damaged one; four threads loading and collecting together). Client and server: the same executable passes in the
headless build and in the debug build with presentation; slice 9's programs (RunicServer, RunicClient) load the cooked
test region the Cooker writes.

Decisions taken in ASSET-1 that await the owner (also in the slice brief): loose files and a text catalog until
ASSET-2's packages; one format header with a version, a kind and an FNV-1a checksum; collision stored as shapes and
triangles (Physics builds them at load; Jolt's own binary shape is not stored, since Physics has no way to read it);
one collision per placement scale, so a runtime never scales a shape; one cell file for client and server until
COOK-3 splits them; the cache loads under its lock.

Costs, measured (release, `-O2`, a 256 × 256 grid: 65,536 vertices with normals, 130,050 triangles, 3.1 MB): writing
it 7.4 ms, reading and checking it 4.2 ms (750 MB/s), its first load from disk through the cache 4.9 ms, a later load
(a hit) 7 ns.

## Future extensions

- ASSET-2: packages with a random-access table of contents, Zstandard, manifests, client and server package
  selection, asynchronous streaming without the cache's lock.
- Jolt's binary shapes in the collision file, once Physics can read them (faster loads of large meshes).
- Client cell visual lists split from server cells (COOK-3), LOD and proxy references.
