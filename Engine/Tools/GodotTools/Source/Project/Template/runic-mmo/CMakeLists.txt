# {{GAME_NAME}}: a RunicEngine project (project.godot and runic.project.json at the root). This file builds the
# game's editor library, Build/bin/(lib)game_editor, which the RunicEditor loads through game_editor.gdextension
# and reloads after each build. The game's server and client executables join here as the game grows.
#
#   cmake -S . -B Build/debug -DRUNIC_ENGINE_DIR=/path/to/RunicEngine && cmake --build Build/debug
cmake_minimum_required(VERSION 3.24)
project(RunicProject LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # the IDE bridge points clangd at it

set(RUNIC_ENGINE_DIR "" CACHE PATH "The RunicEngine checkout this project builds against")
if(NOT EXISTS "${RUNIC_ENGINE_DIR}/cmake/RunicModule.cmake")
    message(FATAL_ERROR "Pass -DRUNIC_ENGINE_DIR=/path/to/RunicEngine")
endif()
set(RUNIC_ENGINE_ENABLE_PRESENTATION ON CACHE BOOL "godot-cpp, for the editor library")
set(RUNIC_ENGINE_BUILD_TOOLS ON CACHE BOOL "The Engine's editor tools")
set(RUNIC_ENGINE_BUILD_TESTS OFF CACHE BOOL "The Engine's own tests run in the Engine")
add_subdirectory("${RUNIC_ENGINE_DIR}" "${CMAKE_BINARY_DIR}/engine")

add_library(game_editor SHARED Source/Editor/GameEditor.cpp)
target_link_libraries(game_editor PRIVATE Runic::GodotTools)
set_target_properties(game_editor PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Build/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Build/bin")
