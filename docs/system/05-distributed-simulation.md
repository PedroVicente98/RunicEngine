---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# 05 — Distributed simulation

This file explains how one seamless world is split into spatial pieces and simulation processes: world space, authority, ghosts, transfer, foreign interactions, interest and bandwidth.

Source: ARCH[^arch] § World space: coordinates, cells and activation, § Networking: stack, sessions and responsiveness,
§ Networking: replication, interest and handoff, § Water system, § Environment, § Platform and operations.

## Package map

| Concern | Packages | Slice |
| --- | --- | --- |
| Coordinates, worlds, 3D cell keys | WORLD-1[^engine-world] | 4 |
| Sparse 3D spatial index, exact queries, authored volumes | WORLD-2 | 11 |
| Activation volumes and streaming | WORLD-3, ASSET-2[^engine-assets], COOK-3[^engine-cooker] | 21 |
| Replication, prediction | SIM-6[^engine-simulation], NET-2[^engine-network], NET-3, SIM-4 | 13, 14 |
| Interest, visibility tiers | NET-4, PRC-2[^engine-perception] | 25 |
| Authority, ghosts, transfer, links | DSIM-1[^engine-distributedsimulation], DSIM-2, DSIM-3, NET-5, PCS-2[^platform-coreservices], PTR-3[^platform-transport] | 36 |
| Foreign interactions, sequence and quest handoff | DSIM-4, CMB-8[^engine-combat], FLOW-3[^engine-gameplayflow], QST-3[^engine-quest] | 37 |
| Parallel replication, degradation | NET-6 | 38 |
| Ship-group handoff | WATER-7[^engine-water] | 42b |
| Authority partitions (busy areas), world transitions, links by locality, placement by traffic | DSIM-5, NET-8, PTR-4, PCS-7 | 44+ |

## World space

Positions are 64-bit doubles with a real Y axis, the world is indexed by sparse 3D cells, and activation volumes
around players decide what loads and simulates. There are no spatial layers.

### 64-bit coordinates (decided)

World positions are 64-bit doubles in meters with a real Y axis: altitude and depth are ordinary coordinates. The
smallest unit gameplay relies on is 1 cm. The server works in doubles everywhere; only the wire is cell-relative. A
32-bit position loses about 1 cm of precision for every 130 km from the origin, which an engine for continent-scale
worlds cannot accept; a double stays far below 1 cm across any planet-sized space.

| Place | Precision |
| --- | --- |
| Server simulation and Jolt | 64-bit double positions (Jolt in double precision); shapes and local math stay 32-bit |
| Network | Quantized relative to the entity's cell origin on all three axes (16 bits per axis on 64 m cells ≈ 1 mm) |
| Client rendering | 32-bit relative to the camera |
| Authoring | Region-local coordinates plus a 64-bit region origin |

### Spatial hierarchy

| Level | Nature | Purpose |
| --- | --- | --- |
| World | Persistent world identity (`WorldId`) | Global coordinates and registry; each separate map is its own `WorldId` |
| Region | Authored content area (a Flecs entity) | Gameplay state: PvP rules, ambient light profile, music, spawns, special mechanics |
| Sector | Automatic coarse XZ bucket (for example 512 m) | Streaming and coarse lookup |
| Cell | Automatic fine 3D bucket (for example 64 m), sparse | Candidate lookup for combat, AI, interest, light, map knowledge |
| Volume | Authored 3D volume, drawn in Blender | Caves and underground spaces, rule areas (for example flight or build zones), indivisible volumes |
| SimulationOwner | Runtime assignment | Which process is authoritative now |

- Cell, sector and region transitions inside one simulation are index updates, never entity recreation or handoff.
- Gameplay locations (a harbor, a market) are game data volumes and may cross any partition.
- Large objects (watercraft, world bosses, floating islands) register in every cell their bounds touch, deduplicated
  on query.

### Sparse 3D cells and exact queries

- **Cells are 3D and sparse by column.** A cell key is (x, y, z) at a fixed size per world. Each XZ column stores only
  the Y cells that hold content or activity, so empty air and solid rock cost nothing; `Cell[x][y][z]` is never
  allocated everywhere.
- **Every test is exact XYZ.** Combat is action-based and characters jump, glide and fly, so a query gathers the 3D
  cells its volume touches and then tests exactly (Jolt shapes, CharacterVirtual). A 2D distance is never a gameplay
  answer.
- **One vocabulary, separate structures.** Region, Sector, Cell and Volume are the shared spatial language; each
  subsystem keeps its own structure.

| Concern | Internal structure |
| --- | --- |
| World partition | Region, Sector, sparse 3D Cell, authored Volumes |
| Physics | Jolt broadphase |
| Rendering | Frustum and occlusion culling, HLOD, GPU visibility structures |
| Navigation | Navmesh tiles; 3D flight paths for flying actors |
| Networking | Interest candidate sets |
| AI | Spatial candidates and activation |

- **Visual relevance is not simulation relevance.** A client may render distant geography or a floating island while
  receiving no NPC, player or projectile data from that area.

### Activation volumes

- Each player carries an **activation volume**: an asymmetric ellipsoid around the player, wider than it is tall (the
  game sets the radii). The union of all players' volumes decides which cells load and which simulate. A cell outside
  every volume is not simulated.
- The smaller vertical radius keeps spaces far above or below the player, such as a deep cave or a high island, out
  of the active set until the player approaches them.
- **Hysteresis.** Activation never uses a single threshold. A cell starts loading (prepare) when it nears the union,
  becomes active inside it (commit), and unloads only after it has stayed outside a larger release volume (release).
  Walking back and forth on an edge never thrashes loads. Radii and margins are per-world data.
- The client streams presentation with the same idea and its own, usually larger, radii, plus LOD, HLOD and world
  proxies at distance.

### Volumes for caves and rules

- Caves, underground spaces and other rule areas are volumes drawn in Blender and cooked with the cells they touch. A
  volume carries a kind the game gives meaning to (for example a cave, a flight zone with an altitude ceiling, a build
  zone, a light or music profile) and is queried with exact XYZ.
- Underground spaces live in the same world and coordinates as the surface; nothing in the Engine knows a surface, a
  sky or an underground. Weather, wind and skylight read volume flags (a cave volume is sheltered and dark).
- **Indivisible volumes** (an island, a castle, a docked ship) are flagged so that no ownership boundary ever cuts them:
  the whole volume always belongs to one simulation.

```cpp
// Illustrative contract, not a frozen API.
struct WorldPosition { double x, y, z; };     // meters; y is real altitude or depth

struct CellKey {                              // a sparse 3D cell (for example 64 m)
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

### Sparse and expandable world

- The coordinate space reaches far beyond current content: sector and cell coordinates are 32-bit integers.
- Only cells with content or activity exist in memory. Empty sea and unbuilt land cost nothing; open ocean is
  generated by the water system, not stored as cells.
- A new continent is new regions in that space. A content patch adds cooked cells, streaming makes them appear without
  restarts or loading screens, and the World Coordinator[^platform-coreservices] assigns their simulation.

### Worlds, instances and transitions

"One world" is a property of the logical identity and the coordinate space, not of one source file, one scene, one
package or one process.

- **Process boundary versus world and instance boundary.** A process boundary answers "who computes this?"; a world
  boundary answers "which map is this?"; an instance boundary answers "which copy of that map is this?". A neighboring
  region may run in another simulation process and still be the same world.
- **The main world** is one `WorldId` with InstanceId 0: its surface, floating islands and underground spaces in one
  continuous coordinate space.
- **Separate persistent maps** are their own `WorldId` with InstanceId 0, shared by every player. **Instances**
  (dungeons, raids) are extra InstanceIds of a map.
- **Worlds never share a border.** There is no handoff or ghost between worlds. A **world transition** is a checkpoint
  plus a new connect ticket (NET-8[^engine-network]): the source simulation checkpoints the character through the service port, the
  Platform issues a ticket for the destination simulation, and the client connects there while it loads the
  destination. Presentation covers the load; portals to other maps and entries to instances use the same path.
- **Partition:** Region (authored gameplay and content concept), then automatic Sector, then automatic Cell. Simulation
  ownership is separate again and is never encoded as "one chunk equals one server".

### Runtime loading

- **Client streaming:** cells inside the client's activation volume, with LOD, HLOD and world proxies at distance.
- **Server streaming:** cells inside the union of activation volumes: static collision, gameplay and world metadata,
  nav tiles where active, spawn, routine and state definitions and spatial indices — never textures, vegetation
  shaders or other presentation data.
- **Activation** (WORLD-3[^engine-world]) emits load and unload requests per cell. Who performs the load is open
  ([11 § OQ-3](11-open-questions.md#oq-3)).
- **Floating islands** are fixed bodies inside a world that at most rise and sink slightly. Each is an indivisible
  volume, so it never crosses a simulation boundary. A moving island (archived, but supported) would be a kinematic
  indivisible volume that hands off as one group, like a watercraft (WATER-7[^engine-water]).

## Partition versus ownership

- **Spatial partitions are indexes, not servers.** Regions, Sectors and Cells stay fixed. Which simulation owns what
  is an operational decision made by the World Coordinator[^platform-coreservices] (PCS-2), which also issues the world epoch.
- A simulation may own many quiet cells or regions. A busy area can later move or split without changing world
  coordinates or authored assets.
- Region, Sector or Cell code never refers to a simulation process identity (forbidden in
  [03](03-dependency-rules.md#forbidden-dependencies)).
- Players are never spread round-robin across simulations, which would turn most local interactions into remote ones.

## Authority and epochs

- **Exactly one simulation is authoritative for an entity at any moment.**
- Authority state carries an **authority epoch**. Messages with a stale epoch are rejected (DSIM-1[^engine-distributedsimulation]) and dropped on
  simulation links (NET-5[^engine-network]).
- DistributedSimulation maps `GlobalEntityId` to local entities. Local Flecs IDs never cross a process boundary.
- Exactly one owner holds a player at all times, including during transfer.
- **Handoff is required, not optional.** Inside one world, any boundary between owners is crossed by transfer;
  watercraft cross with their passengers and cargo.

## Ghosts

- Near a border, neighbor simulations receive **ghost state**: a read-mostly mirror of an entity owned elsewhere.
- DistributedSimulation[^engine-distributedsimulation] holds ghost state as plain data (DSIM-2); Network encodes ghost deltas with the snapshot codec
  and sends them over simulation-to-simulation links (NET-5[^engine-network]).
- A ghost never decides an outcome for its entity. Effects on it become foreign interactions to its owner.
- Ghosts stay in sync across a border under packet loss.

## Transfer protocol

Transfer is make-before-break: the destination is prepared before authority commits.

1. **Prepare.** The destination is made ready before anything moves: it already receives the entity as a ghost, and
   the client already holds a candidate connection to it.
2. **Final snapshot.** The source builds the final transfer snapshot. Every module that owns state for the entity adds
   it through the **snapshot-contributor registry** (DSIM-3[^engine-distributedsimulation]).
3. **Authority commit.** Authority moves in one step with a new epoch. The client's connection is promoted. The input
   sequence continues; there is no loading screen and no explicit reconnect.

Snapshot contributors:

| Contributor | Package | What travels |
| --- | --- | --- |
| GameplayFlow[^engine-gameplayflow] | FLOW-3 | Transferable sequences: their step, timers relative to simulation time, restriction source IDs, quest or encounter context |
| Quest | QST-3[^engine-quest] | Quest instances with their objectives and timers |
| Water | WATER-7[^engine-water] | A watercraft with its crew, passengers and cargo as one group, in one authority epoch |

- A module adds its state by registering into the registry; DistributedSimulation never depends on those modules.
- Static water is not transferred: neighbor simulations load matching fragments.
- Environment values are deterministic functions of world time and position, so neighbors compute identical values at a
  border and nothing jumps on crossing. World time counts from a world epoch that the World Coordinator[^platform-coreservices] issues
  (PCS-2, slice 36); until then the Environment clock (ENV-1[^engine-environment]) reads the epoch from configuration.

## Simulation links

- Simulation-to-simulation connections are provided by the Platform transport (PTR-3[^platform-transport]). **Links by locality
  (decided, PTR-4):** a link uses the cheapest channel for where the peers run — an in-memory queue in one process, a
  local socket or shared memory on one machine, GameNetworkingSockets between machines. DistributedSimulation[^engine-distributedsimulation]
  semantics do not change with the channel.
- There is no shared central memory between simulations; each owns its entities and talks through links.
- Network encodes ghost deltas and transfer messages with the snapshot codec (NET-5[^engine-network]), envelope included. Protobuf,
  NATS and gRPC are never used in this hot path.

## Foreign interactions

When an action crosses an authority border, **the target's owner resolves it**.

- The foreign-interaction contract (DSIM-4[^engine-distributedsimulation]): a request carries enough context for the owner to resolve it.
- Foreign combat (CMB-8[^engine-combat]): combat volumes crossing an authority border become requests to the owner
  (ForeignCombatVolume, then RemoteHitResult). The target is damaged exactly once, by its owner.
- A projectile or area effect crossing the border uses foreign effect volumes that each simulation applies only to
  its own entities; nobody mutates a remote ghost.
- A **world boss** keeps its health, phase, AI and cooldowns on one simulation. Attacks from players owned elsewhere
  arrive as foreign combat; the boss's area effects reach other simulations as foreign effect volumes.

## Sessions and candidate connections

1. The client logs in through the Platform accounts service and receives a signed, short-lived connect ticket for its
   simulation.
2. The client opens a transport connection directly to the simulation and presents the ticket first.
3. The simulation checks the ticket locally (signature, expiry and key ID, against a configured set of public keys),
   with no service call in the hot path (PTR-2[^platform-transport]). PCS-1[^platform-coreservices] signs tickets with rotating keys whose public halves the
   simulations load; before slice 27 tickets are test-signed with a local key.
4. The **WorldSession** survives handoffs: one authoritative connection plus **up to three candidate connections**
   near borders (PTR-3). Crossing promotes one and drops the others.

| Connection role | Meaning |
| --- | --- |
| Primary | The authoritative connection |
| WarmCandidate | Connected and prepared for promotion |
| Dormant | Kept with minimal traffic |
| Retiring | The old authority during a grace window |

The Platform tells the client to prepare, promote, sleep, retire or drop connections; gameplay code never reasons about
addresses. Dropping a candidate or retiring connection never duplicates or rolls back authority.

## Replication and prediction

- Per client, the simulation builds a delta against the last state that client acknowledged; unchanged fields cost
  nothing (NET-2[^engine-network]).
- Values are quantized: positions relative to the cell origin, angles in 16 bits, health in steps where acceptable.
- Snapshot building runs in parallel, one client per task (NET-6).
- **Prediction** runs `RunicGame::Shared`, the same code the server runs, with only a subset of systems enabled: the
  local player's movement, its own skill timelines and nearby collision (SIM-4[^engine-simulation]). There is no separate copy.
- The client applies its input immediately and keeps a short history of numbered inputs; the server returns state with
  the last input it processed; the client replays the inputs after it. Small errors blend, large ones snap (NET-3).
- Skill starts are predicted cosmetically and cancelled in the rare case the server refuses.
- Speed and teleport hacks are stopped by server validation of every movement, independent of prediction.

## Interest and visibility tiers

Decided. Candidates come from the 3D cells around the client inside its interest volume (plus a preload margin),
filtered by category.
Each candidate pair then gets a tier — server-side fog of war (PRC-2[^engine-perception], applied in NET-4[^engine-network]).

| Tier | Condition | Data sent |
| --- | --- | --- |
| Full | Within visibility range, set by the target's luminosity and concealment | Everything |
| Silhouette | Within the minimum silhouette range in any light, or revealed by contact in the last few seconds | Coarse position and pose at a low rate; no name, gear or identity |
| Hidden | Otherwise | Nothing |

- Contact reveals: hitting or being hit, a heal nearby, AoE contact.
- Projectiles and effects from hidden players are sent without the caster's identity.
- Cost: one multiplication per candidate pair inside an interest pass that already runs.
- **Combat never uses tiers.** The server resolves every hit regardless of visibility.
- World anchors carry the tier to the client, so a Hidden player produces no nameplate (CLI-6[^engine-client]).
- Nothing on the client (music, UI) may reveal what the tiers hide.

## Priority and bandwidth budget

Each client has a byte budget per update. Entities are ranked by distance, combat involvement, targeting, party
membership and importance; the highest go first and the rest accumulate priority until they are sent. Rough planning
at about 12 bytes per entity update:

| Battle size | Plan per client | Download per client | Upload per simulation |
| --- | --- | --- | --- |
| 200 (100 vs 100) | Everyone nearby at 20 Hz | ≈ 48 KB/s | ≈ 9.6 MB/s |
| 500 | The ~100 most relevant at 20 Hz, the rest at 5 Hz | ≈ 48 KB/s | ≈ 24 MB/s |

Beyond the budget, degradation follows a fixed order (NET-6[^engine-network]):

1. Lower update rates for distant entities.
2. Less cosmetic detail (gear and animation flags).
3. A cap on visible effects.

**Combat correctness never degrades.**

## Authority partitions

- World partition is not simulation ownership. An **AuthorityPartition** is an operational area with an owner and an
  epoch; it can give a busy area such as a city its own simulation (DSIM-5[^engine-distributedsimulation], slice 44+). A partition never cuts an
  indivisible volume.
- Every participating simulation loads the same static geometry. Each dynamic entity keeps exactly one owner and
  appears as a ghost elsewhere.
- Splitting one region across several simulations is the last resort for capacity; placement rules are in
  [07 § Simulation placement](07-service-backed-systems.md#simulation-placement).

## Rules for any session touching this area

- Never encode a simulation identity in a Region, Sector or Cell.
- Never let an ownership boundary cut an indivisible volume.
- Never hand off or ghost across worlds; moving between `WorldId`s is a world transition.
- Never let two simulations mutate the same entity; route cross-border effects as foreign interactions.
- Never send a local Flecs ID across a process boundary.
- To carry a module's state across a transfer, register a snapshot contributor, as GameplayFlow[^engine-gameplayflow], Quest and Water do.
  The contributing module must have DistributedSimulation[^engine-distributedsimulation] in its catalog row; DistributedSimulation never depends on
  the contributors.
- Never use NATS or gRPC for ghost, combat or replication traffic.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../system/README|System README]] · [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-world]: [[../../../RunicEngine/docs/modules/World|World]] — Engine module · Simulation core
[^engine-assets]: [[../../../RunicEngine/docs/modules/Assets|Assets]] — Engine module · Foundation
[^engine-cooker]: [[../../../RunicEngine/docs/modules/Cooker|Cooker]] — Engine module · Tools
[^engine-simulation]: [[../../../RunicEngine/docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-network]: [[../../../RunicEngine/docs/modules/Network|Network]] — Engine module · Networking
[^engine-perception]: [[../../../RunicEngine/docs/modules/Perception|Perception]] — Engine module · World systems
[^engine-distributedsimulation]: [[../../../RunicEngine/docs/modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^platform-transport]: [[../../../RunicPlatform/docs/modules/Transport|Transport]] — Platform module · C++
[^engine-combat]: [[../../../RunicEngine/docs/modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-gameplayflow]: [[../../../RunicEngine/docs/modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-quest]: [[../../../RunicEngine/docs/modules/Quest|Quest]] — Engine module · GameplayAPI
[^engine-water]: [[../../../RunicEngine/docs/modules/Water|Water]] — Engine module · World systems
[^engine-environment]: [[../../../RunicEngine/docs/modules/Environment|Environment]] — Engine module · World systems
[^engine-client]: [[../../../RunicEngine/docs/modules/Client|Client]] — Engine module · Presentation
