# Third parties fetched by need (BUILD-2; decided by the owner on 2026-11-12). Contract: cmake/README.md.
#
#   runic_third_party_recipe(<id> TARGETS <target>... FETCH <function> [PRESENTATION] [INFRASTRUCTURE])
#   runic_require_third_party(<id>...)
#   runic_fetch_archive(<name> FOLDER <folder> URL <url> SHA256 <hash> [SOURCE_SUBDIR <dir>])
#
# A recipe names a library ID, the targets the library provides and the function that brings it into the build.
# Declaring a recipe fetches nothing. runic_add_module requires the recipe behind every dependency that names a recipe
# ID (glm) or one of its targets (glm::glm), when the module's catalog row lists it; RunicEngine's transitional
# Runic::Runtime bundle requires Flecs, GLM and Jolt. A recipe runs once per build tree, whichever repository asks
# first; its targets are then registered with runic_third_party (the catalog checks) and marked SYSTEM (warnings in
# third-party headers never fail a Runic build). A third party that no configured target needs is never downloaded.
include_guard(GLOBAL)
include(FetchContent)

function(runic_third_party_recipe id)
    cmake_parse_arguments(R "PRESENTATION;INFRASTRUCTURE" "FETCH" "TARGETS" ${ARGN})
    if(NOT R_TARGETS OR NOT R_FETCH)
        runic_fatal("RUNIC: runic_third_party_recipe(${id}) needs TARGETS and FETCH")
    endif()
    if(NOT COMMAND "${R_FETCH}")
        runic_fatal("RUNIC: runic_third_party_recipe(${id}): FETCH '${R_FETCH}' is not a function defined before it")
    endif()
    get_property(declared GLOBAL PROPERTY "RUNIC_RECIPE_${id}_FETCH" SET)
    if(declared)
        runic_fatal("RUNIC: the third-party recipe '${id}' is declared twice")
    endif()
    set_property(GLOBAL PROPERTY "RUNIC_RECIPE_${id}_FETCH" "${R_FETCH}")
    set_property(GLOBAL PROPERTY "RUNIC_RECIPE_${id}_TARGETS" "${R_TARGETS}")
    set_property(GLOBAL PROPERTY "RUNIC_RECIPE_${id}_PRESENTATION" "${R_PRESENTATION}")
    set_property(GLOBAL PROPERTY "RUNIC_RECIPE_${id}_INFRASTRUCTURE" "${R_INFRASTRUCTURE}")
    foreach(target IN LISTS R_TARGETS)
        set_property(GLOBAL PROPERTY "RUNIC_RECIPE_OF_${target}" "${id}")
    endforeach()
endfunction()

# The recipe ID behind <name>, which is a recipe ID or one of a recipe's targets; "" when no recipe covers it.
function(_runic_recipe_for name out_var)
    get_property(is_id GLOBAL PROPERTY "RUNIC_RECIPE_${name}_FETCH" SET)
    if(is_id)
        set(${out_var} "${name}" PARENT_SCOPE)
    else()
        get_property(id GLOBAL PROPERTY "RUNIC_RECIPE_OF_${name}")
        set(${out_var} "${id}" PARENT_SCOPE)
    endif()
endfunction()

function(runic_require_third_party)
    foreach(id IN LISTS ARGN)
        get_property(fetch GLOBAL PROPERTY "RUNIC_RECIPE_${id}_FETCH")
        if(NOT fetch)
            runic_fatal(
                "RUNIC: no recipe provides the third party '${id}': the package that first links it adds its pinned recipe (cmake/Dependencies.cmake in the Engine)")
        endif()
        get_property(done GLOBAL PROPERTY "RUNIC_RECIPE_${id}_DONE")
        if(done)
            continue()
        endif()
        set_property(GLOBAL PROPERTY "RUNIC_RECIPE_${id}_DONE" TRUE)
        cmake_language(CALL "${fetch}")

        get_property(targets GLOBAL PROPERTY "RUNIC_RECIPE_${id}_TARGETS")
        foreach(target IN LISTS targets)
            if(NOT TARGET "${target}")
                runic_fatal("RUNIC: the recipe of the third party '${id}' (${fetch}) did not create the target ${target}")
            endif()
            _runic_real_target("${target}" real)
            set_target_properties(${real} PROPERTIES SYSTEM TRUE)
        endforeach()
        set(flags "")
        get_property(presentation GLOBAL PROPERTY "RUNIC_RECIPE_${id}_PRESENTATION")
        get_property(infrastructure GLOBAL PROPERTY "RUNIC_RECIPE_${id}_INFRASTRUCTURE")
        if(presentation)
            list(APPEND flags PRESENTATION)
        endif()
        if(infrastructure)
            list(APPEND flags INFRASTRUCTURE)
        endif()
        runic_third_party(${id} TARGETS ${targets} ${flags})
        list(JOIN targets ", " text)
        message(STATUS "RUNIC: third party ${id} configured (${text})")
    endforeach()
endfunction()

# A pinned, SHA-256 checked archive, downloaded once into the Engine's external/<FOLDER> (git-ignored; the folder
# name carries the version, so a changed pin never reuses an old tree) and reused by every later build tree of the
# three repositories. A recipe sets the library's options before calling it; they stay in the recipe's scope.
function(runic_fetch_archive name)
    cmake_parse_arguments(A "" "FOLDER;URL;SHA256;SOURCE_SUBDIR" "" ${ARGN})
    foreach(required FOLDER URL SHA256)
        if(NOT A_${required})
            runic_fatal("RUNIC: runic_fetch_archive(${name}) needs ${required}")
        endif()
    endforeach()
    get_filename_component(external "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../external" ABSOLUTE)
    string(TOUPPER "${name}" key)
    if(NOT FETCHCONTENT_SOURCE_DIR_${key} AND IS_DIRECTORY "${external}/${A_FOLDER}")
        set(FETCHCONTENT_SOURCE_DIR_${key} "${external}/${A_FOLDER}")
    endif()
    set(subdir "")
    if(A_SOURCE_SUBDIR)
        set(subdir SOURCE_SUBDIR "${A_SOURCE_SUBDIR}")
    endif()
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)   # the recipe's plain variables set the library's options
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(${name}
        SOURCE_DIR "${external}/${A_FOLDER}"
        URL "${A_URL}"
        URL_HASH "SHA256=${A_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
        ${subdir}
    )
    FetchContent_MakeAvailable(${name})
endfunction()
