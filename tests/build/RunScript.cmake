# BUILD-1 acceptance-test driver for the check scripts in cmake/checks/.
#
#   cmake -DSCRIPT=<path> -DEXPECT=<pass|fail> [-DEXPECT_REGEX=<regex>] [-DARGS=<-Dk=v>|<-Dk=v>...]
#         -P RunScript.cmake
#
# Runs `cmake <ARGS> -P <SCRIPT>` and checks that it passes, or fails with output matching EXPECT_REGEX.

cmake_minimum_required(VERSION 3.24)

foreach(required SCRIPT EXPECT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "RunScript: -D${required}=... is required")
    endif()
endforeach()
if(NOT EXISTS "${SCRIPT}")
    message(FATAL_ERROR "RunScript: check script '${SCRIPT}' does not exist (BUILD-1 delivers it)")
endif()

string(REPLACE "|" ";" script_args "${ARGS}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${script_args} -P "${SCRIPT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
set(log "${out}\n${err}")

if(EXPECT STREQUAL "pass")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "RunScript: '${SCRIPT}' failed, success was expected\n${log}")
    endif()
elseif(EXPECT STREQUAL "fail")
    if(result EQUAL 0)
        message(FATAL_ERROR "RunScript: '${SCRIPT}' passed, a failure was expected\n${log}")
    endif()
    if(NOT log MATCHES "${EXPECT_REGEX}")
        message(FATAL_ERROR "RunScript: '${SCRIPT}' failed, but not with: ${EXPECT_REGEX}\n${log}")
    endif()
else()
    message(FATAL_ERROR "RunScript: EXPECT must be pass or fail, not '${EXPECT}'")
endif()
message(STATUS "RunScript: ${EXPECT} as expected")
