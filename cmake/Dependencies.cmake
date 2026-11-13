include_guard(GLOBAL)

# The Engine's pinned headless third parties as recipes (cmake/RunicThirdParty.cmake, BUILD-2). Nothing is fetched
# here: a recipe runs once, when a configured module links its library or the transitional Runic::Runtime bundle
# needs it (OQ-8). Every archive is pinned and SHA-256 checked. Sources live in external/<name>-<version>
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
    runic_fetch_archive(flecs FOLDER flecs-4.0.4
        URL https://codeload.github.com/SanderMertens/flecs/tar.gz/refs/tags/v4.0.4
        SHA256 a3b6238a913f65d90db18759ab5442393901da914e4a9bfe30aa8823687dce86)
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

# ---- Jolt 5.2.0: headless static library ----
# The architecture pins Jolt v5.6.0 in double precision (PHYS-1, slice 7; the v5.6.0 archive's SHA-256 is
# 6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e). Until PHYS-1 the bootstrap pin stays v5.2.0.
# Its SIMD paths follow RUNIC_CPU_BASELINE: AVX2 and FMA on for x86-64-v3.
function(_runic_recipe_jolt)
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
    runic_fetch_archive(jolt FOLDER JoltPhysics-5.2.0
        URL https://codeload.github.com/jrouwe/JoltPhysics/tar.gz/refs/tags/v5.2.0
        SHA256 f478afe3050c885e21403748e10ab18e3e8df8b0982c540e75f1e078ef8b2c88
        SOURCE_SUBDIR Build)
endfunction()
runic_third_party_recipe(jolt TARGETS Jolt FETCH _runic_recipe_jolt)
