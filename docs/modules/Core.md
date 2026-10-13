# Core

Contract seed for `Runic::Core`: what a Claude Code session needs to implement the Core work packages. When `Engine/Foundation/Core/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Foundation |
| Directory | `Engine/Foundation/Core/` |
| CMake target | `Runic::Core` |
| Namespace | `Runic::Core`; RenderData uses `Runic::RenderData` |
| Headless | yes |
| Status | Not started |

Today `cmake/Targets.cmake` defines a bootstrap INTERFACE target with the same name (C++20 and the `include/` root). BUILD-1 and CORE-1 replace it.

## Purpose

Core is the foundation every Engine module links. It holds plain ID and time types, math, memory, logging, configuration, the worker pool and the metrics registry. It also holds RenderData: the plain render structs shared by the Client's extractors, both render pipelines, the world UI and the debug tools (ARCH § Repositories, build targets and processes; ARCH § Render data, extraction and world UI).

## Owns

- Plain ID types: `GlobalEntityId`, `GlobalPlayerId`, `SimulationId`, `WorldId`, `TickId`, `AuthorityEpoch` (CORE-1; decided in OQ-2). Core declares the types; their meaning belongs to whoever issues them: authority semantics belong to DistributedSimulation, the Platform accounts service (PCS-1) issues `GlobalPlayerId`, and World gives `WorldId` its meaning (each separate map is its own world).
- Plain value types that other modules give meaning to: `AssetId` (Assets), `WaterBodyId` (Water), `FieldId` and `WidgetId` (CORE-1, ARCH revision 369).
- `Result`, asserts and logging hooks (CORE-1).
- Time types. Time comes from one clock as integer ticks or double seconds, never accumulated floats (ARCH § Application and LayerStack).
- Math on GLM (CORE-1). GLM types may appear in Core's public headers: the third approved public-header exception.
- Memory and configuration utilities (listed in the catalog row; no package names them yet).
- The worker pool, sized from the process's CPU allotment (CORE-2).
- The debug-stream registry where modules publish traces (CORE-2); the Diagnostics layer (NET-7) streams them to the Editor in development and staging builds.
- The metrics registry: per-tick timings and counters per tick domain (CORE-2). Every GameServer records its metrics here: the Simulation records tick-domain timings (SIM-1) and the Network module's Diagnostics layer adds network and replication metrics (NET-7). The Platform exporter (PBND-3) publishes the whole registry to Prometheus.
- RenderData in `Public/RenderData/`: the double-buffered `RenderFrame`, `DebugGeometry` and the render-data kinds below (CORE-3 to CORE-7).

### RenderData rules

- Plain data only: header-only structs with trivial helpers. No Flecs, Jolt, bgfx or RmlUi types. Nothing allocates on its own.
- Camera-relative: each frame carries one 64-bit origin; every position inside it is a 32-bit float relative to that origin.
- Stable references: meshes, materials and textures are `AssetId`s; entities are `GlobalEntityId`s; nothing points into Flecs memory.
- Double-buffered: extraction writes frame N+1 while the Renderer reads frame N, so they never lock each other.
- Grown by packages: a kind is added by the package that first needs it, so the folder never holds unused types.

| Kind | Example | Filled from | Package |
| --- | --- | --- | --- |
| Debug geometry | Collision shapes, paths, cells, volumes, telegraphs | Physics, AI, any system; also the server | CORE-3 |
| Mesh instance | Buildings, props, ships, terrain pieces | Flecs Transform and RenderMesh | CORE-4 |
| Skinned instance | Characters, creatures | Flecs and Animation poses | CORE-4 |
| World anchor | Where a nameplate, prompt or panel belongs | Flecs Transform, visibility tier, distance | CORE-4 |
| Light | Torches, spell lights, lanterns | Perception's emitters (the ones the server uses) | CORE-5 |
| Environment frame | Sun, skylight, darkness, fog, wind, rain | Environment clock and weather state | CORE-5 |
| Volume | Darkness zones, fog banks, area effects | Flecs components, Perception | CORE-5 |
| Field grid | Ocean current, wind map, darkness map, fog of war | Water current fields, Environment wind | CORE-6 |
| Water surface state | Tide level, wave strength, flow scale per water body | Water and Environment | CORE-6 |
| Effect and decal | Impacts, footprints, ground telegraphs | Flecs, ability timelines | CORE-7 |

## Does not own

- The Flecs world (ECS wrapper, owned by the SimulationRuntime), Jolt objects (Physics), GPU resources and draw calls (Renderer), render extraction (Client).
- The meaning of IDs: authority and epochs (DistributedSimulation), `ContentId` (Content), asset handles and cache (Assets).
- Metrics export: the Platform exporter (PBND-3) publishes the registry. Core only stores what modules record.
- Wire encoding (Network) and any game rule.

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | — (none) |
| Third-party (PUBLIC) | GLM (math), an approved public-header exception: the Engine's math vocabulary |

### Must never depend on

- Any other Engine module. Core is the bottom of the graph.
- Flecs, Jolt, bgfx, RmlUi, ImGui, GLFW, BehaviorTree.CPP, Detour, GameNetworkingSockets.
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

Core's public headers are consumed outside the Engine: RunicPlatform's C++ links `Runic::Core` (the metrics registry), and the two header-only interface targets the Platform implements, `Runic::NetworkTransport` and `Runic::SimulationPort`, include only Core types. Keep every Core public header free of library types other than GLM, so the Platform sees GLM math types and nothing else.

## Public API sketch

Sketch, fixed in the contract session.

| Header | Holds | Package |
| --- | --- | --- |
| `GlobalEntityId.hpp`, `GlobalPlayerId.hpp`, `SimulationId.hpp`, `WorldId.hpp`, `TickId.hpp`, `AuthorityEpoch.hpp` | Plain ID types | CORE-1 |
| `Result.hpp`, `Assert.hpp`, `Log.hpp` | Result, asserts, logging hooks | CORE-1 |
| `Time.hpp`, `Math.hpp` | Time types, GLM-based math | CORE-1 |
| `WorkerPool.hpp`, `MetricsRegistry.hpp` | Worker pool, metrics registry | CORE-2 |
| `RenderData/RenderFrame.hpp`, `RenderData/DebugGeometry.hpp` | Frame packet and debug geometry | CORE-3 |

ARCH's illustrative `RenderFrame` (ARCH § Render data, extraction and world UI):

```cpp
// Illustrative: Core/Public/RenderData/RenderFrame.hpp
namespace Runic::RenderData {
struct MeshInstance      { AssetId mesh; AssetId material; Mat4 transform; uint8_t lod; uint32_t flags; };
struct SkinnedInstance   { MeshInstance base; uint32_t poseOffset; uint16_t boneCount; };
struct WorldAnchor       { GlobalEntityId entity; Vec3 position; float distance;
                           uint8_t visibilityTier; uint8_t priority; WidgetId widget; };
struct Light             { LightType type; Vec3 position; Vec3 direction; Color color;
                           float intensity; float radius; float coneAngle; };
struct EnvironmentFrame  { Vec3 sunDirection; float skylight; float darkness;
                           Color fogColor; float fogDensity; Vec2 wind; float rain; };
struct Volume            { VolumeShape shape; Mat4 transform; VolumeKind kind; float density; Color tint; };
struct FieldGrid         { FieldId id; Vec3 origin; float cellSize; uint16_t width, height;
                           FieldFormat format; uint32_t version; std::span<const std::byte> texels; };
struct WaterSurfaceState { WaterBodyId body; float level; float waveAmplitude;
                           Vec2 waveDirection; Vec2 flowScale; uint32_t version; };

struct RenderFrame {
    DVec3 origin;                          // 64-bit; everything below is relative to it
    Camera camera;
    EnvironmentFrame environment;
    std::span<const MeshInstance> meshes;
    std::span<const SkinnedInstance> skinned;
    std::span<const Mat4> poses;
    std::span<const WorldAnchor> anchors;
    std::span<const Light> lights;
    std::span<const Volume> volumes;
    std::span<const FieldGrid> fields;
    std::span<const WaterSurfaceState> water;
    DebugGeometry debug;                   // lines, triangles, text, cached debug meshes
};
}
```

## Threading and tick rules

- A `RenderFrame` is written on one thread and read on another with no lock (CORE-3).
- Simulation phases and Jolt jobs run on the worker pool; Jolt never owns threads (PHYS-4). Network I/O and service clients run on their own threads and only enqueue.
- Early slices run the pool with one worker. Code is phase-safe from day one, so more workers is a configuration change.
- The worker count changes only at a safe point between ticks (for example after a CPU resize).
- Metrics are recorded per tick domain; every clock domain and major system reports its cost per tick (ARCH § Principles and performance targets).
- Nothing in Core reads the wall clock on behalf of simulation systems; systems get time from the Simulation's `TickContext`.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| CORE-1 | Plain ID types (GlobalEntityId, GlobalPlayerId, SimulationId, WorldId, TickId, AuthorityEpoch, and value types other modules give meaning to, such as AssetId, WaterBodyId, FieldId and WidgetId), Result, asserts, logging hooks, time types, GLM math | Unit tests cover IDs, Result and time conversions | BUILD-1 | 1 |
| CORE-2 | Worker pool, metrics registry (per-tick timings and counters) and debug-stream registry where modules publish traces | Jobs run on the pool; metrics are recorded per tick domain | CORE-1 | 3 |
| CORE-3 | RenderData foundation: the double-buffered RenderFrame and DebugGeometry (lines, triangles, text, cached debug meshes) | A frame is written on one thread and read on another with no lock; debug vertices need no conversion before upload | CORE-1 | 5 |
| CORE-4 | RenderData scene kinds: mesh instances, skinned instances with pose buffers, world anchors | Every kind round-trips through a frame packet in tests | CORE-3 | 22 |
| CORE-5 | RenderData world-state kinds: lights, environment frame, volumes | Every kind round-trips through a frame packet in tests | CORE-3 | 25a |
| CORE-6 | RenderData water and field kinds: water surface state, field grids | Versions change only when their source changes | CORE-3 | 42a |
| CORE-7 | RenderData effect kinds: particles, decals, trails | Defined when the production pipeline is planned | CORE-3 | 44+ |

## Acceptance tests

The Done when column is the acceptance list; each contract session turns its row into failing tests first. The headless server also uses `DebugGeometry` for the Editor's live attach, so `RenderData/` must compile in the headless build.

## Open items

- Decided in ARCH revision 369: `AssetId`, `WaterBodyId`, `FieldId` and `WidgetId` are plain value types in Core (CORE-1); the modules that own them (Assets, Water, UI) give them meaning. `RenderFrame` and Environment's API use them without depending on those modules.
- Decided in ARCH revision 369: GLM math types in Core's public headers are the third approved public-header exception, and GLM is a PUBLIC dependency of Core ([OQ-19](../system/11-open-questions.md#oq-19)). Record the exception in this module's ARCHITECTURE.md.
- Memory and configuration utilities appear in the catalog row but in no package; add them only when a package's tests need them.
- [OQ-2](../system/11-open-questions.md#oq-2) (decided): plain ID and time types live here. [OQ-7](../system/11-open-questions.md#oq-7) (decided): the test framework is GoogleTest, wired by BUILD-1.
