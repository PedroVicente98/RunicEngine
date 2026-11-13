cmake_minimum_required(VERSION 3.24)
project(FixturePublicHeaderPaths LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../FixturePrologue.cmake")
# Alpha exports Shared/Name.hpp and its interface target Port exports Port/PortApi.hpp. FIXTURE_BETA picks Beta's
# folder: BetaSame (Shared/Name.hpp again), BetaCase (shared/name.hpp), BetaPort (Port/PortApi.hpp) or BetaUnique.
if(NOT FIXTURE_BETA)
    set(FIXTURE_BETA BetaSame)
endif()
runic_catalog_module(Alpha GROUP Foundation HEADLESS yes)
runic_catalog_module(Beta GROUP Foundation HEADLESS yes)
runic_catalog_interface(Port FROM Alpha)
add_subdirectory(Alpha)
add_subdirectory(${FIXTURE_BETA})
