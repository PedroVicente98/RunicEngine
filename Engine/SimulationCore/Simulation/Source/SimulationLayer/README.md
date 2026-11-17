# Runic::Simulation › Source/SimulationLayer

The Simulation layer: the Application layer that drives the SimulationRuntime inside a process's LayerStack (GameServer, the RunicEditor's local world).

- **Filled by:** SIM-1 (slice 3)
- **Status:** SIM-1 (slice 3) wrote `SimulationLayer.cpp`: owed master steps from the loop's time, the catch-up limit and the backlog

Private to `Runic::Simulation`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Simulation` (or `Runic::Simulation::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
