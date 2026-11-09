# Godot as a library (BUILD-1; RunicDocs › Godot integration (Engine) › Building Godot).
#
# The Engine pins Godot and godot-cpp as submodules at a release tag (external/godot, external/godot-cpp) and
# builds libgodot from source in two targets with the module lists in scripts/godot/:
#
#   Runic::LibGodotEditor     the editor target: RunicEditor, the development GameClient, the Cooker
#   Runic::LibGodotTemplate   the template target: the shipping GameClient
#
# Linking one of them makes the build produce that library and stage it next to the executables
# (${CMAKE_BINARY_DIR}/bin), where the LibGodot host loads it at runtime: the same client binary can run
# the editor library in development and the template library when shipping. godot-cpp (the GDExtension
# C++ API) is compiled into the Runic executables. libgodot, godot-cpp, GodotBridge and GodotTools are
# catalogued as presentation: a headless target that reaches any of them fails configure.
#
#   runic_godot_setup()       once, from the Engine's CMakeLists.txt, when presentation is enabled
include_guard(GLOBAL)

function(runic_godot_setup)
    get_filename_component(engine "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(RUNIC_GODOT_SOURCE_DIR "${engine}/external/godot" CACHE PATH "Godot source (submodule pinned at a release tag)")
    set(RUNIC_GODOT_CPP_SOURCE_DIR "${engine}/external/godot-cpp" CACHE PATH "godot-cpp source (submodule)")
    set(RUNIC_GODOT_API_VERSION "4.7" CACHE STRING "Godot API version godot-cpp targets; matches the pinned Godot")
    set(RUNIC_GODOT_API_FILE "" CACHE FILEPATH "Optional extension_api.json dumped from the Runic build (overrides the version)")
    set(RUNIC_GODOT_CPP_TARGET "editor" CACHE STRING "godot-cpp flavor: editor (development and tools) or template_release (shipping)")
    set_property(CACHE RUNIC_GODOT_CPP_TARGET PROPERTY STRINGS editor template_debug template_release)
    option(RUNIC_GODOT_BUILD_LIBRARIES "Build libgodot from source with SCons when a target links it" ON)
    set(RUNIC_GODOT_PREBUILT_DIR "" CACHE PATH "Copy libgodot from this folder instead of building it (CI artifacts)")
    set(RUNIC_GODOT_TEMPLATE_TARGET "template_release" CACHE STRING "Godot template target staged for Runic::LibGodotTemplate")
    set_property(CACHE RUNIC_GODOT_TEMPLATE_TARGET PROPERTY STRINGS template_release template_debug)
    set(RUNIC_GODOT_COMPILER "auto" CACHE STRING "Compiler for the Godot build on Linux: auto, gcc or llvm (clang with lld)")
    set_property(CACHE RUNIC_GODOT_COMPILER PROPERTY STRINGS auto gcc llvm)
    cmake_host_system_information(RESULT cores QUERY NUMBER_OF_LOGICAL_CORES)
    math(EXPR default_jobs "(${cores} + 1) / 2")
    set(RUNIC_GODOT_JOBS "${default_jobs}" CACHE STRING "Parallel SCons jobs for the Godot build (Godot is memory hungry)")
    set(RUNIC_GODOT_SCONS_CACHE "" CACHE PATH "SCons cache folder; CI keeps it between runs so only what changed recompiles")

    # godot-cpp is always compiled; the Godot source is needed only to build libgodot (not with a prebuilt one).
    set(required_sources RUNIC_GODOT_CPP_SOURCE_DIR)
    if(RUNIC_GODOT_BUILD_LIBRARIES AND NOT RUNIC_GODOT_PREBUILT_DIR)
        list(APPEND required_sources RUNIC_GODOT_SOURCE_DIR)
    endif()
    foreach(dir IN LISTS required_sources)
        if(NOT EXISTS "${${dir}}/SConstruct")
            runic_fatal(
                "RUNIC: ${dir} '${${dir}}' is not populated. In RunicEngine run:\n"
                "  git submodule update --init --depth 1 external/godot external/godot-cpp\n"
                "or configure with RUNIC_ENGINE_ENABLE_PRESENTATION=OFF for a headless build.")
        endif()
    endforeach()

    # ---- godot-cpp: the GDExtension C++ API, built from the submodule ----------------------------------------
    # godot-cpp declares its own options as cache entries; set them here so Runic's choices win:
    # exceptions stay on and libstdc++ stays shared, so godot-cpp does not change how Runic code is compiled.
    set(GODOTCPP_TARGET "${RUNIC_GODOT_CPP_TARGET}" CACHE STRING "Set from RUNIC_GODOT_CPP_TARGET" FORCE)
    if(RUNIC_GODOT_API_FILE)
        set(GODOTCPP_CUSTOM_API_FILE "${RUNIC_GODOT_API_FILE}" CACHE FILEPATH "Set from RUNIC_GODOT_API_FILE" FORCE)
    else()
        set(GODOTCPP_API_VERSION "${RUNIC_GODOT_API_VERSION}" CACHE STRING "Set from RUNIC_GODOT_API_VERSION" FORCE)
    endif()
    set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "Godot headers are third-party headers" FORCE)
    set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "Runic code is standard C++ with exceptions" FORCE)
    set(GODOTCPP_USE_STATIC_CPP OFF CACHE BOOL "One shared libstdc++ in the process, with libgodot" FORCE)
    add_subdirectory("${RUNIC_GODOT_CPP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/godot-cpp" EXCLUDE_FROM_ALL)
    runic_third_party(godot-cpp TARGETS godot-cpp godot::cpp PRESENTATION)

    # The loader's dlopen: a system library, wrapped so the catalog can name it.
    add_library(runic_system_dl INTERFACE)
    target_link_libraries(runic_system_dl INTERFACE ${CMAKE_DL_LIBS})
    runic_third_party(dl TARGETS runic_system_dl)

    # ---- libgodot: built from source, staged next to the executables -------------------------------------------
    set(stage "${CMAKE_BINARY_DIR}/bin")
    set(tools "${engine}/scripts/godot")

    # Rebuild when the pinned commit, the profiles or the compiler choice change.
    set(stamp "${CMAKE_BINARY_DIR}/godot/godot-source.stamp")
    if(RUNIC_GODOT_BUILD_LIBRARIES)
        set(godot_commit "prebuilt")
        if(NOT RUNIC_GODOT_PREBUILT_DIR)
            execute_process(COMMAND git -C "${RUNIC_GODOT_SOURCE_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE godot_commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE git_result)
            if(NOT git_result EQUAL 0)
                file(SHA256 "${RUNIC_GODOT_SOURCE_DIR}/version.py" godot_commit)
            endif()
        endif()
        file(WRITE "${stamp}.new" "${godot_commit}\n${RUNIC_GODOT_COMPILER}\n${RUNIC_GODOT_PREBUILT_DIR}\n")
        file(COPY_FILE "${stamp}.new" "${stamp}" ONLY_IF_DIFFERENT)
    endif()

    if(CMAKE_HOST_WIN32)
        set(platform windows)
    elseif(CMAKE_HOST_APPLE)
        set(platform macos)
    else()
        set(platform linuxbsd)
    endif()
    set(arch x86_64)
    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
        set(arch arm64)
    endif()

    if(RUNIC_GODOT_BUILD_LIBRARIES)
        find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    endif()

    foreach(kind Editor Template)
        if(kind STREQUAL "Editor")
            set(target editor)
            set(profile "${tools}/runic_editor.py")
        else()
            set(target "${RUNIC_GODOT_TEMPLATE_TARGET}")
            set(profile "${tools}/runic_template.py" "${tools}/runic_client.gdbuild")
        endif()
        if(platform STREQUAL "windows")
            set(library "${stage}/godot.windows.${target}.${arch}.dll")
        elseif(platform STREQUAL "macos")
            set(library "${stage}/libgodot.macos.${target}.${arch}.dylib")
        else()
            set(library "${stage}/libgodot.${platform}.${target}.${arch}.so")
        endif()
        string(TOLOWER "${kind}" lower)

        add_library(runic_libgodot_${lower} INTERFACE)
        add_library(Runic::LibGodot${kind} ALIAS runic_libgodot_${lower})
        set_target_properties(runic_libgodot_${lower} PROPERTIES RUNIC_LIBGODOT_FILE "${library}")

        if(RUNIC_GODOT_BUILD_LIBRARIES)
            set(command "${Python3_EXECUTABLE}" "${tools}/build_libgodot.py"
                --godot "${RUNIC_GODOT_SOURCE_DIR}" --target ${target} --out "${stage}"
                --jobs ${RUNIC_GODOT_JOBS} --compiler ${RUNIC_GODOT_COMPILER})
            if(RUNIC_GODOT_SCONS_CACHE)
                list(APPEND command --cache "${RUNIC_GODOT_SCONS_CACHE}")
            endif()
            if(RUNIC_GODOT_PREBUILT_DIR)
                list(APPEND command --prebuilt "${RUNIC_GODOT_PREBUILT_DIR}")
            endif()
            add_custom_command(OUTPUT "${library}"
                COMMAND ${command}
                DEPENDS "${tools}/build_libgodot.py" ${profile} "${stamp}"
                COMMENT "libgodot (${target}): building Godot from source with SCons (first build: several minutes)"
                USES_TERMINAL VERBATIM)
            add_custom_target(runic_libgodot_${lower}_build DEPENDS "${library}")
            add_dependencies(runic_libgodot_${lower} runic_libgodot_${lower}_build)
        endif()
    endforeach()
    runic_third_party(libgodot TARGETS Runic::LibGodotEditor Runic::LibGodotTemplate PRESENTATION)

    set_property(GLOBAL PROPERTY RUNIC_GODOT_STAGE_DIR "${stage}")
    message(STATUS "RUNIC: Godot ${RUNIC_GODOT_API_VERSION} from ${RUNIC_GODOT_SOURCE_DIR} (godot-cpp ${RUNIC_GODOT_CPP_TARGET}); libgodot staged in ${stage}")
endfunction()

# Executables that host Godot: output in bin/ next to the staged libgodot, the right subsystem on Windows.
function(runic_godot_host_executable target)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(WIN32)
        target_link_libraries(${target} PRIVATE shell32)
    endif()
endfunction()
