# 01 — Architecture overview

The project is a server-authoritative MMORPG built from three independent Git
repositories that are developed side by side:

| Repository | License | Language | One-line role |
| --- | --- | --- | --- |
| **RunicEngine** | MIT (public) | C++20 | Reusable real-time and game *mechanisms*. |
| **RunicPlatform** | MIT (public) | Go + C++ bindings | Reusable networking, service, persistence, and distributed *infrastructure*. Formerly named RunicFabric. |
| **RunicGame** | Proprietary | C++ + Go | The actual MMORPG: every *rule, policy, content definition, and presentation*. |

The repositories are expected as siblings (`../RunicEngine`, `../RunicPlatform`,
`../RunicGame`). RunicGame consumes the other two; neither public repository may
depend on RunicGame.

## Non-negotiable invariants

These hold across every module and every repository. [J intro, J.7.2, J.8.2]

1. **Server-authoritative C++ world simulation.** Clients speculate and present;
   they never decide hits, damage, quest completion, trade validity, or durable
   ownership.
2. **60 Hz master scheduling** with **30 / 20 / 10 Hz** tick domains.
3. **Flecs owns gameplay state.** Flecs is the authoritative simulation
   state/rule graph — not the database, geometry engine, distributed
   coordinator, or renderer.
4. **Jolt is geometry and physics, not gameplay rules.** Physics answers "what
   was intersected"; Combat and Game decide what it means.
5. **One logical seamless world.** Region/Sector/Cell describe the world; which
   process simulates it is a separate, operational concern.
6. **Platform owns networking.** Engine exposes a narrow adapter; it never
   implements sockets, transports, or service clients.
7. **Headless simulation is a first-class target.** The authoritative server
   builds and runs without GLFW, bgfx, RmlUi, ImGui, client audio, or the editor.
8. **External I/O never blocks a simulation tick.** gRPC, NATS, Agones, and
   database work complete off-thread and enqueue typed work that is applied only
   at explicit safe tick boundaries.
9. **Exactly one simulation owns mutable authority** for a transferable entity at
   any time. Local Flecs entity IDs never cross a process boundary;
   `GlobalEntityId` is the stable cross-simulation identity.

## Decisions introduced by Appendix J

| Decision | Summary | Source |
| --- | --- | --- |
| Substantial modules | A *module* is a meaningful CMake target or Go service/package with its own public boundary, tests, `ARCHITECTURE.md`, and build file. Small systems (CooldownSystem, DamageSystem, AI perception, DebugRenderer, physics queries) are **subsystems** inside a larger module unless build isolation, independent deployment, or a stable reusable ABI justifies extraction. Narrow seams such as NetworkAdapter are the allowed exception. | J.1.1 |
| Flat public include layout | `Physics/Public/PhysicsWorld.hpp`, not `Public/Runic/Physics/...`. File names are explicit (no generic `API.hpp`/`Types.hpp`). Namespaces stay explicit (`Runic::Physics`). | J.1.2 |
| Domain services move to RunicGame | Platform keeps transport, contracts, service runtime, persistence primitives, idempotency, world control, orchestration. **Concrete** inventory, quest, trade, auction, economy, guild, mail services and their rules are private RunicGame server code. | J.1.3 |
| New Engine modules | `Events`, `DistributedSimulation`, `GameplayFlow`, `Quest`. | J.1.4 |

## The three-way rule [J.2.1]

- **Engine** provides mechanisms that can affect authoritative gameplay.
- **Platform** provides mechanisms that cross a process, machine, service,
  orchestration, or persistence boundary.
- **Game** defines every actual RunicGame policy, rule, quest, sequence, boss
  behavior, economy rule, content definition, and presentation.

```text
RUNICGAME
actual MMORPG semantics
        |
        | uses reusable mechanisms
        v
RUNICENGINE
realtime/game mechanisms
        |
        | adapters + contracts
        v
RUNICPLATFORM
transport + distributed infrastructure
```

Quick test for placing new code:

| If the code… | It belongs in |
| --- | --- |
| would be useful to a *different* game and runs inside a simulation or client | RunicEngine |
| would be useful to a different game and moves bytes/state across processes, stores durable data, or manages processes | RunicPlatform |
| encodes *how RunicGame plays* (numbers, rules, content, scripts, UI screens, economy) | RunicGame |
| is a reusable primitive first discovered while writing Game code | RunicGame first; extract to Engine/Platform **only after it proves generic** [J.6.2 GameWorldGameplay] |

## Repository ownership [J.1.3]

| Repository | Owns | Does not own |
| --- | --- | --- |
| RunicEngine | Reusable real-time/game mechanisms: ECS integration, world model, physics, simulation, gameplay events, combat framework, AI framework, water, quests, gameplay flow, rendering, client runtime, distributed-simulation semantics, tooling. | Concrete RunicGame rules/content; transport implementation; database/service infrastructure. |
| RunicPlatform | Networking implementation and contracts; client↔simulation and simulation↔simulation transport; service runtime; persistence primitives; identity; world-control plane; Agones/Kubernetes integration; observability. | RunicGame economy/quest/inventory/auction policy; combat/gameplay rules; client presentation. |
| RunicGame | Closed-source MMORPG semantics, content, server rules, C++ simulation extensions, Go domain-service implementations, client presentation/UI, boss/quest scripts, economy and anti-exploit policy. | Generic engine or platform infrastructure that another game should be able to reuse. |

## Complete project map (target) [J.2.2]

```text
RunicEngine/
└── Engine/
    ├── Core/
    ├── Application/
    ├── Events/
    ├── Content/
    ├── ECS/
    ├── World/
    ├── Physics/
    ├── Simulation/
    ├── DistributedSimulation/
    ├── Assets/
    ├── Renderer/
    ├── Client/
    ├── Animation/
    ├── Audio/
    ├── UI/
    ├── Navigation/
    ├── Gameplay/
    ├── GameplayFlow/
    ├── Quest/
    ├── Combat/
    ├── AI/
    ├── Water/
    ├── NetworkAdapter/
    ├── Cooker/
    └── Editor/

RunicPlatform/
├── Contracts/
├── Realtime/
├── ServiceRuntime/
├── Persistence/
├── WorldControl/
├── Orchestration/
├── Identity/
└── Observability/

RunicGame/
├── Shared/
├── Client/
├── Server/
│   ├── Simulation/
│   └── Services/
├── Content/
└── Assets/
```

This is the **target** shape. Each repository's `docs/current-state.md` records
how far the actual tree is from it.

## Feature ownership examples [J.2.3]

| Feature | Engine mechanism | Platform mechanism | Game semantics |
| --- | --- | --- | --- |
| Combat | Ability/hit/effect/cooldown/CC framework; physics queries; events. | Realtime delivery/replication only. | Skills, formulas, class rules, PvP/PvE tuning. |
| Quest | Quest runtime, objectives, triggers, GameplayFlow, world-interaction APIs. | Service runtime, contracts, durable persistence primitives. | Quest definitions, branching, rewards, story scripts, private QuestService behavior. |
| Inventory | Runtime item/inventory/equipment model. | Transactions, persistence, idempotency, authenticated service transport. | Stack/bind/equipment/trade/loot rules; InventoryService domain logic. |
| World | Region/Sector/Cell, spatial queries, streaming vocabulary. | Simulation directory, authority directory, load balancing, orchestration. | Continents, regions, territories, world rules. |
| Cross-sim combat | Ghost/authority semantics and foreign combat contracts. | Sim-peer transport, sequencing, topology, connection management. | Concrete ability/damage rules. |
| Auction | Normally nothing beyond item/currency types. | Service/persistence/transaction APIs. | Listing/bid/fee/tax/market/anti-abuse rules and server implementation. |
| Cutscene | GameplayFlow + restrictions + authoritative sequence state. | Transport/replication if needed. | When it runs, cinematic content, camera/audio/UI presentation. |

## Process composition (derived)

| Process | Built from | Notes |
| --- | --- | --- |
| Authoritative simulation server (headless) | RunicGame `Server/Simulation` + headless Engine modules + Platform realtime/identity bindings | One process per simulation; many simulations form one logical world. |
| Game client | RunicGame `Client/` + Engine client/presentation modules + Platform realtime client binding | Connects **directly** to authoritative simulations for realtime play. |
| Domain services (Go) | RunicGame `Server/Services/*` + Platform ServiceRuntime/Persistence/Identity | Outside the realtime path. |
| Control plane (Go) | Platform WorldControl + Orchestration | Maps the logical world onto running simulations; uses Agones/Kubernetes for capacity. |
| Editor / Cooker | Engine `Editor`, `Cooker` | Developer tools; never a dependency of the server. |

Realtime movement and combat never pass through a Go proxy:
`client -> C++ authoritative simulation`.

## Relationship to the v2.1 reference [J.15]

Appendix J should be read with v2.1 sections A.3.3 (architecture axes and process
composition), B/C (Flecs, world, physics, scheduling), D (Platform networking,
handoff, service fabric), E (Combat technical contract), F (Quest/world-interaction
bridge), G (Renderer/UI), and H (vertical-slice development). It preserves those
mechanisms except for the ownership refinement that concrete domain services are
private Game server modules built on reusable Platform APIs. The subsystem
references keep their internal contracts; Appendix J maps them onto the module
graph. Those documents are not yet in the repositories ([OQ-12](11-open-questions.md#oq-12)).
