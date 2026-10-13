# CLAUDE.md — RunicEngine

RunicEngine is the public (MIT) C++20 engine of a three-repository MMORPG project:

- **RunicEngine** (this repo) — generic real-time and game mechanisms: 27 modules in seven groups
  (Foundation, Simulation core, GameplayAPI, World systems, Networking, Presentation, Tools), the
  entry point and layers, the Editor and the Cooker.
- **RunicPlatform** (`../RunicPlatform`, MIT) — realtime transport, Protobuf contracts, server
  bindings, Go SDK, core services, infrastructure. It implements Engine interfaces.
- **RunicGame** (`../RunicGame`, proprietary) — all concrete game rules, content and executables.

The source of truth is ARCH (RunicEngine — Technical Architecture v3); these docs are generated from it.

## Read before changing anything

1. `docs/README.md`
2. `docs/system/01-architecture-overview.md`, `02-module-contract.md`, `03-dependency-rules.md`
3. `docs/architecture.md` and `docs/current-state.md`
4. The module you are working on: `docs/modules/<Module>.md` (or `Engine/<Group>/<Module>/ARCHITECTURE.md` once it exists)
5. `docs/system/12-work-packages.md` and `docs/system/10-ai-development-workflow.md` — how a package is done

## Hard rules

- **One work package = one module = one PR.** Write only inside that module's folder, its CMake
  target and its docs. If another module needs a change, stop and report the interface change
  instead of making it.
- **Contract first.** The module's `ARCHITECTURE.md`, the public headers and the acceptance tests
  are approved before any `Source/` code. Implement **Stage A only**: what the tests need, no
  speculative features, no placeholders in other modules.
- **Link only the module's allowed dependencies** (the Depends on column is a ceiling). A package's
  Needs orders merges; it never grants a link. Third-party libraries are PRIVATE except the
  approved public-header exceptions below (GLM is a PUBLIC dependency of Core).
- **Public headers carry no library types** (Flecs, Jolt, bgfx, RmlUi, ImGui, BehaviorTree.CPP,
  Detour, GameNetworkingSockets). Exactly three exceptions are approved: GLM math types in Core,
  the Flecs world handle for modules that register systems (for example `AIModule`; ECS re-exports
  Flecs only to them), and BehaviorTree.CPP node base types for Game custom AI nodes. Record a used
  exception in the module's `ARCHITECTURE.md`.
- **Headless modules** never link GLFW, bgfx, RmlUi, ImGui, an audio backend or editor code. The
  Window class in Application is compiled only in client and tools builds.
- **No game content.** No RunicGame rules, names, numbers or content anywhere in this repository,
  tests and docs included. Tests use neutral IDs such as `engine:test_ability`.
- **No Agones, Kubernetes, database, gRPC or NATS** dependency anywhere in the Engine.
- **The Engine never depends on RunicPlatform or RunicGame.** The Platform implements Engine
  interfaces through header-only interface targets whose headers include only Core types
  (`Runic::NetworkTransport`, `Runic::SimulationPort`), plus `Runic::Core` and `Runic::Application`.
  Keep those headers free of anything above Core.
- **Network encodes every realtime message**, envelope included. Protobuf (Platform) carries only
  service APIs, published facts and the connect-ticket format.
- **Never block a simulation tick.** I/O and service work run on their own threads and only
  enqueue; results are applied at safe ticks through the Simulation's service port.
- **Never mutate Flecs from an I/O callback.** Only the simulation's own phases mutate the world.
- **Local Flecs entity IDs never leave the process.** Across processes use `GlobalEntityId`,
  `GlobalPlayerId` and the other plain ID types in Core.
- **Do not silently resolve items in `docs/system/11-open-questions.md`;** ask the owner.

## Build

```sh
cmake --preset debug && cmake --build --preset debug --parallel 2
./build/debug/bin/RunicSandbox
```

Headless engine-only build: configure with `-DRUNIC_BUILD_SANDBOX=OFF -DRUNIC_ENGINE_ENABLE_PRESENTATION=OFF`.
Do not change a pin in `cmake/Dependencies.cmake` without its SHA-256 and without making sure the old
`ThirdParty/` source folder cannot be reused.

## Docs maintenance

- Update the module contract, `docs/modules/README.md` status and `docs/current-state.md`
  in the same change as the code.
- `docs/system/` is mirrored in all three repositories; edit all copies together.
- The docs carry generated Obsidian links: frontmatter tags, `[^…]` markers after names, and the
  `## Connections` footer. Never edit them by hand; after any docs change, run
  `python3 RunicEngine/tools/docs/obsidian_graph.py RunicEngine RunicPlatform RunicGame --write` from the
  folder that holds the three repositories (sibling checkouts).
