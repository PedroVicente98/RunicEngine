runic_add_module(Alpha GROUP Foundation PRIVATE_DEPS fixture_library HEADLESS)
runic_add_native_target(AlphaNative FROM Alpha HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/AlphaNative" LIBRARY fixturelib)
