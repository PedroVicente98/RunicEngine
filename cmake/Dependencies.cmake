include_guard(GLOBAL)
include(FetchContent)

# Headless third parties of the transitional Runic::Runtime bundle (Flecs, GLM, Jolt). Every archive is
# pinned and SHA-256 checked. Sources live in external/ (git-ignored, next to the Godot submodules); generated files and compiled libraries stay in
# each consumer's build tree. Function scope keeps upstream build options out of consumer directories.
#
# Presentation is Godot (cmake/RunicGodot.cmake): GLFW, bgfx, bx, bimg and Dear ImGui of the v3 architecture
# are no longer part of the Engine (RunicDocs › Godot integration; Decisions log › Rendering, UI).
function(runic_engine_dependencies)
    get_filename_component(third_party_dir
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../external" ABSOLUTE)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)

    # Reuse populated source folders across Debug/Release and sibling builds. The folder name carries the
    # version, so changing a pin can never silently reuse an old tree.
    set(names flecs glm jolt)
    set(folders flecs-4.0.4 glm-1.0.1 JoltPhysics-5.2.0)
    foreach(name folder IN ZIP_LISTS names folders)
        string(TOUPPER "${name}" key)
        if(NOT FETCHCONTENT_SOURCE_DIR_${key} AND IS_DIRECTORY "${third_party_dir}/${folder}")
            set(FETCHCONTENT_SOURCE_DIR_${key} "${third_party_dir}/${folder}")
        endif()
    endforeach()

    # ---- 1. Flecs: C++ bindings over the static C core ----
    set(FLECS_STATIC ON)
    set(FLECS_SHARED OFF)
    set(FLECS_TESTS OFF)
    set(FLECS_PIC ON)
    FetchContent_Declare(flecs
        SOURCE_DIR "${third_party_dir}/flecs-4.0.4"
        URL https://codeload.github.com/SanderMertens/flecs/tar.gz/refs/tags/v4.0.4
        URL_HASH SHA256=a3b6238a913f65d90db18759ab5442393901da914e4a9bfe30aa8823687dce86
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
    )
    FetchContent_MakeAvailable(flecs)

    # ---- 2. GLM: header-only mathematics (Core's approved public exception) ----
    set(GLM_BUILD_LIBRARY OFF)
    set(GLM_BUILD_TESTS OFF)
    set(GLM_BUILD_INSTALL OFF)
    FetchContent_Declare(glm
        SOURCE_DIR "${third_party_dir}/glm-1.0.1"
        URL https://codeload.github.com/g-truc/glm/tar.gz/refs/tags/1.0.1
        URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
    )
    FetchContent_MakeAvailable(glm)

    # ---- 3. Jolt: headless static library ----
    # The architecture pins Jolt v5.6.0 in double precision (PHYS-1, slice 7; the v5.6.0 archive's SHA-256 is
    # 6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e). Until PHYS-1 the bootstrap pin
    # stays v5.2.0. Its SIMD paths follow RUNIC_CPU_BASELINE: AVX2 and FMA on for x86-64-v3.
    set(OVERRIDE_CXX_FLAGS OFF)
    set(INTERPROCEDURAL_OPTIMIZATION OFF)
    set(ENABLE_ALL_WARNINGS OFF)
    set(ENABLE_INSTALL OFF)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
    set(DEBUG_RENDERER_IN_DISTRIBUTION OFF)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
    set(PROFILER_IN_DISTRIBUTION OFF)
    _runic_cpu_baseline_applies(baseline)
    foreach(feature SSE4_1 SSE4_2 AVX AVX2 LZCNT TZCNT F16C FMADD)
        set(USE_${feature} ${baseline})
    endforeach()
    set(USE_AVX512 OFF)
    FetchContent_Declare(jolt
        SOURCE_DIR "${third_party_dir}/JoltPhysics-5.2.0"
        URL https://codeload.github.com/jrouwe/JoltPhysics/tar.gz/refs/tags/v5.2.0
        URL_HASH SHA256=f478afe3050c885e21403748e10ab18e3e8df8b0982c540e75f1e078ef8b2c88
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(jolt)

    # Third-party headers are system headers: their warnings never fail a Runic build (CMake 3.25+).
    foreach(library flecs::flecs_static glm::glm Jolt)
        get_target_property(real ${library} ALIASED_TARGET)
        if(NOT real)
            set(real ${library})
        endif()
        set_target_properties(${real} PROPERTIES SYSTEM TRUE)
    endforeach()

    # Library IDs used by the catalog rows (THIRD_PARTY) and the public-header exceptions.
    runic_third_party(flecs TARGETS flecs::flecs_static)
    runic_third_party(glm TARGETS glm::glm)
    runic_third_party(jolt TARGETS Jolt)

    # Transitional bundle (OQ-8): RunicGame's executables link it until they link per-module targets.
    target_link_libraries(runic_runtime INTERFACE flecs::flecs_static glm::glm Jolt)
endfunction()
