# Glossary

| Term | Meaning | Owner |
| --- | --- | --- |
| **Authority** | The right to mutate an entity's authoritative state. Exactly one simulation holds it for a transferable entity at a time. | Engine `DistributedSimulation` (semantics), Platform `WorldControl` (assignment) |
| **AuthorityEpoch** | Monotonic counter incremented when authority over an entity/partition changes; used to reject stale messages. | Engine `DistributedSimulation` |
| **AuthorityPartition** | Operational runtime subdivision of space assigned to one simulation (`PartitionId`, owner `SimulationId`, `SpatialBounds`, epoch). Not a world-structure concept. | Platform `WorldControl` |
| **Cell / Sector / Region / Continent** | Logical/spatial world structure (Continent ⊃ Region ⊃ Sector ⊃ Cell). Independent of which process simulates it. Continents/territories are Game content. | Engine `World` (mechanism), Game `GameWorld` (content) |
| **Command / Intent** | A request to attempt an action (MoveInput, UseSkill, Interact). Validated by the authority. | — |
| **ContentId** | Namespaced, versioned identifier of a data definition: `engine:default_interaction`, `game:infernal_slash`. | Engine `Content` |
| **Cooker** | Tool pipeline that converts source/interchange content into native runtime assets and world packages. Runtime never loads `.blend`. | Engine `Cooker` |
| **Event / Fact** | Something the authority has decided happened (EntityKilled, DamageApplied). Observed by many consumers; never "handled-away". | Engine `Events` |
| **FinalTransferSnapshot** | Authoritative state needed to reconstruct an entity on a new owner at a known tick and input sequence. | Engine `DistributedSimulation`, transported by Platform `Realtime` |
| **ForeignCombatRequest** | Attack/effect context sent to the simulation that owns the target so it can evaluate and apply the consequence itself. | Engine `Combat` |
| **Ghost** | Read-mostly mirror of an entity owned by another simulation. Never decides outcomes for that entity. | Engine `DistributedSimulation` |
| **GlobalEntityId** | Stable cross-process entity identity. Local Flecs IDs never cross a process boundary. | Engine (see [OQ-2](11-open-questions.md#oq-2)) |
| **GlobalPlayerId** | Stable player identity across sessions and simulations. | Platform `Identity` |
| **HandoffPolicy** | How an active GameplayFlow sequence behaves on authority transfer: `Transferable`, `PinAuthority`, `AbortOnTransfer`. | Engine `GameplayFlow` |
| **Headless** | Built and run without GLFW, bgfx, RmlUi, ImGui, client audio, or editor. Mandatory for the authoritative server. | all |
| **Hot city** | A crowded area whose dynamic entities are split across several simulations via AuthorityPartitions while sharing static world data. | Platform `WorldControl` |
| **Idempotency** | Re-executing the same operation (same OperationID/EventId) has no additional effect. | Platform `Persistence` (store), Game services (domain semantics) |
| **Layer / LayerStack** | Process-composition units of an application (client, editor, headless host). Not a gameplay scheduler. | Engine `Application` |
| **Make-before-break** | Client connects to candidate simulations before authority moves, then promotes the new connection without a gap. | Platform `Realtime` + `WorldControl` |
| **Module** | Substantial CMake target or Go package/service with Public/Source/Tests/ARCHITECTURE.md/build file. | — |
| **Presentation cue** | Server-emitted signal that tells clients to play camera/animation/dialogue/audio. Carries no authority. | Engine `GameplayFlow` (emit), Game client (render) |
| **Restriction (source-owned)** | Authoritative gameplay restriction (input, movement, abilities, interactions, targeting, damage, external motion) acquired with a source ID and released centrally via `ReleaseAllFromSource`. | Engine `Gameplay` |
| **Sequence / SequenceInstance** | Authoritative scripted flow (steps, waits, actions, conditions) and a running instance of it. | Engine `GameplayFlow` |
| **SimulationGroup** | Set of simulations jointly serving an area. | Platform `WorldControl` |
| **SimulationId** | Identity of one simulation process. Never used as the identity of a Region/Sector/Cell. | Engine `DistributedSimulation` / Platform `WorldControl` |
| **Stage A / B / C** | Module maturity: vertical proof / generalize / harden. | — |
| **Subsystem** | A cohesive part of a module (e.g. CooldownSystem inside Combat) that is not its own build target. | — |
| **Tick domain** | Update rate class under the 60 Hz master clock: 60, 30, 20, or 10 Hz. | Engine `Simulation` |
| **WorldConnectionRole** | Client connection role toward a simulation: `Primary`, `WarmCandidate`, `Dormant`, `Retiring`. | Platform `Realtime` |
| **WorldSession** | Stable, logical client session with the world, independent of which simulation is currently authoritative. | Platform `Realtime` |

## Third-party libraries

| Library | Role |
| --- | --- |
| Flecs | ECS; authoritative simulation state/rule graph. |
| Jolt Physics | Physics world, collision, character controller, queries; collision cooking. |
| GLM | Foundational math types. |
| bgfx (bx, bimg) | Rendering backend. |
| GLFW | Window/OS input (client/editor only). |
| Dear ImGui | Developer/editor UI (never player-facing, never headless). |
| RmlUi | Player-facing UI framework. |
| BehaviorTree.CPP | AI behavior-tree runtime behind Engine `AI`. |
| Recast / Detour | Navmesh building (Cooker) / runtime path queries (Navigation). |
| fastgltf, meshoptimizer, MikkTSpace, KTX, Zstd | Cooker import/optimization/compression. |
| Protobuf, gRPC, NATS | Contracts schemas and service messaging (Platform). |
| Agones, Kubernetes | Game-server process capacity and lifecycle (Platform `Orchestration`). |
