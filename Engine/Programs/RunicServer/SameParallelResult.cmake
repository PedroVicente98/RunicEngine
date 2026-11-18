# Slice 10: runs RunicServer's parallel demo once per worker count and checks that every run exits with 0 and logs
# the same scene hash at the same realtime tick (the CTest programs.runic_server_parallel).
#
#   cmake -DSERVER=<RunicServer> -DWORKERS=1|4 -P SameParallelResult.cmake
cmake_minimum_required(VERSION 3.24)
foreach(required SERVER WORKERS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "SameParallelResult: -D${required}=... is required")
    endif()
endforeach()
string(REPLACE "|" ";" worker_counts "${WORKERS}")
set(report "RUNIC: parallel demo at realtime tick ([0-9]+) on ([0-9]+) worker\\(s\\): state ([0-9a-f]+)")
set(first_hash "")
set(failures "")
foreach(workers IN LISTS worker_counts)
    execute_process(
        COMMAND "${SERVER}" --quit-after 150 --parallel-demo --set "simulation.workers=${workers}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        TIMEOUT 240)
    set(log "${out}\n${err}")
    message("${log}")
    if(NOT result STREQUAL "0")
        list(APPEND failures "${workers} worker(s): exit code ${result}")
        continue()
    endif()
    if(NOT log MATCHES "${report}")
        list(APPEND failures "${workers} worker(s): no report")
        continue()
    endif()
    if(NOT CMAKE_MATCH_2 STREQUAL "${workers}")
        list(APPEND failures "${workers} worker(s): the report says ${CMAKE_MATCH_2}")
    endif()
    if(first_hash STREQUAL "")
        set(first_hash "${CMAKE_MATCH_3}")
    elseif(NOT CMAKE_MATCH_3 STREQUAL first_hash)
        list(APPEND failures "${workers} worker(s): state ${CMAKE_MATCH_3}, not ${first_hash}")
    endif()
endforeach()
if(failures)
    string(REPLACE ";" "\n  " listed "${failures}")
    message(FATAL_ERROR "SameParallelResult failed:\n  ${listed}")
endif()
message("SameParallelResult: state ${first_hash} with ${WORKERS} worker(s)")
