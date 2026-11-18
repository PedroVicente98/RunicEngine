# Runic::Cooker (Tools, never shipped): the scenes Godot imported, handed over as plain data by GodotTools, into
# Runic's native cooked formats. COOK-1 (slice 9) starts it. Contract: ARCHITECTURE.md. It links no Godot: it runs
# inside the RunicEditor and in build machines' windowless RunicEditor. Jolt is reached through Runic::Physics.
runic_add_module(Cooker
    GROUP Tools
    PUBLIC_DEPS Runic::Core Runic::Content Runic::Assets
    PRIVATE_DEPS Runic::Physics)

if(TARGET RunicCookerTests)
    # The tests walk a character on what they cooked, as a runtime would: they build Physics bodies themselves.
    target_link_libraries(RunicCookerTests PRIVATE Runic::Physics)
endif()
