# Compiler settings shared by every Runic target in the three repositories (BUILD-1).
#
#   runic_apply_cpu_baseline(<target>)   the x86-64-v3 CPU baseline (AVX2, FMA), or none
#   runic_enable_warnings(<target>)      warnings, the float-conversion rule, warnings as errors in CI
#
# RUNIC_CPU_BASELINE: x86-64-v3 (default; servers and clients, decided in the architecture) or none (old
# machines: the compiler's default target, no AVX2). RUNIC_WARNINGS_AS_ERRORS: CI turns it on.
include_guard(GLOBAL)

# Stops configure with RUNIC: lines printed exactly as written. message(FATAL_ERROR) re-wraps its text at 80
# columns, which would split the one-line diagnostics that the build tests and CI logs rely on.
function(runic_fatal)
    set(text "")
    math(EXPR last "${ARGC} - 1")
    foreach(i RANGE 0 ${last})
        string(APPEND text "${ARGV${i}}")
    endforeach()
    message(NOTICE "${text}")
    message(FATAL_ERROR "RUNIC: configure stopped by the check above")
endfunction()

set(RUNIC_CPU_BASELINE "x86-64-v3" CACHE STRING "CPU baseline for Runic targets: x86-64-v3 or none")
set_property(CACHE RUNIC_CPU_BASELINE PROPERTY STRINGS "x86-64-v3" "none")
if(NOT RUNIC_CPU_BASELINE MATCHES "^(x86-64-v3|none)$")
    runic_fatal("RUNIC: RUNIC_CPU_BASELINE must be x86-64-v3 or none, not '${RUNIC_CPU_BASELINE}'")
endif()
option(RUNIC_WARNINGS_AS_ERRORS "Treat warnings as errors in Runic targets (CI)" OFF)

# True when the baseline applies to this toolchain: x86-64 targets only (an arm64 build ignores it).
function(_runic_cpu_baseline_applies out_var)
    set(applies FALSE)
    if(RUNIC_CPU_BASELINE STREQUAL "x86-64-v3")
        set(processor "${CMAKE_SYSTEM_PROCESSOR}")
        if(MSVC AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
            set(processor "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
        endif()
        if(processor MATCHES "^(x86_64|AMD64|amd64|x64)$")
            set(applies TRUE)
        endif()
    endif()
    set(${out_var} ${applies} PARENT_SCOPE)
endfunction()

function(runic_apply_cpu_baseline target)
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    _runic_cpu_baseline_applies(applies)
    if(NOT applies)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -march=x86-64-v3)
    endif()
endfunction()

function(runic_enable_warnings target)
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    if(MSVC)
        # C4244 and C4305: a double silently becoming a float is an error (the 64-bit type rule).
        target_compile_options(${target} PRIVATE /W4 /permissive- /we4244 /we4305)
        if(RUNIC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # WorldPosition and WorldDelta are 64-bit; going down to 32-bit only through ToLocal, so an
        # implicit double-to-float conversion is always an error (RunicDocs › Decisions log › Type rule).
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror=float-conversion)
        if(RUNIC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# C++20 without compiler extensions, the warnings and the CPU baseline: what every Runic target gets.
function(runic_apply_toolchain target)
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        target_compile_features(${target} INTERFACE cxx_std_20)
        return()
    endif()
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    runic_enable_warnings(${target})
    runic_apply_cpu_baseline(${target})
endfunction()
