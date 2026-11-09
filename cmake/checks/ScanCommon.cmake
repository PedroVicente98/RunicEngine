# Shared helpers for the scan scripts (cmake -P).
include_guard(GLOBAL)

# Source and header files under <root>, relative to it, skipping trees that break the rules on purpose or are
# not ours: build test fixtures, third parties, submodules, build folders, VCS and tool state.
function(runic_scan_files root out_var)
    file(GLOB_RECURSE files RELATIVE "${root}" LIST_DIRECTORIES FALSE
        "${root}/*.h" "${root}/*.hpp" "${root}/*.hh" "${root}/*.hxx" "${root}/*.inl"
        "${root}/*.c" "${root}/*.cc" "${root}/*.cpp" "${root}/*.cxx")
    set(kept "")
    foreach(file IN LISTS files)
        if(file MATCHES "(^|/)(tests/build/fixtures|external|[Bb]uild[^/]*|cmake-build-[^/]*|out|\\.git|\\.godot|node_modules)/")
            continue()
        endif()
        list(APPEND kept "${file}")
    endforeach()
    set(${out_var} "${kept}" PARENT_SCOPE)
endfunction()

# The module a file belongs to: the folder that holds its Public/, Source/ or Tests/ folder ("" if none).
function(runic_scan_module_of path out_var)
    set(module "")
    if(path MATCHES "(^|/)([A-Za-z0-9_]+)/(Public|Source|Tests)/")
        set(module "${CMAKE_MATCH_2}")
    endif()
    set(${out_var} "${module}" PARENT_SCOPE)
endfunction()

# The #include lines of a file, with // comments removed: a list of the included paths.
function(runic_scan_includes file out_var)
    file(STRINGS "${file}" lines REGEX "^[ \t]*#[ \t]*include")
    set(includes "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "//.*$" "" line "${line}")
        if(line MATCHES "#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
            list(APPEND includes "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    set(${out_var} "${includes}" PARENT_SCOPE)
endfunction()

# The code of a file without // comments and without the contents of string literals, one string.
function(runic_scan_code file out_var)
    file(STRINGS "${file}" lines)
    set(code "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "//.*$" "" line "${line}")
        string(REGEX REPLACE "\"[^\"]*\"" "\"\"" line "${line}")
        string(APPEND code "${line}\n")
    endforeach()
    set(${out_var} "${code}" PARENT_SCOPE)
endfunction()

# Fails the script with every RUNIC: line printed exactly as written (FATAL_ERROR would re-wrap long lines).
function(runic_scan_fail errors)
    list(REMOVE_DUPLICATES errors)
    list(JOIN errors "\n" text)
    message(NOTICE "${text}")
    list(LENGTH errors count)
    message(FATAL_ERROR "RUNIC: ${count} problem(s), listed above")
endfunction()
