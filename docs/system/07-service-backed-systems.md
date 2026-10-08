# 07 — Service-backed systems: inventory, trade, auction, economy, quests

Durable, transactional game features are split three ways. Platform provides the
reusable mechanism; a private RunicGame Go service owns the rule; the Engine
provides the runtime representation used inside the simulation.

## Generic path [J.10.1]

```text
RunicGame Client
      |
      | typed request / command
      v
RunicPlatform networking / ServiceRuntime
      |
      v
RunicGame Server domain service (Go)
      |
      | uses reusable transaction/persistence APIs
      v
RunicPlatform Persistence
      |
      v
Database
```

The Platform package is reusable infrastructure. The Game service owns the rule
that decides whether a domain operation is legal and what durable mutation is
committed.

## Inventory split [J.10.2]

| Layer | Responsibilities |
| --- | --- |
| Engine `Gameplay` | Runtime ItemId/ItemDefinition/ItemInstance/inventory/equipment representation used by the simulation. |
| Platform | Authenticated service transport, transactions, persistence, idempotency primitives, DB adapters. |
| Game Server `InventoryService` | Capacity, stack/bind/equip/trade restrictions, grant/removal policy, duplication-prevention checks, loot integration. |
| Game Client | Inventory UI, drag/drop intent, visible metadata. No authoritative item ownership. |

## Trade split [J.10.3]

| Platform mechanisms | Game Server rules | Client |
| --- | --- | --- |
| Atomic transaction primitives, locking/versioning, idempotency, RPC/service runtime, durable item/currency transfer. | Can these players trade? Can this item trade? Bound status? Tax? Cooldown? Region restriction? Confirmation rules? | Trade window, offered items, confirmation intent, display of the authoritative result. |

## Auction split [J.10.4]

| Platform mechanisms | Game Server `AuctionService` | Client-safe surface |
| --- | --- | --- |
| Service hosting, authentication context, pagination/query helpers, persistence, transactions, idempotency, event publication. | Listing rules, fees/taxes, min/max price, duration, listing caps, bid/buyout rules, regional markets, seller restrictions, anti-exploit logic, balancing algorithms. | SearchListings, CreateListing, CancelListing, PlaceBid, Buyout, plus displayable returned fields. |

The client never needs the internal economic algorithms, hidden modifiers,
anti-abuse heuristics, or server-only market rules to present the auction house.

## Quest persistence [J.10.5]

```go
// Platform-side reusable interfaces (conceptual)
type QuestStore interface {
    Load(ctx context.Context, player PlayerID) (QuestStateSet, error)
    Commit(ctx context.Context, tx QuestTransaction) error
}

type IdempotencyStore interface {
    Begin(ctx context.Context, key OperationID) (Decision, error)
    Complete(ctx context.Context, key OperationID, result Result) error
}

// RunicGame implements the actual quest progression policy on top.
```

The authoritative C++ simulation emits facts such as `EntityKilled` or
`WorldInteractionCompleted`. The private Game `QuestService` validates
origin/context, deduplicates operation/event identity, advances durable
progression, commits rewards and other durable operations, and returns accepted
state/results through Platform mechanisms.

> Open point: `QuestStore` is a quest-shaped interface in Platform, while J.1.3
> says Platform owns no quest policy. Whether Platform exposes generic
> repository/transaction helpers and Game defines `QuestStore` is tracked in
> [OQ-16](11-open-questions.md#oq-16).

## Simulation-originated loot flow [J.10.6]

```text
Client attack intent
      |
      v
Authoritative Simulation
      |
Game Combat Rules
      |
EntityKilled gameplay fact
      |
Game Loot Rules
      |
async durable grant request
      v
Platform ServiceRuntime
      |
Game InventoryService
      |
Platform transaction/persistence
      v
Database

Simulation tick never blocks waiting for the database.
```

The accepted result returns asynchronously and is applied at a later safe tick;
service callbacks never mutate Flecs directly. [J.14.4]

## Private Game services (names only)

The concrete services are RunicGame code: CharacterService, InventoryService,
QuestService, TradeService, AuctionService, EconomyService, GuildService,
MailService. Their contracts live in RunicGame `docs/services/`. [J.6.3]

## Client secrecy boundary [J.6.4]

Client-safe schemas may expose identifiers, display metadata, visible stats,
endpoint methods, and results needed for UI. Server-only packages keep drop
probabilities, hidden rarity weighting, anti-abuse thresholds, economic
coefficients, server-only spawn logic, hidden quest conditions, and encounter
logic. Shipping less rule implementation reduces easy reverse engineering,
although players can still infer behavior by observation.
