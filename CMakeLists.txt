# RunicEngine. Contract of this build skeleton: docs/build/BUILD-1.md (slice 0).
cmake_minimum_required(VERSION 3.24)
project(RunicEngine VERSION 0.1.0 LANGUAGES C CXX)

set_property(GLOBAL PROPERTY RUNIC_ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ---- 1. What this configuration builds ----------------------------------------------------------------------
option(RUNIC_ENGINE_BUILD_TESTS "Module tests (GoogleTest), the BUILD-1 acceptance tests and the scans" ${PROJECT_IS_TOP_LEVEL})
option(RUNIC_ENGINE_BUNDLES "The transitional Runic::Runtime bundle (Flecs, GLM, Jolt) RunicGame links" ON)
option(RUNIC_ENGINE_ENABLE_PRESENTATION "Godot as a library: godot-cpp, libgodot, Runic::GodotBridge" ${PROJECT_IS_TOP_LEVEL})
option(RUNIC_ENGINE_BUILD_TOOLS "The RunicEditor and Runic::GodotTools (never shipped)" ${PROJECT_IS_TOP_LEVEL})
option(RUNIC_BUILD_SANDBOX "The disposable check that the bundle's third parties compile and link" ${PROJECT_IS_TOP_LEVEL})
if(RUNIC_ENGINE_BUILD_TOOLS AND NOT RUNIC_ENGINE_ENABLE_PRESENTATION)
    message(FATAL_ERROR "RUNIC: the RunicEditor needs Godot; enable RUNIC_ENGINE_ENABLE_PRESENTATION or disable RUNIC_ENGINE_BUILD_TOOLS.")
endif()
if(RUNIC_BUILD_SANDBOX AND NOT RUNIC_ENGINE_BUNDLES)
    message(FATAL_ERROR "RUNIC: the sandbox checks the bundle; enable RUNIC_ENGINE_BUNDLES or disable RUNIC_BUILD_SANDBOX.")
endif()
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# ---- 2. Helpers, catalog, toolchain -------------------------------------------------------------------------
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(RunicModule)
set(RUNIC_MODULE_NAMESPACE Runic)
include(RunicModuleCatalog)
include(Targets)
if(RUNIC_ENGINE_BUILD_TESTS)
    runic_enable_tests()
endif()

# ---- 3. Third parties ---------------------------------------------------------------------------------------
if(RUNIC_ENGINE_BUNDLES)
    include(Dependencies)
    runic_engine_dependencies()
endif()
if(RUNIC_ENGINE_ENABLE_PRESENTATION)
    include(RunicGodot)
    runic_godot_setup()
    # A copy of the neutral spike project, so the editor's .godot/ import cache stays out of the sources.
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/godot/SpikeProject" DESTINATION "${CMAKE_BINARY_DIR}/godot")
    set_property(GLOBAL PROPERTY RUNIC_GODOT_SPIKE_PROJECT "${CMAKE_BINARY_DIR}/godot/SpikeProject")
endif()

# ---- 4. Modules (Engine/<Group>/<Module>/) and the Engine's programs (Engine/Programs/<Program>/) ---------
add_subdirectory(Engine)
if(RUNIC_BUILD_SANDBOX)
    add_subdirectory(tests/sandbox)
endif()

# ---- 5. Tests: BUILD-1 acceptance tests and the scans --------------------------------------------------------
if(RUNIC_ENGINE_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL AND NOT RUNIC_BUILD_TESTS_NESTED)
    add_subdirectory(tests/build)
endif()
