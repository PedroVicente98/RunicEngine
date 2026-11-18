# RunicClient: the Engine's own game-agnostic client, on the Engine's entry point with the GodotLayer on top. Created
# by GDB-1 (slice 1); a composition root, not a module. Never shipped.
add_executable(RunicClient Main.cpp DebugDemo.cpp MovingEntities.cpp SpatialScene.cpp WalkScene.cpp)
target_link_libraries(RunicClient PRIVATE Runic::Application Runic::Client Runic::GodotBridge Runic::LibGodotEditor
                                          Runic::LibGodotTemplate)
runic_apply_toolchain(RunicClient)
runic_godot_host_executable(RunicClient)

# The neutral test project, copied so Godot's .godot/ import cache stays out of the sources.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Project/" DESTINATION "${CMAKE_BINARY_DIR}/godot/RunicClientProject")
target_compile_definitions(RunicClient PRIVATE RUNIC_CLIENT_PROJECT_DIR="${CMAKE_BINARY_DIR}/godot/RunicClientProject")

# Slice 1, checks 3 and 4, slice 2's debug view, slice 5's moving entities and slice 7's walk (label godot: they need
# libgodot built).
if(RUNIC_ENGINE_BUILD_TESTS)
    add_test(NAME godot.runic_client_headless
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|60"
            "-DEXPECT=RUNIC: Godot started headless \\(editor library\\)|RUNIC: debug view stopped at frame [1-9][0-9]*: [1-9][0-9]* line vertices, [1-9][0-9]* triangle vertices, [1-9][0-9]* texts|RUNIC: Godot stopped after 60 frames|RUNIC: RunicClient exited cleanly after 60 frames"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    add_test(NAME godot.runic_client_input
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|30|--input-check"
            "-DEXPECT=RUNIC: AppEvent MouseButton reached the stack at \\(600, 400\\)|RUNIC: action engine:demo_mark pressed at frame [0-9]+|RUNIC: AppEvent Key reached the stack \\(key 65\\)|RUNIC: walk scene: .*marks 1,|RUNIC: RunicClient exited cleanly"
            "-DFORBID=reached the stack at \\(120, 50\\)"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    # Slice 5: the walkers steered by commands are drawn by the presenter, 16 walkers and 4 crates in Godot.
    add_test(NAME godot.runic_client_moving_entities
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|120|--moving-scene"
            "-DEXPECT=RUNIC: presenter stopped at frame [1-9][0-9]*: 20 instances, 0 rebases|RUNIC: moving entities: 16 walkers steered by [1-9][0-9]* commands|RUNIC: Simulation layer ran [1-9][0-9]* master ticks|RUNIC: RunicClient exited cleanly after 120 frames"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    # Slice 7: R sets the capsule walking on the terrain far from the origin, Space jumps, R stops it; all through
    # Godot's input, the Input layer's actions and the walk commands.
    add_test(NAME godot.runic_client_walk
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|600|--walk-check"
            "-DEXPECT=RUNIC: walk scene: the capsule walked [1-9][0-9]*\\.[0-9] m on terrain 8602 km from the origin, jumps 1, marks 0, highest [0-9.]+ m, ends on the ground|RUNIC: RunicClient exited cleanly after 600 frames"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    # Slice 8: the physics geometry layer draws the simulation's collision (the terrain as one cached mesh) and the
    # capsule's collision lines up with its gameplay shape in Godot, frame after frame, while it walks.
    add_test(NAME godot.runic_client_physics_view
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|300|--walk-check|--physics-check"
            "-DEXPECT=RUNIC: physics geometry stopped at frame [1-9][0-9]*: [1-9][0-9]* line vertices, [0-9]+ triangle vertices, [1-9][0-9]* mesh instances of 1 cached meshes \\(1 uploads\\)|RUNIC: physics check: the capsule's collision lines up with its shape within [0-9]\\.[0-9] mm over [1-9][0-9]* frames|RUNIC: RunicClient exited cleanly after 300 frames"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    # Slice 11: the spatial scene draws the index's cells, its members, a query sweeping from the cave to the island
    # and the volumes; every query matches testing every walker, and walkers pass through the cave.
    add_test(NAME godot.runic_client_spatial
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicClient>|--headless|--quit-after|240|--spatial-scene"
            "-DEXPECT=RUNIC: debug view stopped at frame [1-9][0-9]*: [1-9][0-9]* line vertices|RUNIC: spatial scene: 150 walkers in [1-9][0-9]* cells of [1-9][0-9]* columns \\([1-9][0-9]* cell changes\\), [1-9][0-9]* queries matched testing every walker \\([1-9][0-9]* found, 0 differed\\), [1-9][0-9]* walker positions inside the cave, volumes 4 \\(3 rule areas, 1 need a Physics body, 1 indivisible\\)|RUNIC: RunicClient exited cleanly after 240 frames"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    set_tests_properties(godot.runic_client_headless godot.runic_client_input godot.runic_client_moving_entities
        godot.runic_client_walk godot.runic_client_physics_view godot.runic_client_spatial
        PROPERTIES LABELS godot TIMEOUT 600)
endif()
