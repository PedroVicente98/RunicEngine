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
(`../RunicDocs/content/Architecture/`). RunicDocs is the only place the architecture and the development plan are
written: this repository keeps no copy of them (decided 2026-11-12). RunicDocs is edited on GitHub, so run
`git -C ../RunicDocs pull` before reading it; the local clone can lag. When RunicDocs and anything here disagree,
RunicDocs wins: report the difference. This repository's own documents sit next to the code: each module's
`README.md` and `ARCHITECTURE.md`, `Engine/README.md`, `cmake/README.md` and this file.

## Read before changing anything

1. The module you are working on: `Engine/<Group>/<Module>/README.md` (its folders and the package that fills each
   one) and its `ARCHITECTURE.md` once the module has one.
2. The RunicDocs notes that README lists under Sources, and the module's row in RunicDocs › Architecture ›
   Repositories, build targets and processes › Engine modules.
3. RunicDocs › Architecture › Modules, work packages and the implementation contract — the package (write scope,
   Done when, Needs, slice), the module contract and the Building with Claude Code cycle; RunicDocs › Development ›
   the slice note for the package's slice.
4. `Engine/README.md` (the folder map) and `cmake/README.md` (the build helpers and checks).

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
- **Header paths are unique.** Headers are included by their path under `Public/`, and configure fails when two
  modules (or interface targets) of the three repositories export the same path, case ignored: name files after
  what they hold (`PhysicsWorld.hpp`), or put them under a folder the architecture defines.
- **Public headers carry no library types** (Flecs, Jolt, Godot and godot-cpp, BehaviorTree.CPP,
  Detour, GameNetworkingSockets). Exactly three exceptions are approved: GLM math types in Core,
  BehaviorTree.CPP node base types for Game custom AI nodes, and godot-cpp types in GodotTools' public
  headers. Flecs has none: modules use the Runic API, and the Flecs world is reachable only through the
  opt-in `Runic::ECSNative` target (discouraged). Record a used exception in the module's `ARCHITECTURE.md`.
- **Godot is presentation and tooling only.** Only GodotBridge and GodotTools include Godot or
  godot-cpp; Godot never reads Flecs, Jolt or domain modules; the RenderFrame is the only door.
  No GDScript in the game (the module is off in both Godot targets until slices 20–21, editor only).
- **Headless modules** never link libgodot, godot-cpp, GodotBridge, GodotTools, an audio backend or
  editor code; the configure-time headless check fails the build if they do. There is no window
  class: the window comes from Godot through the GodotLayer.
- **Folders.** Every group, module and subsystem folder already exists with a README (`Engine/<Group>/<Module>/`
  with `Public/`, `Source/<Subsystem>/`, `Tests/`; `Engine/Programs/` for the Engine's executables). A package puts
  its code in the folders its README names and never moves or renames a folder; a module's first package adds its
  `CMakeLists.txt` and `ARCHITECTURE.md`, and the build picks the module up from the catalog (never edit
  `Engine/CMakeLists.txt` or another shared CMake file to add one). PascalCase for what the architecture defines,
  lowercase for repository plumbing (`cmake/`, `docs/`, `external/`, `scripts/`, `tests/`, `build/`). Never two
  folders that differ only by case. Third-party code lives in `external/`: Godot and godot-cpp as submodules,
  everything else as pinned downloads.
- **Programs are composition roots, not modules.** A package may also edit the Engine program that shows its result
  (`Engine/Programs/`: RunicServer, RunicClient, the RunicEditor), and nothing else outside its module. A program
  joins the build when its folder has a `CMakeLists.txt`.
- **Code visibility.** `Public/` headers, namespace `Runic::<Module>`: the module's API. Headers in
  `Source/<Subsystem>/`, namespace `Runic::<Module>::Detail` when several files of the module share them: private to
  the module. An unnamed namespace for helpers one `.cpp` uses.
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
- **Do not silently resolve open questions** (RunicDocs › Architecture › Open questions, and the open decisions in
  the RunicDocs notes); ask the owner.

## Build

```sh
git submodule update --init --depth 1 external/godot external/godot-cpp   # once
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/RunicEditor --editor --path build/debug/godot/SpikeProject   # the GDT-1 editor
cmake --preset headless && cmake --build --preset headless                     # no Godot, no graphics
```

The first `debug` build compiles libgodot from `external/godot` (editor and template targets, several minutes);
later builds only check it. The contract of the build is `cmake/README.md`. Do not change a pin in
`cmake/Dependencies.cmake` without its SHA-256 (third-party folders carry their version; a library is fetched only
when a configured module links it, and the package that first links one adds its recipe there), and change the
Godot pins only to release tags, after reviewing `scripts/godot/` with `scripts/godot/list_modules.py`.

## Docs maintenance

- In the same change as the code: the module's `ARCHITECTURE.md` (its contract and package status) and its
  `README.md` when what a folder holds changes. The PR closes the package's GitHub issue; RunicDocs' Development notes
  read the status from the issues.
- A change to the architecture or the plan (a rule, a catalog row, a package, a slice) is a RunicDocs change, made
  there by the owner or with the owner's approval, never a copy here. Then the code follows (`cmake/RunicModuleCatalog.cmake`
  must equal RunicDocs' Engine modules table: `checks.catalog_sync`).
- `docs/` is reserved for generated API documentation (Doxygen, later).
