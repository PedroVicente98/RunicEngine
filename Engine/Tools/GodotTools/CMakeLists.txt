# Runic::GodotTools (Tools, never shipped): the RunicEditor's plugins, docks and assistants.
# Contract: ARCHITECTURE.md. Slice 0 holds the GDT-1 spike's test plugin.
runic_add_module(GodotTools
    GROUP Tools
    PUBLIC_DEPS Runic::GodotBridge
    PRIVATE_DEPS godot::cpp)
