# Runic::Assets (Foundation): asset handles, the cache shared by client and server, and the native cooked formats
# (meshes, collision, cells). ASSET-1 (slice 9) starts it. Contract: ARCHITECTURE.md. Zstandard and packages are
# ASSET-2's (slice 21): ASSET-1 links no third-party library.
runic_add_module(Assets
    GROUP Foundation
    PUBLIC_DEPS Runic::Core Runic::Content
    HEADLESS)

if(TARGET RunicAssetsTests)
    # The golden file the format tests read (written once by the writer; see Tests/Data/README.md).
    target_compile_definitions(RunicAssetsTests PRIVATE RUNIC_ASSETS_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Data")
endif()
