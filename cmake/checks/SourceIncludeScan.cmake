# Source-include scan (BUILD-1; "Any module → another module's Source/" is a forbidden dependency).
#
#   cmake -DROOT=<repository> -P SourceIncludeScan.cmake
#
# Fails when a file includes a path inside another module's Source/ folder. Only Public/ headers cross a
# module boundary; the build also enforces it, because Source/ is never an exported include directory.
cmake_minimum_required(VERSION 3.24)
if(NOT DEFINED ROOT)
    message(FATAL_ERROR "SourceIncludeScan: -DROOT=... is required")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/ScanCommon.cmake")

get_filename_component(ROOT "${ROOT}" ABSOLUTE)
runic_scan_files("${ROOT}" files)
set(errors "")
foreach(file IN LISTS files)
    runic_scan_module_of("${file}" own)
    runic_scan_includes("${ROOT}/${file}" includes)
    foreach(inc IN LISTS includes)
        if(NOT inc MATCHES "(^|/)([A-Za-z0-9_]+)/Source/")
            continue()
        endif()
        set(target_module "${CMAKE_MATCH_2}")
        if(target_module STREQUAL own)
            continue()
        endif()
        list(APPEND errors
            "RUNIC: ${file}: includes \"${inc}\" but ${target_module}/Source belongs to another module, and only Public/ headers cross a module boundary")
    endforeach()
endforeach()

if(errors)
    runic_scan_fail("${errors}")
endif()
list(LENGTH files count)
message(STATUS "RUNIC: source-include scan passed (${count} files)")
