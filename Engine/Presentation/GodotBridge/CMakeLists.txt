# Runic::GodotBridge (Presentation): the only Engine module that talks to Godot at runtime.
# Contract: ARCHITECTURE.md. Slice 0 holds the LibGodot host; GDB-1 (slice 1) adds the GodotLayer.
runic_add_module(GodotBridge
    GROUP Presentation
    PRIVATE_DEPS godot::cpp runic_system_dl)

if(TARGET RunicGodotBridgeTests)
    # The integration tests start the editor library headless on the spike project and load the template
    # library, when they are built (both are staged next to the test).
    target_link_libraries(RunicGodotBridgeTests PRIVATE Runic::LibGodotEditor Runic::LibGodotTemplate)
    get_property(spike_project GLOBAL PROPERTY RUNIC_GODOT_SPIKE_PROJECT)
    target_compile_definitions(RunicGodotBridgeTests PRIVATE RUNIC_GODOT_SPIKE_PROJECT="${spike_project}")
    set_target_properties(RunicGodotBridgeTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
