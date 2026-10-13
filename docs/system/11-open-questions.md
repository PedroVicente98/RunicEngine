---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# 11 — Open questions and decisions

This file lists the architecture questions that are still open, and records the outcome of every question already decided.

Each open item needs an owner decision before the code that depends on it is written. A **proposal** is a suggested
default, not a decision. **Never resolve an open item silently in code**: ask the owner.

When an item is decided, move it to [Decided](#decided) with the date and outcome, record the decision in ARCH[^arch], and
update the affected module contracts in all three repositories.

| ID | Topic | Blocks |
| --- | --- | --- |
| [OQ-3](#oq-3) | Who performs asset loading for streamed cells, and why ASSET-2[^engine-assets] needs WORLD-3[^engine-world] | ASSET-2 contract session (slice 21) |
| [OQ-9](#oq-9) | Canonical Go module path for RunicPlatform | PCT-1[^platform-contracts] Go code generation (slice 12) and every Go package after it |

---

<a id="oq-3"></a>
## OQ-3 — Asset loading for streamed cells (narrowed)

**Context.** Runic::Assets[^engine-assets] depends only on Core and Content (catalog row in
[03](03-dependency-rules.md#engine-modules)). World's activation volumes (WORLD-3[^engine-world]) emit load and unload requests per
cell. ASSET-2 (random-access packages, asynchronous streaming) lists WORLD-3 in its Needs, but Assets may not link
World.

**Still open.** Who performs the load (the Client, or the server runtime) and why ASSET-2 "needs" WORLD-3. Both must be
confirmed in the ASSET-2 contract session.

**Proposal.** The catalog wins: no World → Assets edge, and Assets keeps depending only on Core and Content. WORLD-3
emits what to load; a consumer above both performs the load.

<a id="oq-9"></a>
## OQ-9 — Platform naming: canonical Go module path

**Decided part.** The Platform module split is Transport (C++), Contracts, Server bindings[^platform-serverbindings] (C++), Go SDK[^platform-gosdk], Core services[^platform-coreservices]
(Go), Building blocks[^platform-buildingblocks] (Go) and Infrastructure ([03 § Platform modules](03-dependency-rules.md#platform-modules)).
Platform C++ targets are named `RunicPlatform::<Module>`.

**Still open.** The canonical Go module path: the placeholder `runic.local/platform` or the hosted path
`github.com/PedroVicente98/RunicPlatform`.

**Proposal.** None recorded. The owner decides before the first Go code is generated or written.

---

## Decided

| ID | Topic | Decided |
| --- | --- | --- |
| [OQ-1](#oq-1) | Network transport binding direction | 2026-10-09 |
| [OQ-2](#oq-2) | Where shared ID and time types live | 2026-10-09 |
| [OQ-4](#oq-4) | OS window and input layer | 2026-10-09 |
| [OQ-5](#oq-5) | Debug module | 2026-10-09 |
| [OQ-6](#oq-6) | Dear ImGui ownership | 2026-10-09 |
| [OQ-7](#oq-7) | Test framework and test location | 2026-10-09 |
| [OQ-8](#oq-8) | Engine repository layout | 2026-10-09 |
| [OQ-10](#oq-10) | Agones SDK inside the simulation process | 2026-10-09 |
| [OQ-11](#oq-11) | RunicGame target layout | 2026-10-09 |
| [OQ-12](#oq-12) | Source documents | 2026-10-09 |
| [OQ-13](#oq-13) | RunicFabric → RunicPlatform rename | 2026-10-07 |
| [OQ-14](#oq-14) | How Game simulation code is linked into the server | 2026-10-09 |
| [OQ-15](#oq-15) | What the client runs for prediction | 2026-10-09 |
| [OQ-16](#oq-16) | Quest storage placement | 2026-10-09 |
| [OQ-17](#oq-17) | Client-safe versus server-only content packaging | 2026-10-09 |
| [OQ-18](#oq-18) | GameplayFlow[^engine-gameplayflow] built-in actions versus its dependency list | 2026-10-09 |
| [OQ-19](#oq-19) | GLM in public headers | 2026-10-11 |

<a id="oq-1"></a>
### OQ-1 — Network transport binding direction (decided 2026-10-09)

**Question.** Which side defines the realtime transport interface, and which side depends on whom?

**Outcome.** Engine `Runic::Network`[^engine-network] owns the transport interface; Platform Transport[^platform-transport] implements it. Platform C++
depends on Engine public headers, never the reverse. There is no NetworkAdapter module.

Refined in ARCH[^arch] revision 306: the transport interface and the connect-ticket hook ship as the header-only
`Runic::NetworkTransport` target (Core types only), and Platform Transport links only that target and Contracts.

Applied in: [03 § Engine modules](03-dependency-rules.md#engine-modules), [04 § Network channels and delivery classes](04-events-and-messages.md#network-channels-and-delivery-classes), packages NET-1 and PTR-1.

<a id="oq-2"></a>
### OQ-2 — Where shared ID and time types live (decided 2026-10-09)

**Question.** Where do `GlobalEntityId`, `SimulationId`, `TickId` and `AuthorityEpoch` live without creating a cycle,
and how are event IDs made stable?

**Outcome.** Plain ID and time types (GlobalEntityId, SimulationId, TickId, AuthorityEpoch) are in Core (CORE-1[^engine-core]).
Stable event IDs are SimulationId plus a sequence (EVT-1[^engine-events]). DistributedSimulation[^engine-distributedsimulation] owns authority semantics. Wire
encodings are in Platform Contracts[^platform-contracts] and Engine Network.

Refined in ARCH revision 306: `GlobalPlayerId` joins the CORE-1 plain ID types and PCS-1[^platform-coreservices] issues it. Runic::Network
encodes every realtime message, envelope included; Platform Contracts carry only service APIs, published facts and the
connect-ticket format.

Applied in: [04 § Stable event IDs](04-events-and-messages.md#stable-event-ids).

<a id="oq-4"></a>
### OQ-4 — OS window and input layer (decided 2026-10-09)

**Question.** Is there a Platform or Input module in the Engine for the OS window and raw input?

**Outcome.** Window is a class in Application (GLFW, client and tools builds only). Raw input becomes application
events. Input mapping lives in Client. There are no Platform or Input Engine modules.

Applied in: packages APP-2[^engine-application] and CLI-2[^engine-client].

<a id="oq-5"></a>
### OQ-5 — Debug module (decided 2026-10-09)

**Question.** Is there a separate Debug module in the Engine?

**Outcome.** No Debug module. The debug pipeline is in Renderer, DebugGeometry in Core RenderData, Physics extracts
Jolt debug geometry, inspectors are in Editor, and the Diagnostics layer is in Network.

Applied in: packages CORE-3, PHYS-3[^engine-physics], RND-3[^engine-renderer], EDT-2[^engine-editor], NET-7.

<a id="oq-6"></a>
### OQ-6 — Dear ImGui ownership (decided 2026-10-09)

**Question.** Which module owns ImGui, and where may it run?

**Outcome.** ImGui is private to UI (development overlay, client development builds) and Editor. It is never on
servers.

Applied in: packages UI-1[^engine-ui] and EDT-1.

<a id="oq-7"></a>
### OQ-7 — Test framework and test location (decided 2026-10-09)

**Question.** Where do tests live, and which framework is used?

**Outcome.** Location decided: module-local `Tests/` registered with CTest. ONE C++ framework for all three
repositories: **GoogleTest** (with GoogleMock), chosen by the owner on 2026-10-13 for BUILD-1[^doc-build-build-1] (slice 0) and pinned
by BUILD-1. Go uses `testing`.

Applied in: [02 § Tests live inside the module](02-module-contract.md#tests-live-inside-the-module).

<a id="oq-8"></a>
### OQ-8 — Engine repository layout (decided 2026-10-09)

**Question.** What is the Engine folder layout, and what happens to the existing aggregate targets?

**Outcome.** `Engine/<Group>/<Module>/{Public,Source,Tests}` plus `runic_add_module`. The transitional bundles
`Runic::Runtime` and `Runic::Presentation` stay until RunicGame links per-module targets (BUILD-1).

Refined in ARCH revision 369: the group folders are `Foundation`, `SimulationCore`, `GameplayAPI`, `WorldSystems`,
`Networking`, `Presentation` and `Tools`.

Applied in: [02 § Folder layout](02-module-contract.md#folder-layout).

<a id="oq-10"></a>
### OQ-10 — Agones SDK inside the simulation process (decided 2026-10-09)

**Question.** How does a GameServer talk to Agones without the Engine depending on it?

**Outcome.** The LifecycleLayer[^platform-serverbindings] (PBND-2) in Platform server bindings links the Agones SDK; GameServer composes it.

Refined in ARCH revision 306: PBND-2 also has a local stub mode for development without Agones.

Applied in: [01 § Application, layers and loops](01-architecture-overview.md#application-layers-and-loops).

<a id="oq-11"></a>
### OQ-11 — RunicGame target layout (decided 2026-10-09)

**Question.** How are RunicGame's modules mapped onto targets and folders?

**Outcome.** Game targets are `RunicGame::Shared`, `RunicGame::Server` and `RunicGame::Client`, with named areas inside
them (see ARCH § Repositories, build targets and processes, Game modules; detailed in RunicGame).

Applied in: [03 § Game targets](03-dependency-rules.md#game-targets).

<a id="oq-12"></a>
### OQ-12 — Source documents (decided 2026-10-09)

**Question.** The v0 docs were built from one appendix; several source documents were not imported.

**Outcome.** Technical Architecture v3 consolidates every source document. The repositories' docs are generated from
it.

Applied in: [README § Source and authority](README.md#source-and-authority).

<a id="oq-13"></a>
### OQ-13 — RunicFabric → RunicPlatform rename (decided 2026-10-07)

**Decision.** The owner asked to complete the rename in all three repositories.

**Outcome.**

| Repository | Old | New |
| --- | --- | --- |
| RunicPlatform | `project(RunicFabric)`, target `runic_fabric` / `Runic::Fabric`, option `RUNIC_FABRIC_BUILD_TESTS`, test `runic_fabric.bootstrap` | `project(RunicPlatform)`, target `runic_platform` / `Runic::Platform`, option `RUNIC_PLATFORM_BUILD_TESTS`, test `runic_platform.bootstrap` |
| RunicPlatform | namespace `Runic::Fabric`, header `<Runic/Fabric/Version.hpp>` | namespace `Runic::Platform`, header `<Runic/Platform/Version.hpp>` |
| RunicPlatform | Go module `runic.local/fabric`, package `go/fabric` | Go module `runic.local/platform`, package `go/platform` |
| RunicGame | `RUNIC_FABRIC_DIR` (default `../RunicFabric`), links `Runic::Fabric`, build dir `dependencies/fabric` | `RUNIC_PLATFORM_DIR` (default `../RunicPlatform`), links `Runic::Platform`, build dir `dependencies/platform` |
| RunicEngine | `Runic.code-workspace` folder `../RunicFabric` | `../RunicPlatform` |

"Formerly RunicFabric" remains only as a historical note. "Platform
Fabric/Networking" is the title of an external source document and is unchanged.
Per-module naming and the canonical Go module path stay open in [OQ-9](#oq-9).

<a id="oq-14"></a>
### OQ-14 — How Game simulation code is linked into the server (decided 2026-10-09)

**Question.** Are Game simulation rules plugins, or linked code?

**Outcome.** `RunicGame::Shared` and `RunicGame::Server` are static libraries linked into the server; GameCore
registers Game systems, hooks and actions.

Applied in: [03 § Game targets](03-dependency-rules.md#game-targets).

<a id="oq-15"></a>
### OQ-15 — What the client runs for prediction (decided 2026-10-09)

**Question.** Which code does the client run to predict the local player?

**Outcome.** The SIM-4[^engine-simulation] client prediction subset runs `RunicGame::Shared` on the client.

Applied in: [05 § Replication and prediction](05-distributed-simulation.md#replication-and-prediction).

<a id="oq-16"></a>
### OQ-16 — Quest storage placement (decided 2026-10-09)

**Question.** Who owns quest-shaped persistence and the idempotency store?

**Outcome.** The Platform Go SDK[^platform-gosdk] owns transactions and the idempotency store. The Game Quest service stores progress.

Applied in: [06 § Durable quest bridge](06-gameplayflow-quests-cutscenes.md#durable-quest-bridge),
[07 § Idempotency](07-service-backed-systems.md#idempotency).

<a id="oq-17"></a>
### OQ-17 — Client-safe versus server-only content packaging (decided 2026-10-09)

**Question.** How are client-safe and server-only definitions kept apart?

**Outcome.** Content is split into Shared, Client and Server definitions; the Cooker emits separate client and server
packages.

Applied in: [03 § Game targets](03-dependency-rules.md#game-targets).

<a id="oq-18"></a>
### OQ-18 — GameplayFlow built-in actions versus its dependency list (decided 2026-10-09)

**Question.** How do GameplayFlow's Combat, AI and Movement actions work when GameplayFlow may not depend on Combat or
AI?

**Outcome.** Option 1: GameplayFlow acts only through Simulation commands and Gameplay APIs (FLOW-1). It never calls
Combat or AI internals.

Applied in: [06 § GameplayFlow model](06-gameplayflow-quests-cutscenes.md#gameplayflow-model).

<a id="oq-19"></a>
### OQ-19 — GLM in public headers (decided 2026-10-11)

**Question.** Core's public headers use GLM as the Engine's math vocabulary, but only two public-header exceptions
were approved.

**Outcome.** GLM math types in Core's public headers are the third approved exception; GLM is a `PUBLIC` dependency
of Core. Jolt, bgfx, RmlUi, Detour and GameNetworkingSockets stay hidden.

Applied in: [02 § Public/ is the compile boundary](02-module-contract.md#public-is-the-compile-boundary).

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../system/README|System README]] · [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-assets]: [[../../../RunicEngine/docs/modules/Assets|Assets]] — Engine module · Foundation
[^engine-world]: [[../../../RunicEngine/docs/modules/World|World]] — Engine module · Simulation core
[^platform-contracts]: [[../../../RunicPlatform/docs/modules/Contracts|Contracts]] — Platform module · Protobuf
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^platform-gosdk]: [[../../../RunicPlatform/docs/modules/GoSDK|GoSDK]] — Platform module · Go
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^platform-buildingblocks]: [[../../../RunicPlatform/docs/modules/BuildingBlocks|BuildingBlocks]] — Platform module · Go
[^engine-gameplayflow]: [[../../../RunicEngine/docs/modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-network]: [[../../../RunicEngine/docs/modules/Network|Network]] — Engine module · Networking
[^platform-transport]: [[../../../RunicPlatform/docs/modules/Transport|Transport]] — Platform module · C++
[^engine-core]: [[../../../RunicEngine/docs/modules/Core|Core]] — Engine module · Foundation
[^engine-events]: [[../../../RunicEngine/docs/modules/Events|Events]] — Engine module · Foundation
[^engine-distributedsimulation]: [[../../../RunicEngine/docs/modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-application]: [[../../../RunicEngine/docs/modules/Application|Application]] — Engine module · Foundation
[^engine-client]: [[../../../RunicEngine/docs/modules/Client|Client]] — Engine module · Presentation
[^engine-physics]: [[../../../RunicEngine/docs/modules/Physics|Physics]] — Engine module · Simulation core
[^engine-renderer]: [[../../../RunicEngine/docs/modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-editor]: [[../../../RunicEngine/docs/modules/Editor|Editor]] — Engine module · Tools
[^engine-ui]: [[../../../RunicEngine/docs/modules/UI|UI]] — Engine module · Presentation
[^doc-build-build-1]: [[../build/BUILD-1|BUILD-1 contract]] — this repository
[^engine-simulation]: [[../../../RunicEngine/docs/modules/Simulation|Simulation]] — Engine module · Simulation core
