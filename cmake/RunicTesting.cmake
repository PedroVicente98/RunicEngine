# The C++ test framework of all three repositories: GoogleTest with GoogleMock (BUILD-1, decided in slice 0).
#
#   runic_enable_tests()   once per top-level project, when its *_BUILD_TESTS option is ON
#
# Fetches the pinned GoogleTest into the Engine's external/googletest-1.17.0 (reused when present, like the other
# third parties), calls enable_testing() and include(GoogleTest). runic_add_module then builds each module's
# Tests/ into one executable registered with gtest_discover_tests. GoogleTest is never fetched when tests
# are off, so consumers that only build never download it.
include_guard(GLOBAL)
include(FetchContent)

# A macro, because enable_testing() records itself in the calling directory's scope.
macro(runic_enable_tests)
    _runic_fetch_googletest()
    enable_testing()
endmacro()

function(_runic_fetch_googletest)
    get_property(enabled GLOBAL PROPERTY RUNIC_TESTS_ENABLED)
    if(enabled)
        return()
    endif()

    # The Engine's external/, wherever this function is called from (Engine, Platform or Game scope).
    get_filename_component(third_party_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../external" ABSOLUTE)
    if(NOT FETCHCONTENT_SOURCE_DIR_GOOGLETEST AND EXISTS "${third_party_dir}/googletest-1.17.0/CMakeLists.txt")
        set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${third_party_dir}/googletest-1.17.0")
    endif()

    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(INSTALL_GTEST OFF)
    set(BUILD_GMOCK ON)
    set(gtest_force_shared_crt ON)   # MSVC: the same runtime as the Runic targets
    FetchContent_Declare(googletest
        SOURCE_DIR "${third_party_dir}/googletest-1.17.0"
        URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.17.0
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        TLS_VERIFY TRUE
    )
    FetchContent_MakeAvailable(googletest)
    set_property(GLOBAL PROPERTY RUNIC_TESTS_ENABLED TRUE)
endfunction()

# GoogleTest's discovery helpers, available to every caller once this file is included.
include(GoogleTest)
