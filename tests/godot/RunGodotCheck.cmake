# Runs a Godot-hosting executable and checks that it exits with 0, prints every expected line and never
# crashes. Used by the CTest tests labelled `godot` (they need libgodot built).
#
#   cmake -DCOMMAND=<exe>|<arg>... -DEXPECT=<regex>|<regex>... [-DFORBID=<regex>|...] -P RunGodotCheck.cmake
#
# CTest's PASS_REGULAR_EXPRESSION would ignore the exit code; this driver checks both.
cmake_minimum_required(VERSION 3.24)
foreach(required COMMAND EXPECT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "RunGodotCheck: -D${required}=... is required")
    endif()
endforeach()
string(REPLACE "|" ";" command "${COMMAND}")
string(REPLACE "|" ";" expected "${EXPECT}")
set(forbidden "Segmentation fault|SIGSEGV|Program crashed with signal|handle_crash")
if(DEFINED FORBID AND NOT FORBID STREQUAL "")
    string(APPEND forbidden "|${FORBID}")
endif()

execute_process(COMMAND ${command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    TIMEOUT 540)
set(log "${out}\n${err}")
message("${log}")

set(failures "")
if(NOT result STREQUAL "0")
    list(APPEND failures "exit code ${result}")
endif()
foreach(line IN LISTS expected)
    if(NOT log MATCHES "${line}")
        list(APPEND failures "missing output: ${line}")
    endif()
endforeach()
if(log MATCHES "${forbidden}")
    list(APPEND failures "crash output: ${CMAKE_MATCH_0}")
endif()
if(failures)
    list(JOIN failures "; " text)
    message(FATAL_ERROR "RunGodotCheck: ${text}")
endif()
message(STATUS "RunGodotCheck: passed")
