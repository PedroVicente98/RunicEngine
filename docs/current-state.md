---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/current-state
---

# RunicEngine — current state

This file records what exists in this repository today, the gaps against the architecture, who depends on today's targets, and the next work packages. Update it in the same pull request as any module or build change.

Snapshot date: **2026-11-08** (branch `slice-0/build-skeleton`). The architecture source of truth is ARCH[^arch] v4, kept in
RunicDocs (RunicEngine — Technical Architecture); this repository's `docs/system/03-dependency-rules.md`[^doc-system-03-dependency-rules] follows it.
The other files of `docs/system/` and `docs/modules/` are still written from v3 and are regenerated separately
(see Gaps).

## Summary

**Slice 0 (Build skeleton) is implemented and awaiting review**: BUILD-1[^doc-build-build-1] (helpers, catalog, checks, GoogleTest,
presets, CI, CPU baseline, Godot and godot-cpp pinned, libgodot built from source in the editor and template targets)
and GDT-1[^engine-godottools] (the RunicEditor on libgodot opens the editor with a test plugin, without GDScript, and exits cleanly).
Two modules have slice 0 code: Runic::GodotBridge[^engine-godotbridge] (the LibGodot host) and Runic::GodotTools (the test plugin).
24 of the 26 modules have no code yet.

## What exists

| Item | Details |
| --- | --- |
| Build | `CMakeLists.txt` (CMake ≥ 3.24), the helpers in `cmake/` ([build/BUILD-1.md](build/BUILD-1.md)), the v4 catalog `cmake/RunicModuleCatalog.cmake` (26 modules), presets `debug`, `release`, `headless`, `ci-gcc`, `ci-clang`, `ci-godot` and `libgodot` |
| Checks | Configure-time: catalog rows, dependency directions, PUBLIC third parties, interface targets, the headless check. Scripts: public-header scan (with the Godot rule), source-include scan, catalog sync, docs check, workflow pin check |
| Tests | GoogleTest 1.17.0 (pinned). `tests/build/`: 29 acceptance tests for the helpers and scans; `godot.*`: the GDT-1[^engine-godottools] spike tests; module tests in `Engine/**/Tests` |
| CI | `.github/workflows/ci.yml` (build-gcc, build-clang, headless, godot, docs, build-msvc non-blocking), every Action pinned by SHA, read-only token; `.github/dependabot.yml` for Actions. Not run on GitHub yet |
| Godot | Submodules `external/godot` (4.7.2-stable) and `external/godot-cpp` (10.0.0-stable), shallow. `scripts/godot/`: the editor and template module lists, the client build profile, the SCons driver `build_libgodot.py`, `list_modules.py` |
| Runic::GodotBridge[^engine-godotbridge] | `Engine/Presentation/GodotBridge/`: `GodotRuntime` (load libgodot, create the instance with the command line, start, iterate, destroy; class registration hooks by level). Contract: its `ARCHITECTURE.md` |
| Runic::GodotTools | `Engine/Tools/GodotTools/`: `RegisterEditorTools` and the GDT-1 test plugin. Contract: its `ARCHITECTURE.md` |
| RunicEditor | `Engine/Programs/RunicEditor/Main.cpp`: the Godot editor through LibGodot with the Runic tools; arguments reach Godot unchanged |
| Bundle | `Runic::Runtime` (transitional, OQ-8[^oq-8]): Flecs 4.0.4, GLM 1.0.1, Jolt 5.2.0 with SIMD from the CPU baseline; `RunicSandbox` checks that it compiles and links |
| Third parties | `external/`: the Godot and godot-cpp submodules, and (git-ignored) versioned source folders downloaded on configure with SHA-256 checks and `scons-venv/` with the pinned SCons |
| Editors | `.vscode/` tasks and launch configurations (build, tests, RunicEditor on the spike project, the project manager, the GDT-1 check), `Runic.code-workspace` (Engine, Platform, Game, Docs) |
| License | MIT |

## Gaps against the architecture

| Area | Today | Target | Where it lands |
| --- | --- | --- | --- |
| Docs | `docs/system/` except 03, and `docs/modules/`, describe v3 (bgfx Renderer[^engine-renderer], RmlUi and ImGui UI, the ImGui Editor[^engine-editor], a Window class in Application) | Regenerated from ARCH[^arch] v4: GodotBridge[^engine-godotbridge] and GodotTools[^engine-godottools] notes, no Renderer, UI or Editor modules | A docs package before CORE-1[^engine-core] |
| Jolt | 5.2.0, single precision | 5.6.0 in double precision with its AVX2 path | PHYS-1[^engine-physics] (the v5.6.0 hash is recorded in `cmake/Dependencies.cmake`) |
| Godot API per build | godot-cpp uses its bundled 4.7 API | The Runic build's own `extension_api.json`, so a removed class fails at compile time | A later BUILD or GDT package (`RUNIC_GODOT_API_FILE` is ready) |
| Modules | GodotBridge and GodotTools have slice 0 code; 24 modules none | See the catalog | One work package at a time |
| Interface targets | None | `Runic::NetworkTransport`[^engine-network], `Runic::SimulationPort`[^engine-simulation] | NET-1 (slice 12), SIM-5 (slice 27) |
| Missing third parties | — | BehaviorTree.CPP, Detour and DetourTileCache, Recast, meshoptimizer, Zstandard, an audio backend on Godot's AudioServer | The package that first needs each one |

## Consumers of today's targets

- **RunicGame** links `Runic::Runtime` in both executables, and `Runic::GodotBridge`[^engine-godotbridge] with `Runic::LibGodotEditor` and
  `Runic::LibGodotTemplate` in its client. Its server is registered with `runic_check_headless`.
- **RunicPlatform** adds this repository for its helpers only (bundles, tests, tools and Godot off). It will link
  only `Runic::Core`[^engine-core], `Runic::Application`[^engine-application], `Runic::NetworkTransport`[^engine-network] and `Runic::SimulationPort`[^engine-simulation].

## Next steps

Slice 1 (Application and layers): CORE-1[^engine-core], APP-1[^engine-application], GDB-1[^engine-godotbridge] (the GodotLayer on today's `GodotRuntime`) and GAME-0[^doc-game-packages] (in
RunicGame), in that order of Needs. After slice 1, slice 2 (GDB-2) and slice 3 (SIM-1[^engine-simulation], CORE-2) can start.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../docs/README|Engine docs README]] · [[../docs/system/README|System README]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture" (v4, in the RunicDocs vault): the architecture source of truth
[^doc-system-03-dependency-rules]: [[../docs/system/03-dependency-rules|03 — Dependency rules]] — System doc (mirrored)
[^doc-build-build-1]: [[../docs/build/BUILD-1|Engine BUILD-1 contract]] — RunicEngine doc
[^engine-godottools]: [[../Engine/Tools/GodotTools/ARCHITECTURE|GodotTools]] — Engine module · Tools
[^engine-godotbridge]: [[../Engine/Presentation/GodotBridge/ARCHITECTURE|GodotBridge]] — Engine module · Presentation
[^oq-8]: [[../docs/system/11-open-questions#OQ-8 — Engine repository layout (decided 2026-10-09)|OQ-8]] — open question or decision
[^engine-renderer]: [[../docs/modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-editor]: [[../docs/modules/Editor|Editor]] — Engine module · Tools
[^engine-core]: [[../docs/modules/Core|Core]] — Engine module · Foundation
[^engine-physics]: [[../docs/modules/Physics|Physics]] — Engine module · Simulation core
[^engine-network]: [[../docs/modules/Network|Network]] — Engine module · Networking
[^engine-simulation]: [[../docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^engine-application]: [[../docs/modules/Application|Application]] — Engine module · Foundation
[^doc-game-packages]: [[../../RunicGame/docs/game-packages|Game work packages]] — RunicGame work packages
