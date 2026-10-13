runic_add_module(Alpha GROUP Foundation PRIVATE_DEPS Runic::Core HEADLESS)
runic_add_interface_target(Port FROM Alpha HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/PortHeaders")
# An interface target may link only Runic::Core; this extra link must fail at the end of configure.
get_target_property(port_real Fixture::Port ALIASED_TARGET)
target_link_libraries(${port_real} INTERFACE fake_flecs)
