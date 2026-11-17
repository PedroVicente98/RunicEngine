include_guard(GLOBAL)

# The Engine's pinned headless third parties as recipes (cmake/RunicThirdParty.cmake, BUILD-2). Nothing is fetched
# here: a recipe runs once, when a configured module links its library. Every archive is pinned and SHA-256 checked. Sources live in external/<name>-<version>
# (git-ignored, next to the Godot submodules); generated files and compiled libraries stay in each consumer's build
# tree. Each recipe is a function, so upstream build options stay out of consumer directories.
#
# The library IDs are the ones catalog rows list (THIRD_PARTY) and the public-header exceptions name. A package that
# first links a library without a recipe adds its recipe here, pinned the same way.
#
# Presentation is Godot (cmake/RunicGodot.cmake): GLFW, bgfx, bx, bimg and Dear ImGui of the v3 architecture are no
# longer part of the Engine (RunicDocs › Godot integration; Decisions log › Rendering, UI).

# ---- Flecs 4.0.4: C++ bindings over the static C core (private to Runic::ECS) ----
function(_runic_recipe_flecs)
    set(FLECS_STATIC ON)
    set(FLECS_SHARED OFF)
    set(FLECS_TESTS OFF)
    set(FLECS_PIC ON)
    runic_fetch_archive(flecs FOLDER flecs-4.2.0
        URL https://codeload.github.com/SanderMertens/flecs/tar.gz/refs/tags/v4.2.0
        SHA256 4107b0a36aa25f80ddcd6f939451ccfe1bacafcccdd2695840a4a8c413d05b40)
endfunction()
runic_third_party_recipe(flecs TARGETS flecs::flecs_static FETCH _runic_recipe_flecs)

# ---- GLM 1.0.1: header-only mathematics (Core's approved public exception) ----
function(_runic_recipe_glm)
    set(GLM_BUILD_LIBRARY OFF)
    set(GLM_BUILD_TESTS OFF)
    set(GLM_BUILD_INSTALL OFF)
    runic_fetch_archive(glm FOLDER glm-1.0.1
        URL https://codeload.github.com/g-truc/glm/tar.gz/refs/tags/1.0.1
        SHA256 9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c)
endfunction()
runic_third_party_recipe(glm TARGETS glm::glm FETCH _runic_recipe_glm)

# ---- Jolt 5.6.0: headless static library in double precision (private to Runic::Physics) ----
# The architecture's pin (RunicDocs › Simulation core › Jolt rules), from PHYS-1 (slice 7): v5.6.0 with
# JPH_DOUBLE_PRECISION, so bodies sit at 64-bit world positions, and with RTTI. Jolt's asserts, floating-point
# exceptions, object stream and profiler are off. Its debug renderer, which Physics' debug view draws through (PHYS-3,
# slice 8), follows RUNIC_PHYSICS_DEBUG_VIEW: on unless a build turns it off (a shipping server, say).
# Its SIMD paths follow RUNIC_CPU_BASELINE: AVX2 and FMA on for x86-64-v3. Jolt creates no threads: Physics runs it
# on a single-threaded job system until PHYS-4 puts it on the Core worker pool.
option(RUNIC_PHYSICS_DEBUG_VIEW "Build Jolt with its debug renderer, for Physics' debug view (PHYS-3)" ON)
function(_runic_recipe_jolt)
    set(DOUBLE_PRECISION ON)
    set(USE_ASSERTS OFF)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
    set(ENABLE_OBJECT_STREAM OFF)
    # RTTI on, as in the Runic code that derives from Jolt's classes: one setting on both sides, and UBSan's vptr
    # check finds Jolt's type information.
    set(CPP_RTTI_ENABLED ON)
    set(OVERRIDE_CXX_FLAGS OFF)
    set(INTERPROCEDURAL_OPTIMIZATION OFF)
    set(ENABLE_ALL_WARNINGS OFF)
    set(ENABLE_INSTALL OFF)
    set(JPH_BUILD_SHARED_LIBS OFF)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
    # Every configuration or none: Jolt's own switch for Debug and Release leaves RelWithDebInfo out.
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
    set(DEBUG_RENDERER_IN_DISTRIBUTION ${RUNIC_PHYSICS_DEBUG_VIEW})
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
    set(PROFILER_IN_DISTRIBUTION OFF)
    _runic_cpu_baseline_applies(baseline)
    foreach(feature SSE4_1 SSE4_2 AVX AVX2 LZCNT TZCNT F16C FMADD)
        set(USE_${feature} ${baseline})
    endforeach()
    set(USE_AVX512 OFF)
    runic_fetch_archive(jolt FOLDER JoltPhysics-5.6.0
        URL https://codeload.github.com/jrouwe/JoltPhysics/tar.gz/refs/tags/v5.6.0
        SHA256 6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e
        SOURCE_SUBDIR Build)
endfunction()
runic_third_party_recipe(jolt TARGETS Jolt FETCH _runic_recipe_jolt)
