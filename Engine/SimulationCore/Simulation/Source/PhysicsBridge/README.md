# Runic::Simulation › Source/PhysicsBridge

The physics bridge: Flecs holds transforms and opaque Physics handles; a sync phase writes kinematic targets before the step and transforms back after it; contacts become facts for the next phase.

- **Filled by:** SIM-8 (slice 7)
- **Status:** empty until SIM-8 (slice 7) starts

Private to `Runic::Simulation`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Simulation` (or `Runic::Simulation::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
