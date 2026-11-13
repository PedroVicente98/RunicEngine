# Runic::SimulationPort (a target of Runic::Simulation)

Headers of the header-only `Runic::SimulationPort` interface target: the service port the Platform's services-bridge layer implements. They include only Core types, so the Platform builds against Core and these headers without Flecs, Jolt or the gameplay modules.

- **Created by:** SIM-5 (slice 27)
- **Status:** empty until SIM-5 (slice 27) starts
- This folder is the target's own include root, the way `Public/` is the module's: its headers are included by their path under this folder.

Layout and rules: [the module's README](../README.md).
