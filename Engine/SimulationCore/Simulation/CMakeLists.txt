# Runic::Simulation (Simulation core): the SimulationRuntime that owns the tick. SIM-1 (slice 3) starts it with the
# 60 Hz master clock, the 30, 20 and 10 Hz domains, TickContext, DomainTickCount, the Simulation layer and its
# settings. Contract: ARCHITECTURE.md.
runic_add_module(Simulation
    GROUP SimulationCore
    PUBLIC_DEPS Runic::Core Runic::Application
    HEADLESS)
