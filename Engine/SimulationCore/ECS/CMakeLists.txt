# Runic::ECS (Simulation core): the EntityWorld, the Runic face of Flecs. ECS-1 (slice 4) starts it with the world
# wrapper, component and query registration through Runic types, and the opt-in Runic::ECSNative target.
# Contract: ARCHITECTURE.md.
runic_add_module(ECS
    GROUP SimulationCore
    PUBLIC_DEPS Runic::Core
    PRIVATE_DEPS flecs
    HEADLESS)

# The opt-in door to the native Flecs world (discouraged). A module that needs Flecs itself links Runic::ECSNative
# PRIVATE; the end of configure lists every module that does (cmake/README.md › runic_add_native_target).
runic_add_native_target(ECSNative FROM ECS HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/ECSNative" LIBRARY flecs)

# The module's own tests go through the door once, to show it reaches the same world.
if(TARGET RunicECSTests)
    target_link_libraries(RunicECSTests PRIVATE Runic::ECSNative)
endif()
