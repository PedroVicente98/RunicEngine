# Docs check (BUILD-1; Automated checks: "a code change without its ARCHITECTURE.md or package-status update").
#
#   cmake -DCHANGED=<file with one changed path per line> -DROOT=<repository> [-DRULES=<rules file>]
#         -P DocsCheck.cmake
#
# Every changed code path inside a module folder needs, in the same change, the module's contract: its
# ARCHITECTURE.md, which also carries the module's package status. A docs-only change (.md) needs nothing.
# RULES defines runic_docs_rule(<path> <out_module> <out_contracts>): the module a path belongs to ("" for
# none) and the contract paths that satisfy it. The default is this repository's cmake/checks/DocsRules.cmake.
# In CI the list comes from `git diff --name-only origin/<base>...HEAD`.
cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/ScanCommon.cmake")
foreach(required CHANGED ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "DocsCheck: -D${required}=... is required")
    endif()
endforeach()
get_filename_component(ROOT "${ROOT}" ABSOLUTE)
if(NOT DEFINED RULES OR RULES STREQUAL "")
    set(RULES "${CMAKE_CURRENT_LIST_DIR}/DocsRules.cmake")
endif()
include("${RULES}")

file(STRINGS "${CHANGED}" changed)
set(clean "")
foreach(path IN LISTS changed)
    string(STRIP "${path}" path)
    if(NOT path STREQUAL "")
        list(APPEND clean "${path}")
    endif()
endforeach()
set(changed "${clean}")

set(errors "")
set(code_changed FALSE)
foreach(path IN LISTS changed)
    runic_docs_rule("${path}" module contracts)
    if(module STREQUAL "")
        continue()
    endif()
    if(path IN_LIST contracts OR path MATCHES "\\.md$")
        continue()   # a docs change is never code that needs docs
    endif()
    set(code_changed TRUE)
    set(satisfied FALSE)
    foreach(contract IN LISTS contracts)
        if(contract IN_LIST changed)
            set(satisfied TRUE)
        endif()
    endforeach()
    if(NOT satisfied)
        list(JOIN contracts " or " wanted)
        list(APPEND errors "RUNIC: ${path} changes module ${module} without ${wanted}")
    endif()
endforeach()

if(errors)
    runic_scan_fail("${errors}")
endif()
list(LENGTH changed count)
message(STATUS "RUNIC: docs check passed (${count} changed paths)")
