# BUILD-1 acceptance-test driver: configures (and optionally builds and tests) one fixture project
# against this repository's CMake helpers, then checks the outcome.
#
#   cmake -DFIXTURE=<dir> -DWORK=<dir> -DENGINE=<engine source dir> -DEXPECT=<outcome>
#         [-DEXPECT_REGEX=<regex>] [-DTEST_REGEX=<regex>] [-DABSENT=<path>|<path>...]
#         [-DARGS=<arg>|<arg>...] [-DGENERATOR=<generator>] -P RunFixture.cmake
#
# EXPECT:
#   pass            configure and build succeed (and CTest succeeds when TEST_REGEX is given)
#   configure-fail  configure fails and its output matches EXPECT_REGEX
#   build-fail      configure succeeds, the build fails and its output matches EXPECT_REGEX
# EXPECT_REGEX on a passing fixture must match the configure output.
# TEST_REGEX: run CTest after the build; its output must match (for example a discovered test name).
# ABSENT: paths relative to WORK that must not exist after configure (for example _deps/glfw-build).
# ARGS and ABSENT use "|" as separator so they survive add_test().
# Every fixture gets -DRUNIC_BUILD_TESTS_NESTED=ON: a repository configured as a fixture never registers
# its own tests/build/ again, so the tests cannot recurse.

cmake_minimum_required(VERSION 3.24)

foreach(required FIXTURE WORK ENGINE EXPECT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "RunFixture: -D${required}=... is required")
    endif()
endforeach()
if(NOT EXPECT MATCHES "^(pass|configure-fail|build-fail)$")
    message(FATAL_ERROR "RunFixture: EXPECT must be pass, configure-fail or build-fail, not '${EXPECT}'")
endif()
if(NOT DEFINED GENERATOR OR GENERATOR STREQUAL "")
    set(GENERATOR "Ninja")
endif()

string(REPLACE "|" ";" fixture_args "${ARGS}")
string(REPLACE "|" ";" absent_paths "${ABSENT}")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${WORK}" -G "${GENERATOR}"
            "-DRUNIC_ENGINE_DIR=${ENGINE}" -DCMAKE_BUILD_TYPE=Debug -DRUNIC_BUILD_TESTS_NESTED=ON
            ${fixture_args}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err)
set(configure_log "${configure_out}\n${configure_err}")

function(fixture_fail what log)
    message(FATAL_ERROR "RunFixture [${FIXTURE}]: ${what}\n----- output -----\n${log}")
endfunction()

if(EXPECT STREQUAL "configure-fail")
    if(configure_result EQUAL 0)
        fixture_fail("configure succeeded, a failure was expected" "${configure_log}")
    endif()
    if(NOT configure_log MATCHES "${EXPECT_REGEX}")
        fixture_fail("configure failed, but not with the expected message: ${EXPECT_REGEX}" "${configure_log}")
    endif()
    message(STATUS "RunFixture: configure failed as expected")
    return()
endif()

if(NOT configure_result EQUAL 0)
    fixture_fail("configure failed" "${configure_log}")
endif()
if(EXPECT STREQUAL "pass" AND DEFINED EXPECT_REGEX AND NOT EXPECT_REGEX STREQUAL "")
    if(NOT configure_log MATCHES "${EXPECT_REGEX}")
        fixture_fail("configure output does not match: ${EXPECT_REGEX}" "${configure_log}")
    endif()
endif()
foreach(path IN LISTS absent_paths)
    if(EXISTS "${WORK}/${path}")
        fixture_fail("'${path}' exists after configure but must not" "${configure_log}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${WORK}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE build_err)
set(build_log "${build_out}\n${build_err}")

if(EXPECT STREQUAL "build-fail")
    if(build_result EQUAL 0)
        fixture_fail("build succeeded, a failure was expected" "${build_log}")
    endif()
    if(NOT build_log MATCHES "${EXPECT_REGEX}")
        fixture_fail("build failed, but not with the expected message: ${EXPECT_REGEX}" "${build_log}")
    endif()
    message(STATUS "RunFixture: build failed as expected")
    return()
endif()

if(NOT build_result EQUAL 0)
    fixture_fail("build failed" "${build_log}")
endif()

if(DEFINED TEST_REGEX AND NOT TEST_REGEX STREQUAL "")
    execute_process(
        COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${WORK}" --output-on-failure
        RESULT_VARIABLE test_result
        OUTPUT_VARIABLE test_out
        ERROR_VARIABLE test_err)
    set(test_log "${test_out}\n${test_err}")
    if(NOT test_result EQUAL 0)
        fixture_fail("CTest failed" "${test_log}")
    endif()
    if(NOT test_log MATCHES "${TEST_REGEX}")
        fixture_fail("CTest output does not match: ${TEST_REGEX}" "${test_log}")
    endif()
endif()

message(STATUS "RunFixture: passed")
