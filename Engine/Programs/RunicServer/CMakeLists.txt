# RunicServer: the Engine's own game-agnostic server, on the Engine's entry point with the fixed 60 Hz loop. Created by
# APP-1 (slice 1); SIM-1 (slice 3) puts the Simulation layer on it, with its settings from the process configuration.
# A composition root, not a module. Never shipped. Slice 9 (COOK-1, GDT-4) adds --cooked: a cooked folder's cells
# loaded through Runic::Assets and walked on. Slice 10 (PHYS-4, SIM-3) adds --parallel-demo: a scene whose steps split
# their work across the simulation's workers, with the same result for any worker count.
add_executable(RunicServer Main.cpp CookedWalk.cpp ParallelDemo.cpp)
target_link_libraries(RunicServer PRIVATE Runic::Application Runic::Simulation Runic::Assets)
runic_apply_toolchain(RunicServer)
runic_check_headless(RunicServer)  # a server never reaches Godot, graphics, audio or editor code

# Slice 1, check 2: the fixed loop stops cleanly, and a push, a replacement and a pop apply at frame safe points.
if(RUNIC_ENGINE_BUILD_TESTS)
    add_test(NAME programs.runic_server
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicServer>|--quit-after|120|--layer-demo"
            "-DEXPECT=RUNIC: RunicServer started \\(fixed 60 Hz\\)|RUNIC: engine:probe_a attached after 0 frames|RUNIC: engine:probe_b attached after 30 frames|RUNIC: engine:probe_b detached after 60 frames|RUNIC: engine:probe_c attached after 60 frames|RUNIC: engine:probe_c detached after 90 frames|RUNIC: engine:probe_a detached after 120 frames|RUNIC: RunicServer stopped cleanly after 120 frames in 2\\.0 s"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    set_tests_properties(programs.runic_server PROPERTIES LABELS programs TIMEOUT 60)

    # Slice 3: the scheduler runs 60/30/20/10 Hz on the server loop with metrics. Two seconds of the fixed loop are
    # 120, 60, 40 and 20 domain ticks; the probes of the first master step run in slot order (one expression over
    # several lines); the worker count comes from the command line.
    add_test(NAME programs.runic_server_clock
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMMAND=$<TARGET_FILE:RunicServer>|--quit-after|120|--clock-demo|--set|simulation.workers=2"
            "-DEXPECT=RUNIC: setting simulation.workers = 2 \\(command line\\)|RUNIC: setting simulation.max_ticks_per_update = 5 \\(default\\)|RUNIC: Simulation layer attached: simulation 1, 2 worker\\(s\\)|ran in hz20 producers \\(master tick 0, hz20 tick 0, dt 0.0500 s\\).*ran in hz10 producers \\(master tick 0, hz10 tick 0, dt 0.1000 s\\).*ran in hz30 realtime \\(master tick 0, hz30 tick 0, dt 0.0333 s\\).*ran in hz20 consumers.*ran in hz10 consumers.*ran in hz60 hook \\(master tick 0, hz60 tick 0, dt 0.0167 s\\)|RUNIC: Simulation layer ran 120 master ticks \\(hz60 120, hz30 60, hz20 40, hz10 20\\), 0 owed|RUNIC: clock domain hz60: 120 ticks, mean|RUNIC: clock domain hz30: 60 ticks, mean|RUNIC: clock domain hz20: 40 ticks, mean|RUNIC: clock domain hz10: 20 ticks, mean|RUNIC: RunicServer stopped cleanly after 120 frames in 2\\.0 s"
            -P "${PROJECT_SOURCE_DIR}/tests/godot/RunGodotCheck.cmake")
    set_tests_properties(programs.runic_server_clock PROPERTIES LABELS programs TIMEOUT 60)

    # Slice 3: a bad setting is reported with its source and stops the server with exit code 1, before any frame.
    add_test(NAME programs.runic_server_bad_setting
        COMMAND RunicServer --quit-after 120 --set simulation.workers=0 --set simulation.wokers=2)
    set_tests_properties(programs.runic_server_bad_setting PROPERTIES LABELS programs TIMEOUT 60
        PASS_REGULAR_EXPRESSION "command line --set simulation.workers=0: simulation.workers: 0 is outside 1 to 64.*command line --set simulation.wokers=2: unknown setting 'simulation.wokers'.*Configuration failed to attach: 2 problem\\(s\\) in the settings.*RunicServer stopped with exit code 1 after 0 frames")

    # Slice 10: the parallel demo on one, two and four workers logs the same hash of its whole scene.
    add_test(NAME programs.runic_server_parallel
        COMMAND "${CMAKE_COMMAND}" "-DSERVER=$<TARGET_FILE:RunicServer>" "-DWORKERS=1|2|4"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/SameParallelResult.cmake")
    set_tests_properties(programs.runic_server_parallel PROPERTIES LABELS programs TIMEOUT 600)
endif()
