# Runic::Combat

GameplayAPI · Headless: yes · Packages: CMB-1, CMB-2, CMB-3, CMB-4, CMB-5, CMB-6, CMB-7, CMB-8, CMB-9 · Status:
empty until CMB-1 (slice 16) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Ability toolkit and phase timelines, hit timing per skill, the input buffer, target slots and selection helpers, combat volumes and hit detection, status storage with Game application policies, cooldowns, rule pipeline, behavior registries, hit resolution, invulnerability restrictions, threat table, control primitives, foreign combat

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Gameplay, Physics, Environment (projectile wind), DistributedSimulation

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Abilities/`](Source/Abilities/README.md) | The ability toolkit: timelines of named phases from data, phase lengths set by a Game hook, phase events, cancel requests, and hit timing per skill (ServerTime, CappedRewind). | CMB-1 (slice 16) |
| [`Source/Targeting/`](Source/Targeting/README.md) | Target slots the Game names, selection helpers, targets in the command payload checked for range, line of sight and relation, aim-assist and homing hooks. | CMB-2 (slice 16) |
| [`Source/HitDetection/`](Source/HitDetection/README.md) | Hit detection: combat volumes and projectile sweeps over Physics queries; every hit as an ordered list along the hit timeline (time of contact, then distance) with deterministic tie-breaking; filters (relation, tags, already hit, line of sight); per-ability hit limits applied as data. | CMB-2 (slice 16) |
| [`Source/Statuses/`](Source/Statuses/README.md) | Status storage (instances, timers, periodic ticks, events, queries by tag, replication); every application goes through a policy the Game registers. | CMB-3 (slice 17) |
| [`Source/Cooldowns/`](Source/Cooldowns/README.md) | Cooldowns with shared groups and charges. | CMB-3 (slice 17) |
| [`Source/Costs/`](Source/Costs/README.md) | Ability costs as a list against Game pools. | CMB-3 (slice 17) |
| [`Source/Rules/`](Source/Rules/README.md) | The rule pipeline and behavior registries (fixed hooks for Game code); value variants selected by a per-hit context the Game defines. | CMB-4 (slice 17) |
| [`Source/HitResolution/`](Source/HitResolution/README.md) | Hit resolution: what a detected hit does. | CMB-5 (slice 18) |
| [`Source/Threat/`](Source/Threat/README.md) | The threat table. | CMB-6 (slice 19) |
| [`Source/Control/`](Source/Control/README.md) | Control primitives: control restriction sets (input, movement, abilities), forced motion through Movement, control-state hooks. | CMB-6 (slice 19) |
| [`Source/InputBuffer/`](Source/InputBuffer/README.md) | The input buffer: depth, window and priority classes set by the Game. | CMB-7 (slice 19) |
| [`Source/ForeignCombat/`](Source/ForeignCombat/README.md) | Foreign combat: volumes crossing an authority border become requests to the owner. | CMB-8 (slice 37) |
| [`Source/BehaviorSamples/`](Source/BehaviorSamples/README.md) | Combat behavior samples (reaction time from a telegraph cue, aim error) recorded into the behavior counters. | CMB-9 (slice 39) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. CMB-1 (slice 16) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Engine gameplay APIs (Engine)
- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules
