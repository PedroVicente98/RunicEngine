# RunicEngine — current state

Snapshot taken when the architecture docs were imported (October 2026, commit
`c2657ee` "Refactor engine structure and dependencies"), updated on 2026-10-07
after the RunicFabric → RunicPlatform rename. Update this file whenever a module
or migration step lands.

## Summary

**Phase 0 (bootstrap), partially done.** The build skeleton and pinned
third-party dependencies exist. No catalog module is implemented. The folder
scaffold predates Appendix J and must be migrated to the target layout.

## What exists

| Item | Details |
| --- | --- |
| Build | CMake ≥ 3.24, `project(RunicEngine LANGUAGES C CXX)`, Ninja presets `debug`/`release`, `compile_commands.json` exported. |
| Options | `RUNIC_BUILD_SANDBOX` (default = top-level project), `RUNIC_ENGINE_ENABLE_PRESENTATION` (default = sandbox). Enabling the sandbox without presentation is a configure error. |
| `cmake/Dependencies.cmake` | `runic_engine_dependencies()`: FetchContent with pinned URLs + SHA-256 into `ThirdParty/` (reused across builds). Headless: Flecs 4.0.4, GLM 1.0.1, Jolt 5.2.0 (no SIMD extensions, no debug renderer/profiler). Presentation: GLFW 3.4 (X11), bx/bimg/bgfx + bgfx.cmake `0fb9ec06…`, ImGui 1.91.8 core compiled as `runic_imgui`. |
| `cmake/Targets.cmake` | `Runic::Core` — INTERFACE, C++20, include root `include/`. `Runic::Runtime` — INTERFACE, links Core + `flecs::flecs_static` + `glm::glm` + `Jolt`. |
| Presentation bundle | `Runic::Presentation` — INTERFACE, links `glfw`, `bgfx`, `runic_imgui`, defines `GLFW_INCLUDE_NONE`. Only configured when presentation is enabled. |
| `cmake/CompilerWarnings.cmake` | `runic_enable_warnings(target)`. |
| `apps/Sandbox/Main.cpp` | Disposable dependency check: Flecs entity, GLM dot, Jolt allocator, GLFW/bgfx/ImGui version checks; prints `RunicSandbox ready.` No window, renderer, or simulation. |
| Scaffold folders | Empty (`.gitkeep`): `include/RunicEngine/{Core,Debug,Input,Physics,Platform,Renderer,Simulation,UI}/`, `src/` with the same names, `tests/`. |
| Editor setup | `.vscode/launch.json` + `tasks.json` (configure/build Debug/Release), `.clangd`, `.clang-format`, `Runic.code-workspace` (RunicEngine, RunicPlatform, RunicGame sibling folders). |
| License | MIT. |

## Gaps against the target architecture

| Area | Today | Target | Reference |
| --- | --- | --- | --- |
| Directory layout | `include/RunicEngine/<M>/` + `src/<M>/` | `Engine/<Module>/{Public,Source,Tests}` + `ARCHITECTURE.md` + `CMakeLists.txt` | [J.2.2, J.3], [OQ-8](system/11-open-questions.md#oq-8) |
| Include style | README promises `<RunicEngine/Module/Header.hpp>` | Flat `Public/` with explicit names (`PhysicsWorld.hpp`) | [J.1.2] |
| Targets | Aggregate bundles `Runic::Runtime`, `Runic::Presentation` | One target per module; backends linked `PRIVATE` by their owner | [J.3.2] |
| Scaffold `Platform/` | Empty folder | No Engine module; OS/window adapter under Application (proposal) | [OQ-4](system/11-open-questions.md#oq-4) |
| Scaffold `Input/` | Empty folder | `AppEvent` (Application) + `InputMapping` (Client) | [OQ-4](system/11-open-questions.md#oq-4) |
| Scaffold `Debug/` | Empty folder | `DebugRenderer` (Renderer), `PhysicsDebug` (Physics), inspectors (Editor) | [OQ-5](system/11-open-questions.md#oq-5) |
| Modules | 0 of 25 | See [modules/README.md](modules/README.md) | [J.4] |
| Executables | `RunicSandbox` only | Headless host, client shell, editor (phase 0), `RunicCooker` (phase 3) | [J.12] |
| Tests | `tests/` empty; no framework | Module-local `Tests/` + CTest; framework to choose | [OQ-7](system/11-open-questions.md#oq-7) |
| Headless check | Manual (presentation option) | Automated headless dependency test | [J.14.6] |
| Missing third parties | — | RmlUi, BehaviorTree.CPP, Recast/Detour, fastgltf, meshoptimizer, MikkTSpace, KTX, Zstd, audio backend | [J.4] |

## Consumers that depend on today's targets

RunicGame links `Runic::Runtime` (server and client) and `Runic::Presentation`
(client). Keep both aggregate targets working until RunicGame links per-module
targets. RunicGame's `debug` and `server-only` presets build against these targets
and pass their smoke tests (verified on 2026-10-07); the server links no
presentation library.

## Suggested next steps to finish Phase 0 (proposal — confirm with the owner)

1. Decide [OQ-4](system/11-open-questions.md#oq-4), [OQ-5](system/11-open-questions.md#oq-5),
   [OQ-7](system/11-open-questions.md#oq-7), [OQ-8](system/11-open-questions.md#oq-8).
2. Add `Engine/` with a CMake helper for module targets (Public/Source/Tests,
   alias, warnings, test registration).
3. Implement **Core** (Stage A) as a real target; keep `Runic::Core` alias
   semantics compatible or migrate RunicGame in the same change.
4. Implement **Application** (Stage A) with a headless host.
5. Add headless host, client shell, and editor shell executables; retire or
   reduce `RunicSandbox`.
6. Add the automated headless dependency check (CI or CTest).
7. Remove the empty scaffold folders that no longer map to a module and update
   `README.md` to the new include/layout conventions.
