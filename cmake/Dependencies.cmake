include_guard(GLOBAL)
include(FetchContent)

# Function scope keeps generic upstream build options out of consumer directories.
# Every archive is pinned and SHA-256 checked. Sources live in ThirdParty;
# generated files and compiled libraries stay in each consumer's build tree.
function(runic_engine_dependencies)
    get_filename_component(third_party_dir
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../ThirdParty" ABSOLUTE)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)

    # Reuse populated source folders across Debug/Release and sibling builds.
    # Explicit FetchContent source overrides still take precedence.
    set(names flecs glm jolt glfw runic_bx runic_bimg runic_bgfx bgfx_cmake imgui)
    set(folders flecs glm JoltPhysics glfw bx bimg bgfx bgfx.cmake imgui)
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
    # flecs.h exposes its C++ bindings automatically to C++ consumers. The
    # bindings link the same static core; do not disable FLECS_CPP.
    FetchContent_Declare(flecs
        SOURCE_DIR "${third_party_dir}/flecs"
        URL https://codeload.github.com/SanderMertens/flecs/tar.gz/refs/tags/v4.0.4
        URL_HASH SHA256=a3b6238a913f65d90db18759ab5442393901da914e4a9bfe30aa8823687dce86
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
    )

    FetchContent_MakeAvailable(flecs)

    # ---- 2. GLM: header-only mathematics ----
    set(GLM_BUILD_LIBRARY OFF)
    set(GLM_BUILD_TESTS OFF)
    set(GLM_BUILD_INSTALL OFF)
    FetchContent_Declare(glm
        SOURCE_DIR "${third_party_dir}/glm"
        URL https://codeload.github.com/g-truc/glm/tar.gz/refs/tags/1.0.1
        URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
    )

    FetchContent_MakeAvailable(glm)

    # ---- 3. Jolt: portable, headless static library ----
    set(OVERRIDE_CXX_FLAGS OFF)
    set(INTERPROCEDURAL_OPTIMIZATION OFF)
    set(ENABLE_ALL_WARNINGS OFF)
    set(ENABLE_INSTALL OFF)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
    set(DEBUG_RENDERER_IN_DISTRIBUTION OFF)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
    set(PROFILER_IN_DISTRIBUTION OFF)
    foreach(feature SSE4_1 SSE4_2 AVX AVX2 AVX512 LZCNT TZCNT F16C FMADD)
        set(USE_${feature} OFF)
    endforeach()
    FetchContent_Declare(jolt
        SOURCE_DIR "${third_party_dir}/JoltPhysics"
        URL https://codeload.github.com/jrouwe/JoltPhysics/tar.gz/refs/tags/v5.2.0
        URL_HASH SHA256=f478afe3050c885e21403748e10ab18e3e8df8b0982c540e75f1e078ef8b2c88
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(jolt)
    target_link_libraries(runic_runtime INTERFACE flecs::flecs_static glm::glm Jolt)

    # Everything below this point belongs only to clients/tools.
    if(NOT RUNIC_ENGINE_ENABLE_PRESENTATION)
        return()
    endif()

    # ---- 4. GLFW: platform dependency ----
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_INSTALL OFF)
    # Linux bootstrap uses X11. A Wayland build can opt in through the cache.
    if(NOT DEFINED GLFW_BUILD_WAYLAND)
        set(GLFW_BUILD_WAYLAND OFF)
    endif()
    FetchContent_Declare(glfw
        SOURCE_DIR "${third_party_dir}/glfw"
        URL https://codeload.github.com/glfw/glfw/tar.gz/refs/tags/3.4
        URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
    )

    FetchContent_MakeAvailable(glfw)

    # ---- 5. bgfx: matching bgfx/bx/bimg revisions and CMake integration ----
    # bgfx's CMake integration tracks a compatible bx/bimg/bgfx set. Fetch their
    # exact gitlink revisions as archives instead of cloning large Git histories.
    FetchContent_Declare(runic_bx
        SOURCE_DIR "${third_party_dir}/bx"
        URL https://codeload.github.com/bkaradzic/bx/tar.gz/f86bece7967be1b8a7fd39262cdc8ce99d123c3b
        URL_HASH SHA256=d75965502b2fdd10d2b36fd300762babb84dacf1ffb59d74afa50a84dcafc3a7
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
        SOURCE_SUBDIR .runic-source-only
    )
    FetchContent_Declare(runic_bimg
        SOURCE_DIR "${third_party_dir}/bimg"
        URL https://codeload.github.com/bkaradzic/bimg/tar.gz/ddbeeae05779f84f97694553eb41605a60f86f0a
        URL_HASH SHA256=290f736a93960e17f26bc80179c702598231db3b0c34ac0b0f0d8a87ea1ad039
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
        SOURCE_SUBDIR .runic-source-only
    )
    FetchContent_Declare(runic_bgfx
        SOURCE_DIR "${third_party_dir}/bgfx"
        URL https://codeload.github.com/bkaradzic/bgfx/tar.gz/dd38b306c85472d0c89bb025970da7c2d42838d4
        URL_HASH SHA256=4d2db626f4cc049689137dcdd0ff595c3bc6df6312fb0b94025c1fdc39b3cbb3
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
        SOURCE_SUBDIR .runic-source-only
    )
    FetchContent_MakeAvailable(runic_bx runic_bimg runic_bgfx)
    set(BX_DIR "${runic_bx_SOURCE_DIR}")
    set(BIMG_DIR "${runic_bimg_SOURCE_DIR}")
    set(BGFX_DIR "${runic_bgfx_SOURCE_DIR}")
    set(BGFX_BUILD_TOOLS OFF)
    set(BGFX_BUILD_EXAMPLES OFF)
    set(BGFX_BUILD_EXAMPLE_COMMON OFF)
    set(BGFX_BUILD_TESTS OFF)
    set(BGFX_INSTALL OFF)
    set(BGFX_CUSTOM_TARGETS OFF)
    set(BGFX_CONFIG_VIDEO OFF)
    if(NOT DEFINED BGFX_WITH_WAYLAND)
        set(BGFX_WITH_WAYLAND OFF)
    endif()
    FetchContent_Declare(bgfx_cmake
        SOURCE_DIR "${third_party_dir}/bgfx.cmake"
        URL https://codeload.github.com/bkaradzic/bgfx.cmake/tar.gz/0fb9ec06bdaa7c3ae31ce6a3521e211183a0cead
        URL_HASH SHA256=9c88dcea2ac345bce1278e162ef6cbc4c8253f0e202a72f1328440bcd66d0535
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
    )

    FetchContent_MakeAvailable(bgfx_cmake)
    # The integration creates offline codec targets even with tools disabled.
    # Build them only if a future consumer explicitly needs them.
    set_target_properties(bimg_encode bimg_decode PROPERTIES EXCLUDE_FROM_ALL TRUE)

    # ---- 6. Dear ImGui: core only; no backend implementation ----
    FetchContent_Declare(imgui
        SOURCE_DIR "${third_party_dir}/imgui"
        URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.8
        URL_HASH SHA256=db3a2e02bfd6c269adf0968950573053d002f40bdfb9ef2e4a90bce804b0f286
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
        SOURCE_SUBDIR .runic-source-only
    )
    FetchContent_MakeAvailable(imgui)

    # Dear ImGui has no upstream CMake target. Compile only its core sources;
    # platform/renderer backends will be chosen when presentation is implemented.
    add_library(runic_imgui STATIC)
    target_sources(runic_imgui PRIVATE
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    )
    target_include_directories(runic_imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")
    target_compile_features(runic_imgui PUBLIC cxx_std_20)

    # ---- 7. Client-only includes, definitions, and transitive linker inputs ----
    add_library(runic_presentation INTERFACE)
    add_library(Runic::Presentation ALIAS runic_presentation)
    target_compile_features(runic_presentation INTERFACE cxx_std_20)
    target_compile_definitions(runic_presentation INTERFACE GLFW_INCLUDE_NONE)
    target_link_libraries(runic_presentation INTERFACE glfw bgfx runic_imgui)
endfunction()
