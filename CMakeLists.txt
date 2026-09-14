cmake_minimum_required(VERSION 3.24)
project(RunicEngine VERSION 0.1.0 LANGUAGES C CXX)

option(RUNIC_ENGINE_BUILD_TESTS "Build the disposable engine link smoke tests" ${PROJECT_IS_TOP_LEVEL})
option(RUNIC_ENGINE_FETCH_DEPENDENCIES "Fetch pinned Flecs, GLM, and Jolt sources" OFF)
option(RUNIC_ENGINE_ENABLE_PRESENTATION "Also fetch GLFW, bgfx, and ImGui for client tools" OFF)

if(RUNIC_ENGINE_ENABLE_PRESENTATION AND NOT RUNIC_ENGINE_FETCH_DEPENDENCIES)
    message(FATAL_ERROR "Presentation requires RUNIC_ENGINE_FETCH_DEPENDENCIES=ON")
endif()

include(cmake/RunicWarnings.cmake)

add_library(runic_core STATIC)
add_library(Runic::Core ALIAS runic_core)
target_sources(runic_core PRIVATE src/Core/Version.cpp)
target_include_directories(runic_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_compile_features(runic_core PUBLIC cxx_std_20)
set_target_properties(runic_core PROPERTIES CXX_EXTENSIONS OFF)
runic_engine_warnings(runic_core)

# This is a dependency boundary, not a simulation implementation.
add_library(runic_runtime INTERFACE)
add_library(Runic::Runtime ALIAS runic_runtime)
target_compile_features(runic_runtime INTERFACE cxx_std_20)
target_link_libraries(runic_runtime INTERFACE Runic::Core)

if(RUNIC_ENGINE_FETCH_DEPENDENCIES)
    include(cmake/RunicDependencies.cmake)
    runic_engine_dependencies()
    target_link_libraries(runic_runtime INTERFACE flecs::flecs_static glm::glm Jolt)
endif()

if(RUNIC_ENGINE_ENABLE_PRESENTATION)
    add_library(runic_presentation INTERFACE)
    add_library(Runic::Presentation ALIAS runic_presentation)
    target_compile_features(runic_presentation INTERFACE cxx_std_20)
    target_link_libraries(runic_presentation INTERFACE glfw bgfx runic_imgui)
endif()

if(RUNIC_ENGINE_BUILD_TESTS)
    enable_testing()
    add_executable(RunicEngineBootstrap tests/Bootstrap.cpp)
    target_link_libraries(RunicEngineBootstrap PRIVATE Runic::Runtime)
    runic_engine_warnings(RunicEngineBootstrap)
    add_test(NAME RunicEngine.Bootstrap COMMAND RunicEngineBootstrap)
    set_tests_properties(RunicEngine.Bootstrap PROPERTIES
        PASS_REGULAR_EXPRESSION "RunicEngine 0\\.1\\.0"
    )

    if(RUNIC_ENGINE_FETCH_DEPENDENCIES)
        add_executable(RunicEngineDependencySmoke tests/Dependencies.cpp)
        target_link_libraries(RunicEngineDependencySmoke PRIVATE Runic::Runtime)
        runic_engine_warnings(RunicEngineDependencySmoke)
        add_test(NAME RunicEngine.Dependencies COMMAND RunicEngineDependencySmoke)
    endif()
    if(RUNIC_ENGINE_ENABLE_PRESENTATION)
        add_executable(RunicEnginePresentationSmoke tests/Presentation.cpp)
        target_link_libraries(RunicEnginePresentationSmoke PRIVATE Runic::Presentation)
        runic_engine_warnings(RunicEnginePresentationSmoke)
        add_test(NAME RunicEngine.Presentation COMMAND RunicEnginePresentationSmoke)
    endif()
endif()
