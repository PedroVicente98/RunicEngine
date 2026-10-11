# Glossary

This file defines, in alphabetical order, the terms used across `docs/system/` and the module contracts.

| Term | Meaning | Owner |
| --- | --- | --- |
| **Ability toolkit** | Timeline runner (startup, active, recovery, cancel windows in DomainTickCount), hit volumes and projectile queries, cooldowns, resource pools and the one-slot input queue. Games supply the abilities | Engine Combat |
| **Accepted result** | A service's answer to a durable operation, queued back to the simulation and applied at a later safe tick through the service port's inbox | Engine Simulation (port), Platform bridge |
| **Admission** | Per-world capacity limits, a login queue and cohort waves (PCS-6, slice 44+). When and how a game uses them is a Game decision | Platform Core services |
| **Anchored element** | A world-UI mode: an RmlUi element that follows an entity on screen, scaled with distance | Engine UI |
| **Application event** | Key press, resize, UI focus, connection state. Delivered through the LayerStack top-down; a layer can mark it handled | Engine Application |
| **ApplicationHost** | Runs an Application's loop (frame, fixed 60 Hz or job loop) over its LayerStack | Engine Application |
| **ARCH** | *RunicEngine — Technical Architecture v3*, the single source of truth this folder is generated from | — |
| **Authority** | The right to mutate an entity's authoritative state. Exactly one simulation holds it at any moment | Engine DistributedSimulation |
| **Authority epoch** (`AuthorityEpoch`) | Counter that changes when authority moves; messages with a stale epoch are rejected or dropped. The plain type lives in Core | Engine Core (type), DistributedSimulation (semantics) |
| **Authority partition** (`AuthorityPartition`) | An operational area with an owner and an epoch inside one region, used to split a busy city between simulations. Not a world-partition concept (DSIM-5) | Engine DistributedSimulation |
| **Barrier** | The point between phases where every worker has finished; per-worker buffers merge in a fixed order and events flush | Engine Simulation |
| **Batched (world UI)** | A world-UI mode: labels and bars drawn by one instanced Renderer call from an atlas RmlUi renders | Engine Renderer, UI |
| **Behavior registry** | ID-to-C++ registration with parameter schemas, a fixed hook list and read-only context plus output buffers; serves abilities, item effects, AI nodes and interactions | Engine Combat |
| **BrainHandle** | Lightweight handle stored in Flecs that refers to a behavior-tree brain owned by the AI runtime; 0 while sleeping | Engine AI |
| **BuoyantBody** | Component that makes a Jolt dynamic body float; separate from any movement component | Engine Water |
| **Candidate connection** | A client connection to a neighbor simulation, prepared before a border crossing; up to three. Roles: Primary, WarmCandidate, Dormant, Retiring | Platform Transport, Engine Network |
| **CappedRewind** | Hit timing that tests hits against a past position within a cap, using a per-character position history; chosen per ability. Full rewind is not offered | Engine Combat |
| **Cell** | Automatic fine spatial bucket (for example 64 m), sparse; used for candidate lookup by combat, AI, interest, light and map knowledge | Engine World |
| **CharacterVirtual** | Jolt's character controller, used for players and combat NPCs (not a rigid body); updated every 30 Hz tick | Engine Physics |
| **Command** | A request to attempt an action, from a player, AI intent or GameplayFlow action. Validated by the authoritative simulation when applied | Engine Simulation |
| **Connect ticket** | Signed, short-lived token a client presents first to its simulation; checked locally (signature, expiry and key ID against a configured set of public keys, PTR-2). Test-signed with a local key until PCS-1, which signs with rotating keys whose public halves the simulations load. Its format is a Contracts message | Platform Core services, Transport |
| **ContentId** | Namespaced ID of a data definition: `engine:` for Engine built-ins, `game:` for Game content | Engine Content |
| **Contract session** | The first session of a package: writes `ARCHITECTURE.md`, the public headers and the failing acceptance tests; no `Source/` code | Process |
| **Contracts** | Platform module of Protobuf service APIs, published facts and the connect-ticket format, generated for C++ and Go (PCT-1). Never carries per-tick traffic; realtime messages are encoded by Runic::Network | Platform Contracts |
| **Cooker** | Development-only executable that turns authored assets and world data into Runic-native client and server packages | Engine Tools |
| **Currency wallet** | Balances and atomic transfers with operation IDs (PBB-2), plus one-transaction transfers that move ledger items and wallet currency together (PBB-3); mechanisms only | Platform Building blocks |
| **Debug-stream registry** | Core registry (CORE-2) where modules publish traces; the Diagnostics layer streams them to the Editor in development and staging builds | Engine Core |
| **Debug pipeline** | The first render pipeline: one toggled layer per kind of RenderData, drawn simply | Engine Renderer |
| **DebugGeometry** | Plain lines, triangles, text and cached debug meshes in Core RenderData; filled by any system (Physics included) and drawn by the Renderer | Engine Core |
| **Defense intent** | Game-neutral AI intents BeginDefense, EndDefense and Evade, each carrying a game-registered `DefenseId`; the Game maps the ID to its own defensive rules (AI-4) | Engine AI |
| **Degradation order** | Fixed order applied beyond the bandwidth budget: lower distant update rates, then less cosmetic detail, then a cap on visible effects. Combat correctness never degrades | Engine Network |
| **Diagnostics layer** | GameServer layer that records network and replication metrics in the Core metrics registry and, in development and staging builds only, hosts the Editor attach channel over a development socket transport, streaming debug shapes and the traces in Core's debug-stream registry (NET-7) | Engine Network |
| **Diegetic panel** | A world-UI mode: an RmlUi document rendered into a texture on a surface in the world, with input by ray | Engine UI, Renderer |
| **DomainTickCount** | A tick count tagged with its clock domain. An untagged tick count is a bug | Engine Simulation |
| **Durable fact** | A fact that must reach a durable service, carried with a stable ID through the service port and deduplicated by the service | Engine, Platform, Game services |
| **Entry point** | The Engine-owned `main()`, which calls the executable's `CreateApplication()` | Engine Application |
| **Extractor** | A registered function that reads the client world and domain APIs read-only each frame and writes RenderData into the RenderFrame; `engine:` or `game:` IDs | Engine Client (registry), Game (its extractors) |
| **Fact** | Something the authority decided happened (EntityKilled, DamageEvent). Delivered to every consumer through tick-safe buffers; never handled away | Engine Events |
| **Foreign interaction** | A request sent to the simulation that owns the target so it resolves the effect itself; foreign combat uses ForeignCombatVolume then RemoteHitResult | Engine DistributedSimulation, Combat |
| **GameplayAPI** | The group and umbrella target `Runic::GameplayAPI` linking Gameplay, Combat, GameplayFlow and Quest | Engine |
| **GameplayFlow** | The one authoritative runtime for scripted gameplay: sequences of trigger, conditions, actions and state transition | Engine GameplayFlow |
| **Ghost** | Read-mostly mirror of an entity owned by another simulation near a border; never decides outcomes for it | Engine DistributedSimulation (state), Network (encoding) |
| **GlobalEntityId** | Stable entity identity across processes. Local Flecs IDs never leave the process | Engine Core (type), DistributedSimulation (mapping) |
| **GlobalPlayerId** | Plain player ID type in Core (CORE-1), issued by the accounts service (PCS-1) | Engine Core (type), Platform Core services (issue) |
| **Group** | A folder plus an `INTERFACE` umbrella target that links related modules; no code of its own | Build |
| **HandoffPolicy** | How a sequence behaves when its owner's authority moves: Transferable, PinAuthority or AbortOnTransfer | Engine GameplayFlow |
| **Headless** | Built and run without GLFW, bgfx, RmlUi, ImGui, an audio backend or editor code. Mandatory for the server | All |
| **Hysteresis** | Separate prepare, commit and release thresholds so small movement around a border or layer boundary never flips loading or ownership back and forth | Engine World |
| **Idempotency** | Re-running the same operation (same operation ID or event ID) has no additional effect | Platform Go SDK (store), services (domain meaning) |
| **Instance** | A separate copy of reality (a dungeon, a raid), allocated from Agones fleets; different from a process boundary | Platform Core services |
| **Intent** | What an AI brain wants to attempt (MoveTo, UseAbility…); becomes an ordinary command validated like player input | Engine AI |
| **Interest** | Per-client selection of relevant entities from cells around the client, with priority, budgets and visibility tiers | Engine Network |
| **Interface target** | A header-only Engine target, with Core types only, for an interface the Platform implements: `Runic::NetworkTransport` (transport interface and connect-ticket hook, NET-1) and `Runic::SimulationPort` (service port, SIM-5) | Engine Network, Simulation |
| **Item ledger** | Item instances, containers, slots, atomic transfers and operation IDs (PBB-1); mechanisms only | Platform Building blocks |
| **Layer (application)** | A coarse feature of an executable on the LayerStack (Network, Simulation, Presentation…). Not a spatial layer | Engine Application |
| **Layer (spatial)** | See SpatialLayer | Engine World |
| **LayerStack** | Ordered stack of layers and overlays; events top-down, updates bottom-up; push, pop and replace applied at one safe point per loop | Engine Application |
| **LifecycleLayer** | Platform server binding that links the Agones SDK (ready, health, shutdown), with a local stub mode for development without Agones (PBND-2) | Platform Server bindings |
| **Make-before-break** | Handoff style: the destination and the client's candidate connection are prepared before authority commits | Engine DistributedSimulation, Platform Transport |
| **Metrics registry** | Per-tick timings and counters in Core (CORE-2). Every process records into it; the Platform exporter (PBND-3) publishes it to Prometheus | Engine Core (registry), Platform Server bindings (export) |
| **Movement-mode registry** | Gameplay registry of movement modes (GPL-1): Gameplay owns walk, sprint, dash, glide and mount; other modules register theirs, as Water registers swimming | Engine Gameplay |
| **Module** | A CMake target (or Go package) with Public/, Source/, Tests/ and `ARCHITECTURE.md`; the unit of ownership and dependency | Build |
| **Operation ID** | Stable ID on a durable operation so retries commit once | Engine service port, Platform |
| **Phase** | An ordered stage of a simulation tick; phases run in a fixed order separated by barriers | Engine Simulation |
| **Preload band** | The range in which a layer or cell starts streaming before it becomes relevant; wider than its logical bounds | Engine World |
| **Presentation cue** | Server-emitted signal telling clients to play camera, animation, dialogue, audio or UI; carries no authority | Engine GameplayFlow (emit), Game client (plays) |
| **Production pipeline** | The later render pipeline (materials, shaders, lighting, VFX) reading the same RenderFrame (RND-6) | Engine Renderer |
| **Region** | Authored content area with gameplay state; a Flecs entity. Never encodes which simulation owns it | Engine World (mechanism), Game (content) |
| **RenderData** | Plain render structs in Core (`Runic::RenderData`): frame packet, debug geometry, instances, anchors, lights, environment, volumes, fields, water | Engine Core |
| **RenderFrame** | The double-buffered frame packet extraction writes and the Renderer and world UI read; positions are 32-bit relative to one 64-bit origin | Engine Core |
| **Replication registry** | Simulation registry (SIM-6) where modules register replicated components and world-state values with a schema, quantization profile and relevance; Network encodes only what is registered | Engine Simulation |
| **Restriction (source-owned)** | A set of gameplay restrictions (input, movement, abilities, interactions, targeting, damage, external motion) acquired under a source ID and released by source | Engine Gameplay |
| **Rule pipeline** | Typed conditions and modifiers evaluated in named stages and compiled into a resolved loadout per character | Engine Combat |
| **Safe tick / safe point** | A defined point where queued work (service results, layer changes, worker-count changes, tree reloads) is applied without racing the simulation | Engine Simulation, Application |
| **Sector** | Automatic coarse spatial bucket (for example 512 m) for streaming and coarse lookup | Engine World |
| **ServerTime** | Default hit timing: hits are tested at the server's current time | Engine Combat |
| **Service port** | The Simulation's only door to durable services: an outbox of operations with stable IDs and an inbox of accepted results drained at safe ticks (SIM-5); shipped as the header-only `Runic::SimulationPort` target | Engine Simulation |
| **ServicesBridgeLayer** | Platform server binding that implements the service port: asynchronous gRPC calls (PBND-1) and NATS publish and subscribe (PBND-4) | Platform Server bindings |
| **SimulationId** | Identity of one simulation process. Part of every stable event ID; never the identity of a Region, Sector or Cell | Engine Core (type) |
| **Simulation link** | A simulation-to-simulation connection carrying ghost deltas and transfer messages, encoded by Network over the Platform transport | Engine Network, Platform Transport |
| **SimulationOwner** | The runtime assignment of which process is authoritative for an area now | Platform World Coordinator |
| **Slice** | A roadmap step that ends in something runnable and testable; lettered slices (20a…) were inserted later | Roadmap |
| **Snapshot** | Per-client delta of entity state against the last acknowledged state, quantized | Engine Network |
| **Snapshot contributor** | A module that registers its state (sequences, quests, ship groups) into DistributedSimulation's registry so it travels in the final transfer snapshot | Engine DistributedSimulation (registry) |
| **SpatialLayer** | A per-world streaming and population regime with the same horizontal footprint and its own logical and preload height envelopes | Engine World |
| **Stage A / B / C** | Maturity: vertical proof / generalize / harden. Every package starts at A | Process |
| **Subsystem** | A folder inside a module's `Source/`; not linkable on its own | Build |
| **TickContext** | What a system receives each tick: domain, master tick, domain tick, dt, simulation time | Engine Simulation |
| **TickId** | Plain tick identifier type | Engine Core |
| **Tick domain** | One of the clock rates: Hz60 master, Hz30 realtime, Hz20 AI, Hz10 maintenance | Engine Simulation |
| **Transfer** | Moving an entity's authority: prepare, final snapshot, authority commit (DSIM-3) | Engine DistributedSimulation |
| **Visibility tier** | Per-pair PvP visibility: Full, Silhouette or Hidden, from luminosity, concealment and distance; reveal on contact. Combat never uses tiers | Engine Perception (tiers), Network (applies) |
| **WaterSample** | The fact a water query returns: surface position and normal, flow, depth, submersion, body | Engine Water |
| **Window** | A class in Application (GLFW), compiled only in client and tools builds; not a module | Engine Application |
| **Work package** | One Claude Code session and one pull request inside one module: write scope, interface change, Done when, Needs, Slice | Process |
| **World anchor** | RenderData entry saying where a nameplate, prompt or panel belongs, with distance, visibility tier and priority | Engine Core, Client |
| **World Coordinator** | Platform service owning the region-to-simulation map, neighbor topology, handoff candidates, drains, placement and the world epoch (PCS-2) | Platform Core services |
| **World directive** | A world-wide directive (for example a weather event) with an effective world time, so every owning simulation applies it on the same tick | Platform Core services |
| **World epoch** | The start point world time counts from; issued by the World Coordinator (PCS-2), read from configuration until then (ENV-1) | Platform Core services (issue), Engine Environment (use) |
| **WorldSession** | The player's session, which survives handoffs: one authoritative connection plus candidate connections | Engine Network, Platform Transport |
