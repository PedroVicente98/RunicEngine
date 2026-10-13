# Shared start of every BUILD-1 fixture project. RunFixture.cmake passes RUNIC_ENGINE_DIR.
if(NOT RUNIC_ENGINE_DIR)
    message(FATAL_ERROR "fixture: -DRUNIC_ENGINE_DIR=<engine source> is required")
endif()
list(APPEND CMAKE_MODULE_PATH "${RUNIC_ENGINE_DIR}/cmake")
include(RunicModule)
set(RUNIC_MODULE_NAMESPACE Fixture)
