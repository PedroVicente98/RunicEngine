# Perception

Contract seed for `Runic::Perception`: what a Claude Code session needs to implement the Perception work packages. When `Engine/WorldSystems/Perception/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | World systems |
| Directory | `Engine/WorldSystems/Perception/` |
| CMake target | `Runic::Perception` |
| Namespace | `Runic::Perception` |
| Headless | yes |
| Status | Not started |

## Purpose

Perception is the light and perception API. Light scales how far mobs notice entities and how far players see each other; it never changes how a hit is resolved (ARCH § Engine gameplay APIs, light and perception). The module owns light emitters, ambient light and skylight, luminosity queries, concealment, warn and detect radii, and the PvP visibility tiers that Network applies as server-side fog of war.

## Owns

| Element | What it is | What a game supplies |
| --- | --- | --- |
| AmbientLightProfile | Light level per region and layer over time of day and weather, with a floor ("skylight") | Curves per region, layer and weather |
| LightEmitter | Radius, intensity and duration on an entity or area | Which entities, abilities, items and areas emit light |
| Luminosity | Effective light on an entity: the brighter of ambient light and emitters reaching it | Nothing; computed |
| Concealment | Multiplier on how far others detect or see the entity | Concealment values |
| PerceptionProfile | Warn and detect base radii, minimum and maximum, light curve, dark vision | Per mob definition |
| VisibilityProfile | Maximum visible range, silhouette range, light curve | Global or per region |

Rules:

- Emitters are indexed in cells, so luminosity is a few lookups in neighboring cells. It updates at 10 Hz for players and for mobs awake or near players; lighting an emitter updates it at once.
- Mob radius = base × light curve (target luminosity) × target concealment, clamped to the profile's minimum and maximum. The minimum guarantees detection in total darkness.
- Inside the warn radius a mob investigates; inside the detect radius it engages. A game that sets warn at or below detect never uses warn.
- Stealth and invisibility are concealment values; zero means undetectable. Changing concealment changes a value, not an archetype.
- There is no line-of-sight vision model: perception is radii scaled by light and concealment.

### PvP visibility tiers (decided)

| Tier | Condition | Data sent |
| --- | --- | --- |
| Full | Within visibility range, set by the target's luminosity and concealment | Everything |
| Silhouette | Within the minimum silhouette range in any light, or revealed by contact in the last few seconds | Coarse position and pose at a low rate; no name, gear or identity |
| Hidden | Otherwise | Nothing |

- Contact reveals: hitting or being hit, a heal nearby, AoE contact.
- Perception computes the tier per candidate pair; Network applies it inside the interest pass (NET-4). The cost is one multiplication per candidate pair inside a pass that already runs.
- Combat never uses tiers; the server resolves every hit regardless of visibility.

## Does not own

- AI wake, sleep, leash and the Aggroable toggle (AI); AI asks this module for radii.
- Interest filtering, priority and what is sent (Network). Projectiles and effects from hidden players are sent without the caster's identity by Network.
- World time, time of day and weather (Environment); Perception reads them.
- Drawing lights and darkness (the Client extracts lights and volumes, CLI-7; the Renderer draws them, RND-7).
- Every value: curves, radii, concealment values, emitter definitions (RunicGame).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Gameplay, World, Environment (time of day, weather), Water (underwater light) |
| Third-party | — |

### Must never depend on

- AI, Combat or Network. They read Perception; Perception never reads them.
- Client, Renderer, UI or any presentation library (the module is headless).
- Game values in code or tests; RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH's AI code calls the API as `perception.Profile(profileId)` and `perception.RadiiFor(profile, target)`, the latter returning warn and detect radii already scaled by light and concealment (ARCH § AI architecture, perception, wake, leash and sleep).

Headers (sketch, fixed in the contract session): `LightEmitter.hpp`, `AmbientLightProfile.hpp`, `Luminosity.hpp`, `Concealment.hpp`, `PerceptionProfile.hpp`, `PerceptionApi.hpp`, `VisibilityTiers.hpp`.

## Threading and tick rules

- Luminosity updates run in the 10 Hz domain; AI perception reads radii in the 20 Hz domain (ARCH § Simulation core: clock and threads).
- Queries are read-only and safe from any worker in a parallel phase; emitter registration happens at the points the Simulation defines.
- Client and server compute the same luminosity from the same emitters and ambient profiles, so what the server treats as lit matches what the player sees.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| PRC-1 | Light: emitters, ambient profiles, skylight curve driven by the Environment clock, luminosity at a point | Luminosity matches emitter falloff; client and server agree | WORLD-2, GPL-2, ENV-1 | 25 |
| PRC-2 | PvP visibility tiers (Full, Silhouette, Hidden) with reveal on contact | Tiers change at the configured distances and light levels | PRC-1 | 25 |
| PRC-3 | Perception profiles: warn and detect radii from light and concealment, with a minimum | Radii shrink with darkness and stealth but never below the minimum | PRC-1 | 33 |

## Acceptance tests

The Done when column is the acceptance list. Slice 25 shows visibility tiers at night; slice 33 shows mobs that warn, detect, fight and leash at night (with AI-3). ARCH's AI integration tests also exercise this module: at night an unlit entity passes outside the reduced detect radius unnoticed while one carrying a light is detected from farther away; raising concealment drops a target according to policy, and lowering it makes the target detectable again. Tests use Engine test profiles.

## Open items

- Decided in ARCH revision 369: Perception may link Water to read each water body's underwater light attenuation by depth (Game values); luminosity and visibility use it like any ambient profile.
