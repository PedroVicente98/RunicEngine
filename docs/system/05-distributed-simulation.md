# 05 — Distributed simulation, hot-city partitioning, handoff, cross-simulation combat

One logical seamless world is simulated by many processes. This document defines
who answers which question and the rules that keep authority unique.

## Ownership split [J.8.1]

| Layer | Question answered | Examples |
| --- | --- | --- |
| Engine `DistributedSimulation` | What does remote authority/ghost/transfer *mean* to simulation state? | AuthorityEpoch, GhostState, TransferState, ForeignCombatRequest. |
| Platform `Realtime` | How are bytes/messages delivered quickly and correctly? | Connections, channels, sequencing, reliability, serialization, peer transport. |
| Platform `WorldControl` | Which simulation should own what, and which peers/clients should be prepared? | AuthorityPartition assignment, load balancing, neighbor topology, connection directives. |
| Platform `Orchestration` | Where do simulation processes come from and how are they kept healthy? | Agones/Kubernetes capacity, lifecycle, fleets. |
| Game | What does the interaction mean? | Ability damage, boss rules, quest consequences. |

## World partition is not simulation ownership [J.8.2]

```text
Logical/spatial world:            Operational authority:
Continent                         SimulationGroup
└── Region                        └── AuthorityPartition -> SimulationId
    └── Sector
        └── Cell
```

- Region/Sector/Cell is stable world structure (Engine `World`).
- `AuthorityPartition` is an operational runtime subdivision used when load needs
  more than one simulation inside the same logical area (Platform `WorldControl`).
- A busy city can be one Region — even one loaded Cell set — while dynamic-entity
  ownership is split among several simulations.

```cpp
// Conceptual
struct AuthorityPartition {
    PartitionId    id;
    SimulationId   owner;
    SpatialBounds  bounds;
    AuthorityEpoch epoch;
};
```

**Hard invariants**

- Exactly one simulation owns mutable authority for a transferable entity at a time.
- Local Flecs entity IDs never cross a process boundary; `GlobalEntityId` is
  stable across simulations.
- Region/Sector/Cell must never use `SimulationId` as identity. [J.11.2]

Authority states (Engine `DistributedSimulation`) [J.4.9]:

```cpp
enum class EntityAuthority { LocalAuthority, RemoteGhost, TransferringOut, TransferringIn };
struct AuthorityInfo { SimulationId owner; AuthorityEpoch epoch; };
```

Ghosts are read-mostly mirrors; they never independently decide gameplay
outcomes for the remote entity.

## Hot-city load balancing [J.8.3]

Do **not** round-robin nearby players across simulations — that would turn most
local interactions into remote ones. WorldControl preserves interaction/spatial
locality and splits hot areas into authority partitions, with overlap/ghost bands
where cross-boundary interaction is likely.

```text
             crowded city

        +---------+---------+
        | Sim A   | Sim B   |
        |         |         |
        +---------+---------+
        | Sim C   | Sim D   |
        |         |         |
        +---------+---------+

Static city geometry may be loaded by all relevant simulations.
Dynamic entities retain exactly one SimulationOwner.
```

All participating simulations may load the same terrain, buildings, static
collision, navigation, water, and static decorations. Dynamic doors, NPCs,
players, projectiles, encounter state, and other mutable entities have explicit
authority ownership and may appear as ghosts elsewhere.

## Direct sim-to-sim message set [J.8.4]

| Traffic | Purpose |
| --- | --- |
| GhostState / GhostDelta | Keep boundary-relevant remote copies sufficiently current. |
| PrepareTransfer / TransferReady | Warm destination state before authority moves. |
| FinalTransferSnapshot | Authoritative reconstruction state at a known tick/input sequence. |
| AuthorityCommit | Atomically change the authoritative owner using epochs. |
| ForeignCombatVolume / remote effect context | Ask the remote owner to evaluate effects against its authoritative entities. |
| RemoteHitResult / feedback | Return the authoritative result/feedback to the source simulation. |

This traffic is **direct and broker-free** on the realtime path. NATS, gRPC,
databases, and Agones are never inserted into per-tick ghost or combat
synchronization.

## Cross-simulation melee / projectile / AoE [J.8.5]

```text
Alice owned by Simulation A
Bob   owned by Simulation B

Alice attack on A
    |
    +-- local A-owned candidates -> A resolves locally
    |
    +-- attack volume reaches B authority domain
            |
            v
      ForeignCombatRequest
            |
            v
      Simulation B
      - intersect against authoritative Bob
      - run Game combat eligibility/formulas
      - mutate Bob HP/status locally
      - emit DamageApplied / Hit result
            |
            v
      RemoteHitResult as needed
            |
            v
      Simulation A feedback/presentation
```

Simulation A must never apply damage to a stale Bob ghost and then overwrite B.
**The owner of the target state decides and applies the consequence.** The
foreign request carries enough attack/effect context for the target owner to
reproduce the intended authoritative evaluation:

```cpp
// Conceptual (Engine Combat)
struct ForeignCombatRequest {
    GlobalEntityId     source;
    AbilityExecutionId execution;
    CombatVolume       volume;
    TickId             source_tick;
    AuthorityEpoch     source_epoch;
};
```

## World-boss example [J.8.6]

```text
Boss owner = Simulation B
Player A   = Simulation A
Player B   = Simulation B
Player C   = Simulation C

Players attack boss:
A -> foreign combat -> B -> boss HP mutation
B -> local combat   -> B -> boss HP mutation
C -> foreign combat -> B -> boss HP mutation

Boss large AoE:
B resolves B-owned targets locally
B sends foreign effect volume/context to A and C
A applies consequences only to A-owned targets
C applies consequences only to C-owned targets
```

Boss HP, phase, AI, cooldowns, and encounter state are owned by one simulation at
a time. Neighbors may mirror presentation/ghost state but never advance the boss
state machine.

## Client make-before-break connection set [J.8.7, J.5.9]

```text
WorldSession
  player = GlobalPlayerId
  authority = Simulation A
  authority_epoch = N
  primary = A
  warm_candidates = B, C
  dormant = D
  input_sequence continues across handoff
```

1. A remains authoritative while the player enters a preparation band.
2. WorldControl/current authority identifies candidate simulations.
3. Platform issues signed connection/transfer directives; the client preconnects.
4. Neighbor simulations prepare ghost/transfer state.
5. When the authority boundary is crossed, A sends the final transfer state and
   the last processed input sequence.
6. The authority epoch increments and the chosen candidate is committed as owner.
7. The client promotes that connection without exposing topology to gameplay code.
8. Old A becomes Retiring or Dormant for a grace window; unused candidates sleep
   or drop according to policy.

Connection roles and control operations (Platform `Realtime`):

```cpp
enum class WorldConnectionRole {
    Primary,        // authoritative simulation
    WarmCandidate,  // connected and prepared for promotion
    Dormant,        // temporarily retained with minimal traffic
    Retiring        // old authority during grace/cleanup
};
// Control operations:
// PrepareConnection, WakeConnection, PromoteConnection,
// SleepConnection, RetireConnection, DropConnection
```

The user-facing `WorldSession` is logical and stable even when the underlying
authoritative endpoint changes. Gameplay code never reasons about IP addresses
or chooses its own authority server.

## Active scripted sequences during handoff

See [06 §Handoff policy](06-gameplayflow-quests-cutscenes.md#handoff-policy-for-active-sequences-j99):
`Transferable`, `PinAuthority`, or `AbortOnTransfer`.

## Agones boundary [J.8.8]

```text
Agones / Kubernetes
    provide process capacity and lifecycle
             |
             v
Platform WorldControl
    assigns world/authority responsibility
             |
             v
Engine DistributedSimulation
    enforces ghost/authority/transfer semantics
```

For a hot city, Platform may request/warm new simulation capacity before
repartitioning authority. Agones does not understand players, cities, ghosts,
attacks, or quests — it manages game-server processes; WorldControl owns world
semantics.
