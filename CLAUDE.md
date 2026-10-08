# CLAUDE.md — RunicEngine

RunicEngine is the public (MIT) C++20 engine of a three-repository MMORPG project:

- **RunicEngine** (this repo) — reusable real-time/game mechanisms.
- **RunicPlatform** (`../RunicPlatform`) — networking, services, persistence, orchestration.
- **RunicGame** (`../RunicGame`, proprietary) — all concrete game rules, content, presentation.

## Read before changing anything

1. `docs/README.md`
2. `docs/system/01-architecture-overview.md`, `02-module-contract.md`, `03-dependency-rules.md`
3. `docs/current-state.md`
4. The module you are working on: `docs/modules/<Module>.md` (or `Engine/<Module>/ARCHITECTURE.md` once it exists)
5. `docs/system/10-ai-development-workflow.md` — how a module task is done

## Hard rules

- Work on **one module at a time**. Write only inside that module, its CMake target,
  its tests, and its docs. If another module needs a change, report the required
  interface change instead of making it.
- Link only the module's **allowed dependencies**. Backend types (Jolt, bgfx, Flecs,
  RmlUi, BehaviorTree.CPP, Recast/Detour) never appear in `Public/` headers.
- The authoritative server is **headless**: no GLFW, bgfx, RmlUi, ImGui, client audio,
  or editor code in headless modules.
- No RunicGame rules, names, or content in this repository (tests included).
- No Agones, Kubernetes, database, gRPC, or NATS dependency in Engine.
- Never block a simulation tick on I/O; never mutate Flecs from an I/O callback.
- Local Flecs entity IDs never leave the process; use `GlobalEntityId`.
- Do not silently resolve items in `docs/system/11-open-questions.md`; ask.
- Implement Stage A only (what the acceptance tests need); no speculative features.

## Build

```sh
cmake --preset debug && cmake --build --preset debug --parallel 2
./build/debug/bin/RunicSandbox
```

Headless engine-only build: configure with `-DRUNIC_BUILD_SANDBOX=OFF -DRUNIC_ENGINE_ENABLE_PRESENTATION=OFF`.

## Docs maintenance

- Update the module contract, `docs/modules/README.md` status, and `docs/current-state.md`
  in the same change as the code.
- `docs/system/` is mirrored in all three repositories; edit all copies together.
