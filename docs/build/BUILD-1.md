# BUILD-1 — Build skeleton (contract)

This file is the contract for BUILD-1, the slice 0 package that builds the skeleton every later package stands on: the `runic_add_module` helper, presets, CI, the test framework, the automated checks, the submodule pins and the CPU baseline. It is the canonical contract for all three repositories; RunicPlatform and RunicGame each keep a short `docs/build/BUILD-1.md` with their part.

| Field | Value |
| --- | --- |
| Package | BUILD-1 (slice 0), Stage A |
| Repositories | RunicEngine (this file, the helpers and checks), RunicPlatform, RunicGame |
| Needs | — |
| Status | **Contract — awaiting owner review.** The acceptance tests in `tests/build/` exist and fail; no helper or CI code exists yet |
| Source | ARCH § Modules, work packages and the implementation contract (Module contract, Automated checks, Work packages); [docs/system/02](../system/02-module-contract.md), [03](../system/03-dependency-rules.md), [10](../system/10-ai-development-workflow.md), [12](../system/12-work-packages.md) |

## Done when

From [12-work-packages.md](../system/12-work-packages.md): every repository configures and builds; the server target links no graphics library; a forbidden link fails CI.

Slice 0's runnable result ([08](../system/08-implementation-roadmap.md)): the Engine library plus empty GameClient, GameServer, Editor and Cooker build in CI in all three repositories, and the server links no graphics.

The acceptance tests below turn that into checks that fail today and pass when BUILD-1 is implemented.

## Decisions in this contract

Approving this contract approves these choices. Items marked **Proposal** are the ones to look at first.

| # | Decision | Status |
| --- | --- | --- |
| 1 | C++ test framework: **GoogleTest** with GoogleMock, one version for all three repositories | Decided by the owner, 2026-10-13 ([OQ-7](../system/11-open-questions.md#oq-7)) |
| 2 | GoogleTest **v1.17.0**, fetched like the other third parties: pinned archive URL and SHA-256 into `ThirdParty/googletest`, reused when present. Fetched only when tests are enabled | Proposal |
| 3 | The helpers and checks are **CMake** code and CMake scripts (`cmake -P`), so no other tool (Python, shell) is needed on any developer machine. Platform and Game reuse the Engine's helpers through the Engine they already add | Proposal |
| 4 | A module's include root is its `Public/` folder; headers are included by their path under it (`#include "PhysicsWorld.hpp"`, `#include "RenderData/RenderFrame.hpp"`). This is why public file names must be explicit | Proposal |
| 5 | The allowed-dependency rows of [03](../system/03-dependency-rules.md) are written once more as a CMake **catalog** per repository, and a test keeps the catalog equal to 03. The catalog, not the call site, decides what a module may link | Proposal |
| 6 | CI is **GitHub Actions**. Required jobs run on Linux with GCC 13 and Clang 18. A Windows MSVC 2022 job is added as non-blocking until a module needs Windows | Proposal |
| 7 | CPU baseline option `RUNIC_CPU_BASELINE`, default `x86-64-v3` (`-march=x86-64-v3` on GCC and Clang, `/arch:AVX2` on MSVC); `none` turns it off for old machines | Proposal (the baseline itself is decided) |
| 8 | Warnings: `-Wall -Wextra -Wpedantic` (or `/W4 /permissive-`) on every module target; CI turns warnings into errors with `RUNIC_WARNINGS_AS_ERRORS=ON` | Proposal |
| 9 | The pre-v3 scaffold (the empty `include/RunicEngine/*` and `src/*` folders and `tests/.gitkeep`) is removed; `Engine/<Group>/<Module>/` folders are created by each module's first package, never here | Follows [OQ-4](../system/11-open-questions.md#oq-4), [OQ-5](../system/11-open-questions.md#oq-5), [OQ-8](../system/11-open-questions.md#oq-8) |
| 10 | The empty Editor and Cooker executables live in `apps/Editor/` and `apps/Cooker/` (one `Main.cpp` each, returning 0). EDT-1 and COOK-1 replace them with the real modules | Proposal |
| 11 | The transitional bundles `Runic::Runtime` and `Runic::Presentation` stay ([OQ-8](../system/11-open-questions.md#oq-8)) and gain one switch, `RUNIC_ENGINE_BUNDLES`, so the Platform's standalone build configures the helpers without Flecs, GLM or Jolt | Proposal |

Not in BUILD-1: Jolt 5.6.0 in double precision (PHYS-1; the pin stays 5.2.0 and `docs/current-state.md` keeps it as a known gap), any module code (CORE-1 onward), the canonical Go module path ([OQ-9](../system/11-open-questions.md#oq-9), before PCT-1).

## Interface: CMake helpers

All helpers live in `cmake/` in this repository. A consumer gets them with `list(APPEND CMAKE_MODULE_PATH "${RUNIC_ENGINE_DIR}/cmake")` and `include(RunicModule)`; RunicPlatform and RunicGame get them automatically because they add the Engine with `add_subdirectory`.

| File | Provides |
| --- | --- |
| `cmake/RunicModule.cmake` | `runic_catalog_module`, `runic_catalog_interface`, `runic_third_party`, `runic_add_module`, `runic_add_interface_target`, `runic_add_group`; includes the three files below |
| `cmake/RunicChecks.cmake` | `runic_mark_presentation`, `runic_check_headless`, the deferred end-of-configure link checks |
| `cmake/RunicTesting.cmake` | `runic_enable_tests` (GoogleTest pin, CTest discovery) |
| `cmake/RunicToolchain.cmake` | `runic_apply_cpu_baseline`, warnings (absorbs today's `CompilerWarnings.cmake`) |
| `cmake/RunicModuleCatalog.cmake` | The Engine catalog: one `runic_catalog_module` call per row of [03 § Engine modules](../system/03-dependency-rules.md#engine-modules) |
| `cmake/checks/PublicHeaderScan.cmake` | Script: library types and includes in `Public/` |
| `cmake/checks/SourceIncludeScan.cmake` | Script: includes of another module's `Source/` |
| `cmake/checks/CatalogSync.cmake` | Script: the catalog equals the tables in `docs/system/03-dependency-rules.md` |
| `cmake/checks/DocsCheck.cmake` | Script: a code change without its contract or status update |

### runic_catalog_module

```cmake
runic_catalog_module(<Name>
    GROUP <Group>                          # Foundation, SimulationCore, …; Platform and Game use their own labels
    [ALLOWS <target>...]                   # module targets it may link (the "Depends on" column)
    [THIRD_PARTY <id-or-target>...]        # third parties it may link, PRIVATE unless listed in PUBLIC_EXCEPTIONS
    [PUBLIC_EXCEPTIONS <id>...]            # approved Public/ exceptions: glm, flecs-world, behaviortree-nodes
    [ALLOWS_ANY_MODULE]                    # Editor only ("Most modules" in 03)
    HEADLESS <yes|no|tool|never-shipped>)  # the Headless column; "yes (Window excluded)" is written yes
```

Declares what a module may link. A module that is not in the catalog cannot be created ([02 § Checklist](../system/02-module-contract.md#checklist-creating-a-module)). `ALLOWS` is a ceiling: a module links only what its packages use. A target name in `ALLOWS` uses the full alias (`Runic::Core`, `RunicPlatform::Contracts`).

The Engine catalog is written in this contract ([cmake/RunicModuleCatalog.cmake](../../cmake/RunicModuleCatalog.cmake)) so its rows can be reviewed with it. Third parties in it are library IDs taken from the parentheses in 03 and the module contracts (`jolt`, `detour`, `behaviortree`, `bgfx`, `rmlui`, `imgui`, …). The Flecs world-handle exception is listed for ECS and AI, the two modules 03 names; another module that registers systems gets it from its own package, with the owner's approval.

```cmake
runic_catalog_interface(<Name> FROM <Module>)   # a header-only interface target; it may link Runic::Core only
runic_third_party(<id> TARGETS <target>... [PRESENTATION] [INFRASTRUCTURE])
```

`runic_third_party` is called by `Dependencies.cmake` (or the package that adds a library) and maps a library ID to its real targets. `PRESENTATION` marks the targets for the headless check; `INFRASTRUCTURE` (gRPC, NATS, Agones, database clients) makes them forbidden to every Engine target. A linked target that is neither a module, an interface target, nor a registered or listed third party fails configure.

### runic_add_module

```cmake
runic_add_module(<Name>
    GROUP <Group>
    [PUBLIC_DEPS <target>...]
    [PRIVATE_DEPS <target>...]
    [HEADLESS])
```

Called once in `<Group>/<Name>/CMakeLists.txt` (in the Engine: `Engine/<Group>/<Name>/CMakeLists.txt`).

| Behavior | Rule |
| --- | --- |
| Target and alias | Target `<prefix>_<name>` with alias `<Namespace>::<Name>`; the namespace comes from `RUNIC_MODULE_NAMESPACE` in the calling directory scope (`Runic`, `RunicPlatform`, `RunicGame`) |
| Sources | `Source/**/*.cpp` → a static library; no source files → an INTERFACE library |
| Include root | `Public/` is the only include directory exported (PUBLIC, or INTERFACE for header-only); `Source/` is added PRIVATE and never exported |
| Language | C++20, no compiler extensions, warnings from `RunicToolchain`, the CPU baseline |
| Tests | When tests are enabled, `Tests/**/*.cpp` → one executable `<Name>Tests` linking the module, `GTest::gtest_main` and `GTest::gmock`, registered with `gtest_discover_tests` and the CTest label `module` |
| Catalog | Configure fails if `<Name>` is not in the catalog, if `GROUP` or `HEADLESS` disagree with it, or if a dependency is not in `ALLOWS` or `THIRD_PARTY` |
| PUBLIC third parties | Configure fails if a third-party target is in `PUBLIC_DEPS` without a matching `PUBLIC_EXCEPTIONS` entry. The three exception IDs map to libraries: `glm` → GLM, `flecs-world` → Flecs, `behaviortree-nodes` → BehaviorTree.CPP. GLM in Core is the one PUBLIC third party today |
| Headless | `HEADLESS` registers the target for the headless check below |
| Directions | Configure fails on an Engine target linking any `RunicPlatform::` or `RunicGame::` target, a Platform target linking `RunicGame::`, or an Engine target linking an infrastructure library (gRPC, NATS, Agones, a database client) |
| Diagnostics | Every failure names the module, the offending target and the catalog row, and starts with `RUNIC:` |

### runic_add_interface_target

```cmake
runic_add_interface_target(<Name> FROM <Module> HEADERS <dir>)
```

The one header-only interface target a module may export for an interface the Platform implements (`Runic::NetworkTransport` from Network, `Runic::SimulationPort` from Simulation). It is an INTERFACE library with `<dir>` as include root, linking only `Runic::Core`; configure fails at the end of configure if anything else was linked to it. It needs a `runic_catalog_interface` row.

### runic_add_group

```cmake
runic_add_group(<Group> MODULES <target>...)
```

The umbrella INTERFACE target of a group (`Runic::GameplayAPI`), linking only its modules. It has no sources.

### Presentation marking and the headless check

```cmake
runic_mark_presentation(<target>...)   # third-party targets that a headless target must never reach
runic_check_headless(<target>...)      # executables and libraries that must stay headless
```

- The Engine marks GLFW, bgfx, bimg, bx, Dear ImGui and the `Runic::Presentation` bundle; each later package that brings a presentation library (RmlUi, an audio backend, imnodes) marks it in the same change. A module whose catalog row is `no`, `tool` or `never-shipped` is marked automatically.
- At the end of configure (a deferred call in the top-level directory), every target registered with `runic_check_headless` — and, in builds where presentation is off (`RUNIC_ENGINE_ENABLE_PRESENTATION` OFF or not defined), every module registered with `HEADLESS` — has its full link closure walked (`LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES`, through `$<LINK_ONLY:…>`). Reaching a marked target fails configure with the chain, for example `RUNIC: headless target RunicGameServer reaches presentation target glfw through Runic::Presentation`.
- When every deferred check passes, configure prints `RUNIC: link checks passed (<n> targets)`; the tests use this line to know the helpers ran.
- Module `HEADLESS` checks run only where presentation is off, because Application's Window class links GLFW in client and tools builds ("yes (Window excluded)" in 03). The `headless` presets and the Game's `server-only` presets are those builds, and their CI jobs are required.
- RunicGame registers `RunicGameServer`. Together with the `server-only` preset (which never configures presentation libraries), this is the headless build of GameServer that ARCH's Automated checks require.

### runic_enable_tests

```cmake
runic_enable_tests()   # once per top-level project, when its *_BUILD_TESTS option is ON
```

Fetches GoogleTest (decision 2), sets `INSTALL_GTEST OFF` and `gtest_force_shared_crt ON`, calls `enable_testing()` and `include(GoogleTest)`. GoogleTest is never fetched when tests are off, so consumers that only build never download it.

## Scans (CMake scripts, run by CTest with the label `checks` and by CI)

| Script | Input | Fails when |
| --- | --- | --- |
| `PublicHeaderScan.cmake` | `-DROOT=<repo> -DCATALOG=<file>` | A header under any `Public/` includes or names a library from the forbidden list (Flecs, Jolt, bgfx/bx/bimg, RmlUi, ImGui, BehaviorTree.CPP, Detour/Recast, GameNetworkingSockets, gRPC, NATS, Agones, Protobuf, Prometheus) — except a module's approved `PUBLIC_EXCEPTIONS` (glm in Core; the Flecs world handle; BehaviorTree.CPP node base types) |
| `SourceIncludeScan.cmake` | `-DROOT=<repo>` | Any file includes a path inside another module's `Source/` |
| `CatalogSync.cmake` | `-DCATALOG=<file> -DDOCS=<03 file> -DTABLE=<engine\|platform\|game>` | A module, its group, an allowed module dependency or its Headless value differs between the catalog and the table in 03 (third-party IDs are reviewed with the catalog, since 03 names only some of them) |
| `DocsCheck.cmake` | `-DCHANGED=<file with changed paths> -DROOT=<repo> [-DRULES=<file>]` | A change under a module folder has no change to that module's `ARCHITECTURE.md` (or `docs/modules/<Module>.md` before the folder exists), or no change to `docs/current-state.md` |

`CatalogSync` reads names the way 03 writes them: parentheses are notes and are dropped; a bare module name after a `Runic::` name is `Runic::<Name>` in the Engine and Game tables; "Contracts" in the Platform table is `RunicPlatform::Contracts`; "Platform server bindings and contracts" in the Game table is `RunicPlatform::ServerBindings` and `RunicPlatform::Contracts`; "Most modules" is `ALLOWS_ANY_MODULE`.

The scans skip `tests/build/fixtures/` (whose trees break the rules on purpose), `ThirdParty/`, `external/` and build folders. `DocsCheck` maps a changed path to its required contract with per-repository rules (`cmake/checks/DocsRules.cmake` in the Engine: `Engine/<Group>/<Module>/…` needs `Engine/<Group>/<Module>/ARCHITECTURE.md`, or `docs/modules/<Module>.md` before the folder exists); RunicPlatform and RunicGame pass their own with `-DRULES`. In CI, `DocsCheck` gets the pull request's changed files (`git diff --name-only origin/<base>...HEAD`). A failing check is fixed by changing the code, never by widening the catalog; widening a row is an owner decision recorded in ARCH first ([03 § How CI enforces the rules](../system/03-dependency-rules.md#how-ci-enforces-the-rules)).

## Options and presets (this repository)

| Option | Default | Meaning |
| --- | --- | --- |
| `RUNIC_ENGINE_BUILD_TESTS` | top-level project | Calls `runic_enable_tests`, adds module tests and `tests/build/` |
| `RUNIC_ENGINE_BUNDLES` | `ON` | Configures `Runic::Runtime`, `Runic::Presentation` and their third parties. RunicPlatform sets it `OFF` |
| `RUNIC_ENGINE_ENABLE_PRESENTATION` | as today | Configures GLFW, bgfx and ImGui (client and tools builds) |
| `RUNIC_ENGINE_BUILD_TOOLS` | top-level project | Builds the empty `RunicEditor` and `RunicCooker` (decision 10); requires presentation for the Editor |
| `RUNIC_BUILD_SANDBOX` | as today | The disposable dependency check |
| `RUNIC_CPU_BASELINE` | `x86-64-v3` | Decision 7 |
| `RUNIC_WARNINGS_AS_ERRORS` | `OFF` | CI sets `ON` |

| Configure preset | For |
| --- | --- |
| `debug`, `release` | Today's presets, plus tests and tools |
| `headless` | Presentation, sandbox and tools off: proves the Engine configures and builds with no graphics library |
| `ci-gcc`, `ci-clang` | `debug` with the compiler fixed, warnings as errors |

Each configure preset has a matching build and test preset.

## CI (this repository)

`.github/workflows/ci.yml`, on every push and pull request:

| Job | Runs | Required |
| --- | --- | --- |
| `build-gcc`, `build-clang` | `ci-gcc` / `ci-clang` preset: configure, build, `ctest` (module tests and `tests/build/`) | Yes |
| `headless` | `headless` preset: configure and build; the configure-time headless check runs | Yes |
| `checks` | `ctest -L checks`: public-header scan, source-include scan, catalog sync | Yes |
| `docs` | `DocsCheck` on the pull request's changed files | Yes (pull requests only) |
| `build-msvc` | `debug` on Windows with MSVC 2022 | No (decision 6) |

The X11 and OpenGL development packages are installed on Linux runners for the presentation jobs.

## RunicPlatform and RunicGame

Their parts are in their own `docs/build/BUILD-1.md`. In short:

- **RunicPlatform** adds the pinned Engine with `RUNIC_ENGINE_BUNDLES=OFF`, `RUNIC_ENGINE_BUILD_TESTS=OFF` and `RUNIC_ENGINE_BUILD_TOOLS=OFF`, so its standalone build configures the helpers and never Flecs, GLM, Jolt or presentation libraries; adds its catalog (Platform C++ modules may link only the Engine interface targets); turns its smoke test into a GoogleTest test; adds C++ and Go CI. Its Engine pin moves to the merge commit of this repository's BUILD-1.
- **RunicGame** adds its catalog (Shared, Server, Client per [03 § Game targets](../system/03-dependency-rules.md#game-targets)), registers `RunicGameServer` with `runic_check_headless`, and runs CI from its pinned submodules. Its pins move to the BUILD-1 merge commits of the Engine and the Platform.

Merge order: RunicEngine, then RunicPlatform, then RunicGame.

## Acceptance tests

`tests/build/` holds the tests. Each one configures a small fixture project under `tests/build/fixtures/` against this repository's helpers (driver: `tests/build/RunFixture.cmake`) and expects success, a configure failure with a given `RUNIC:` message, or a build failure; the scan tests run a check script on a fixture tree (driver: `tests/build/RunScript.cmake`). They are registered with CTest under the label `build` when `RUNIC_ENGINE_BUILD_TESTS` is ON. Today every one fails, because the helpers do not exist.

| Test | Fixture | Passes when |
| --- | --- | --- |
| `build.module_layout` | `module_layout` | A module with `Public/`, `Source/` and `Tests/` configures and builds, `Fixture::Alpha` exists, and its GoogleTest test `AlphaValue.Answer` is discovered by CTest and passes |
| `build.header_only_module` | `header_only_module` | A module with only `Public/` becomes an INTERFACE target that another module uses |
| `build.source_not_exported` | `source_not_exported` | A module that includes another module's `Source/` header by name fails to compile |
| `build.not_in_catalog` | `not_in_catalog` | Configure fails with `RUNIC:` naming the module missing from the catalog |
| `build.forbidden_link` | `forbidden_link` | A module linking a target outside its catalog row fails configure with `RUNIC:` naming both |
| `build.public_third_party` | `public_third_party` | A third party in `PUBLIC_DEPS` without a `PUBLIC_EXCEPTIONS` entry fails configure |
| `build.wrong_direction` | `wrong_direction` | A `Runic::` module linking a `RunicPlatform::` target fails configure, even when its row allows it |
| `build.interface_target` | `interface_target` | An interface target that links anything but `Runic::Core` fails configure |
| `build.headless_module` | `headless_module` | A `HEADLESS` module that reaches a marked presentation target through another target fails configure with the chain |
| `build.headless_executable` | `headless_executable` | `runic_check_headless` on an executable that reaches a marked target fails configure |
| `build.headless_executable_clean` | `headless_executable` | The same executable without the edge passes and prints `RUNIC: link checks passed` |
| `build.cpu_baseline_default` | `cpu_baseline` | With the default baseline, a source that requires `__AVX2__` and `__FMA__` compiles (x86-64 GCC and Clang only) |
| `build.cpu_baseline_none` | `cpu_baseline` | With `RUNIC_CPU_BASELINE=none`, AVX2 is off |
| `build.engine_headless_configure` | this repository | The Engine configures with presentation, sandbox and tools off, the helpers run, and no GLFW, bgfx or ImGui build folder exists |
| `checks.public_header_forbidden` | `scan_tree/public_forbidden` | The scan fails on `#include <flecs.h>` in a `Public/` header |
| `checks.public_header_exception` | `scan_tree/public_exception` | The scan passes on GLM in Core, an approved exception |
| `checks.source_include_scan` | `scan_tree/source_include` | The scan fails on an include of another module's `Source/` |
| `checks.docs_check_missing` | `scan_tree/docs_check` | A change list with module code and no contract update fails |
| `checks.docs_check_complete` | `scan_tree/docs_check` | The same change with the module's `ARCHITECTURE.md` and `docs/current-state.md` passes |
| `checks.catalog_sync` | this repository | `cmake/RunicModuleCatalog.cmake` equals the Engine modules table in `docs/system/03-dependency-rules.md` |

The tests run on their own without configuring any Engine third party (only `build.engine_headless_configure` downloads the Engine's pinned sources, and `build.module_layout` GoogleTest): `cmake -S tests/build -B build/build-tests -G Ninja && ctest --test-dir build/build-tests`.

CI jobs are part of the implementation: they are accepted when they run these tests and the jobs listed above on a pull request.

## After merge

Update in the same pull request: [docs/current-state.md](../current-state.md) (BUILD-1 done, the gaps it closes, the remaining Jolt gap), [docs/modules/README.md](../modules/README.md) (the build skeleton note), the README build section and this file's status.
