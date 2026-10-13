---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/world-systems
  - headless/yes
  - slice/25
aliases:
  - "Runic::Environment"
---

# Environment

Contract seed for `Runic::Environment`: what a Claude Code session needs to implement the Environment work packages. When `Engine/WorldSystems/Environment/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | World systems |
| Directory | `Engine/WorldSystems/Environment/` |
| CMake target | `Runic::Environment` |
| Namespace | `Runic::Environment` |
| Headless | yes |
| Status | Not started |

## Purpose

Environment makes wind, time-varying currents, tides, gameplay swell and weather deterministic functions of world time, position and a small replicated weather state. Every simulation and every client computes the same environment from the same inputs, so nothing about it is replicated per tick; the Game supplies the patterns and decides what they mean (ARCH[^arch] § Environment: wind, currents, tides, waves and weather).

## Owns

| Part | Contains |
| --- | --- |
| EnvironmentClock | Shared world time and what derives from it: time of day, tide phase, season, weather schedule steps |
| WindField | Cyclic baseline routine (direction at any angle, signed speed), cooked regional wind maps, altitude profile, deterministic gusts, wind volumes, weather scaling |
| Current modulation | Time variation of ocean current fields by a cyclic baseline routine, tide phase, season and weather |
| TideSystem | Water[^engine-water]-level offset over time per water body or route |
| WaveField | Optional deterministic gameplay swell, scaled by weather |
| WeatherState | Per-region weather parameters, transitions, the baseline schedule evaluator and time-boxed overrides |
| World routines | Cooked routes and schedules evaluated as position = f(route, world time, seed), with slow deterministic drift (ENV-6) |

### Environment clock

- World time counts from a world epoch that the Platform World Coordinator[^platform-coreservices] issues. Until PCS-2 ships in slice 36, the epoch is read from configuration (ENV-1). Each simulation derives world time from its synchronized clock at start, advances it with the fixed-step master clock and corrects drift slowly.
- Clients receive world time at connection and in snapshots, so they evaluate the same functions as the server.
- Time of day (which drives ambient light), tide phase, season and the weather schedule step all derive from world time; day length and season length are Game data.

### Wind, currents, tides, waves and weather

- **Wind** at a position combines the region's baseline routine (a direction at any angle and a signed speed that cycle with world time), a cooked regional wind map (a coarse vector grid, for example one vector per 512 m), an altitude profile (stronger at height, zero inside sheltered volumes such as caves), the region's weather multiplier, deterministic gusts (seeded noise, smooth over time) and authored wind volumes such as updrafts and downdrafts. A routine speed of 0 has no effect, and a negative speed reverses the push abruptly without changing the angle. A game may let characters only glide over an area, carried by this wind; gliding against it is then impossible and pushes the character back.
- **Currents over time:** the base current comes from the water body; Environment modulates it with a tidal term, a seasonal factor, the weather's current multiplier and any active override:

```latex
c(p,t) = c_{0}(p) \times \left(1 + a(p)\,\sin(\phi_{tide}(t) + \delta(p))\right) \times k_{season}(t) \times k_{weather}(p,t)
```

  c0 = cooked base current, which follows the same cyclic baseline routine model as wind; a and δ = cooked tidal amplitude and phase offset; k = Game-defined factors. `WaterSample.flow_velocity` already contains the result.
- **Tides:** a TideProfile (data) per water body or route sets period, amplitude and phase, or a custom curve. Terrain stays static, so low tide exposes walkable ground and high tide turns it back into water.
- **Waves:** by default the authoritative surface is flat or smoothly sloped and visual waves exist only on the client. Optionally an ocean enables a WaveField: a few deterministic swell components, amplitude scaled by weather, evaluated identically by server and client.
- **Weather:** WeatherState per region holds a weather type (a content ID), intensity, wind multiplier, wave amplitude, precipitation, fog density, ambient light multiplier, current multiplier and transition progress. A baseline schedule is evaluated from the world seed, region, season and world time against data tables, so every simulation and client agrees without messages. Overrides carry an effective world time a few seconds ahead and a duration, so every owning simulation applies them on the same tick and the baseline returns when they end. There is no weather or wind inside sheltered volumes (caves and underground spaces, World authored volumes); there, light comes only from ambient profiles and emitters.

### Routines, events and world routines

- **Cyclic routines.** Wind and sea currents follow a baseline routine that cycles with world time and the world seed, so it is mappable: a game can chart it.
- **Operator events.** Events that change wind or currents for a duration arrive as Platform world directives with an effective time and a duration (PCS-3, ENV-5); the baseline returns when they end. Whether and from when an event can be predicted is game data.
- **World routines (ENV-6).** Escorts, trade routes, ships and mob routines use the same model: cooked routes and schedules, with position = f(route, world time, seed). A cycle that does not match the clock plus a small seeded variation makes routines drift slowly and deterministically.
- **No simulation of empty areas.** When a cell activates, routine entities spawn where their routine puts them (Gameplay, GPL-8[^engine-gameplay]), so an NPC is at the right point when a player arrives.
- **Exceptions only.** A game service stores only player-caused exceptions, such as a delayed escort, and each exception fades back into the routine over time; Environment only evaluates the routine.

### Determinism and replication

- Every value is a pure function of position, world time, weather state and cooked fields, implemented once in Engine code that client and server both run.
- Replicated: world time (at connection and periodically), weather, wind and current overrides (reliable facts) and dynamic water states. Never per-tick wind, current or wave values.
- Bit-exact agreement is not required: the server is authoritative and client prediction is corrected like any other.
- Neighboring simulations compute identical values at a border, and overrides reach every owner of the affected regions, so ships and gliders cross without a jump. Routine positions are identical too, so a routine entity crosses a border without a jump.

## Does not own

- Water[^engine-water] sampling: the WaterSystem calls Environment internally for tide offsets, current modulation and swell (Water).
- The sail model and watercraft forces (Water, WATER-6), the glide mode (Gameplay), the projectile wind flag (Combat[^engine-combat]).
- Light: Perception[^engine-perception] reads time of day and weather light.
- Sheltered volumes (caves, underground spaces): World authored volumes; Environment only samples calm inside them.
- Spawning routine entities when a cell activates (Gameplay, GPL-8[^engine-gameplay]) and storing player-caused exceptions to routines (a RunicGame service).
- Patterns and meaning: wind and current maps, tide profiles, day and season lengths, weather types and tables, storm effects (RunicGame data).
- Deciding special weather (a RunicGame Go service) and delivering it: Platform world directives[^platform-coreservices] (PCS-3) reach the simulation through the services bridge[^platform-serverbindings] and the Simulation's service port (PBND-4).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core[^engine-core], World[^engine-world], Simulation[^engine-simulation] |
| Third-party | — |

### Must never depend on

- Water[^engine-water], Perception[^engine-perception], Gameplay, Combat[^engine-combat], AI or Network: they read Environment.
- Presentation modules (the module is headless; clients run the same functions through the Client's extractors).
- NATS, gRPC or any service: overrides arrive as accepted results through the service port.
- Game patterns or values in code or tests; RunicPlatform or RunicGame code; Agones or Kubernetes.

## Public API sketch

ARCH[^arch]'s illustrative API (ARCH § Environment, API):

```cpp
// Illustrative contract, not a frozen API.
struct WindSample {
    Vec3  velocity;      // m/s, world space
    float gust;          // 0..1, current gust strength
    float turbulence;    // for control noise and visuals
};

struct WeatherSample {
    WeatherTypeId type;
    float intensity, wind_multiplier, wave_amplitude;
    float precipitation, fog_density;
    float ambient_light_multiplier, current_multiplier;
};

class Environment {
public:
    WorldTime    Now() const;
    TimeOfDay    TimeOfDayAt(WorldTime t) const;
    float        TidePhase(WaterBodyId body, WorldTime t) const;
    float        WaterLevelOffset(WaterBodyId body, WorldTime t) const;
    WindSample   SampleWind(const WorldPosition& p, WorldTime t) const;
    WeatherSample SampleWeather(RegionId region, WorldTime t) const;
    float        WaveHeight(WaterBodyId body, const WorldPosition& p, WorldTime t) const;
    RoutineSample SampleRoutine(RoutineId routine, WorldTime t) const;   // world routines (ENV-6)

    // From a Platform directive; takes effect at override.effective_time.
    void ApplyOverride(const EnvironmentOverride& override);
};
```

Headers (sketch, fixed in the contract session): `EnvironmentClock.hpp`, `WindField.hpp`, `CurrentModulation.hpp`, `Tides.hpp`, `WaveField.hpp`, `Weather.hpp`, `WorldRoutines.hpp`, `Environment.hpp`.

## Threading and tick rules

- Environment's own update runs at 10 Hz and only advances weather transitions and per-region caches (tide and weather state are 10 Hz work).
- Each sample is an interpolation in a coarse cooked grid plus a few sine evaluations: constant cost, read-only, safe in parallel phases.
- Nothing reads the wall clock: world time comes from the master clock through the `TickContext`.
- An override is applied at its effective world time, at a safe tick, never from an I/O callback.
- Memory: wind and current maps at 512 m need about 2,300 vectors each per 600 km² map; a 1,000,000 km² world needs about 3.8 million vectors per map (about 30 MB), streamed by sector.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| ENV-1 | Environment clock: world time, day and night phase, sun direction and the skylight input Perception[^engine-perception] reads; the world epoch comes from configuration until PCS-2[^platform-coreservices] issues it | Two simulations agree on the time of day; night starts at the same world time everywhere | SIM-1[^engine-simulation], WORLD-1[^engine-world] | 25 |
| ENV-2 | Deterministic wind field with a cyclic baseline routine: per-region direction at any angle and a signed speed (0 has no effect; a negative speed reverses the push without changing the angle) | The same position and time give the same wind on two simulations; a negative speed reverses the push without changing its angle | ENV-1 | 42 |
| ENV-3 | Current modulation over time: cyclic baseline routine, tide phase, season, weather | Every simulation samples the same current at the same time | ENV-1, WATER-5[^engine-water] | 42 |
| ENV-4 | Tides and gameplay waves | A tide opens and closes a path on schedule | ENV-1 | 43 |
| ENV-5 | Weather state and time-boxed overrides (effective time and duration) for weather, wind and currents; the baseline routine returns when they end | A storm and a wind event reach two simulations at the same world time and end on schedule, after which the routine resumes | ENV-1 | 43 |
| ENV-6 | World routines: cooked routes and schedules evaluated as position = f(route, world time, seed), with slow deterministic drift (a cycle that does not match the clock plus a small seeded variation) | Two simulations compute the same routine position at the same world time, drift included | ENV-1 | 43a |

The Environment clock ships in slice 25, before night-time perception (PRC-1 needs ENV-1).

## Acceptance tests

ARCH[^arch]'s acceptance list (ARCH § Environment, acceptance tests). Several run together with their consumer module:

| Test | Passes when |
| --- | --- |
| Same wind everywhere | Two simulations and two clients sample identical wind at a shared border at the same world time |
| Sailing angles | A sailing ship gains speed at favorable wind angles and none straight into the wind, matching its curve (with WATER-6[^engine-water]) |
| Leeway | A crosswind pushes a ship sideways in proportion to its lateral resistance (with WATER-6) |
| Tidal path | At low tide a tidal path is walkable; at high tide characters swim there |
| Storm override | A storm override applies on the same world time in two simulations; wind, waves and light change; clients follow without per-tick data |
| Updraft | A glider gains altitude crossing an authored updraft volume (with Gameplay's glide mode) |
| Cave volume | Wind and weather sample as calm inside a cave volume; light comes only from ambient profiles and emitters |
| Projectile flag | Projectiles ignore wind unless their ability sets the wind flag (with Combat[^engine-combat]) |

Tests use Engine test patterns, never RunicGame weather tables.

## Open items

- Decided in ARCH[^arch] revision 369: `WaterBodyId` is a plain value type in Core (CORE-1[^engine-core]), so Environment's API uses it without depending on Water[^engine-water].
- Decided in ARCH revision 369: Environment registers world time and weather state as world-state values in Simulation's replication registry (SIM-6[^engine-simulation]), so clients receive them in snapshots without a Network → Environment edge.
- ENV-5 receives overrides through the service port inbox; the operation's shape is fixed with PBND-4[^platform-serverbindings] (slice 30) and PCS-3[^platform-coreservices] (slice 43).

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Core|Core]] · [[../modules/World|World]] · [[../modules/Simulation|Simulation]]
- **Depended on by:** [[../modules/Gameplay|Gameplay]] · [[../modules/Combat|Combat]] · [[../modules/Perception|Perception]] · [[../modules/Water|Water]] · [[../modules/Client|Client]]
- **Packages that need it:** [[../../../RunicGame/docs/game-packages|Game work packages]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-water]: [[../modules/Water|Water]] — Engine module · World systems
[^platform-coreservices]: [[../../../RunicPlatform/docs/modules/CoreServices|CoreServices]] — Platform module · Go
[^engine-gameplay]: [[../modules/Gameplay|Gameplay]] — Engine module · GameplayAPI
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-perception]: [[../modules/Perception|Perception]] — Engine module · World systems
[^platform-serverbindings]: [[../../../RunicPlatform/docs/modules/ServerBindings|ServerBindings]] — Platform module · C++
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
[^engine-world]: [[../modules/World|World]] — Engine module · Simulation core
[^engine-simulation]: [[../modules/Simulation|Simulation]] — Engine module · Simulation core
