# cmake/ — the build helpers and checks

This folder holds the CMake helpers, the module catalog, the toolchain, the Godot build and the checks that the three
repositories share. This README is their reference: what each helper and check does, the presets, CI and the
acceptance tests. It is also the contract of BUILD-1, the slice 0 package that builds the skeleton every later package stands on: the `runic_add_module` helper, presets, CI, the test framework, the automated checks, the submodule pins, the CPU baseline, and libgodot built from source in the editor and template targets. It is the canonical contract for all three repositories; RunicPlatform and RunicGame each keep a short `cmake/README.md` with their part. The slice 0 spike that runs on it (GDT-1) is reported in RunicDocs › Architecture › Godot integration (Engine) › Slice 0 spike results (GDT-1). The architecture itself is only in RunicDocs.

| Field | Value |
| --- | --- |
| Package | BUILD-1 (slice 0), Stage A · [RunicEngine#6](https://github.com/PedroVicente98/RunicEngine/issues/6) |
| Repositories | RunicEngine (this file, the helpers and checks), RunicPlatform, RunicGame |
| Needs | — |
| Status | **Done** in slice 0 and committed on `main` in all three repositories. Every acceptance test below passes locally; CI has not run on GitHub yet |
| Source | ARCH v4 (RunicDocs: RunicEngine — Technical Architecture) § Modules, work packages and the implementation contract (Module contract, Automated checks, Work packages), § Repositories, build targets and processes, § Godot integration (Engine) › Building Godot, § Vertical-slice roadmap (slice 0); Appendix Security (CI hardening) |

## Done when

From the work package (v4): every repository configures and builds; libgodot builds in both targets; the server target links no graphics library and no Godot; a forbidden link fails CI. Security: CI fails when a workflow references an Action by tag.

Slice 0's runnable result (roadmap): the Engine library plus empty GameClient, GameServer and RunicEditor build in CI in all three repositories; libgodot builds in the editor and template targets; the client starts Godot headless and exits; the server links no graphics.

## Decisions in this contract

Approving this contract approves these choices. The v3 draft of this contract (bgfx, GLFW, ImGui, empty Editor and Cooker executables) was superseded by ARCH v4, where Godot is the presentation and the editor.

| # | Decision | Status |
| --- | --- | --- |
| 1 | C++ test framework: **GoogleTest** with GoogleMock, one version for all three repositories | Decided (Decisions log) |
| 2 | GoogleTest **v1.17.0**, fetched like the other third parties: pinned archive URL and SHA-256 into `external/googletest-1.17.0`, reused when present, fetched only when tests are enabled | Proposal |
| 3 | The helpers and checks are **CMake** code and CMake scripts (`cmake -P`). The only Python is the Godot build driver, because SCons is Python | Proposal |
| 4 | A module's include root is its `Public/` folder; headers are included by their path under it (for example `RenderData/RenderFrame.hpp`). BUILD-2 adds a check that no two modules of the three repositories export the same path | Decided by the owner, 2026-11-12 |
| 5 | The allowed-dependency rows of RunicDocs › Repositories, build targets and processes are written once more as a CMake **catalog** per repository, and `checks.catalog_sync` keeps the catalog equal to those tables, read from the RunicDocs checkout (`RUNIC_DOCS_DIR`, default `../RunicDocs`; the check is skipped when none is found) | Decided; source changed to RunicDocs on 2026-11-13 |
| 6 | CI is **GitHub Actions**. Required jobs run on Linux (Ubuntu 24.04, GCC and Clang); a Windows MSVC 2022 job is non-blocking until a module needs Windows | Proposal |
| 7 | CPU baseline option `RUNIC_CPU_BASELINE`, default `x86-64-v3` (`-march=x86-64-v3`, `/arch:AVX2`); `none` leaves the compiler's default target. Jolt's SIMD options follow it | Proposal (the baseline itself is decided) |
| 8 | Warnings: `-Wall -Wextra -Wpedantic` (or `/W4 /permissive-`) on every Runic target, **`-Werror=float-conversion`** always (the Decisions log type rule), all warnings as errors with `RUNIC_WARNINGS_AS_ERRORS=ON` in CI | Proposal |
| 9 | The pre-v3 scaffold (`include/RunicEngine/*`, `src/*`, `tests/.gitkeep`) is removed; every `Engine/<Group>/<Module>/` folder and its subsystem folders exist from the start with a README (decided by the owner on 2026-11-12); a module's first package adds its `CMakeLists.txt` and `ARCHITECTURE.md` | Done |
| 10 | **Godot 4.7.2-stable** and **godot-cpp 10.0.0-stable** pinned as shallow submodules (`external/godot`, `external/godot-cpp`); godot-cpp targets API 4.7 | Proposal |
| 11 | libgodot is built by `scripts/godot/build_libgodot.py` (SCons with `scripts/godot/runic_editor.py` and `runic_template.py`, the build profile `runic_client.gdbuild`, the SCons cache in CI) and **loaded at runtime** by the host, staged next to the executables. One client binary runs the editor library in development and the template library when shipping | Proposal |
| 12 | The LibGodot host (`GodotRuntime`) is GodotBridge's first code, written in slice 0 because the client and the RunicEditor spike both need it; GDB-1 builds the GodotLayer on it | Proposal |
| 13 | The RunicEditor executable lives in `Engine/Programs/RunicEditor/` (the Engine owns it); the GDT-1 test plugin lives in Runic::GodotTools | Proposal |
| 14 | The transitional bundle `Runic::Runtime` (Flecs, GLM, Jolt) stays until RunicGame links per-module targets (OQ-8, decided 2026-10-09); `RUNIC_ENGINE_BUNDLES=OFF` lets the Platform configure the helpers without it. `Runic::Presentation` (GLFW, bgfx, ImGui) is removed | Proposal |
| 15 | Third-party source folders carry their version (`external/flecs-4.0.4`, …), so changing a pin can never silently reuse an old tree | Done |
| 16 | Folder names: PascalCase for what the architecture defines (`Engine/<Group>/<Module>/` with `Public/`, `Source/`, `Tests/`; `Engine/Programs/` for the Engine's executables), lowercase for repository plumbing (`cmake/`, `docs/`, `external/`, `scripts/`, `tests/`, `build/`); never two folders that differ only by case (Windows and macOS would merge them) | Decided by the owner, 2026-11-09 |
| 17 | `external/` holds all third-party code: Godot and godot-cpp as the only submodules, every other library as a pinned, versioned download (ignored by git), plus the SCons environment. The disposable bundle check lives in `tests/sandbox/` and runs as the CTest test `build.bundle_links` | Decided by the owner, 2026-11-09 |

Not in BUILD-1: Jolt 5.6.0 in double precision (PHYS-1; the v5.6.0 archive's SHA-256 is `6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e`; the pin stays 5.2.0), any module code beyond the slice 0 spike, the API dump of the Runic Godot build for godot-cpp (`RUNIC_GODOT_API_FILE` is ready for it), the canonical Go module path (RunicDocs › Architecture › Open questions).

## Interface: CMake helpers

All helpers live in `cmake/` in this repository. A consumer gets them with `list(APPEND CMAKE_MODULE_PATH "${RUNIC_ENGINE_DIR}/cmake")` and `include(RunicModule)`; RunicPlatform and RunicGame add the Engine with `add_subdirectory` first.

| File | Provides |
| --- | --- |
| `cmake/RunicModule.cmake` | `runic_catalog_module`, `runic_catalog_interface`, `runic_third_party`, `runic_add_module`, `runic_add_interface_target`, `runic_add_group`; includes the three files below |
| `cmake/RunicChecks.cmake` | `runic_mark_presentation`, `runic_check_headless`, the deferred end-of-configure link checks |
| `cmake/RunicTesting.cmake` | `runic_enable_tests` (GoogleTest pin, CTest discovery) |
| `cmake/RunicToolchain.cmake` | `runic_apply_toolchain`, `runic_apply_cpu_baseline`, `runic_enable_warnings`, `runic_fatal` |
| `cmake/RunicModuleCatalog.cmake` | The Engine catalog: one `runic_catalog_module` call per row of RunicDocs › Repositories, build targets and processes › Engine modules (26 rows) |
| `cmake/RunicGodot.cmake` | `runic_godot_setup` (godot-cpp, the `Runic::LibGodotEditor` and `Runic::LibGodotTemplate` targets that build and stage libgodot), `runic_godot_host_executable` |
| `cmake/Dependencies.cmake` | The bundle's pinned third parties (Flecs 4.0.4, GLM 1.0.1, Jolt 5.2.0) and their library IDs |
| `cmake/checks/PublicHeaderScan.cmake` | Script: library types in `Public/`; Godot outside GodotBridge and GodotTools |
| `cmake/checks/SourceIncludeScan.cmake` | Script: includes of another module's `Source/` |
| `cmake/checks/CatalogSync.cmake` | Script: the catalog equals the module tables of RunicDocs › Repositories, build targets and processes |
| `cmake/checks/DocsCheck.cmake`, `DocsRules.cmake` | Script: a module code change without its `ARCHITECTURE.md` |
| `cmake/checks/WorkflowPinCheck.cmake` | Script: an Action referenced by tag or branch instead of a full commit SHA |

Every diagnostic is one line that starts with `RUNIC:` and names the module, the offending target and the catalog row. The lines are printed verbatim (`message(NOTICE)`) before configure stops, because `message(FATAL_ERROR)` re-wraps text at 80 columns.

### runic_catalog_module

```cmake
runic_catalog_module(<Name>
    GROUP <Group>                          # Foundation, SimulationCore, …; Platform and Game use their own labels
    [ALLOWS <target>...]                   # module targets it may link (the "Depends on" column)
    [THIRD_PARTY <id-or-target>...]        # third parties it may link, PRIVATE unless listed in PUBLIC_EXCEPTIONS
    [PUBLIC_EXCEPTIONS <id>...]            # approved Public/ exceptions: glm, behaviortree-nodes, godot-cpp
    [ALLOWS_ANY_MODULE]                    # GodotTools only ("Most modules" in 03)
    HEADLESS <yes|no|tool|never-shipped>)  # the Headless column
```

Declares what a module may link. A module that is not in the catalog cannot be created. `ALLOWS` is a ceiling: a module links only what its packages use. A target name in `ALLOWS` uses the full alias (`Runic::Core`, `RunicPlatform::Contracts`).

```cmake
runic_catalog_interface(<Name> FROM <Module>)   # a header-only interface target; it may link Runic::Core only
runic_third_party(<id> TARGETS <target>... [PRESENTATION] [INFRASTRUCTURE])
```

`runic_third_party` maps a library ID to its real targets (Dependencies.cmake: `flecs`, `glm`, `jolt`; RunicGodot.cmake: `godot-cpp`, `libgodot`, `dl`). `PRESENTATION` marks the targets for the headless check; `INFRASTRUCTURE` (gRPC, NATS, Agones, database clients) makes them forbidden to every Engine target. A linked target that is neither a module, an interface target, nor a registered or listed third party fails configure.

### runic_add_module

```cmake
runic_add_module(<Name> GROUP <Group> [PUBLIC_DEPS <target>...] [PRIVATE_DEPS <target>...] [HEADLESS])
```

Called once in `<Group>/<Name>/CMakeLists.txt` (in the Engine: `Engine/<Group>/<Name>/CMakeLists.txt`).

| Behavior | Rule |
| --- | --- |
| Target and alias | Target `<namespace>_<name>` in lower case, alias `<Namespace>::<Name>`; the namespace comes from `RUNIC_MODULE_NAMESPACE` in the calling directory (`Runic`, `RunicPlatform`, `RunicGame`) |
| Sources | `Source/**/*.cpp` → a static library; no source files → an INTERFACE library (its dependencies are all interface) |
| Include root | `Public/` is the only include directory exported; `Source/` is added PRIVATE and never exported |
| Language | C++20 without extensions, the warnings and the CPU baseline (`runic_apply_toolchain`) |
| Tests | When tests are enabled, `Tests/**/*.cpp` → one executable `<Namespace><Name>Tests` (namespaced, so `Runic::Client` and `RunicGame::Client` never collide) linking the module, `GTest::gtest_main` and `GTest::gmock`, registered with `gtest_discover_tests` and the CTest label `module` |
| Catalog | Configure fails if `<Name>` is not in the catalog, if `GROUP` or `HEADLESS` disagree with it, or if a module dependency is not in `ALLOWS` |
| PUBLIC third parties | Configure fails if a third-party target is in `PUBLIC_DEPS` without a matching `PUBLIC_EXCEPTIONS` entry (`glm` → GLM, `behaviortree-nodes` → BehaviorTree.CPP, `godot-cpp` → godot-cpp). Flecs has no exception since 2026-11-12: the Flecs world is reachable only through the opt-in `Runic::ECSNative` target, whose helper support ECS-1 adds |
| Headless | `HEADLESS` registers the module for the headless check; a module whose row is `no`, `tool` or `never-shipped` is marked presentation automatically |
| Directions | Configure fails on an Engine target linking any `RunicPlatform::` or `RunicGame::` target, a Platform target linking `RunicGame::`, or an Engine target linking an infrastructure library |

### runic_add_interface_target

```cmake
runic_add_interface_target(<Name> FROM <Module> HEADERS <dir>)
```

The one header-only interface target a module may export for an interface the Platform implements (`Runic::NetworkTransport` from Network, `Runic::SimulationPort` from Simulation). An INTERFACE library with `<dir>` as include root, linking only `Runic::Core`; configure fails at the end of configure if anything else was linked to it.

### runic_add_group

```cmake
runic_add_group(<Group> MODULES <target>...)
```

The umbrella INTERFACE target of a group (`Runic::GameplayAPI`), linking only its modules.

### Presentation marking and the headless check

```cmake
runic_mark_presentation(<target>...)   # targets a headless target must never reach
runic_check_headless(<target>...)      # executables and libraries that must stay headless
```

- Marked presentation: godot-cpp, the libgodot targets, and every module whose row is not `yes` (GodotBridge, GodotTools, Client, Animation, Audio, Cooker). Each later package that brings a presentation library (an audio backend) marks it in the same change.
- At the end of configure (a deferred call in the top-level directory) every target registered with `runic_check_headless` and every `HEADLESS` module has its full link closure walked (`LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES`, through `$<LINK_ONLY:…>`). Reaching a marked target fails configure with the chain, for example `RUNIC: headless target RunicGameServer reaches presentation target godot::cpp through Runic::GodotBridge`.
- When every deferred check passes, configure prints `RUNIC: link checks passed (<n> targets)`.
- RunicGame registers `RunicGameServer`; with the `server-only` preset, which never configures Godot, this is the headless build of GameServer that ARCH's Automated checks require. libgodot is loaded at runtime, so the link closure is what counts: a server that links GodotBridge, godot-cpp or a libgodot target fails.

### runic_enable_tests

```cmake
runic_enable_tests()   # once per top-level project, when its *_BUILD_TESTS option is ON (a macro)
```

Fetches GoogleTest (decision 2), sets `INSTALL_GTEST OFF` and `gtest_force_shared_crt ON`, calls `enable_testing()` in the caller's directory and `include(GoogleTest)`.

## Godot (this repository)

| Item | Rule |
| --- | --- |
| Pins | `external/godot` at `4.7.2-stable` (`ed1daf0b`), `external/godot-cpp` at `10.0.0-stable` (`507ed9d8`); shallow submodules. Fetch with `git submodule update --init --depth 1 external/godot external/godot-cpp` |
| Module lists | `scripts/godot/runic_editor.py` (editor: freetype, regex, svg, text_server_adv, msdfgen, glslang, raycast, lightmapper_rd, xatlas_unwrap, meshoptimizer, gltf, fbx, image importers and texture compressors, ogg, vorbis, mp3, noise, visual_shader, zip, mbedtls) and `scripts/godot/runic_template.py` (client: text, rendering, the .pck texture and audio decoders; physics, navigation and XR disabled), from RunicDocs › Building Godot. GDScript, C#, every physics and navigation module, multiplayer, WebRTC, WebSocket and XR are off in both. Review both with `scripts/godot/list_modules.py` at every Godot update |
| Build profile | `scripts/godot/runic_client.gdbuild` (template target only) |
| Build | `scripts/godot/build_libgodot.py` runs SCons (pinned SCons 4.10.1 in `external/scons-venv`, created with a hash-checked wheel) with clang and lld when available; `RUNIC_GODOT_SCONS_CACHE` keeps the SCons cache (CI). First build on an 8-job desktop: about 5 minutes (editor) and 3 minutes (template) |
| Staging | `Runic::LibGodotEditor` and `Runic::LibGodotTemplate` stage `libgodot.linuxbsd.editor.x86_64.so` and `libgodot.linuxbsd.template_release.x86_64.so` in `<build>/bin`, next to the executables; `GodotRuntime::DefaultLibraryPath` loads them from there |
| godot-cpp | Built from the submodule for `RUNIC_GODOT_CPP_TARGET` (`editor` by default, `template_release` for shipping builds), API 4.7, with exceptions on and a shared libstdc++ so it never changes how Runic code compiles |

| Option | Default | Meaning |
| --- | --- | --- |
| `RUNIC_GODOT_BUILD_LIBRARIES` | `ON` | Build libgodot when a target links a LibGodot target; CI build jobs turn it off |
| `RUNIC_GODOT_PREBUILT_DIR` | empty | Copy libgodot from this folder instead of building it |
| `RUNIC_GODOT_TEMPLATE_TARGET` | `template_release` | The template target `Runic::LibGodotTemplate` stages |
| `RUNIC_GODOT_COMPILER` | `auto` | `llvm` (clang, lld), `gcc`, or `auto` |
| `RUNIC_GODOT_JOBS` | half the cores | SCons jobs (Godot's build is memory hungry) |
| `RUNIC_GODOT_API_VERSION`, `RUNIC_GODOT_API_FILE` | `4.7`, empty | The API godot-cpp targets; a dumped `extension_api.json` overrides the version |

## Scans (CMake scripts, run by CTest with the label `checks` and by CI)

| Script | Input | Fails when |
| --- | --- | --- |
| `PublicHeaderScan.cmake` | `-DROOT=<repo> -DCATALOG=<file>` | A header under a module's `Public/` includes or names a library (Flecs, GLM, Jolt, godot-cpp, BehaviorTree.CPP, Detour/Recast, GameNetworkingSockets, gRPC, NATS, Agones, Protobuf, Prometheus, Zstandard, meshoptimizer, and the v3 libraries bgfx, RmlUi, ImGui, GLFW) without the module's approved exception; or any file of a module uses Godot unless its row lists godot-cpp or libgodot, or allows Runic::GodotTools |
| `SourceIncludeScan.cmake` | `-DROOT=<repo>` | Any file includes a path inside another module's `Source/` |
| `CatalogSync.cmake` | `-DCATALOG=<file> -DDOCS=<03 file> -DTABLE=<engine\|platform\|game>` | A module, its group, an allowed module dependency or its Headless value differs between the catalog and the table in 03 |
| `DocsCheck.cmake` | `-DCHANGED=<file> -DROOT=<repo> [-DRULES=<file>]` | A code change under a module folder has no change to that module's `ARCHITECTURE.md` (its contract and package status; a README-only change needs none) |
| `WorkflowPinCheck.cmake` | `-DROOT=<repo>` | A `uses:` in `.github/workflows/` is not `owner/repo@<40-hex SHA>` (local actions and digest-pinned images are allowed) |

The scans skip `tests/build/fixtures/` (whose trees break the rules on purpose), `external/`, build folders and `.godot/`. A failing check is fixed by changing the code, never by widening the catalog; widening a row is an owner decision recorded in the architecture first.

## Options and presets (this repository)

| Option | Default | Meaning |
| --- | --- | --- |
| `RUNIC_ENGINE_BUILD_TESTS` | top-level project | `runic_enable_tests`, module tests, `tests/build/` and the Godot spike tests |
| `RUNIC_ENGINE_BUNDLES` | `ON` | The `Runic::Runtime` bundle and its third parties. RunicPlatform sets it `OFF` |
| `RUNIC_ENGINE_ENABLE_PRESENTATION` | top-level project | Godot: godot-cpp, the libgodot targets, Runic::GodotBridge |
| `RUNIC_ENGINE_BUILD_TOOLS` | top-level project | The RunicEditor and Runic::GodotTools; requires presentation |
| `RUNIC_BUILD_SANDBOX` | top-level project | The disposable check that the bundle compiles and links |
| `RUNIC_CPU_BASELINE` | `x86-64-v3` | Decision 7 |
| `RUNIC_WARNINGS_AS_ERRORS` | `OFF` | CI sets `ON` |

| Configure preset | For |
| --- | --- |
| `debug`, `release` | Everything: helpers, Godot, GodotBridge, GodotTools, the RunicEditor, the sandbox, the tests |
| `headless` | Presentation, tools and sandbox off: the Engine configures and builds with no Godot and no graphics library |
| `ci-gcc`, `ci-clang` | `debug` with the compiler fixed, warnings as errors, libgodot not built (its runtime tests skip) |
| `ci-godot` | `debug` with the SCons cache: builds libgodot and runs the Godot tests |

Build presets match; `libgodot` builds only the two libraries. Test presets: `debug` (all), `godot`, `checks`, `headless`, `ci-gcc`, `ci-clang` (without the `godot` label), `ci-godot`.

## CI (this repository)

`.github/workflows/ci.yml`, on pushes to main and on pull requests. Every Action is pinned to a full commit SHA, the default token is read-only, credentials are not persisted, and no job uses a secret, so fork pull requests get nothing to leak. `.github/dependabot.yml` opens pull requests for Action updates; nothing merges automatically.

| Job | Runs | Required |
| --- | --- | --- |
| `build-gcc`, `build-clang` | godot-cpp submodule; `ci-gcc` / `ci-clang`: configure, build, `ctest` | Yes |
| `headless` | `headless` preset: configure (the headless check runs), build, `ctest -L checks` | Yes |
| `godot` | Both Godot submodules, the SCons cache; `ci-godot`: libgodot in both targets, the RunicEditor, `ctest` (GDT-1 spike, GodotRuntime) | Yes |
| `docs` | `DocsCheck` on the pull request's changed files | Yes (pull requests only) |
| `build-msvc` | Headless configure, build and tests on Windows with MSVC 2022 | No (decision 6) |

## RunicPlatform and RunicGame

Their parts are in their own `cmake/README.md`. In short:

- **RunicPlatform** adds the pinned Engine with bundles, tests, tools and presentation off, so its standalone build configures the helpers and never Flecs, GLM, Jolt or Godot; adds its catalog; turns its smoke test into the GoogleTest test `PlatformBootstrap.Version`; adds C++ and Go CI with govulncheck. Its Engine pin moves to the merge commit of this repository's BUILD-1.
- **RunicGame** adds its v4 catalog (Shared, Server, Client, Editor), registers `RunicGameServer` with `runic_check_headless`, builds the slice 0 GameClient on GodotBridge, is a Godot project (`project.godot`, `Content/UI/Boot.tscn`), and runs CI from its pinned submodules. Its pins move to the BUILD-1 merge commits of the Engine and the Platform.

Merge order: RunicEngine, then RunicPlatform, then RunicGame.

## Acceptance tests

`tests/build/` holds the build tests. Each one configures a small fixture project under `tests/build/fixtures/` against this repository's helpers (driver: `tests/build/RunFixture.cmake`) and expects success, a configure failure with a given `RUNIC:` message, or a build failure; the scan tests run a check script on a fixture tree (driver: `tests/build/RunScript.cmake`). They run on their own with `cmake -S tests/build -B build/build-tests -G Ninja && ctest --test-dir build/build-tests`.

| Test | Fixture | Passes when |
| --- | --- | --- |
| `build.module_layout` | `module_layout` | A module with `Public/`, `Source/` and `Tests/` builds, `Fixture::Alpha` exists, and its GoogleTest test `AlphaValue.Answer` is discovered and passes |
| `build.header_only_module` | `header_only_module` | A module with only `Public/` becomes an INTERFACE target that another module uses |
| `build.source_not_exported` | `source_not_exported` | A module that includes another module's `Source/` header fails to compile |
| `build.not_in_catalog` | `not_in_catalog` | Configure fails with `RUNIC:` naming the module missing from the catalog |
| `build.forbidden_link` | `forbidden_link` | A module linking a target outside its catalog row fails configure, naming both |
| `build.public_third_party` | `public_third_party` | A third party in `PUBLIC_DEPS` without a `PUBLIC_EXCEPTIONS` entry fails configure |
| `build.wrong_direction` | `wrong_direction` | A `Runic::` module linking a `RunicPlatform::` target fails configure, even when its row allows it |
| `build.interface_target` | `interface_target` | An interface target that links anything but `Runic::Core` fails configure |
| `build.headless_module` | `headless_module` | A `HEADLESS` module that reaches a marked presentation target through another target fails configure with the chain |
| `build.headless_executable` | `headless_executable` | `runic_check_headless` on an executable that reaches a marked target fails configure |
| `build.headless_executable_clean` | `headless_executable` | The same executable without the edge passes and prints `RUNIC: link checks passed` |
| `build.cpu_baseline_default` | `cpu_baseline` | With the default baseline, a source that requires `__AVX2__` and `__FMA__` compiles (x86-64 GCC and Clang) |
| `build.cpu_baseline_none` | `cpu_baseline` | With `RUNIC_CPU_BASELINE=none`, AVX2 is off |
| `build.engine_headless_configure` | this repository | The Engine configures with presentation, sandbox and tools off, the helpers run, and no godot-cpp build tree, Godot stamp or libgodot exists |
| `build.bundle_links` | `tests/sandbox` | `RunicSandbox` (Flecs, GLM, Jolt of the transitional bundle, with the CPU baseline) builds, links and runs |
| `checks.public_header_forbidden` | `scan_tree/public_forbidden` | The scan fails on `#include <flecs.h>` in a `Public/` header |
| `checks.public_header_exception` | `scan_tree/public_exception` | The scan passes on GLM in Core |
| `checks.public_header_godot_outside` | `scan_tree/godot_outside` | The scan fails on godot-cpp in Physics' `Source/` |
| `checks.public_header_godot_bridge` | `scan_tree/godot_bridge_public` | The scan fails on godot-cpp in GodotBridge's `Public/` (no exception) |
| `checks.public_header_godot_tools` | `scan_tree/godot_tools_public` | The scan passes on godot-cpp in GodotTools' `Public/` (the fourth exception) |
| `checks.source_include_scan` | `scan_tree/source_include` | The scan fails on an include of another module's `Source/` |
| `checks.docs_check_missing` | `scan_tree/docs_check` | A change list with module code and no contract update fails |
| `checks.docs_check_complete` | `scan_tree/docs_check` | The same change with the module's `ARCHITECTURE.md` passes |
| `checks.workflow_pin_tag` | `scan_tree/workflow_tag` | A workflow with `actions/checkout@v4` fails |
| `checks.workflow_pin_sha` | `scan_tree/workflow_sha` | A workflow pinned by SHA, with a local action, passes |
| `checks.repository_public_headers`, `checks.repository_source_includes`, `checks.repository_workflow_pins` | this repository | This repository passes every scan |
| `checks.catalog_sync` | this repository | `cmake/RunicModuleCatalog.cmake` equals the Engine modules table of RunicDocs (skipped when no RunicDocs checkout is found; CI checks it out) |

Godot tests (label `godot`, need libgodot built) and module tests:

| Test | Passes when |
| --- | --- |
| `godot.editor_spike` | `RunicEditor --headless --editor --path <spike project> --quit-after 120` exits 0, prints that the test plugin loaded, that GDScript is absent, that the plugin unloaded, and that the editor exited cleanly (GDT-1) |
| `godot.editor_project_manager` | `RunicEditor --headless --quit-after 30` (no project: the project manager) exits cleanly |
| `GodotRuntime.*` (8 tests, label `module`) | The LibGodot host's unit tests, `StartsGodotHeadlessAndExits` (the editor library runs the spike project headless and quits; a second instance is refused) and `LoadsTheTemplateLibrary`; the last two skip when libgodot is not built |

Result on the reference machine (Linux, GCC 16, Clang 23): `debug` 39 of 39, `headless` 28 of 28; before the folder moves, `ci-gcc` and `ci-clang` built with no warnings under `-Werror`.

## After slice 0

The Platform's pin of the Engine (`fb84529`) and the Game's pins of the Engine (`fb84529`) and the Platform (`ce8475a`)
are older than BUILD-1: a build from those submodules fails, and local builds work only through the sibling checkouts.
Move them to the commits that hold slice 0 once it is on `main`.

## BUILD-2 (approved, not built)

Build plumbing the owner approved on 2026-11-12, built before CORE-1 so every module package writes only inside its
own folder:

- **Unique public headers.** A check fails when two modules of the three repositories export the same header path,
  because with every `Public/` folder on the include path the first one found would win silently.
- **Modules from the catalog.** `Engine/CMakeLists.txt` adds each catalogued module whose folder has a
  `CMakeLists.txt` (folders already exist with only READMEs), so no package edits that shared file.
- **Third parties by need.** Each library is fetched once into its versioned `external/` folder and declared as one
  target when any configured module lists it, instead of only with the `Runic::Runtime` bundle (GLM first: the Platform
  turns bundles off but will link `Runic::Core`).
