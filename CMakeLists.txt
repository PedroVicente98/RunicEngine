# 1. Project
cmake_minimum_required(VERSION 3.24)
project(RunicEngine LANGUAGES C CXX)

# 2. Executable and headless build choices
option(RUNIC_BUILD_SANDBOX "Build the disposable dependency-check application" ${PROJECT_IS_TOP_LEVEL})
option(RUNIC_ENGINE_ENABLE_PRESENTATION "Build GLFW, bgfx, and ImGui" ${RUNIC_BUILD_SANDBOX})

if(RUNIC_BUILD_SANDBOX AND NOT RUNIC_ENGINE_ENABLE_PRESENTATION)
    message(FATAL_ERROR "The sandbox checks all dependencies; enable presentation or disable the sandbox.")
endif()

# 3. Compiler settings, public target boundaries, and all third-party setup
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Targets)
include(Dependencies)
runic_engine_dependencies()

# 4. Disposable executable; no engine implementation exists yet
if(RUNIC_BUILD_SANDBOX)
    include(Sandbox)
endif()

# 5. BUILD-1 acceptance tests (contract: docs/build/BUILD-1.md). They fail until BUILD-1 is implemented.
option(RUNIC_ENGINE_BUILD_TESTS "Register the Engine tests" ${PROJECT_IS_TOP_LEVEL})
if(RUNIC_ENGINE_BUILD_TESTS)
    enable_testing()
    if(PROJECT_IS_TOP_LEVEL AND NOT RUNIC_BUILD_TESTS_NESTED)
        add_subdirectory(tests/build)
    endif()
endif()
