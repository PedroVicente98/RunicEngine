# BUILD-1 acceptance tests (contract: docs/build/BUILD-1.md).
#
# Added by the Engine when RUNIC_ENGINE_BUILD_TESTS is ON, or run on their own without configuring any
# Engine third party:
#   cmake -S tests/build -B build/build-tests -G Ninja && ctest --test-dir build/build-tests
#
# Every test configures a fixture under fixtures/ (or runs a check script) against this repository's
# cmake/ helpers. Until BUILD-1 is implemented, every test fails.

cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(RunicEngineBuildTests LANGUAGES NONE)
    enable_testing()
endif()

get_filename_component(RUNIC_BUILD_TESTS_ENGINE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(RUNIC_BUILD_TESTS_FIXTURES "${CMAKE_CURRENT_LIST_DIR}/fixtures")
set(RUNIC_BUILD_TESTS_WORK "${CMAKE_CURRENT_BINARY_DIR}/work")
set(RUNIC_BUILD_TESTS_CHECKS "${RUNIC_BUILD_TESTS_ENGINE}/cmake/checks")

# runic_fixture_test(<name> FIXTURE <dir> EXPECT <outcome> [REGEX <re>] [TEST_REGEX <re>]
#                    [ARGS <arg>...] [ABSENT <path>...] [LABEL <label>])
function(runic_fixture_test name)
    cmake_parse_arguments(T "" "FIXTURE;EXPECT;REGEX;TEST_REGEX;LABEL" "ARGS;ABSENT" ${ARGN})
    if(NOT T_LABEL)
        set(T_LABEL build)
    endif()
    string(REPLACE ";" "|" args "${T_ARGS}")
    string(REPLACE ";" "|" absent "${T_ABSENT}")
    add_test(NAME ${T_LABEL}.${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DFIXTURE=${T_FIXTURE}"
            "-DWORK=${RUNIC_BUILD_TESTS_WORK}/${name}"
            "-DENGINE=${RUNIC_BUILD_TESTS_ENGINE}"
            "-DEXPECT=${T_EXPECT}"
            "-DEXPECT_REGEX=${T_REGEX}"
            "-DTEST_REGEX=${T_TEST_REGEX}"
            "-DARGS=${args}"
            "-DABSENT=${absent}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            -P "${CMAKE_CURRENT_LIST_DIR}/RunFixture.cmake")
    set_tests_properties(${T_LABEL}.${name} PROPERTIES LABELS "${T_LABEL}")
endfunction()

# runic_script_test(<name> SCRIPT <file> EXPECT <pass|fail> [REGEX <re>] [ARGS <-Dk=v>...])
function(runic_script_test name)
    cmake_parse_arguments(T "" "SCRIPT;EXPECT;REGEX" "ARGS" ${ARGN})
    string(REPLACE ";" "|" args "${T_ARGS}")
    add_test(NAME checks.${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DSCRIPT=${T_SCRIPT}"
            "-DEXPECT=${T_EXPECT}"
            "-DEXPECT_REGEX=${T_REGEX}"
            "-DARGS=${args}"
            -P "${CMAKE_CURRENT_LIST_DIR}/RunScript.cmake")
    set_tests_properties(checks.${name} PROPERTIES LABELS checks)
endfunction()

set(F "${RUNIC_BUILD_TESTS_FIXTURES}")

# ---- runic_add_module: layout, targets, tests ----
runic_fixture_test(module_layout FIXTURE "${F}/module_layout" EXPECT pass
    REGEX "RUNIC: link checks passed"
    TEST_REGEX "AlphaValue\\.Answer")
runic_fixture_test(header_only_module FIXTURE "${F}/header_only_module" EXPECT pass)
runic_fixture_test(source_not_exported FIXTURE "${F}/source_not_exported" EXPECT build-fail
    REGEX "AlphaInternal\\.hpp")

# ---- catalog and dependency rules ----
runic_fixture_test(not_in_catalog FIXTURE "${F}/not_in_catalog" EXPECT configure-fail
    REGEX "RUNIC:[^\n]*Alpha[^\n]*catalog")
runic_fixture_test(forbidden_link FIXTURE "${F}/forbidden_link" EXPECT configure-fail
    REGEX "RUNIC:[^\n]*Beta[^\n]*Fixture::Alpha")
runic_fixture_test(public_third_party FIXTURE "${F}/public_third_party" EXPECT configure-fail
    REGEX "RUNIC:[^\n]*fixture_thirdparty[^\n]*PUBLIC")
runic_fixture_test(wrong_direction FIXTURE "${F}/wrong_direction" EXPECT configure-fail
    REGEX "RUNIC:[^\n]*Runic::Alpha[^\n]*RunicPlatform::Transport")
runic_fixture_test(interface_target FIXTURE "${F}/interface_target" EXPECT configure-fail
    REGEX "RUNIC: interface target[^\n]*Port[^\n]*Runic::Core")

# ---- headless check ----
runic_fixture_test(headless_module FIXTURE "${F}/headless_module" EXPECT configure-fail
    REGEX "RUNIC: headless target Fixture::Alpha reaches presentation target fake_glfw through")
runic_fixture_test(headless_executable FIXTURE "${F}/headless_executable" EXPECT configure-fail
    REGEX "RUNIC: headless target FixtureServer reaches presentation target fake_glfw through")
runic_fixture_test(headless_executable_clean FIXTURE "${F}/headless_executable" EXPECT pass
    ARGS "-DFIXTURE_CLEAN=ON"
    REGEX "RUNIC: link checks passed")

# ---- CPU baseline (x86-64 GCC and Clang only) ----
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND NOT CMAKE_HOST_WIN32)
    runic_fixture_test(cpu_baseline_default FIXTURE "${F}/cpu_baseline" EXPECT pass
        ARGS "-DFIXTURE_EXPECT_BASELINE=ON")
    runic_fixture_test(cpu_baseline_none FIXTURE "${F}/cpu_baseline" EXPECT pass
        ARGS "-DRUNIC_CPU_BASELINE=none")
endif()

# ---- the Engine itself: headless configure, no presentation library configured ----
runic_fixture_test(engine_headless_configure FIXTURE "${RUNIC_BUILD_TESTS_ENGINE}" EXPECT pass
    ARGS "-DRUNIC_ENGINE_ENABLE_PRESENTATION=OFF" "-DRUNIC_BUILD_SANDBOX=OFF"
         "-DRUNIC_ENGINE_BUILD_TOOLS=OFF" "-DRUNIC_ENGINE_BUILD_TESTS=OFF"
    REGEX "RUNIC: link checks passed"
    ABSENT "_deps/glfw-build" "_deps/runic_bgfx-build" "_deps/bgfx_cmake-build" "_deps/imgui-build")

# ---- scans ----
set(S "${F}/scan_tree")
runic_script_test(public_header_forbidden
    SCRIPT "${RUNIC_BUILD_TESTS_CHECKS}/PublicHeaderScan.cmake" EXPECT fail
    REGEX "RUNIC:[^\n]*AlphaWorld\\.hpp[^\n]*flecs"
    ARGS "-DROOT=${S}/public_forbidden" "-DCATALOG=${S}/public_forbidden/catalog.cmake")
runic_script_test(public_header_exception
    SCRIPT "${RUNIC_BUILD_TESTS_CHECKS}/PublicHeaderScan.cmake" EXPECT pass
    ARGS "-DROOT=${S}/public_exception" "-DCATALOG=${S}/public_exception/catalog.cmake")
runic_script_test(source_include_scan
    SCRIPT "${RUNIC_BUILD_TESTS_CHECKS}/SourceIncludeScan.cmake" EXPECT fail
    REGEX "RUNIC:[^\n]*Beta\\.cpp[^\n]*Alpha/Source"
    ARGS "-DROOT=${S}/source_include")
runic_script_test(docs_check_missing
    SCRIPT "${RUNIC_BUILD_TESTS_CHECKS}/DocsCheck.cmake" EXPECT fail
    REGEX "RUNIC:[^\n]*Alpha[^\n]*ARCHITECTURE\\.md"
    ARGS "-DROOT=${S}/docs_check" "-DCHANGED=${S}/docs_check/changes_code_only.txt")
runic_script_test(docs_check_complete
    SCRIPT "${RUNIC_BUILD_TESTS_CHECKS}/DocsCheck.cmake" EXPECT pass
    ARGS "-DROOT=${S}/docs_check" "-DCHANGED=${S}/docs_check/changes_with_docs.txt")

# ---- the catalog equals docs/system/03 ----
runic_script_test(catalog_sync
    SCRIPT "${RUNIC_BUILD_TESTS_CHECKS}/CatalogSync.cmake" EXPECT pass
    ARGS "-DCATALOG=${RUNIC_BUILD_TESTS_ENGINE}/cmake/RunicModuleCatalog.cmake"
         "-DDOCS=${RUNIC_BUILD_TESTS_ENGINE}/docs/system/03-dependency-rules.md"
         "-DTABLE=engine")
