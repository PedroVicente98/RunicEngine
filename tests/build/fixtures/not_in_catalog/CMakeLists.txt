cmake_minimum_required(VERSION 3.24)
project(FixtureNotInCatalog LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../FixturePrologue.cmake")
# Alpha has no catalog row.
add_subdirectory(Alpha)
