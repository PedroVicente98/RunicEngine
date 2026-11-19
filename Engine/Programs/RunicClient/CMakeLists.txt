# RunicClient: the Engine's own game-agnostic client, on the Engine's entry point with the GodotLayer on top. Created
# by GDB-1 (slice 1); a composition root, not a module. Never shipped.
add_executable(RunicClient Main.cpp DebugDemo.cpp)
target_link_libraries(RunicClient PRIVATE Runic::Application Runic::GodotBridge Runic::LibGodotEditor
                                          Runic::LibGodotTemplate)
runic_apply_toolchain(RunicClient)
runic_godot_host_executable(RunicClient)

# The neutral test project, copied so Godot's .godot/ import cache stays out of the sources.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Project/" DESTINATION "${CMAKE_BINARY_DIR}/godot/RunicClientProject")
target_compile_definitions(RunicClient PRIVATE RUNIC_CLIENT_PROJECT_DIR="${CMAKE_BINARY_DIR}/godot/RunicClientProject")

# Slice 1, checks 3 and 4, and slice 2's debug view (label godot: they need libgodot built).
if(RUNIC_ENGINE_BUILD_TESTS)
    add_test(NAME godot.runic_client_headless
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|60"
            "-DEXPECT=RUNIC: Godot started headless \\(editor library\\)|RUNIC: debug view stopped at frame [1-9][0-9]*: [1-9][0-9]* line vertices, [1-9][0-9]* triangle vertices, [1-9][0-9]* texts|RUNIC: Godot stopped after 60 frames|RUNIC: RunicClient exited cleanly after 60 frames"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    add_test(NAME godot.runic_client_input
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|30|--input-check"
            "-DEXPECT=RUNIC: AppEvent MouseButton reached the stack at \\(600, 400\\)|RUNIC: AppEvent Key reached the stack \\(key 65\\)|RUNIC: RunicClient exited cleanly"
            "-DFORBID=reached the stack at \\(120, 50\\)"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    set_tests_properties(godot.runic_client_headless godot.runic_client_input PROPERTIES LABELS godot TIMEOUT 600)
endif()
