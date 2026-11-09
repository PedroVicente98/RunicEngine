# Disposable check that the transitional bundle's third parties (Flecs, GLM, Jolt) compile, link and run with the
# CPU baseline. Headless by construction: registered with runic_check_headless. It goes away with the bundle, when
# RunicGame links per-module targets.
add_executable(RunicSandbox Main.cpp)
target_link_libraries(RunicSandbox PRIVATE Runic::Runtime)
runic_apply_toolchain(RunicSandbox)
runic_check_headless(RunicSandbox)

if(RUNIC_ENGINE_BUILD_TESTS)
    add_test(NAME build.bundle_links COMMAND RunicSandbox)
    set_tests_properties(build.bundle_links PROPERTIES LABELS build PASS_REGULAR_EXPRESSION "RunicSandbox ready")
endif()
