---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/current-state
---

# RunicEngine — current state

This file records what exists in this repository today, the gaps against the v3 architecture, who depends on today's targets, and the next work packages. Update it in the same pull request as any module or build change.

Snapshot date: **2026-10-13** (branch `docs/v3-regeneration`, docs regenerated from ARCH[^arch] revision 502: no spatial layers; sparse 3D cells, authored volumes, activation volumes and `WorldId`).

## Summary

The repository holds a bootstrap build skeleton with pinned third-party dependencies and a disposable dependency check. **No Engine module exists yet (0 of 27)**, and slice 0 (BUILD-1[^doc-build-build-1]) has not started. The folder scaffold predates v3 and moves to `Engine/<Group>/<Module>/` in BUILD-1.

## What exists

| Item | Details |
| --- | --- |
| Build | `CMakeLists.txt`: CMake ≥ 3.24, `project(RunicEngine LANGUAGES C CXX)`. `CMakePresets.json`: Ninja presets `debug` and `release`, `compile_commands.json` exported. No `enable_testing()`, no CTest, no CI. |
| Options | `RUNIC_BUILD_SANDBOX` (default: top-level project) and `RUNIC_ENGINE_ENABLE_PRESENTATION` (default: the sandbox value). Enabling the sandbox without presentation is a configure error. No CPU-baseline option. |
| `cmake/Dependencies.cmake` | `runic_engine_dependencies()`: FetchContent with pinned codeload URLs and SHA-256 hashes into `ThirdParty/`. A populated `ThirdParty/<folder>` is reused whenever it exists (the reuse logic keys on the folder name only). Headless: Flecs 4.0.4, GLM 1.0.1, Jolt 5.2.0. Presentation only: GLFW 3.4 (X11; Wayland opt-in), bx, bimg and bgfx at fixed revisions with bgfx.cmake `0fb9ec06…`, Dear ImGui 1.91.8 core compiled as `runic_imgui`. |
| Jolt configuration | v5.2.0, single precision (no `DOUBLE_PRECISION`), every SIMD extension off (SSE4.1, SSE4.2, AVX, AVX2, AVX-512, LZCNT, TZCNT, F16C, FMA), debug renderer and profiler off. |
| `cmake/Targets.cmake` | `Runic::Core`[^engine-core]: INTERFACE, C++20, include root `include/` (a bootstrap target, not the CORE-1 module). `Runic::Runtime`: INTERFACE, links `Runic::Core`, and `Dependencies.cmake` adds `flecs::flecs_static`, `glm::glm` and `Jolt`. |
| Presentation bundle | `Runic::Presentation`: INTERFACE, links `glfw`, `bgfx` and `runic_imgui`, defines `GLFW_INCLUDE_NONE`. Configured only when presentation is enabled. |
| `cmake/CompilerWarnings.cmake` | `runic_enable_warnings(target)`: `-Wall -Wextra -Wpedantic`, or `/W4 /permissive-` on MSVC. |
| Sandbox | `cmake/Sandbox.cmake` and `apps/Sandbox/Main.cpp`: `RunicSandbox` checks Flecs, GLM, Jolt, GLFW, bgfx and ImGui headers and linkage and prints `RunicSandbox ready.` No window, renderer or simulation. |
| Scaffold folders | Empty (`.gitkeep` only): `include/RunicEngine/{Core,Debug,Input,Physics,Platform,Renderer,Simulation,UI}/`, `src/` with the same names, `tests/`. |
| Editor[^engine-editor] setup | `.vscode/launch.json` and `tasks.json` (configure and build Debug or Release), `.clangd`, `.clang-format`, `Runic.code-workspace` (RunicEngine, RunicPlatform and RunicGame as sibling folders). |
| Docs | `CLAUDE.md`, `README.md`, `docs/` (this folder; `docs/system/` is mirrored from the coordinator), `ThirdParty/README.md`. |
| BUILD-1[^doc-build-build-1] contract (in review) | [build/BUILD-1.md](build/BUILD-1.md), the Engine catalog `cmake/RunicModuleCatalog.cmake`, and 20 acceptance tests in `tests/build/` (registered with CTest when `RUNIC_ENGINE_BUILD_TESTS` is ON, or run alone with `cmake -S tests/build`). Every test fails until BUILD-1 is implemented. |
| License | MIT. |

## Gaps against v3

| Area | Today | Target | Where it lands |
| --- | --- | --- | --- |
| Directory layout | `include/RunicEngine/<M>/` and `src/<M>/` scaffold | `Engine/<Group>/<Module>/{Public,Source,Tests}` plus `ARCHITECTURE.md` and `CMakeLists.txt` | BUILD-1[^doc-build-build-1] ([OQ-8](system/11-open-questions.md#oq-8)) |
| Scaffold folders with no module | `Platform/`, `Input/`, `Debug/` | No such Engine modules: Window class in Application, input mapping in Client, debug pipeline in Renderer[^engine-renderer], DebugGeometry in Core | BUILD-1 removes them ([OQ-4](system/11-open-questions.md#oq-4), [OQ-5](system/11-open-questions.md#oq-5)) |
| Modules | 0 of 27 | See [modules/README.md](modules/README.md) | One work package at a time |
| Module helper and checks | None | `runic_add_module`, automated headless and forbidden-dependency checks, public-header scan, CI | BUILD-1 |
| Test framework | `tests/` empty; no framework | Module-local `Tests/` registered with CTest; GoogleTest (with GoogleMock) in all three repositories | BUILD-1 ([OQ-7](system/11-open-questions.md#oq-7), decided 2026-10-13) |
| Targets | Bundles `Runic::Runtime` and `Runic::Presentation`; bootstrap `Runic::Core`[^engine-core] | One target per module; libraries private to their owner; the bundles stay until RunicGame links per-module targets | BUILD-1, then each module; CORE-1 makes `Runic::Core` a real module |
| Interface targets | None | `Runic::NetworkTransport`[^engine-network] (header-only, Core types only) and `Runic::SimulationPort`[^engine-simulation] (header-only, Core types only) | NET-1 (slice 12), SIM-5 (slice 27) |
| Jolt | 5.2.0, single precision, SIMD extensions off | Jolt 5.6.0 (tag v5.6.0) in double precision with its AVX2 path, on the x86-64-v3 baseline | BUILD-1 (CPU baseline) and PHYS-1[^engine-physics] (pin). This session could not download the v5.6.0 archive to compute its SHA-256, so `cmake/Dependencies.cmake` is unchanged. Because the reuse logic keys on `ThirdParty/JoltPhysics` existing, the upgrade must also make sure an old source tree cannot be reused silently |
| CPU baseline | Not set; Jolt built without SIMD extensions | x86-64-v3 (AVX2, FMA) for servers and clients | BUILD-1 |
| Missing third parties | — | RmlUi, BehaviorTree.CPP, Detour and DetourTileCache, Recast, fastgltf, meshoptimizer, MikkTSpace, xatlas, KTX-Software, Basis Universal, Zstandard, imnodes, an audio backend; ImGui backends | The package that first needs each one |
| Executables | `RunicSandbox` only | Empty Editor[^engine-editor] and Cooker[^engine-cooker] executables building in CI (slice 0), then the Editor (EDT-1, slice 6) and the Cooker (COOK-1, slice 9) | BUILD-1, EDT-1, COOK-1 |

## Consumers of today's targets

- **RunicGame** consumes this repository through `RUNIC_ENGINE_DIR` (sibling checkout) or, from BUILD-1[^doc-build-build-1], a submodule pin. It links `Runic::Runtime` for server and client and `Runic::Presentation` for the client. Keep both bundles working until RunicGame links per-module targets. The previous snapshot (2026-10-07) recorded RunicGame's `debug` and `server-only` presets building against these targets with no presentation library in the server; this change did not re-run them.
- **RunicPlatform** pins this repository as a submodule from BUILD-1 so its C++ bindings build and test alone. It will link only `Runic::Core`[^engine-core], `Runic::Application`[^engine-application], `Runic::NetworkTransport`[^engine-network] and `Runic::SimulationPort`[^engine-simulation]; none of the last three exists yet.

## Next steps

Slice 0 and slice 1, in order. Each row is one work package, one Claude Code session and one pull request; the contract (interface and tests) is approved before the code.

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| BUILD-1[^doc-build-build-1] | Build skeleton in all three repositories: `runic_add_module` helper, presets, CI, the chosen test framework, automated headless and forbidden-dependency checks, submodule pins (Game pins Engine and Platform; Platform pins Engine) with a local option for sibling checkouts, an AVX2 (x86-64-v3) CPU baseline | Every repository configures and builds; the server target links no graphics library; a forbidden link fails CI | — | 0 |
| CORE-1[^engine-core] | Plain ID types (GlobalEntityId, GlobalPlayerId, SimulationId, WorldId, TickId, AuthorityEpoch, and value types other modules give meaning to, such as AssetId, WaterBodyId, FieldId and WidgetId), Result, asserts, logging hooks, time types, GLM math | Unit tests cover IDs, Result and time conversions | BUILD-1 | 1 |
| APP-1[^engine-application] | Entry point, Application, ApplicationHost, Layer, LayerStack with queued changes at safe points, application events, loop policies | Layer order, events top-down until handled, updates bottom-up, safe-point push and pop, no allocation per event | CORE-1 | 1 |
| APP-2 | Window class on GLFW, compiled only in client and tools builds; keyboard, mouse and gamepad as application events | A window opens and closes; the headless build compiles without GLFW | APP-1 | 1 |

Slice 1 also needs GAME-0[^doc-game-packages] (defined in RunicGame). After slice 1, slice 2 (RND-1[^engine-renderer], RND-2, UI-1[^engine-ui]) and slice 3 (SIM-1[^engine-simulation], CORE-2) can start; see [system/08-implementation-roadmap.md](system/08-implementation-roadmap.md).

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../docs/README|Engine docs README]] · [[../docs/system/README|System README]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^doc-build-build-1]: [[../docs/build/BUILD-1|Engine BUILD-1 contract]] — RunicEngine doc
[^engine-core]: [[../docs/modules/Core|Core]] — Engine module · Foundation
[^engine-editor]: [[../docs/modules/Editor|Editor]] — Engine module · Tools
[^engine-renderer]: [[../docs/modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-network]: [[../docs/modules/Network|Network]] — Engine module · Networking
[^engine-simulation]: [[../docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-physics]: [[../docs/modules/Physics|Physics]] — Engine module · Simulation core
[^engine-cooker]: [[../docs/modules/Cooker|Cooker]] — Engine module · Tools
[^engine-application]: [[../docs/modules/Application|Application]] — Engine module · Foundation
[^doc-game-packages]: [[../../RunicGame/docs/game-packages|Game work packages]] — RunicGame work packages
[^engine-ui]: [[../docs/modules/UI|UI]] — Engine module · Presentation
