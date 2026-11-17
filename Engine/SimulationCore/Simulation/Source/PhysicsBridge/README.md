# Runic::Simulation › Source/PhysicsBridge

The physics bridge: Flecs holds transforms and opaque Physics handles; a sync phase writes kinematic targets before the step and transforms back after it; contacts become facts for the next phase.

- **Filled by:** SIM-8 (slice 7)
- **Status:** SIM-8 (slice 7) wrote `PhysicsBridge.cpp`: the requests applied in the PhysicsStructure phase, the sync on each side of the step, contacts as facts, bodies and characters that follow their destroyed entities at every barrier, and `SimulationRuntime::EnablePhysics`

Private to `Runic::Simulation`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Simulation` (or `Runic::Simulation::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
