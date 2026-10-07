# 09 — Validation scenarios and acceptance gates

These scenarios are the end-to-end tests that prove the architecture. Module
acceptance tests (in each module contract) should be written so that these
scenarios can be assembled from them. [J.14]

## J.14.1 First true playable MMO slice

- A small streamed world with Region/Sector/Cell structure.
- Two clients connected directly to an authoritative simulation through Platform
  Realtime.
- Server-authoritative movement with client prediction/reconciliation.
- One wolf using Engine AI/Navigation and Game behavior.
- Engine Combat with Game-specific melee/projectile data.
- An `EntityKilled` gameplay fact observed by multiple consumers.
- The private Game InventoryService grants a durable sword through Platform
  service/persistence APIs.
- Disconnect/reconnect reloads the durable result without a duplicate grant.

## J.14.2 Hot-city two-simulation test

1. Load the same city static world data into Simulation A and Simulation B.
2. WorldControl assigns adjacent AuthorityPartitions inside the same logical
   Region/Sector/Cell working set.
3. Alice is A-owned and Bob is B-owned; each appears as a ghost where relevant.
4. Alice crosses the partition boundary with make-before-break client connection
   promotion and an AuthorityEpoch increment.
5. Alice attacks Bob before and after the handoff; Bob is damaged **exactly
   once**, by his authoritative owner.
6. A projectile or AoE crossing the boundary uses foreign effect context rather
   than mutating remote ghosts.
7. No local Flecs entity ID appears on the wire.
8. Dropping a candidate/retiring connection cannot duplicate or roll back
   authority.

## J.14.3 Cutscene safety test

1. The server starts a GameplayFlow sequence and acquires input, movement,
   ability, interaction, targeting, damage, and external-motion restrictions from
   a stable `SequenceInstanceId`.
2. The client disables controls and plays the cinematic presentation.
3. A modified client keeps sending MoveCommand, UseAbility, and Interact; the
   server rejects them.
4. Hostile combat cannot damage/target/move the protected player according to the
   restriction spec.
5. Complete, Abort, Timeout, disconnect, quest abandon, and simulation shutdown
   paths each release all sequence-owned restrictions.
6. A watchdog test deliberately corrupts/loses normal completion and verifies
   temporary restrictions cannot stay permanent without diagnostics/expiry.
7. A `Transferable` sequence survives authority handoff with the same source IDs
   and remaining timers; `PinAuthority` and `AbortOnTransfer` behave explicitly.

## J.14.4 Quest durability test

1. Combat emits `EntityKilled` once with a stable event identity.
2. The Game quest runtime consumes the relevant fact and may execute local world
   behavior.
3. NetworkAdapter/Platform transports the durable quest fact/operation
   asynchronously.
4. The private QuestService validates context, deduplicates identity, updates
   progression, and commits rewards transactionally.
5. Replaying the same event does not increment progress or duplicate rewards.
6. The service callback never mutates Flecs directly; the accepted result is
   queued for a later safe tick.

## J.14.5 Auction secrecy and authority test

- The client contains only endpoint bindings, UI logic, displayable schemas, and
  the visible data required for interaction.
- Listing/tax/price/market/anti-abuse rules execute in the private Game
  AuctionService.
- Platform provides authenticated service runtime, transaction/persistence
  primitives, query/pagination helpers, and idempotency.
- Tampered client price/tax assumptions do not affect server validation or
  committed transaction results.

## J.14.6 Headless dependency test

The authoritative server build must compile and run without GLFW, bgfx, RmlUi,
ImGui, client audio, or editor linkage. ApplicationHost, Simulation, Events,
World, Physics, Gameplay/Combat/AI/Quest/GameplayFlow, DistributedSimulation,
NetworkAdapter, and the required Platform bindings must remain
headless-compatible.

Suggested automation (derived): a CI job that configures the server-only build
with presentation disabled and fails if any presentation target is configured or
linked (e.g. inspect the link line or `cmake --graphviz` output for `glfw`,
`bgfx`, `RmlUi`, `imgui`, or `RunicEditor`).

## J.14.7 Outcome

The target is not a single monolithic MMO server library. It is a
dependency-controlled graph of substantial reusable modules. Engine owns reusable
game/runtime semantics; Platform owns reusable distributed mechanisms; Game owns
all concrete RunicGame policy and content. Cross-cutting features — quests,
cross-simulation combat, cutscenes, inventory, auctions — are deliberately split
across those layers without duplicating authority.
