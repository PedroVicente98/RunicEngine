# RunicEditor: owned by the Engine (RunicDocs › Repositories, build targets and processes › Processes).
# The Godot editor through LibGodot (editor target) with the Runic tools; never shipped.
add_executable(RunicEditor Main.cpp)
target_link_libraries(RunicEditor PRIVATE Runic::GodotTools Runic::GodotBridge Runic::LibGodotEditor)
runic_apply_toolchain(RunicEditor)
runic_godot_host_executable(RunicEditor)

# GDT-1 acceptance: the editor opens a project headless, loads the test plugin and exits cleanly.
if(RUNIC_ENGINE_BUILD_TESTS)
    get_property(spike_project GLOBAL PROPERTY RUNIC_GODOT_SPIKE_PROJECT)
    add_test(NAME godot.editor_spike
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicEditor>|--headless|--editor|--path|${spike_project}|--quit-after|120"
            "-DEXPECT=RUNIC: test plugin loaded|RUNIC: GDScript in this editor build: no|RUNIC: test plugin unloaded|RUNIC: RunicEditor exited cleanly"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    add_test(NAME godot.editor_project_manager
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicEditor>|--headless|--quit-after|30"
            "-DEXPECT=RUNIC: RunicEditor exited cleanly"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    set_tests_properties(godot.editor_spike godot.editor_project_manager PROPERTIES LABELS godot TIMEOUT 600)
endif()
