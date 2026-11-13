# checks.catalog_sync for the three repositories (BUILD-1): a repository's CMake catalog equals its module table in
# RunicDocs › Architecture › Repositories, build targets and processes. RunicDocs is the architecture source of
# truth and is not copied into the repositories, so the test reads the RunicDocs checkout next to them
# (RUNIC_DOCS_DIR, default <repository>/../RunicDocs). Without a checkout the test is skipped, not failed; CI checks
# RunicDocs out so the check always runs there.
#
#   include(<RunicEngine>/tests/build/CatalogSyncTest.cmake)
#   runic_add_catalog_sync_test(CATALOG <catalog file> TABLE <engine|platform|game> ROOT <repository folder>)
include_guard(GLOBAL)

function(runic_add_catalog_sync_test)
    cmake_parse_arguments(C "" "CATALOG;TABLE;ROOT" "" ${ARGN})
    foreach(required CATALOG TABLE ROOT)
        if(NOT C_${required})
            message(FATAL_ERROR "runic_add_catalog_sync_test: ${required} is required")
        endif()
    endforeach()
    set(RUNIC_DOCS_DIR "${C_ROOT}/../RunicDocs" CACHE PATH
        "The RunicDocs checkout (the architecture source of truth) that checks.catalog_sync compares the catalog with")
    get_filename_component(docs "${RUNIC_DOCS_DIR}" ABSOLUTE BASE_DIR "${C_ROOT}")
    set(note "${docs}/content/Architecture/Repositories, build targets and processes.md")
    set(drivers "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    get_filename_component(engine "${drivers}/../.." ABSOLUTE)
    if(EXISTS "${note}")
        add_test(NAME checks.catalog_sync
            COMMAND "${CMAKE_COMMAND}"
                "-DSCRIPT=${engine}/cmake/checks/CatalogSync.cmake" "-DEXPECT=pass"
                "-DARGS=-DCATALOG=${C_CATALOG}|-DDOCS=${note}|-DTABLE=${C_TABLE}"
                -P "${drivers}/RunScript.cmake")
    else()
        add_test(NAME checks.catalog_sync
            COMMAND "${CMAKE_COMMAND}" -E echo
                "RUNIC: catalog sync skipped: no RunicDocs checkout at ${docs} (set RUNIC_DOCS_DIR)")
        set_tests_properties(checks.catalog_sync PROPERTIES SKIP_REGULAR_EXPRESSION "catalog sync skipped")
    endif()
    set_tests_properties(checks.catalog_sync PROPERTIES LABELS checks)
endfunction()
