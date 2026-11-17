# Runic::Physics (Simulation core): Jolt v5.6 in double precision behind Runic types. PHYS-1 (slice 7) starts it with
# the PhysicsWorld, bodies, shapes and collision layers; PHYS-2 adds characters. Jolt is private: no Jolt type appears
# in a public header. Contract: ARCHITECTURE.md.
runic_add_module(Physics
    GROUP SimulationCore
    PUBLIC_DEPS Runic::Core
    PRIVATE_DEPS Jolt
    HEADLESS)
