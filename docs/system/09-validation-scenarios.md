---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# 09 — Validation scenarios

This file turns ARCH[^arch]'s acceptance lists into checklists that CI or a person can run to prove a slice, and with it the architecture.

Source: ARCH § Vertical-slice roadmap, § Modules, work packages and the implementation contract (Automated checks),
§ Debug renderer, § AI architecture (Tests), § Water system, § Environment (Acceptance tests), § Sailing, swimming and
naval play (Sea acceptance list), § Render data, extraction and world UI, § Audio (Acceptance tests),
§ Networking: replication, interest and handoff.

## How to use these checklists

- Each package's **Done when** ([12-work-packages.md](12-work-packages.md)) is an automated test inside its module.
  The scenarios below assemble those tests into end-to-end proofs of a slice.
- **CI** marks a check that runs unattended in CI. **Manual** marks a check a person runs (usually with the debug
  renderer, the Editor or two clients) and records in the pull request or `docs/current-state.md`.
- Scenarios that ARCH[^arch] states with Game content are written neutrally. Engine and Platform tests use neutral IDs such as
  `engine:test_ability`; the Game repeats them with its own content in RunicGame.
- A scenario passes only when every box is checked.

## Build and boundary checks (every pull request)

- [ ] CI: the GameServer target builds alone with no GLFW, bgfx, RmlUi, ImGui, audio backend or editor code.
- [ ] CI: every module links only what its row in [03](03-dependency-rules.md#engine-modules) allows.
- [ ] CI: no file includes another module's `Source/`.
- [ ] CI: no library type appears in a `Public/` header, apart from GLM in Core, the Flecs world handle and
      BehaviorTree.CPP node base types where approved.
- [ ] CI: module tests pass for every merged package.
- [ ] CI: a code change carries its `ARCHITECTURE.md` and package-status update.
- [ ] CI: Engine and Platform code and tests contain no Game rules, names or numbers.
- [ ] CI (RunicPlatform): the standalone C++ build links only `Runic::Core`[^engine-core], `Runic::Application`[^engine-application],
      `Runic::NetworkTransport`[^engine-network] and `Runic::SimulationPort`[^engine-simulation] from the Engine and never configures Flecs, Jolt or the
      gameplay modules.

## Debug renderer and physics view (slices 2, 5, 8)

- [ ] Manual: a grid, one movable capsule, one camera, one attack arc, one ray, and text showing the simulation tick
      and network state draw.
- [ ] Manual: every debug shape renders batched in one frame; filled shapes with opacity draw after the line batch;
      the overlay batch shows hidden bounds without making every line x-ray.
- [ ] Manual: a predicted capsule moves around a grid and the authoritative correction shows separately.
- [ ] Manual: attack volumes render with the same dimensions combat uses.
- [ ] Manual: Jolt collision state can be inspected; collision shapes line up with gameplay shapes; each debug layer
      toggles.
- [ ] CI: no simulation system calls bgfx; Physics links no Renderer and the Renderer links no Physics.
- [ ] CI: entities are drawn from the RenderFrame only (CLI-1[^engine-client]).

## Combat prototype (slice 19)

Engine-neutral form of ARCH[^arch]'s combat prototype acceptance list. The Game's own list, with its values, is in RunicGame.

- [ ] Two players and a dummy are in one scene.
- [ ] One melee attack and one projectile each hit the dummy exactly once.
- [ ] A dodge spends a defensive resource and its invulnerability window blocks hits.
- [ ] A directional block intercepts damage only from the configured direction.
- [ ] A crowd-control chain applies, then an immunity window follows, as the test data says.
- [ ] An AoE proves damage falloff and the crowd-control target cap.
- [ ] One rule interaction: one status changes the effect of another, through the rule pipeline.
- [ ] A FriendlyLock heal selects only a friendly target.
- [ ] A PvE threat override changes the NPC's target through the threat table.
- [ ] A PvP damage-redirect protection applies through damage interception.
- [ ] Debug drawing shows all of the above.

Package checks that the slice also needs (from the Done when of its packages):

- [ ] A crowd-control chain and its immunity follow the test data (CMB-6[^engine-combat]).
- [ ] An early input fires on the first legal tick; a stun in between cancels it (CMB-7).
- [ ] UI clicks never become attacks (UI-2[^engine-ui]).

## Durable path: accounts, items and quests (slices 26 to 30)

- [ ] CI: a retried operation commits once (PSDK-2[^platform-gosdk]).
- [ ] CI: a fake service port that delivers a result twice is applied once; no I/O callback touches Flecs (SIM-5[^engine-simulation]).
- [ ] CI: a slow service never delays a tick (PBND-1[^platform-serverbindings]).
- [ ] Manual: log in, receive a real signed ticket, load and save a character (slice 27).
- [ ] CI: bad or expired tickets are rejected with no service call (PTR-2[^platform-transport]); the check uses signature, expiry and key
      ID against a configured set of public keys.
- [ ] CI: concurrent ledger transfers never duplicate or lose an item (PBB-1[^platform-buildingblocks]).
- [ ] CI: a wallet balance changes once per operation ID under concurrent requests (PBB-2, slice 33a).
- [ ] Manual: kill, loot, log out, keep the item; a loot grant is applied once across reconnects (slice 28).
- [ ] CI: a quest defined only in data advances from kill and interaction facts (QST-1[^engine-quest]).
- [ ] CI: a replayed quest fact never doubles progress; no service callback touches Flecs (QST-2).
- [ ] CI: a fact published once is consumed once per subscriber (PSDK-3).
- [ ] CI: a fact published from a tick reaches a test subscriber once; a slow broker never delays a tick (PBND-4).
- [ ] CI: C++ and Go round-trip the same Contracts message (PCT-1[^platform-contracts], slice 12).
- [ ] Manual: the Contracts define service APIs, published facts and the connect-ticket format only; inputs,
      snapshots, deltas and simulation-link messages are encoded by Runic::Network[^engine-network], never by Protobuf.

## Scripted sequence safety (slices 29 and 37)

- [ ] The server starts a sequence and acquires input, movement, ability, interaction, targeting, damage and
      external-motion restrictions under the sequence's source ID.
- [ ] Commands blocked by a restriction are rejected on the server even if a modified client sends them.
- [ ] Complete, abort, timeout, disconnect and shutdown each release everything the sequence held (FLOW-2[^engine-gameplayflow]).
- [ ] A boss phase locks the arena; a cutscene protects the player; a disconnect releases everything (slice 29).
- [ ] A temporary restriction expires and raises a watchdog diagnostic if no exit path released it.
- [ ] A sequence waits, branches and acts only through commands and Gameplay APIs (FLOW-1).
- [ ] Transferable keeps its timers across a handoff; PinAuthority blocks transfer; AbortOnTransfer cleans up (FLOW-3).
- [ ] A quest in progress crosses a simulation border with its objectives and timers intact (QST-3[^engine-quest]).

## AI (slices 32 to 35 and 38)

Representative unit tests:

- [ ] Perception: a hostile entering the detect radius wakes the NPC engaged; entering only the warn radius wakes it
      alerted; high concealment shrinks the radii; the minimum radius still detects in total darkness; damage wakes
      from outside both radii.
- [ ] Target validity: death, concealment beyond detect range or a leash break invalidates or changes behavior
      according to policy.
- [ ] Reaction timing: a threat before `reactAt` fails HasReactableThreat; the same threat after `reactAt` succeeds.
- [ ] Utility: unavailable actions score invalid; personality biases change ordering; a fixed seed reproduces a
      choice.
- [ ] Intent boundary: a tree action emits the expected intent and never changes combat state directly.
- [ ] Command rejection: choosing a defense while committed is rejected; AI never bypasses combat rules.
- [ ] Wake and sleep: a BrainHandle is created on wake and recycled on sleep; dormant entities never tick a tree.
- [ ] Editor validation: unknown nodes, ports or content IDs are reported; valid assets round-trip without loss.

Integration tests:

1. [ ] An NPC detects a player, approaches, attacks, reacts to a telegraphed attack and returns home after a leash
       break.
2. [ ] At night an unlit player passes outside the reduced detect radius unnoticed, while a player carrying a light
       emitter is detected from farther away.
3. [ ] A player raises concealment while engaged; the target is dropped according to policy, and lowering it makes the
       player detectable again.
4. [ ] An NPC chooses a defense after its action became non-cancellable and correctly fails to execute it.
5. [ ] A SkillStarted fact produced in the realtime phase reaches AI memory only in the post-realtime phase and is
       acted on in a later pre-realtime tick.
6. [ ] Runic::AI[^engine-ai] builds and runs headless with no ImGui, bgfx or editor dependency.
7. [ ] Selecting a live NPC in the Editor shows the same BrainHandle, tree, memory and intents the runtime
       instrumentation records.

Scale (slice 38):

- [ ] The tick budget holds with thousands of loaded NPCs at the target awake percentages (AI-6).

## Interest and visibility (slice 25)

- [ ] Only relevant entities arrive and per-client budgets hold (NET-4[^engine-network]).
- [ ] Visibility tiers change at the configured distances and light levels (PRC-2[^engine-perception]).
- [ ] A Silhouette target sends coarse position and pose only; a Hidden target sends nothing.
- [ ] Contact (hitting, being hit, a nearby heal, AoE contact) reveals for a few seconds.
- [ ] Projectiles and effects from hidden players arrive without the caster's identity.
- [ ] Combat resolves every hit regardless of tier.
- [ ] Two simulations agree on the time of day; night starts at the same world time everywhere (ENV-1[^engine-environment]). Before
      PCS-2[^platform-coreservices] both read the same world epoch from configuration.

## Multiple simulations and handoff (slices 36, 37, 42b, 44+)

1. [ ] Load the same static world data into simulation A and simulation B; the World Coordinator[^platform-coreservices] names the candidates
       for their border (PCS-2).
2. [ ] Ghosts follow their owners near the border and stay in sync under packet loss (DSIM-2[^engine-distributedsimulation], NET-5[^engine-network]).
3. [ ] A message with a stale authority epoch is rejected (DSIM-1, NET-5).
4. [ ] A player walks from A to B with no loading screen; the input sequence continues (DSIM-3).
5. [ ] The client holds up to three candidate connections; promotion happens with no loading screen (PTR-3[^platform-transport]).
6. [ ] Dropping a candidate or retiring connection never duplicates or rolls back authority.
7. [ ] An attack across the border damages its target exactly once, by its owner (CMB-8[^engine-combat]).
8. [ ] A foreign-interaction request carries enough context for the owner to resolve it (DSIM-4).
9. [ ] A projectile or area effect crossing the border is applied by each simulation only to its own entities.
10. [ ] No local Flecs entity ID appears on the wire.
11. [ ] A sequence and a quest survive the handoff (FLOW-3[^engine-gameplayflow], QST-3[^engine-quest]).
12. [ ] A watercraft with passengers and cargo crosses the border with nothing lost or left behind (WATER-7[^engine-water]).
13. [ ] Wind, currents and weather sample identically on both sides of the border at the same world time.
14. [ ] (44+) Two simulations share one busy city and each dynamic entity has exactly one owner (DSIM-5).

## Battle scale (slice 38)

- [ ] 200, then 500 bots fight in one region.
- [ ] 500 bots in one region fit the per-client budget (NET-6[^engine-network]).
- [ ] Beyond the budget, degradation follows the fixed order (distant update rate, then cosmetic detail, then a cap on
      visible effects); combat correctness never degrades.
- [ ] 500 nameplates draw in one call (RND-10[^engine-renderer]).
- [ ] In a 500-player fight the frame budget holds and the target keeps its full nameplate (UI-5[^engine-ui]).
- [ ] Simulation work per 30 Hz tick stays within the planning budget at p99.

## World UI and render checks (slices 22, 23, 23a, 25a, 42a)

- [ ] Cooked meshes appear at their entities' positions far from the origin and draw correctly (CLI-4[^engine-client], RND-4[^engine-renderer]).
- [ ] An animated character renders with blended clips and no pops (CLI-5, RND-5, ANIM-1[^engine-animation]).
- [ ] Anchors follow entities; Hidden players produce none (CLI-6).
- [ ] Nameplates follow entities and respect visibility tiers (UI-3[^engine-ui]).
- [ ] A texture rendered offscreen appears on a quad in the world (RND-9).
- [ ] A click on a world board reaches its RmlUi context and becomes a command the server validates (UI-4).
- [ ] The UI reads only world anchors from the RenderFrame and Game view models; it never reads Flecs.
- [ ] Darkness and light emitters on screen match the server's light values (CLI-7); light, skylight, darkness zones
      and fog are visible and toggle (RND-7).
- [ ] Grids re-extract only when the current or the wind changes (CLI-8); a river surface and the ocean current draw
      from cooked data (RND-8).
- [ ] Every RenderData kind round-trips through a frame packet (CORE-4[^engine-core], CORE-5); a frame is written on one thread and
      read on another with no lock (CORE-3).
- [ ] The ImGui overlay exists only in development builds (UI-1).

## Audio (slice 24)

- [ ] Default soundtrack: with no custom profile, the built-in default preset is active and every context resolves to
      Game defaults with no backend service.
- [ ] Custom preset: separate local albums for two contexts switch with the configured transition and resume from
      stored state.
- [ ] Radio behavior: an album resumes at its stored track and position after another context ends, and after a
      client restart as the policy says.
- [ ] Context compatibility: an old preset without a new context resolves it through fallback and stays editable.
- [ ] Broken local track: a deleted file is skipped or falls back without a crash, and the editor reports it.
- [ ] Sound-effect boundary: an event resolves through the registry, has no player remapping UI, and is inspectable
      in tooling.
- [ ] Headless boundary: the GameServer builds with no audio backend or soundtrack dependency, and gameplay is
      identical whatever soundtrack clients use.

## Water: first water slices (slices 40 to 42)

- [ ] A descending river authored in Blender is cooked.
- [ ] It streams across at least two cells with no seam.
- [ ] The client renders it; the server samples its surface and current.
- [ ] A boat with BuoyantBody and WatercraftMovement travels downstream faster than upstream.
- [ ] A BuoyantBody-only object floats without drifting.
- [ ] A crate with ObjectWaterMovement drifts with zero self-propulsion.
- [ ] A character swims with current influence and no rigid-body buoyancy.
- [ ] The river enters a cave volume continuously and reaches an underground lake or ocean without a teleport or
      instance.
- [ ] Repeated with two networked clients, watercraft and character transitions stay continuous.

## Water and watercraft acceptance (slices 41, 42, 42b, 43)

Engine-neutral form of ARCH[^arch]'s sea acceptance list; the Game's version is in RunicGame.

1. [ ] Board a watercraft at a dock and travel to a destination, faster with a favorable current and wind than against
       them.
2. [ ] Load cargo, cross a simulation border with passengers aboard, and arrive with nothing lost.
3. [ ] Sink a watercraft with projectile fire: passengers switch to swimming, and the cargo floats and drifts with the
       current.
4. [ ] A diving character's WaterInteractionState reports head submerged, and a Game rule driven from it runs.
5. [ ] An AI creature using CharacterWaterMovement, a behavior tree and perception attacks a small watercraft at night.
6. [ ] A client map overlay shows a region's currents and winds drawn from the cooked fields.
7. [ ] A weather override changes wind and swell, and makes steering harder, in two simulations at once.

## Environment (slices 25, 42, 43)

| Test | Passes when |
| --- | --- |
| Same wind everywhere | Two simulations and two clients sample identical wind at a shared border at the same world time |
| Sailing angles | A sailing watercraft gains speed at favorable wind angles and none straight into the wind, matching its configured curve |
| Leeway | A crosswind pushes a watercraft sideways in proportion to its lateral resistance |
| Tidal path | At low tide a tidal path is walkable; at high tide characters swim there |
| Storm override | A storm from a Game service applies on the same world time in two simulations; wind, waves and light change; clients follow without per-tick data |
| Updraft | A glider gains altitude crossing an authored updraft volume |
| Cave volume | Wind and weather sample as calm inside a cave volume; light comes only from ambient profiles and emitters |
| Projectile flag | Projectiles ignore wind unless their skill sets the wind flag |

## Operations (slice 39)

- [ ] Against the Agones local SDK server the LifecycleLayer[^platform-serverbindings] reports Ready, sends health pings and drains on
      shutdown; the stub mode runs with no Agones (PBND-2).
- [ ] A scrape returns time per tick domain in Prometheus format (PBND-3).
- [ ] A fleet runs in a development cluster, servers report Ready, and Grafana shows time per tick domain (PINF-2[^platform-infrastructure]).
- [ ] The Editor attaches to a development server and sees its shapes; a production build has no attach endpoint
      (NET-7[^engine-network]).
- [ ] Entity inspector and debug shapes come from a remote server (EDT-7[^engine-editor]).

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../system/README|System README]] · [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-core]: [[../../../RunicEngine/docs/modules/Core|Core]] — Engine module · Foundation
[^engine-application]: [[../../../RunicEngine/docs/modules/Application|Application]] — Engine module · Foundation
[^engine-network]: [[../../../RunicEngine/docs/modules/Network|Network]] — Engine module · Networking
[^engine-simulation]: [[../../../RunicEngine/docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-client]: [[../../../RunicEngine/docs/modules/Client|Client]] — Engine module · Presentation
[^engine-combat]: [[../../../RunicEngine/docs/modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-ui]: [[../../../RunicEngine/docs/modules/UI|UI]] — Engine module · Presentation
[^platform-gosdk]: [[../../../RunicPlatform/docs/modules/GoSDK|GoSDK]] — Platform module · Go
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^platform-transport]: [[../../../RunicPlatform/docs/modules/Transport|Transport]] — Platform module · C++
[^platform-buildingblocks]: [[../../../RunicPlatform/docs/modules/BuildingBlocks|BuildingBlocks]] — Platform module · Go
[^engine-quest]: [[../../../RunicEngine/docs/modules/Quest|Quest]] — Engine module · GameplayAPI
[^platform-contracts]: [[../../../RunicPlatform/docs/modules/Contracts|Contracts]] — Platform module · Protobuf
[^engine-gameplayflow]: [[../../../RunicEngine/docs/modules/GameplayFlow|GameplayFlow]] — Engine module · GameplayAPI
[^engine-ai]: [[../../../RunicEngine/docs/modules/AI|AI]] — Engine module · World systems
[^engine-perception]: [[../../../RunicEngine/docs/modules/Perception|Perception]] — Engine module · World systems
[^engine-environment]: [[../../../RunicEngine/docs/modules/Environment|Environment]] — Engine module · World systems
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^engine-distributedsimulation]: [[../../../RunicEngine/docs/modules/DistributedSimulation|DistributedSimulation]] — Engine module · Networking
[^engine-water]: [[../../../RunicEngine/docs/modules/Water|Water]] — Engine module · World systems
[^engine-renderer]: [[../../../RunicEngine/docs/modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-animation]: [[../../../RunicEngine/docs/modules/Animation|Animation]] — Engine module · Presentation
[^platform-infrastructure]: [[../../../RunicPlatform/docs/modules/Infrastructure|Infrastructure]] — Platform module · configuration
[^engine-editor]: [[../../../RunicEngine/docs/modules/Editor|Editor]] — Engine module · Tools
