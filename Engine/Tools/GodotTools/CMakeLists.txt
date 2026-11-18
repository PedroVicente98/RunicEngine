# Runic::GodotTools (Tools, never shipped): the RunicEditor's plugins, docks and assistants. Contract: ARCHITECTURE.md.
# GDT-1 (slice 0) held the spike's test plugin; GDT-2 (slice 6) the project model and the runic-mmo template, the IDE
# bridge, the watcher of the project's editor library, the Runic editor plugin and the world dock; GDT-3 (slice 8) the physics
# dock; GDT-4 (slice 9) the Cooker integration, which links Runic::Cooker (public: EditorCook.hpp). godot-cpp is PUBLIC,
# the approved exception: a project's editor library reaches godot-cpp through this target.
runic_add_module(GodotTools
    GROUP Tools
    PUBLIC_DEPS Runic::GodotBridge Runic::Simulation Runic::Cooker godot::cpp)

# The templates New copies, staged where a distribution keeps them (<build>/templates/<name>).
set(_runic_template_source "${CMAKE_CURRENT_SOURCE_DIR}/Source/Project/Template")
set(RUNIC_TEMPLATES_DIR "${CMAKE_BINARY_DIR}/templates" CACHE INTERNAL "Where the project templates are staged")
add_custom_target(runic_project_templates ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_runic_template_source}" "${RUNIC_TEMPLATES_DIR}"
    COMMENT "Staging the project templates in ${RUNIC_TEMPLATES_DIR}")
add_dependencies(runic_godottools runic_project_templates)
unset(_runic_template_source)

if(TARGET RunicGodotToolsTests)
    # GDT-2's reload check runs the editor library headless in the test process, on a project made from the
    # template, with a fixture editor library built twice (build 1 and build 2) to swap under it.
    foreach(build 1 2)
        add_library(runic_game_editor_fixture_${build} SHARED
            "${PROJECT_SOURCE_DIR}/tests/godot/fixtures/game_editor/GameEditorFixture.cpp")
        target_link_libraries(runic_game_editor_fixture_${build} PRIVATE godot::cpp)
        target_compile_definitions(runic_game_editor_fixture_${build} PRIVATE RUNIC_FIXTURE_BUILD=${build})
        set_target_properties(runic_game_editor_fixture_${build} PROPERTIES
            OUTPUT_NAME game_editor
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/godot/fixtures/game_editor_build${build}")
    endforeach()
    target_link_libraries(RunicGodotToolsTests PRIVATE Runic::LibGodotEditor)
    add_dependencies(RunicGodotToolsTests runic_game_editor_fixture_1 runic_game_editor_fixture_2)
    target_compile_definitions(RunicGodotToolsTests PRIVATE
        RUNIC_TEMPLATES_DIR="${RUNIC_TEMPLATES_DIR}"
        RUNIC_FIXTURE_BUILD_1="$<TARGET_FILE:runic_game_editor_fixture_1>"
        RUNIC_FIXTURE_BUILD_2="$<TARGET_FILE:runic_game_editor_fixture_2>"
        RUNIC_TEST_REGION_DIR="${PROJECT_SOURCE_DIR}/tests/godot/fixtures/test_region")
    set_target_properties(RunicGodotToolsTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
