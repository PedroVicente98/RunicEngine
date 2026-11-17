# Runic::Simulation (Simulation core): the SimulationRuntime that owns the tick. SIM-1 (slice 3) starts it with the
# 60 Hz master clock, the 30, 20 and 10 Hz domains, TickContext, DomainTickCount, the Simulation layer and its
# settings; SIM-2 (slice 5) adds the realtime phases and their barriers, command queues and the simulation's
# entities, states and event hub. Contract: ARCHITECTURE.md.
runic_add_module(Simulation
    GROUP SimulationCore
    PUBLIC_DEPS Runic::Core Runic::Application Runic::ECS Runic::Events Runic::World
    HEADLESS)
