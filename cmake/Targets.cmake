# Public include root and C++ level. No dummy .cpp or version API is needed.
add_library(runic_core INTERFACE)
add_library(Runic::Core ALIAS runic_core)
target_compile_features(runic_core INTERFACE cxx_std_20)
target_include_directories(runic_core INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

# Existing sibling consumers can keep linking the headless Runtime boundary.
add_library(runic_runtime INTERFACE)
add_library(Runic::Runtime ALIAS runic_runtime)
target_link_libraries(runic_runtime INTERFACE Runic::Core)
