# Water

Contract seed for `Runic::Water`: what a Claude Code session needs to implement the Water work packages. When `Engine/WorldSystems/Water/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | World systems |
| Directory | `Engine/WorldSystems/Water/` |
| CMake target | `Runic::Water` |
| Namespace | `Runic::Water` |
| Headless | yes |
| Status | Not started |

## Purpose

Water is a queryable, streamed spatial field, not a global height plane: any position can be sampled for surface, depth, flow and submersion. Buoyancy and movement are separate components that consume those facts, so passive water costs nothing and only ships, swimmers and drifting objects pay (ARCH § Water system). The module covers oceans, lakes, rivers, canals, cave rivers, underground seas, rapids, waterfalls, currents, swimming characters, floating rigid bodies and watercraft, and models only what gameplay needs.

## Owns

| Part | Contains |
| --- | --- |
| WaterSystem and water bodies | Stable WaterBody and WaterRoute identities, cooked runtime fragments, the sampling API, planar, ocean, spline-river and flow-volume implementations, connection semantics |
| Interaction components | BuoyantBody, CharacterWaterMovement, ObjectWaterMovement, WatercraftMovement, WaterInteractionState |
| Streaming and handoff integration | Water fragments inside Region, Sector and Cell streaming; continuity across simulations; ship-group handoff |
| Debug drawing | Water debug geometry written into Core DebugGeometry |

Water reports facts; it never decides an entity's response. The caller never needs to know which kind of body answered, and the sample already includes the environment's contribution (tide level, time-varying current and, if enabled, gameplay swell).

| Body type | Representation | Typical uses |
| --- | --- | --- |
| PlanarWaterBody | Bounded polygon with a constant or offset surface height | Lakes, reservoirs, enclosed water, tests |
| OceanWaterBody | Very large bounded surface, an optional analytic swell and a cooked current field | Seas and oceans, including underground oceans |
| SplineRiverBody | 3D centerline spline with width, depth and flow profiles | Rivers, canals, cave rivers, descending waterways |
| FlowVolume | 3D volume with a directional or vector flow field | Waterfalls, rapids, spillways, whirlpools, underwater current tunnels |

| Connection | Meaning |
| --- | --- |
| Smooth | Continuous surface and flow blend |
| Estuary | River-to-lake or river-to-ocean blend |
| Rapids | Navigable, high flow and turbulence |
| Waterfall | Surface discontinuity plus a downward FlowVolume |
| Canal | Authored controlled connection |
| Lock | Stateful controlled elevation change (later feature) |
| Custom | Game policy on Engine connection data |

| Movement component | Physical model | Current behavior |
| --- | --- | --- |
| CharacterWaterMovement | CharacterVirtual | Profile-controlled current influence |
| ObjectWaterMovement | Jolt dynamic body + BuoyantBody | Follows current; self-propulsion may be zero |
| WatercraftMovement | Jolt dynamic body + BuoyantBody | Current plus propulsion, sails (wind), steering and braking |
| Custom movement | Engine extension point | Defined by its policy |

- Rivers are 3D splines that descend continuously; never a stack of horizontal planes. Complex bodies use a cooked low-resolution vector field, still a cheap interpolation at runtime.
- Ocean current fields are coarse cooked vector grids (for example one vector per 512 m), interpolated at sample time and modulated over time by Environment.
- Characters swim; they do not float as rigid bodies. CharacterWaterMovement feeds desired motion into the CharacterVirtual.
- Watercraft combine intent with the local flow and wind and apply forces and torques to the Jolt body. The sail model uses the apparent wind; thrust is zero straight into the wind and peaks at the angles the data curve chooses; lateral resistance limits sideways drift. The Engine never knows what a ship carries or who owns it.
- Tags (such as a ship tag) classify entities but never select behavior.
- Non-goals: no Navier-Stokes grid, no SPH or per-particle server simulation, no pressure, erosion or real-time river formation, no centimeter agreement between shader waves and gameplay water, no assumption that water is horizontal.

| Boundary | Rule |
| --- | --- |
| Jolt | Jolt owns collision, integration, mass, contacts and dynamic ship motion. Buoyancy and watercraft systems turn samples into forces before the Jolt step; CharacterWaterMovement turns them into desired motion |
| Flecs | River samples, vector-field cells and water tiles are never entities. Flecs holds WaterInteractionState, profile references, movement components, dynamic modifiers and tags |
| Movement and handoff | Ships carry passengers because CharacterVirtuals update every tick on moving bodies. A ship hands off as a group with its passengers and cargo in one authority epoch |
| Renderer | The WaterSystem never calls bgfx. The Client extracts water surface state and field grids (CLI-8); the client adds all visual detail |
| Network | Static water ships in cooked world packages and is never replicated per tick. Replication carries only dynamic changes: flow multipliers, water-level offsets, lock or dam state, temporary current modifiers, world-event changes |
| Persistence | Durable water state is stored by a Game service on the Platform Go SDK and reaches the simulation through the service port when the owning region loads |

## Does not own

- Time variation, tides, swell and wind (Environment); water samples include them through Environment calls.
- Cooking: the Cooker's water pipeline (COOK-7) emits client and server water assets with boundary aprons.
- The transfer protocol (DistributedSimulation); Water registers ship groups as snapshot contributors.
- Water content, profile values, water gameplay rules (breath, drowning, restrictions in water), ship contents, ownership and naval rules (RunicGame).
- Durable water state storage (a RunicGame service on the Platform Go SDK).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | World, Physics, Simulation, Gameplay, Environment, DistributedSimulation (ship-group handoff) |
| Third-party | — (Jolt bodies are reached through Physics) |

DistributedSimulation is needed only for ship-group handoff (WATER-7); a package links only what it uses.

### Must never depend on

- Renderer, Client or bgfx; Network; Combat, AI or Perception.
- Jolt types in `Public/`.
- Durable services directly; gRPC, NATS or databases.
- Game water bodies, profiles or values in code or tests (tests use `engine:` IDs; RunicGame names bodies as `game:<water_body>`); RunicPlatform or RunicGame code.

## Public API sketch

ARCH's illustrative sampling API (ARCH § Water system):

```cpp
// Illustrative contract, not a frozen API.
struct WaterSample {
    bool         valid = false;
    WaterBodyId  body  = InvalidWaterBody;
    Vec3d surface_position;          // 64-bit world position
    Vec3  surface_normal {0, 1, 0};
    Vec3  flow_velocity {0, 0, 0};   // world space; includes current modulation
    float depth      = 0.0f;
    float submersion = 0.0f;
    WaterFlags flags {};
};

class WaterSystem {
public:
    bool TrySample(const WorldPosition& position, WaterSample& out) const;
    bool TrySample(const WorldPosition& position, WaterQueryContext& context,
                   WaterSample& out) const;          // cached locality
    void QueryBodies(const WorldBounds& bounds,
                     SmallVector<WaterBodyId, 8>& out) const;
    void RegisterFragment(const CookedWaterFragment& fragment);
    void UnregisterFragment(WaterFragmentId fragment);
};

struct WaterQueryContext {
    WaterBodyId     previous_body     = InvalidWaterBody;
    WaterFragmentId previous_fragment = InvalidWaterFragment;
};

struct BuoyancyProfile {
    Span<BuoyancyPoint> points;   // sampled every tick
    float buoyancy_scale;
    float linear_water_drag;
    float angular_water_drag;
};
struct BuoyantBody { BuoyancyProfileId profile; };

struct WaterInteractionState {      // written once per tick, read by everyone
    WaterBodyId body;
    float submerged_fraction;
    bool  feet_submerged;
    bool  swimming;
    bool  head_submerged;
};
```

Profiles (values are data): `SwimProfile` (max speed, vertical speed, acceleration, deceleration, passive float strength, sink speed, water drag, current influence), `ObjectWaterMovementProfile` (self-propulsion, current influence), `WatercraftMovementProfile` (propulsion, reverse, steering and braking force, current influence, sail force, sail-angle curve, lateral resistance) and `SwimMovementModifiers` (temporary multipliers).

```text
AnchoredFloatingPlatform : Transform, JoltDynamicBody, BuoyantBody
DriftingCrate            : Transform, JoltDynamicBody, BuoyantBody, ObjectWaterMovement
Watercraft               : Transform, JoltDynamicBody, BuoyantBody, WatercraftMovement
Character                : Transform, CharacterVirtual, CharacterMovement, CharacterWaterMovement
```

Headers (sketch, fixed in the contract session): `WaterSystem.hpp`, `WaterSample.hpp`, `WaterBodies.hpp`, `WaterRoutes.hpp`, `BuoyantBody.hpp`, `CharacterWaterMovement.hpp`, `ObjectWaterMovement.hpp`, `WatercraftMovement.hpp`, `WaterInteractionState.hpp`.

## Threading and tick rules

- Sampling is read-only and safe from any worker in a parallel phase; fragment registration happens only at streaming safe points.
- Lookup path: world position, then sector, 3D cell, the cell's candidate fragments, then the precise sample. The WaterSystem never scans every river in the world; the query context tests the previous fragment first.
- Buoyancy and water movement run in the 30 Hz realtime phase, step 4, before the Jolt step. Static water never ticks; tide and weather state update at 10 Hz in Environment.
- WaterInteractionState is written once per tick and read by animation, audio, combat, UI and Game rules, so none of them issues its own query.
- A vessel samples a few buoyancy points (typically 4 to 8), one or two flow samples and one wind sample per tick; cost scales with active consumers, not kilometers of water.
- Fragments follow cells; a simulation loads only the fragments of its own space. Neighbor simulations load matching fragments, and a ship moves with the make-before-break protocol.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| WATER-1 | WaterSystem registry, planar water, WaterSample, cell fragments | Height and flow can be sampled anywhere; fragments load with cells | WORLD-3, PHYS-1 | 40 |
| WATER-2 | Spline rivers, flow, connections and routes | A descending river crosses two cells without a seam | WATER-1 | 40 |
| WATER-3 | BuoyantBody on Jolt bodies | A platform floats without drifting | WATER-1 | 41 |
| WATER-4 | Character and object water movement | A crate drifts with the current; a character swims with current influence | WATER-3, GPL-1 | 41 |
| WATER-5 | Ocean current fields from cooked vector grids | Current interpolates smoothly across the grid | WATER-1 | 42 |
| WATER-6 | WatercraftMovement: sail force, sail angle, lateral resistance | Faster downstream and with a favorable wind; a crosswind pushes sideways | WATER-4, ENV-2 | 42 |
| WATER-7 | Ship-group handoff: a watercraft, its crew, passengers and cargo registered as one transfer-snapshot group | A ship with passengers and cargo crosses a simulation border with nothing lost or left behind | WATER-6, DSIM-3 | 42b |

## Acceptance tests

The Done when column is the acceptance list. ARCH's definition of done for the first water slice: author a descending river in Blender and cook it; stream it across at least two cells with no seam; render it on the client and sample its surface and current on the server; a boat with BuoyantBody and WatercraftMovement travels downstream faster than upstream; a BuoyantBody-only object floats without drifting; a crate with ObjectWaterMovement drifts with zero self-propulsion; a character swims with current influence and no rigid-body buoyancy; the river enters a cave volume continuously and reaches an underground lake or ocean without a teleport or instance; repeated with two networked clients, ship and character transitions stay continuous.

Debug drawing covers body bounds and loaded fragments, river centerlines and sampled surface points, cross sections, flow arrows and FlowVolumes, the connection graph, buoyancy points and applied forces, each character's WaterInteractionState, ownership at fragment boundaries, query counts and cache hit rate.

## Open items

- Decided in ARCH revision 369: `WaterBodyId` is a plain value type in Core (CORE-1); Water gives it meaning.
- Decided in ARCH revision 369: Water registers swimming into Gameplay's movement-mode registry (GPL-1), and owns the water-fragment format the Cooker writes.
- Decided in ARCH revision 369: dynamic water state (flow multipliers, levels, lock or dam state) reaches clients through Simulation's replication registry (SIM-6); Network never depends on Water.
- Implementation stages: Stage A is the vertical proof above; Stage B adds connections, blending, rapids and waterfalls, profiles, the client and server package split, dynamic water-level state and navigation metadata; Stage C adds fleet profiling, gameplay swell if needed, tides, locks, advanced rendering and cross-simulation stress tests (ARCH § Water system, implementation stages). Only Stage A belongs to the current packages.
