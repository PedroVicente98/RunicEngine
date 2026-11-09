# CLAUDE.md — RunicEngine

RunicEngine is the public (MIT) C++20 engine of a three-repository MMORPG project:

- **RunicEngine** (this repo) — generic real-time and game mechanisms: 26 modules in seven groups
  (Foundation, Simulation core, GameplayAPI, World systems, Networking, Presentation, Tools), the
  entry point and layers, the RunicEditor (the Godot editor through LibGodot) and the Cooker. It pins
  Godot and godot-cpp and builds libgodot from source.
- **RunicPlatform** (`../RunicPlatform`, MIT) — realtime transport, Protobuf contracts, server
  bindings, Go SDK, core services, infrastructure. It implements Engine interfaces.
- **RunicGame** (`../RunicGame`, proprietary) — all concrete game rules, content and executables.

The source of truth is ARCH v4 (RunicEngine — Technical Architecture), kept in the RunicDocs vault
(`../RunicDocs/content/Architecture/`). `docs/system/03-dependency-rules.md` and `docs/build/` follow v4; the other
`docs/system/` and `docs/modules/` files are still v3 and lose to RunicDocs where they disagree (Godot replaces
bgfx, RmlUi, ImGui and GLFW; there is no Renderer, UI or Editor module and no Window class).

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
- **Public headers carry no library types** (Flecs, Jolt, Godot and godot-cpp, BehaviorTree.CPP,
  Detour, GameNetworkingSockets). Exactly four exceptions are approved: GLM math types in Core,
  the Flecs world handle for modules that register systems (for example `AIModule`; ECS re-exports
  Flecs only to them), BehaviorTree.CPP node base types for Game custom AI nodes, and godot-cpp
  types in GodotTools' public headers. Record a used exception in the module's `ARCHITECTURE.md`.
- **Godot is presentation and tooling only.** Only GodotBridge and GodotTools include Godot or
  godot-cpp; Godot never reads Flecs, Jolt or domain modules; the RenderFrame is the only door.
  No GDScript in the game (the module is off in both Godot targets until slices 20–21, editor only).
- **Headless modules** never link libgodot, godot-cpp, GodotBridge, GodotTools, an audio backend or
  editor code; the configure-time headless check fails the build if they do. There is no window
  class: the window comes from Godot through the GodotLayer.
- **Folder names.** PascalCase for what the architecture defines (`Engine/<Group>/<Module>/` with `Public/`,
  `Source/`, `Tests/`; `Engine/Programs/` for the Engine's executables); lowercase for repository plumbing
  (`cmake/`, `docs/`, `external/`, `scripts/`, `tests/`, `build/`). Never two folders that differ only by case.
  Third-party code lives in `external/`: Godot and godot-cpp as submodules, everything else as pinned downloads.
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
git submodule update --init --depth 1 external/godot external/godot-cpp   # once
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/RunicEditor --editor --path build/debug/godot/SpikeProject   # the GDT-1 editor
cmake --preset headless && cmake --build --preset headless                     # no Godot, no graphics
```

The first `debug` build compiles libgodot from `external/godot` (editor and template targets, several minutes);
later builds only check it. The contract of the build is `docs/build/BUILD-1.md`. Do not change a pin in
`cmake/Dependencies.cmake` without its SHA-256 (third-party folders carry their version), and change the Godot
pins only to release tags, after reviewing `scripts/godot/` with `scripts/godot/list_modules.py`.

## Docs maintenance

- Update the module contract, `docs/modules/README.md` status and `docs/current-state.md`
  in the same change as the code.
- `docs/system/` is mirrored in all three repositories; edit all copies together.
- The docs carry generated Obsidian links: frontmatter tags, `[^…]` markers after names, and the
  `## Connections` footer. Never edit them by hand; after any docs change, run
  `python3 RunicEngine/scripts/docs/obsidian_graph.py RunicEngine RunicPlatform RunicGame --write` from the
  folder that holds the three repositories (sibling checkouts).
