# Runic::Core (Foundation): the Engine's vocabulary — plain IDs, Result and asserts, logging hooks, time, GLM math,
# the 64-bit world-space types and fixed-capacity containers. Contract: ARCHITECTURE.md. CORE-1 (slice 1) starts it.
runic_add_module(Core
    GROUP Foundation
    PUBLIC_DEPS glm
    HEADLESS)

# The type rule (RunicDocs › World space coordinates, cells and activation › Which values are 64-bit): a GLM vector
# changes precision only explicitly. Public, so every target that includes Core's math compiles GLM the same way.
# The module is header-only until Source/ has code, and an INTERFACE target takes only the INTERFACE keyword.
get_target_property(_runic_core_type runic_core TYPE)
if(_runic_core_type STREQUAL "INTERFACE_LIBRARY")
    target_compile_definitions(runic_core INTERFACE GLM_FORCE_EXPLICIT_CTOR)
else()
    target_compile_definitions(runic_core PUBLIC GLM_FORCE_EXPLICIT_CTOR)
endif()
unset(_runic_core_type)
