# Runic::Simulation › Source/ServicePort

The service port's implementation: an outbox of durable operations with stable IDs and an inbox of accepted results drained at safe ticks. Its interface ships as the header-only `SimulationPort/` target that the Platform implements.

- **Filled by:** SIM-5 (slice 27)
- **Status:** empty until SIM-5 (slice 27) starts

Private to `Runic::Simulation`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Simulation` (or `Runic::Simulation::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
