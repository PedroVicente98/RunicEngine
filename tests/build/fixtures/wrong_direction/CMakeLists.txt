cmake_minimum_required(VERSION 3.24)
project(FixtureWrongDirection LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../FixturePrologue.cmake")
add_library(fake_platform_transport INTERFACE)
add_library(RunicPlatform::Transport ALIAS fake_platform_transport)
# An Engine module may never link a Platform target, even when a catalog row allows it.
set(RUNIC_MODULE_NAMESPACE Runic)
runic_catalog_module(Alpha GROUP Foundation ALLOWS RunicPlatform::Transport HEADLESS yes)
add_subdirectory(Alpha)
