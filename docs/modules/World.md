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

World defines world space: 64-bit coordinates, region-local origins, spatial layers, regions, sectors and cells, the spatial index and its queries, and the streaming state. Positions are 64-bit, every layer shares the same horizontal footprint, and only the parts of the world that hold content exist in memory (ARCH § World space: coordinates, layers and partitions).

## Owns

- 64-bit world coordinates and region-local origins (WORLD-1).
- Spatial layers, defined in data with logical and preload vertical envelopes (WORLD-1). A world might define a surface, a sky and an underground layer; layers are streaming and simulation regimes, not floor numbers.
- Region, Sector and Cell keys (WORLD-1). Sector coordinates are 32-bit integers, so the coordinate space reaches far beyond current content.
- The spatial index and candidate queries: radius, box, layer-filtered; membership updates (WORLD-2).
- Streaming state: preload bands, hysteresis, load and unload requests per cell (WORLD-3).

| Level | Nature | Purpose |
| --- | --- | --- |
| World | Persistent world identity | Global coordinates and registry |
| SpatialLayer | A data-defined layer | Separate streaming and population regimes; same footprint, different height envelopes |
| Region | Authored content area (a Flecs entity) | Gameplay state: rules, ambient light profile, music, spawns |
| Sector | Automatic coarse bucket (e.g., 512 m) | Streaming and coarse lookup |
| Cell | Automatic fine bucket (e.g., 64 m), sparse | Candidate lookup for combat, AI, interest, light, map knowledge |

| Place | Precision |
| --- | --- |
| Server simulation and Jolt | 64-bit positions; shapes and local math stay 32-bit |
| Network | Quantized relative to the entity's cell origin (Network module) |
| Client rendering | 32-bit relative to the camera |
| Authoring | Region-local coordinates plus a 64-bit region origin |

Rules this module enforces:

- Cell, sector and region transitions inside one simulation are index updates, never entity recreation or handoff.
- Large objects (ships, world bosses) register in every cell their bounds touch and are deduplicated on query.
- Only cells with content or activity exist in memory.
- Layer bounds are validity envelopes, not allocated voxels. Each cooked sector and cell also has a generated content AABB.
- Logical and preload bounds differ and neighboring layers overlap on purpose. A layer switch never uses a single `if (y > threshold)`: transitions use prepare, commit and release thresholds so small vertical movement cannot flip ownership back and forth. The values are world data.
- The default partition is the layer plus XZ sectors and cells plus vertical bounds; there is no world-wide voxel grid.

## Does not own

- Simulation ownership: which process owns a cell is an operational assignment (DistributedSimulation and the Platform World Coordinator). The world partition is stable.
- Gameplay locations (a harbor, a market): game data volumes that may cross any partition.
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

ARCH's illustrative layer and sector types (ARCH § World space: coordinates, layers and partitions):

```cpp
// Illustrative contract, not a frozen API.
struct SpatialLayerDefinition {
    SpatialLayerId id;
    double logical_min_y;   // where this layer's content and ownership are relevant
    double logical_max_y;
    double preload_min_y;   // where the layer starts streaming in advance
    double preload_max_y;
};

struct SpatialSectorKey {
    SpatialLayerId layer;
    int32_t x;
    int32_t z;
};
```

Headers (sketch, fixed in the contract session): `WorldPosition.hpp`, `SpatialLayer.hpp`, `SpatialKeys.hpp`, `SpatialIndex.hpp`, `StreamingState.hpp`.

## Threading and tick rules

- Positions and spatial membership are written in the 30 Hz realtime phase, step 5, after the Jolt step (ARCH § Simulation core: clock and threads).
- Queries are read-only and may run on workers in parallel phases; membership changes happen at the points the Simulation defines.
- Discovery always starts from cells and sectors, never "for every NPC, query every player"; Flecs filters then apply to a small candidate set.
- Streaming requests are emitted, never executed here; loading never blocks a tick.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| WORLD-1 | 64-bit coordinates, region-local origins, SpatialLayers, Region, Sector and Cell keys | Conversions round-trip exactly far from the origin; layer bounds are respected | ECS-1 | 4 |
| WORLD-2 | Spatial index and candidate queries (radius, box, layer-filtered), membership updates | Query results match brute force and draw in the debug view | WORLD-1 | 11 |
| WORLD-3 | Streaming state: preload bands, hysteresis, load and unload requests per cell | Walking back and forth on a border never thrashes loads | WORLD-2 | 21 |

## Acceptance tests

The Done when column is the acceptance list. "Draw in the debug view" means writing Core `DebugGeometry`; World never links the Renderer.

## Open items

- [OQ-3](../system/11-open-questions.md#oq-3) (still open, narrowed): who performs the loads WORLD-3 requests, and why ASSET-2 needs WORLD-3, is confirmed in the ASSET-2 contract session. Ask the owner.
