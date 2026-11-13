# Configured at most once per build tree: a second add_subdirectory of this folder would fail configure.
add_library(fixture_fakelib INTERFACE)
add_library(fake::lib ALIAS fixture_fakelib)
target_include_directories(fixture_fakelib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(WRITE "${CMAKE_BINARY_DIR}/fakelib-configured.txt" "configured\n")
message(STATUS "FAKELIB: configured")
