cmake_minimum_required(VERSION 3.24)
project(FixtureThirdPartyNeed LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../FixturePrologue.cmake")
# A stand-in for a pinned library: its recipe adds FakeLib/, which leaves fakelib-configured.txt in the build tree.
# FIXTURE_USERS ("," between names) picks the modules: Alpha links the ID, Beta the target, Gamma nothing, and Delta
# links the ID without listing it in its row.
function(_fixture_recipe_fakelib)
    add_subdirectory("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FakeLib" "${CMAKE_BINARY_DIR}/fakelib-build")
endfunction()
runic_third_party_recipe(fakelib TARGETS fake::lib FETCH _fixture_recipe_fakelib)

runic_catalog_module(Alpha GROUP Foundation THIRD_PARTY fakelib HEADLESS yes)
runic_catalog_module(Beta GROUP Foundation THIRD_PARTY fakelib HEADLESS yes)
runic_catalog_module(Gamma GROUP Foundation HEADLESS yes)
runic_catalog_module(Delta GROUP Foundation HEADLESS yes)
if(NOT FIXTURE_USERS)
    set(FIXTURE_USERS "Alpha,Beta")
endif()
string(REPLACE "," ";" FIXTURE_USERS "${FIXTURE_USERS}")
foreach(module IN LISTS FIXTURE_USERS)
    add_subdirectory(${module})
endforeach()
