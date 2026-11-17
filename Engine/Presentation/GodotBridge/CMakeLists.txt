# Runic::GodotBridge (Presentation): the only Engine module that talks to Godot at runtime.
# Contract: ARCHITECTURE.md. Slice 0 wrote the LibGodot host; GDB-1 (slice 1) added the GodotLayer and the input sink
# (Runic::Application is public: GodotLayer derives from its Layer); GDB-2 (slice 2) the debug view and the overlay;
# GDB-3 (slice 5) the presenter.
runic_add_module(GodotBridge
    GROUP Presentation
    PUBLIC_DEPS Runic::Application
    PRIVATE_DEPS godot::cpp runic_system_dl)

if(TARGET RunicGodotBridgeTests)
    # The integration tests start the editor library headless on the spike project and load the template
    # library, when they are built (both are staged next to the test).
    # The GDB-2 and GDB-3 tests also look at what the debug view and the presenter put in Godot's scene, through
    # godot-cpp.
    target_link_libraries(RunicGodotBridgeTests PRIVATE Runic::LibGodotEditor Runic::LibGodotTemplate godot::cpp)
    get_property(spike_project GLOBAL PROPERTY RUNIC_GODOT_SPIKE_PROJECT)
    target_compile_definitions(RunicGodotBridgeTests PRIVATE RUNIC_GODOT_SPIKE_PROJECT="${spike_project}")
    set_target_properties(RunicGodotBridgeTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
