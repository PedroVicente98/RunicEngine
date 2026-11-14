# RunicServer: the Engine's own game-agnostic server, on the Engine's entry point with the fixed 60 Hz loop. Created by
# APP-1 (slice 1); a composition root, not a module. Never shipped.
add_executable(RunicServer Main.cpp)
target_link_libraries(RunicServer PRIVATE Runic::Application)
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
endif()
