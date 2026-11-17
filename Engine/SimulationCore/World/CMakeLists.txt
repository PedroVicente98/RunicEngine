# Runic::World (Simulation core): the world's coordinates and partition. WORLD-1 (slice 4) starts it with
# region-local origins, the world registry, and Region, Sector and sparse 3D Cell keys. Contract: ARCHITECTURE.md.
runic_add_module(World
    GROUP SimulationCore
    PUBLIC_DEPS Runic::Core Runic::ECS
    HEADLESS)
