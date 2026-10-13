# World

Contract seed for `Runic::World`: what a Claude Code session needs to implement the World work packages. When `Engine/SimulationCore/World/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Simulation core |
| Directory | `Engine/SimulationCore/World/` |
| CMake target | `Runic::World` |
| Namespace | `Runic::World` |
| Headless | yes |
| Status | Not started |

## Purpose

World defines world space: 64-bit double coordinates with a real Y axis, worlds (`WorldId`), region-local origins, regions, sectors, sparse 3D cells, authored volumes, the spatial index and its exact XYZ queries, and activation volumes that decide what loads and simulates. There are no spatial layers: altitude and depth are ordinary coordinates, and only the parts of the world that hold content exist in memory (ARCH § World space: coordinates, cells and activation).

## Owns

- 64-bit double world coordinates in meters with a real Y axis and a 1 cm minimum unit, region-local origins and `WorldId` (WORLD-1). The server works in doubles everywhere; only the wire is cell-relative (Network).
- Region, Sector and sparse 3D Cell keys (WORLD-1). A cell key is (x, y, z) at a fixed size per world; sector and cell coordinates are 32-bit integers, so the coordinate space reaches far beyond current content.
- The sparse 3D spatial index: each XZ column stores only the Y cells that hold content or activity (WORLD-2).
- Exact XYZ candidate queries (radius, box, ellipsoid) and membership updates (WORLD-2). A query gathers the 3D cells its volume touches and then tests exactly; a 2D distance is never a gameplay answer.
- Authored volumes drawn in Blender and cooked with the cells they touch: caves and underground spaces, rule areas and indivisible volumes, with point-in-volume queries (WORLD-2). A volume carries a kind the game gives meaning to.
- Activation: per-player asymmetric ellipsoid activation volumes (wider than tall; the game sets the radii), their union, and prepare, commit and release hysteresis that emits load and unload requests per cell (WORLD-3).

| Level | Nature | Purpose |
| --- | --- | --- |
| World | Persistent world identity (`WorldId`) | Global coordinates and registry; each separate map is its own `WorldId` |
| Region | Authored content area (a Flecs entity) | Gameplay state: rules, ambient light profile, music, spawns |
| Sector | Automatic coarse XZ bucket (e.g., 512 m) | Streaming and coarse lookup |
| Cell | Automatic fine 3D bucket (e.g., 64 m), sparse | Candidate lookup for combat, AI, interest, light, map knowledge |
| Volume | Authored 3D volume | Caves and underground spaces, rule areas, indivisible volumes |

| Place | Precision |
| --- | --- |
| Server simulation and Jolt | 64-bit double positions (Jolt in double precision); shapes and local math stay 32-bit |
| Network | Quantized relative to the entity's cell origin on all three axes (Network module) |
| Client rendering | 32-bit relative to the camera |
| Authoring | Region-local coordinates plus a 64-bit region origin |

Rules this module enforces:

- Cell, sector and region transitions inside one simulation are index updates, never entity recreation or handoff.
- Large objects (ships, world bosses, floating islands) register in every cell their bounds touch and are deduplicated on query.
- Only cells with content or activity exist in memory; `Cell[x][y][z]` is never allocated everywhere.
- The union of activation volumes decides which cells load and which simulate; a cell outside every volume is not simulated. The smaller vertical radius keeps spaces far above or below a player out of the active set until the player approaches them.
- Activation never uses a single threshold: a cell starts loading when it nears the union, becomes active inside it, and unloads only after it has stayed outside a larger release volume. The radii and margins are world data.
- An indivisible volume is flagged so that no ownership boundary ever cuts it (DistributedSimulation enforces this).
- Nothing here knows a surface, a sky or an underground: those are game content placed in one continuous coordinate space, plus volumes.

## Does not own

- Simulation ownership: which process owns a cell is an operational assignment (DistributedSimulation and the Platform World Coordinator). The world partition is stable.
- World transitions between `WorldId`s (Network, NET-8): worlds never share a border, so there is no handoff or ghost between them.
- Gameplay locations (a harbor, a market) and what each volume kind means: game data.
- Asset loading (Assets; see OQ-3), nav tiles (Navigation), water fragments (Water), interest sets (Network), render culling structures (Renderer).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core, ECS |
| Third-party | — |

### Must never depend on

- Region, Sector or Cell → simulation process identity. Spatial boundaries never encode server identity.
- Assets: OQ-3 proposes no World → Assets edge; WORLD-3 emits requests instead.
- Physics, Simulation or any feature module.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH's illustrative types (ARCH § World space: coordinates, cells and activation):

```cpp
// Illustrative contract, not a frozen API.
struct WorldPosition { double x, y, z; };     // meters; y is real altitude or depth

struct CellKey {                              // a sparse 3D cell (e.g., 64 m)
    WorldId world;
    int32_t x, y, z;
};

struct ActivationVolume {                     // asymmetric ellipsoid around a player
    WorldPosition center;
    float horizontal_radius_m;                // game value
    float vertical_radius_m;                  // smaller than the horizontal radius
};

struct AuthoredVolume {                       // drawn in Blender, cooked with its cells
    VolumeId     id;
    VolumeKindId kind;                        // meaning given by the game
    bool         indivisible;                 // never cut by an ownership boundary
};
```

Headers (sketch, fixed in the contract session): `WorldPosition.hpp`, `CellKey.hpp`, `SpatialIndex.hpp`, `Volumes.hpp`, `Activation.hpp`. `WorldId`, `VolumeId` and `VolumeKindId` are plain value types; the WORLD-1 contract session confirms they live in Core with the other plain IDs (CORE-1 lists `WorldId`).

## Threading and tick rules

- Positions and spatial membership are written in the 30 Hz realtime phase, step 5, after the Jolt step (ARCH § Simulation core: clock and threads).
- Queries are read-only and may run on workers in parallel phases; membership changes happen at the points the Simulation defines.
- Discovery always starts from cells and sectors, never "for every NPC, query every player"; Flecs filters then apply to a small candidate set.
- Activation and streaming requests are emitted, never executed here; loading never blocks a tick.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| WORLD-1 | 64-bit double coordinates (real Y, 1 cm minimum unit), WorldId, region-local origins, Region, Sector and sparse 3D Cell keys | Conversions round-trip exactly far from the origin; a cell key round-trips at any altitude or depth | ECS-1 | 4 |
| WORLD-2 | Sparse 3D spatial index (XZ columns holding only occupied Y cells), exact XYZ candidate queries (radius, box, ellipsoid), membership updates; authored volumes (caves, rule areas, indivisible volumes) with point-in-volume queries | Query results match brute force in 3D and draw in the debug view; a point inside a cave volume is reported inside it | WORLD-1 | 11 |
| WORLD-3 | Activation: per-player asymmetric ellipsoid activation volumes, their union, prepare, commit and release hysteresis, load and unload requests per cell | Walking back and forth on an edge never thrashes loads; a cell far above or below the player stays unloaded until approached | WORLD-2 | 21 |

## Acceptance tests

The Done when column is the acceptance list. "Draw in the debug view" means writing Core `DebugGeometry`; World never links the Renderer.

## Open items

- [OQ-3](../system/11-open-questions.md#oq-3) (still open, narrowed): who performs the loads WORLD-3 requests, and why ASSET-2 needs WORLD-3, is confirmed in the ASSET-2 contract session. Ask the owner.
