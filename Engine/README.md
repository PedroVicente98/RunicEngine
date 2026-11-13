# Engine/

Every Engine module and every executable the Engine owns. The layout is complete from the start (decided on
2026-11-12): each group, module and subsystem folder exists with a README that says what will live there and which
work package fills it, so no folder moves during development. Code arrives package by package.

## Groups and modules

The number after each module is the slice of its first package. A group is only a folder (GameplayAPI also has an
umbrella target); it has no code of its own.

| Group | Modules |
| --- | --- |
| [Foundation](Foundation/README.md) | Core 1 · Application 1 · Events 4 · Content 9 · Assets 9 |
| [SimulationCore](SimulationCore/README.md) | Simulation 3 · ECS 4 · World 4 · Physics 7 |
| [GameplayAPI](GameplayAPI/README.md) | Gameplay 14 · Combat 16 · GameplayFlow 29 · Quest 30 |
| [WorldSystems](WorldSystems/README.md) | Environment 25 · Perception 25 · Navigation 31 · AI 32 · Water 40 |
| [Networking](Networking/README.md) | Network 12 · DistributedSimulation 36 |
| [Presentation](Presentation/README.md) | GodotBridge 0 · Client 5 · Animation 23 · Audio 24 |
| [Tools](Tools/README.md) | GodotTools 0 · Cooker 9 |
| [Programs](Programs/README.md) | RunicEditor 0 · RunicServer 1 · RunicClient 1 · RunicBot 38 — executables, not modules |

Slice 0 left code in two modules: GodotBridge (the LibGodot host) and GodotTools (the GDT-1 test plugin), plus the
RunicEditor program. Everything else holds only READMEs.

## The shape of a module

```text
Engine/<Group>/<Module>/
├── README.md          the folder map: what each folder holds and which package fills it
├── ARCHITECTURE.md    the contract, written by the module's first package (contract session)
├── CMakeLists.txt     one runic_add_module(...) call, added by the first package
├── Public/            the only headers other modules may include, by their path under Public/
├── Source/<Subsystem>/  the implementation, one folder per subsystem; private to the module
└── Tests/             GoogleTest, one test executable per module
```

- **Public/** is flat, with one exception the architecture defines: `Core/Public/RenderData/` (namespace
  `Runic::RenderData`). Headers are included by their path under `Public/`: `#include "GodotRuntime.hpp"`,
  `#include "RenderData/RenderFrame.hpp"`.
- **Source/** holds `.cpp` files and the private headers only this module includes, one folder per subsystem.
  Namespace `Runic::<Module>`; `Runic::<Module>::Detail` for helpers several files of the module share; an unnamed
  namespace for helpers one file uses.
- **Extra targets** live in a folder named after the target, at the module's root, as their own include root:
  `SimulationCore/ECS/ECSNative/` (`Runic::ECSNative`, the opt-in door to native Flecs),
  `SimulationCore/Simulation/SimulationPort/` and `Networking/Network/NetworkTransport/` (header-only interface
  targets the Platform implements, Core types only).
- A module joins the build when its first package adds its `CMakeLists.txt`; until then its folder holds only
  READMEs. [`CMakeLists.txt`](CMakeLists.txt) adds every catalogued module that has one (BUILD-2), its extra
  targets from their own `CMakeLists.txt`, the umbrella target of each group, and each program in `Programs/` that
  has one; no package edits it.

## Where things are not

- **GameClient and GameServer** belong to RunicGame (`RunicGame/Source/GameClient`, `Source/GameServer`). They
  stack the layers that Engine modules supply. The Engine's own RunicServer and RunicClient are the same compositions
  without a game, so the Engine runs on its own; see [Programs](Programs/README.md).
- **Platform layers** (services bridge, lifecycle) belong to RunicPlatform. The Engine never depends on the
  Platform or the Game.
- **Godot** is not a module: it is built from `external/godot` into the libgodot libraries that GodotBridge loads
  (`cmake/RunicGodot.cmake`, `scripts/godot/`).

The architecture source of truth is the RunicDocs vault (RunicDocs › Architecture), the only place the architecture
is written; this layout follows its module catalog (Repositories, build targets and processes › Engine modules) and
work-package tables. The build helpers and checks are described in [cmake/README.md](../cmake/README.md).
