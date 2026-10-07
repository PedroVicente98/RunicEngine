# 03 — Dependency rules

## Cross-repository direction

```text
RunicGame ──► RunicEngine
RunicGame ──► RunicPlatform
RunicEngine (NetworkAdapter only) ◄──► RunicPlatform (Realtime binding)   see OQ-1
RunicEngine ──X──► RunicGame        (forbidden)
RunicPlatform ──X──► RunicGame      (forbidden)
```

- Neither public repository may include, link, import, or name RunicGame code.
- Engine and Platform meet only at the **NetworkAdapter ↔ Realtime** seam and at
  shared **Contracts**. The exact direction of that binding is undecided
  ([OQ-1](11-open-questions.md#oq-1)); until it is decided, do not add any other
  Engine↔Platform dependency.
- Engine must run locally and headless without production infrastructure:
  no Agones, Kubernetes, database, gRPC, or NATS in Engine. [J.11.2]

## Engine dependency graph [J.11.1]

The diagram is **directional and conceptual, not a total ordering**. The key rule
is that low-level mechanisms must not depend on the high-level features that
consume them. Where registration is needed, the feature module calls into a
Simulation/registry interface; Simulation never hard-codes the feature.

```text
                               Core
                                |
        +-----------+-----------+-----------+
        v           v                       v
  Application     Content                  Events
                    |                       |
                    v                       v
                   ECS ------------------> World
                                            |
                       +--------------------+------------------+
                       v                    v                  v
                    Physics            Navigation           Assets
                       |                                       |
                       +------------------+                    |
                                          v                   |
                                      Simulation               |
                                          |                   |
             +----------------------------+-------------------+---------+
             v                    v                 v                    v
          Gameplay              Combat            AI                  Water
             |                    |                 |                    |
             +----------+---------+-----------------+--------------------+
                        v
                  GameplayFlow
                        |
                        v
                      Quest

Simulation + World + Events
          |
          v
DistributedSimulation
          |
          v
NetworkAdapter ---> RunicPlatform Realtime

Assets ---> Renderer / Animation / Audio / UI ---> Client

Editor is a leaf consumer of many Engine modules and must not be a headless dependency.
```

### Allowed dependencies per Engine module (authoritative list)

The per-module catalog is more precise than the diagram. When they disagree, the
table below (from J.4) wins and the disagreement is logged in
[11-open-questions.md](11-open-questions.md).

| Module | Allowed dependencies [J.4.x] | Headless? |
| --- | --- | --- |
| Core | C++ standard library; GLM only as the accepted foundational math dependency | yes |
| Application | Core. Optional OS/window adapters are downstream or private. | yes (adapters are client-only) |
| Events | Core. (Simulation integrates ownership and tick boundaries.) | yes |
| Content | Core. (Assets may consume Content IDs; avoid cycles.) | yes |
| ECS | Core; Flecs (private or public only as deliberately chosen) | yes |
| World | Core, ECS, Content where needed | yes |
| Physics | Core, World; Jolt (private backend) | yes |
| Simulation | Core, ECS, Events, World, Physics, Content | yes |
| DistributedSimulation | Core, Events, World, Simulation | yes |
| Assets | Core, Content | yes (package selection is client/server aware) |
| Renderer | Core, Assets, World as needed; bgfx (private backend) | **no** |
| Client | Application, Events, Simulation/client simulation surface, NetworkAdapter, Renderer, Assets; presentation modules | **no** |
| Animation | Core, Assets | **no** (presentation) |
| Audio | Core, Assets; backend private | **no** |
| UI | Core, Application, Assets; RmlUi private/backend-facing | **no** |
| Navigation | Core, World; Detour backend | yes |
| Gameplay | Core, ECS, Events, Content, World, Simulation registration surface | yes |
| GameplayFlow | Core, Events, Gameplay, Simulation, Content | yes |
| Quest | Core, Events, Gameplay, GameplayFlow, Content, Simulation | yes |
| Combat | Core, Events, Gameplay, Physics, World, Simulation, Content; DistributedSimulation for foreign interactions | yes |
| AI | Core, Events, Gameplay, Navigation, World, Simulation, Content | yes |
| Water | Core, World, Physics, Gameplay, Simulation | yes |
| NetworkAdapter | Core, Events, Simulation, DistributedSimulation; Platform contract/binding package at the integration boundary | yes |
| Cooker | Core, Content, asset-format definitions, World data definitions; tool-only fastgltf, meshoptimizer, MikkTSpace, KTX, Jolt cooking, Recast, Zstd | tool (not linked by server/client) |
| Editor | Application, Renderer, ECS, World, Physics, AI, Water, Content, Assets, Simulation, diagnostics | **no** (leaf) |

### Platform module dependencies [J.5]

| Module | Allowed dependencies |
| --- | --- |
| Contracts | Foundation for every Platform module and for RunicGame service/client bindings. |
| Realtime | Contracts, Identity, Observability; integrates Engine NetworkAdapter. |
| ServiceRuntime | Contracts, Identity, Observability; used by RunicGame `Server/Services`. |
| Persistence | Observability; consumed by private Game services. |
| WorldControl | Contracts, Realtime control channels, Orchestration interface, Identity, Observability. |
| Orchestration | Observability; provides capacity to WorldControl. |
| Identity | Contracts; consumed by Realtime, ServiceRuntime, Game services. |
| Observability | May be depended on by all Platform modules without creating gameplay dependencies. |

## Critical forbidden edges [J.11.2]

| Forbidden dependency | Reason |
| --- | --- |
| Physics → Combat/Game | Physics answers geometric/physical questions; it must not know damage, faction, block, dodge, or abilities. |
| Renderer → Simulation mutation | Renderer observes presentation state; rendering cannot become authoritative game logic. |
| Simulation → concrete RunicGame feature code | Game registers/extends systems; Engine simulation stays reusable. |
| AI node → direct combat/physics/persistence mutation | AI emits intents and uses approved APIs; it does not bypass authority pipelines. |
| Quest/Game scripts → Jolt/bgfx/network backend | Scripts call Engine mechanisms; backend libraries remain encapsulated. |
| Engine → Agones/Kubernetes/database | Engine semantics must run locally/headless without production infrastructure. |
| Platform → RunicGame combat/economy/quest policy | Platform remains reusable infrastructure. |
| Game Client → server-only rule implementation | Client gets presentation data and endpoint contracts, not hidden authoritative logic. |
| Region/Sector/Cell → SimulationId as identity | Logical world partition must not be hard-bound to process topology. |

Additional rules stated elsewhere in Appendix J:

- Application layers must not order movement, combat, physics, AI, replication,
  or quest consumers. [J.4.2]
- Simulation, Jolt, combat, AI, and networking never issue bgfx commands. [J.4.11]
- Service callbacks never mutate Flecs directly; results are queued for a later
  safe tick. [J.14.4]
- Editor must never become a dependency of a headless target. [J.11.1]

## Headless boundary [J.14.6]

The authoritative server must compile and run **without** GLFW, bgfx, RmlUi,
ImGui, client audio, or editor linkage. These must stay headless-compatible:
Application host, Simulation, Events, World, Physics, Gameplay, Combat, AI,
Quest, GameplayFlow, DistributedSimulation, NetworkAdapter, and the required
Platform bindings.

## Third-party placement

| Library | Owning module | Visibility | Status |
| --- | --- | --- | --- |
| GLM | Core (foundational math) | public | pinned (1.0.1) |
| Flecs | ECS | private or deliberately public | pinned (4.0.4) |
| Jolt | Physics (runtime); Cooker (collision cooking) | private | pinned (5.2.0) |
| bgfx (+bx, bimg) | Renderer | private | pinned |
| GLFW | Application OS/window adapter, client/editor only (proposal, [OQ-4](11-open-questions.md#oq-4)) | private | pinned (3.4) |
| Dear ImGui | Editor / debug tooling (proposal, [OQ-6](11-open-questions.md#oq-6)) | private | pinned (1.91.8, core only) |
| RmlUi | UI | private | not yet added |
| BehaviorTree.CPP | AI | private | not yet added |
| Detour | Navigation | private | not yet added |
| Recast | Cooker | tool-only | not yet added |
| fastgltf, meshoptimizer, MikkTSpace, KTX, Zstd | Cooker | tool-only | not yet added |
| Audio backend | Audio | private | not chosen |
| Protobuf | Platform Contracts | — | deferred |
| gRPC, NATS | Platform ServiceRuntime | — | deferred |
| Database driver(s) | Platform Persistence | — | deferred |
| Agones SDK / Kubernetes | Platform Orchestration | — | deferred |
