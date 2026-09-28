add_executable(RunicSandbox)
target_sources(RunicSandbox PRIVATE apps/Sandbox/Main.cpp)
target_link_libraries(RunicSandbox PRIVATE Runic::Runtime Runic::Presentation)
set_target_properties(RunicSandbox PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
runic_enable_warnings(RunicSandbox)
