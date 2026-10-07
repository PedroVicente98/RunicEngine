# 11 — Open questions and known inconsistencies

Items found while converting the source architecture into these docs. Each one
needs an owner decision before the code that depends on it is written. A
**proposal** is a suggested default, not a decision.

When an item is decided, move it to [Decided](#decided) with the date and
outcome, and update the affected module contracts in all three repositories.

| ID | Topic | Blocks |
| --- | --- | --- |
| [OQ-1](#oq-1) | NetworkAdapter ↔ Platform Realtime binding direction | Phase 4 |
| [OQ-2](#oq-2) | Where shared ID/time types live | Phase 1 (Events) |
| [OQ-3](#oq-3) | Assets dependencies: catalog vs. diagram | Phase 2 / 7 |
| [OQ-4](#oq-4) | OS window and input layer | Phase 0 / 2 |
| [OQ-5](#oq-5) | `Debug` folder in the Engine scaffold | Phase 0 |
| [OQ-6](#oq-6) | Dear ImGui ownership | Phase 2 |
| [OQ-7](#oq-7) | Test framework and test location | Phase 0 |
| [OQ-8](#oq-8) | Engine repository layout migration | Phase 0 |
| [OQ-9](#oq-9) | Platform language split and naming | Phase 4 |
| [OQ-10](#oq-10) | Agones SDK inside the simulation process | Phase 16 |
| [OQ-11](#oq-11) | RunicGame module → folder mapping | Phase 0 (Game) |
| [OQ-12](#oq-12) | Source documents not yet imported | several |
| [OQ-13](#oq-13) | RunicFabric → RunicPlatform rename is incomplete | Phase 0 |
| [OQ-14](#oq-14) | How Game simulation code is linked into the server | Phase 1 (Game) |
| [OQ-15](#oq-15) | What the client runs for prediction | Phase 2 / 4 |
| [OQ-16](#oq-16) | `QuestStore` placement | Phase 9 / 10 |
| [OQ-17](#oq-17) | Client-safe vs. server-only content packaging | Phase 6 |
| [OQ-18](#oq-18) | GameplayFlow built-in Combat/AI/Movement actions vs. its dependency list | Phase 10 |

---

<a id="oq-1"></a>
## OQ-1 — NetworkAdapter ↔ Platform Realtime binding direction

**Context.** Engine `NetworkAdapter` may depend on "Platform contract/binding
package at the integration boundary" [J.4.23]; Platform `Realtime` "integrates
Engine NetworkAdapter" [J.5.2]; the diagram shows `NetworkAdapter ---> RunicPlatform
Realtime` [J.11.1]. The current RunicPlatform README says Platform has no
dependency on either other repository.

**Options.**
1. Engine `NetworkAdapter` defines simulation-side interfaces only (`SimulationIO`,
   ingress/egress queues); a Platform C++ binding implements them and depends on
   Engine `NetworkAdapter` public headers.
2. Platform publishes a small dependency-free C++ contract package; Engine
   `NetworkAdapter` depends on it; Realtime implements it.
3. A third binding target (built in RunicGame or a separate package) depends on
   both and wires them together.

**Proposal.** Option 2 for generic contracts (message IDs, schemas) plus option 3
for the wiring, so that neither public repository depends on the other's
implementation. Needs owner decision.

<a id="oq-2"></a>
## OQ-2 — Where shared ID and time types live

**Context.** `Events` may depend only on `Core` [J.4.3], but its example fact uses
`TickId` and `GlobalEntityId`. `GlobalEntityId`, `SimulationId`, and
`AuthorityEpoch` are listed as owned by `DistributedSimulation` [J.4.9], which
itself depends on `Events`. `Core` owns "stable IDs" and "time primitives"
[J.4.1]. Platform `Identity` owns `GlobalPlayerId` semantics [J.5.7] and
`Contracts` owns identity/message IDs on the wire [J.5.1].

**Proposal.** Declare the plain value types (`TickId`, `GlobalEntityId`,
`SimulationId`, `AuthorityEpoch`, `EventId` if needed) in `Core` (`IDs.hpp`,
`Time.hpp`). `DistributedSimulation` owns their *semantics* (authority, epochs,
transfer). Wire encodings live in Platform `Contracts`. This keeps the DAG acyclic.

Related: facts need a *stable* identity for deduplication by durable consumers
[J.14.4]. An `EventId` that is only unique inside one process run is not enough;
it should be unique per simulation and survive replay (e.g. `SimulationId` +
monotonic sequence, or an ID derived from the causing operation).

<a id="oq-3"></a>
## OQ-3 — Assets dependencies: catalog vs. diagram

**Context.** The Assets catalog entry allows only `Core, Content` [J.4.10]; the
dependency diagram draws `World → Assets` [J.11.1]. World streaming (phase 7)
needs to load asset packages, and Renderer "World as needed" [J.4.11].

**Proposal.** Keep Assets independent of World (catalog wins). World streaming
emits *what* to load; a client/server streaming coordinator above both (e.g. in
`Client` or a World streaming subsystem that depends on Assets) performs loading.
Decide before phase 7.

<a id="oq-4"></a>
## OQ-4 — OS window and input layer

**Context.** The Engine scaffold has `Platform/` and `Input/` folders; Appendix J
has no such modules. `Application` owns application events (key/mouse/text,
resize) and says "optional OS/window adapters are downstream or private"
[J.4.2, J.7.1]. `Client` owns `InputMapping` [J.4.12]. The name "Platform" also
collides with the RunicPlatform repository.

**Proposal.** No `Platform` or `Input` Engine module. GLFW lives in a private
OS/window adapter used by `Application` in client/editor builds only (headless
builds exclude it). Raw input becomes `AppEvent`s; mapping to gameplay commands
is `Client/InputMapping`. Remove the empty scaffold folders when Application is
created.

<a id="oq-5"></a>
## OQ-5 — `Debug` folder in the Engine scaffold

**Context.** The scaffold has `Debug/`; Appendix J places debug drawing in
`Renderer` (`DebugRenderer`), physics debug extraction in `Physics`
(`PhysicsDebug`), inspectors/profiling in `Editor`, and operational telemetry in
Platform `Observability`.

**Proposal.** No `Debug` module; remove the empty folder. Revisit only if a
headless diagnostics library is needed (e.g. shared logging sinks), which would
then be a Core logging hook or an Editor subsystem.

<a id="oq-6"></a>
## OQ-6 — Dear ImGui ownership

**Context.** ImGui is pinned in Engine and must be absent from headless builds
[J.14.6], but no catalog entry names it. `UI` uses RmlUi for player-facing UI.

**Proposal.** ImGui is a private dependency of `Editor` (inspectors, debug views)
and optionally of a developer overlay in `Client` debug builds. Never used for
player-facing UI.

<a id="oq-7"></a>
## OQ-7 — Test framework and test location

**Context.** No test framework is chosen. J.3.1 puts `Tests/` inside each module;
the AI task template in J.13.1 writes `Tests/Physics/**`.

**Proposal.** Module-local `Engine/<Module>/Tests/` registered with CTest. Pick
one C++ framework (doctest, Catch2, or GoogleTest) for all Engine/Platform/Game
C++ code, and Go's standard `testing` package for Go.

<a id="oq-8"></a>
## OQ-8 — Engine repository layout migration

**Context.** Target layout is `RunicEngine/Engine/<Module>/{Public,Source,Tests}`
with one target per module [J.2.2, J.3]. Today the repository has
`include/RunicEngine/<Module>/` + `src/<Module>/` (empty) and aggregate interface
targets `Runic::Core`, `Runic::Runtime` (Flecs+GLM+Jolt), `Runic::Presentation`
(GLFW+bgfx+ImGui). The current README still promises `<RunicEngine/Module/Header.hpp>`
includes, which J.1.2 supersedes. Phase 0 also asks for headless, client, and
editor executables; only `RunicSandbox` exists.

**Proposal.** Create `Engine/` and migrate module by module; add a small CMake
helper (e.g. `runic_add_module(Name PUBLIC_DEPS … PRIVATE_DEPS …)`). Keep
`Runic::Runtime`/`Runic::Presentation` only as transitional bundles for RunicGame
until it links per-module targets, then delete them. Third-party links move to
the owning module as `PRIVATE`.

<a id="oq-9"></a>
## OQ-9 — Platform language split and naming

**Context.** "Go is expected for service/runtime/control-plane work; native
bindings may exist where the C++ simulation integrates with Platform-owned
networking/contracts" [J.5]. The realtime path is `client → C++ simulation`, so
Realtime must be usable from C++. The repository today has `cpp/`, `go/`, `proto/`
folders, CMake target `Runic::Fabric`, namespace `Runic::Fabric`, and Go module
`runic.local/fabric`.

**Proposal (to confirm per module).**

| Module | Likely implementation |
| --- | --- |
| Contracts | Schema source (`.proto` or equivalent) + generated C++ and Go |
| Realtime | C++ (client and simulation sides); Go only for control-channel endpoints if any |
| Identity | Go service + C++ token-validation helpers for simulations |
| ServiceRuntime | Go; plus a C++ async client binding for simulation-originated requests |
| Persistence | Go |
| WorldControl | Go control plane; simulation-side agent through Realtime control channels |
| Orchestration | Go (Agones/Kubernetes) |
| Observability | Go + C++ hooks |

Also decide: C++ namespace (`Runic::Platform::<Module>`?), CMake alias names
(`Runic::Platform::Realtime`? `Runic::Realtime`?), and the canonical Go module path.

<a id="oq-10"></a>
## OQ-10 — Agones SDK inside the simulation process

**Context.** A game-server process managed by Agones normally calls the Agones
SDK (Ready, Health, Allocate/labels). Engine must not depend on Agones [J.11.2];
Orchestration owns Agones adapters [J.5.6].

**Proposal.** A Platform `Orchestration` C++ sidecar binding (or the Agones
sidecar's local HTTP/gRPC API) linked by the RunicGame server executable — never
by Engine modules. Decide before phase 16.

<a id="oq-11"></a>
## OQ-11 — RunicGame module → folder mapping

**Context.** J.6.1 gives a folder tree (`Server/Simulation/{Combat, Quests,
Encounters, AI, WorldRules, Naval, GameplayScripts}`, `Client/{Presentation, UI,
Cutscenes, Camera, Audio, ServiceBindings}`, …) while J.6.2 names 14 modules
(GameCore, GameContent, GameCombat, …). They do not map one-to-one:
GameProgression, GameEconomy, GameWorldGameplay, GameInstances, and GameCore
have no folder, and `GameplayScripts` has no module.

**Proposal.** See RunicGame `docs/architecture.md` §Module placement.

<a id="oq-12"></a>
## OQ-12 — Source documents not yet imported

Only Appendix J was imported. Missing:

- v2.1 sections A–I (process composition, Flecs/world/physics/scheduling,
  Platform networking/handoff/service fabric, Combat technical contract,
  Quest/world-interaction bridge, Renderer/UI, vertical-slice development).
- Subsystem references: World Authoring/Cooking, Water, AI, Jolt Physics/Multi-Rate
  Tick, Combat System Context, Platform Fabric/Networking.

Details that are therefore undefined here include: which systems run in which
tick domain (60/30/20/10 Hz), simulation phase names, snapshot/replication
formats, interest-management policy, combat timeline/hit-detection contract,
native asset formats, and renderer/UI specifics. Add those documents (or their
relevant parts) under `docs/` when available.

<a id="oq-13"></a>
## OQ-13 — RunicFabric → RunicPlatform rename is incomplete

Remaining references to the old name: RunicPlatform CMake `project(RunicFabric)`,
target `runic_fabric`/`Runic::Fabric`, option `RUNIC_FABRIC_BUILD_TESTS`,
namespace `Runic::Fabric`, header path `Runic/Fabric/Version.hpp`, Go module
`runic.local/fabric` and package `go/fabric`; RunicGame `RUNIC_FABRIC_DIR`
(default `../RunicFabric`), links to `Runic::Fabric`, README and
`Services/README.md`; RunicEngine `Runic.code-workspace` folder `../RunicFabric`.

**Proposal.** Rename in one coordinated change across the three repositories,
together with OQ-9 naming.

<a id="oq-14"></a>
## OQ-14 — How Game simulation code is linked into the server

**Context.** `Server/Simulation` "is primarily C++ and runs inside/with the
authoritative simulation process" [J.6.1]; Game registers systems/actions into
Engine registries [J.4.8, J.4.18].

**Proposal.** Static libraries linked into the RunicGame server executable, with
an explicit registration entry point owned by `GameCore`. No dynamic plugin
system until a concrete need exists.

<a id="oq-15"></a>
## OQ-15 — What the client runs for prediction

**Context.** `Client` depends on a "Simulation/client simulation surface" and
owns prediction/reconciliation [J.4.12]. Predicting movement normally needs the
same movement and character-collision code as the server (Physics character
controller, Gameplay movement), but the catalog does not say which modules the
client links or whether a reduced `Simulation` runs on the client.

**Proposal.** Define a "client simulation surface" in `Simulation` that runs only
predicted local-player systems (movement + character physics) with the same code
as the server. Decide in phase 2, before phase 4.

<a id="oq-16"></a>
## OQ-16 — `QuestStore` placement

**Context.** J.10.5 shows `QuestStore` and `IdempotencyStore` as "Platform-side
reusable interfaces", while J.1.3 moves all quest policy to RunicGame.

**Proposal.** Platform owns `IdempotencyStore`, transactions, and generic
repository helpers. `QuestStore` (quest-shaped persistence) is defined in the
RunicGame QuestService on top of them.

<a id="oq-17"></a>
## OQ-17 — Client-safe vs. server-only content packaging

**Context.** GameContent must split shared/client/server data so secret
coefficients are not shipped [J.6.2, J.6.4]; Assets owns "client/server package
selection" [J.4.10]; Cooker writes packages [J.4.24]. The mechanism (separate
schemas, field-level stripping at cook time, separate files) is not defined.

**Proposal.** Separate definition files/schemas per audience
(`Content/Shared`, `Content/Client`, `Content/Server`), and Cooker emits distinct
client and server packages; never strip fields from a mixed file at runtime.

<a id="oq-18"></a>
## OQ-18 — GameplayFlow built-in actions vs. its dependency list

**Context.** GameplayFlow's built-in action categories include Combat ("request
ability, apply approved effect through Combat"), AI ("change behavior through AI
APIs"), and Movement ("follow path") [J.9.3]. But GameplayFlow may depend only on
Core, Events, Gameplay, Simulation, Content [J.4.18], and Combat/AI do not list
GameplayFlow either [J.4.20, J.4.21]. The diagram draws Combat/AI *below*
GameplayFlow [J.11.1].

**Options.**
1. GameplayFlow built-ins act only through Simulation commands and Gameplay APIs
   (e.g. a `UseAbility` command consumed by Combat); no direct Combat/AI calls.
2. Combat and AI register their own actions into the GameplayFlow
   `ActionRegistry` (adds GameplayFlow to their allowed dependencies).
3. Add Combat and AI to GameplayFlow's allowed dependencies (matches the diagram).

**Proposal.** Option 1 — it keeps every authority pipeline in one place and
needs no new edges. Decide before phase 10.

---

## Decided

_None yet._
