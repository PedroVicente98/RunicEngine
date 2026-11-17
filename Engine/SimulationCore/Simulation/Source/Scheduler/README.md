# Runic::Simulation › Source/Scheduler

The SimulationRuntime's multi-rate scheduler: the 60 Hz master clock, the 30, 20 and 10 Hz domains, TickContext, DomainTickCount and domain metrics. This is where the tick rate lives.

- **Filled by:** SIM-1 (slice 3)
- **Status:** SIM-1 (slice 3) wrote `SimulationRuntime.cpp` (the master step, the slots, the domain metrics), `TickDomain.cpp` (`DomainTickCount`'s conversions) and `SimulationSettings.cpp` (the settings in the process configuration); SIM-2 (slice 5) added `RuntimeState.hpp` (the runtime's state, shared with `Source/Phases/` and `Source/Commands/`) and the phases of the realtime slot to the master step

Private to `Runic::Simulation`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Simulation` (or `Runic::Simulation::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
