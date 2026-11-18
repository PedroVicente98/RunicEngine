# RunicEditor: owned by the Engine (RunicDocs › Repositories, build targets and processes › Processes).
# The Godot editor through LibGodot (editor target) with the Runic tools, on the Engine's entry point since GDB-1, with a
# local simulation for the Runic World dock since GDT-2; never shipped.
add_executable(RunicEditor Main.cpp LocalWorld.cpp)
target_link_libraries(RunicEditor PRIVATE Runic::Application Runic::Simulation Runic::GodotTools Runic::GodotBridge
                                          Runic::LibGodotEditor)
target_compile_definitions(RunicEditor PRIVATE
    RUNIC_TEMPLATES_DIR="${RUNIC_TEMPLATES_DIR}"
    RUNIC_ENGINE_VERSION="${RunicEngine_VERSION}")
runic_apply_toolchain(RunicEditor)
runic_godot_host_executable(RunicEditor)

if(RUNIC_ENGINE_BUILD_TESTS)
    get_property(spike_project GLOBAL PROPERTY RUNIC_GODOT_SPIKE_PROJECT)
    # GDT-1, then GDT-2: the editor opens a plain Godot project headless, loads the Runic plugin, says the project is
    # not a Runic one yet, and exits cleanly.
    add_test(NAME godot.editor_spike
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicEditor>|--headless|--editor|--path|${spike_project}|--quit-after|120"
            "-DEXPECT=RUNIC: Runic editor plugin loaded|RUNIC: GDScript in this editor build: no|is a Godot project without runic.project.json|RUNIC: Runic editor plugin unloaded|RUNIC: RunicEditor exited cleanly"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    add_test(NAME godot.editor_project_manager
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicEditor>|--headless|--quit-after|30"
            "-DEXPECT=RUNIC: RunicEditor exited cleanly"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    set_tests_properties(godot.editor_spike godot.editor_project_manager PROPERTIES LABELS godot TIMEOUT 600)

    # GDT-2 (slice 6): New makes a project from the template; the editor opens it, loads its editor library (the
    # fixture's build 1) and shows the world dock over its local simulation (a region, 12 markers, and since GDT-3 a
    # floor and two crates). GDT-3 (slice 8): the physics dock lists the 15 bodies and the "see what the server sees"
    # layer draws them in the editor's viewport.
    set(_runic_new_project "${CMAKE_BINARY_DIR}/godot/NewProjectCheck")
    add_test(NAME godot.editor_new_project_clean COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_runic_new_project}")
    add_test(NAME godot.editor_new_project
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicEditor>|--runic-new-project|${_runic_new_project}|Check Game"
            "-DEXPECT=RUNIC: new Runic project Check Game in"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    set_tests_properties(godot.editor_new_project_clean PROPERTIES FIXTURES_SETUP runic_new_project_clean)
    set_tests_properties(godot.editor_new_project PROPERTIES
        FIXTURES_REQUIRED runic_new_project_clean FIXTURES_SETUP runic_new_project)
    set(_runic_project_tests godot.editor_new_project_clean godot.editor_new_project)
    if(TARGET runic_game_editor_fixture_1)
        add_test(NAME godot.editor_project_library
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:runic_game_editor_fixture_1>"
                    "${_runic_new_project}/Build/bin/libgame_editor.so")
        add_test(NAME godot.editor_opens_project
            COMMAND "${CMAKE_COMMAND}"
                "-DCOMMAND=$<TARGET_FILE:RunicEditor>|--headless|--editor|--path|${_runic_new_project}|--quit-after|300"
                "-DEXPECT=RUNIC: Runic editor plugin loaded|RUNIC: project Check Game opened \\(Runic format 1, made with Engine ${RunicEngine_VERSION}\\)|RUNIC: game editor library res://Build/bin/libgame_editor.so|RUNIC: game editor fixture loaded \\(build 1\\)|RUNIC: world dock shows 16 entities of simulation 1|RUNIC: physics dock shows 15 bodies of simulation 1|RUNIC: server view draws [0-9]+ line vertices|RUNIC: Runic editor plugin unloaded|RUNIC: RunicEditor exited cleanly"
                -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
        set_tests_properties(godot.editor_project_library PROPERTIES
            FIXTURES_REQUIRED runic_new_project FIXTURES_SETUP runic_project_library)
        set_tests_properties(godot.editor_opens_project PROPERTIES
            FIXTURES_REQUIRED "runic_new_project;runic_project_library")
        list(APPEND _runic_project_tests godot.editor_project_library godot.editor_opens_project)
    endif()
    set_tests_properties(${_runic_project_tests} PROPERTIES LABELS godot TIMEOUT 600)
    unset(_runic_new_project)
    unset(_runic_project_tests)
endif()
