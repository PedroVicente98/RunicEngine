# runic_pinned_source_dir: a build tree that remembers an earlier pin's folder moves to the new pin; a developer's
# override outside the external folder is kept. The fixtures folder stands in for the Engine's external/: this
# fixture's own folder is a pin that exists, cpu_baseline an earlier pin, missing-2.0 a pin not downloaded yet.
# RunFixture passes the remembered values as cache entries, as a build tree configured before would hold them.
cmake_minimum_required(VERSION 3.24)
project(FixturePinnedSourceDir LANGUAGES NONE)
include("${CMAKE_CURRENT_LIST_DIR}/../FixturePrologue.cmake")

get_filename_component(external "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(fixture_expect name expected)
    string(TOUPPER "${name}" key)
    if(NOT "${FETCHCONTENT_SOURCE_DIR_${key}}" STREQUAL "${expected}")
        message(FATAL_ERROR "fixture: ${name} uses '${FETCHCONTENT_SOURCE_DIR_${key}}', expected '${expected}'")
    endif()
    if(NOT "$CACHE{FETCHCONTENT_SOURCE_DIR_${key}}" STREQUAL "${expected}")
        message(FATAL_ERROR "fixture: ${name} cached '$CACHE{FETCHCONTENT_SOURCE_DIR_${key}}', expected '${expected}'")
    endif()
endfunction()

# Remembered an earlier pin; the new pin is downloaded: the new folder.
runic_pinned_source_dir(moved "${external}" pinned_source_dir)
fixture_expect(moved "${external}/pinned_source_dir")

# Remembered an earlier pin; the new pin is not downloaded yet: empty, so FetchContent downloads it.
runic_pinned_source_dir(notyet "${external}" missing-2.0)
fixture_expect(notyet "")

# Nothing remembered (a fresh build tree): the pinned folder.
runic_pinned_source_dir(fresh "${external}" pinned_source_dir)
fixture_expect(fresh "${external}/pinned_source_dir")

# The developer's own folder, outside external/: kept.
runic_pinned_source_dir(override "${external}" pinned_source_dir)
fixture_expect(override "${FIXTURE_OVERRIDE_DIR}")

message(STATUS "fixture: pinned source folders as expected")
