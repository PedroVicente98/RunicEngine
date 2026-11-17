# Runic::World — module contract

Group: Simulation core · Headless: **yes** · Target: `Runic::World` (`runic_world`) · Packages: WORLD-1, WORLD-2,
WORLD-3

Source: RunicDocs › Architecture › World space coordinates, cells and activation (Engine); Repositories, build targets
and processes (Engine modules); Modules, work packages and the implementation contract (WORLD-1). Status: **WORLD-1
implemented** (slice 4, on the preview branch `preview/slices-3-11`): region-local coordinates, the world registry,
cell and sector keys and the components that place things in the world, with their acceptance tests. The folder map
is [README.md](README.md); slice 4's brief and walkthrough are
[preview/slice-04-ecs-and-world-coordinates.md](../../../preview/slice-04-ecs-and-world-coordinates.md).

## Purpose

The world is one continuous 64-bit space, thousands of kilometres wide and with real height and depth. Runic::World
says how that space is addressed: which worlds exist and how each divides its space into cells and sectors, how a
region's authored 32-bit coordinates become places in the world and back, and which components put an entity or a
region there. WORLD-1 builds those. The spatial index and authored volumes (WORLD-2, slice 11) and activation
(WORLD-3, slice 21) are built on them.

## Public API

Namespace `Runic::World`. Headers are included by their path under `Public/` (`#include "WorldRegistry.hpp"`). No
library types.

| Header | Holds | Since |
| --- | --- | --- |
| `CellKeys.hpp` | `CellCoord` (x, y, z, 32-bit), `SectorCoord` (x, z), `CellKey` and `SectorKey` (a `Core::WorldId` and the address), their `std::hash` | WORLD-1 |
| `CoordinateSpace.hpp` | `CoordinateSpace` (origin, `cellSizeM`, `cellsPerSector`; `CellOf`, `CellMin`, `CellCenter`, `OffsetInCell`, `SectorOf`, `SectorSizeM`), `kDefaultCellSizeM` (64), `kDefaultCellsPerSector` (8) | WORLD-1 |
| `RegionFrame.hpp` | `RegionFrame` (a 64-bit origin; `ToWorld`, `ToRegionLocal`, `Reaches`), `kExactRegionRangeM` | WORLD-1 |
| `WorldRegistry.hpp` | `WorldRegistry` (`Register`, `Knows`, `SpaceOf`, `CellAt`, `SectorAt`, `CellMin`, `CellCenter`, `SectorOf`, `Count`, `Describe`), `WorldDescription`, `WorldInfo`, `WorldInstance`, `kSharedInstance` | WORLD-1 |
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

## Owns

- The partition's keys: what a cell and a sector are and how a position maps to them.
- Each world's coordinate space (where its grid starts, its cell and sector sizes) and the registry of worlds.
- What an instance is: an `InstanceId` of a world, sharing that world's space; `kSharedInstance` (0) is the world's
  own copy.
- Region-local coordinates and the rule for when they are exact.
- The `WorldTransform` and `Region` components.

## Does not own

- The 64-bit types themselves and the one narrowing conversion (`Core::WorldPosition`, `Core::ToLocal`).
- Which worlds exist and their sizes: the Game registers them (the sizes in RunicDocs are examples); allocating
  instances is the Platform's.
- Which simulation runs which cells (the Platform's allocation), the cells' files (the Cooker), quantized positions
  on the wire (Network) and the presenter's floating origin (GodotBridge).
- The spatial index, authored volumes and membership updates (WORLD-2); activation volumes and streaming (WORLD-3).
- Physics: a cell's or a volume's collision is Runic::Physics'.

## Allowed dependencies

Catalog row: Core, ECS (both PUBLIC: the public headers use Core's world-space, math, ID and result types, and
`WorldComponents.hpp` registers components in an `ECS::EntityWorld`). No third party.

## Forbidden dependencies

Every other Runic module; Godot, audio and editor code (the module is headless); RunicPlatform and RunicGame.

## Threading and tick model

- `CoordinateSpace`, `RegionFrame` and the keys are plain values: any thread.
- `WorldRegistry`: worlds are registered at start, on one thread; after that every thread may read it.
- Nothing here blocks, and the conversions allocate nothing. A failed lookup builds its error text.

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
- `Source/Partition/WorldComponents.cpp`: the components' registration and their formatters for tools
  (`(6000000.12, -42.50, 7.00) m`; `world 3, origin (100.00, 0.00, -100.00) m`).
- Why the round trip is exact: a float of magnitude at least 2⁻⁷ m (7.8 mm) has a step of at least 2⁻³⁰ m, and a
  64-bit value below 2²³ m has a step of at most 2⁻³⁰ m, so the sum of the origin and the offset is a 64-bit value
  with no rounding.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| WORLD-1 | Region-local origins and conversions; the world registry (WorldId, coordinate space, origin, partition; instances as InstanceIds); Region, Sector and sparse 3D Cell keys | Conversions round-trip exactly far from the origin; a cell key round-trips at any altitude or depth; a WorldId the registry does not know is rejected | **Implemented** (slice 4, preview branch): `RunicWorldTests` passes with the 20 tests below; awaiting the owner's review |
| WORLD-2 | Sparse 3D spatial index, exact radius, box and ellipsoid queries, membership updates, authored volumes | Query results match brute force in 3D and draw in the debug view; a point inside a cave volume is reported inside it; a static rule area creates no Physics body | Not started (slice 11) |
| WORLD-3 | Activation volumes and streaming state | See RunicDocs › Modules, work packages | Not started (slice 21) |

WORLD-1's acceptance tests: `CoordinateSpace.*` (one cell per position, with floor on negatives and exact
boundaries; a cell round-trips from the ends of the 32-bit range, at the surface and 100,000 random addresses; every
point of a far cell maps to it; the offset in a cell; sectors), `CellKeys.*`, `RegionFrame.*` (400,000 local
positions go up and back bit for bit at four origins up to the end of the exact range; the way down is within half a
millimetre; `Reaches`; nanometres beyond the range), `WorldRegistry.*` (each world's space; an unknown `WorldId`
refused by every lookup; bad registrations; two worlds, two cells; a cell through the registry and back; instances)
and `WorldComponents.*` (the slice's runnable result: 1,000 entities one centimetre apart, 6,000 km out, created,
queried and moved; a region as an entity; the tools' text).

Decisions taken in WORLD-1 that await the owner (also in the slice brief): cell and sector sizes are whole metres;
a world's "origin" is where its cell grid starts; "exact" is defined for local coordinates of zero or at least 1 cm
inside 8,388 km; `WorldTransform` and `Region` live here; `InstanceId` 0 means the world's own copy.

One interface note for Core (not changed here): a module other than Core cannot write GLM's identity rotation in a
public header (the public-header scan allows GLM names in Core only), so `kNoRotation` is built from four numbers. A
`Core::kIdentityRotation` would serve every module.

## Future extensions

- The spatial index, membership and authored volumes (WORLD-2), then activation (WORLD-3).
- Scale in `WorldTransform`, when a consumer needs it.
- Region bounds and the region a position belongs to, with the authored volumes.
