# Engine/

Every Engine module and every executable the Engine owns, as far as they have code: only the folders in use are here
(decided on 2026-11-14). The planned layout, every group, module, subsystem and program folder with a README that
says what will live there and which work package fills it, is on the `layout` branch; a package that creates a folder
takes its README from there, so no planned folder moves during development. Code arrives package by package.

## Groups and modules

The number after each module is the slice of its first package. A group is only a folder (GameplayAPI also has an
umbrella target); it has no code of its own.

| Group | Modules |
| --- | --- |
| [Foundation](Foundation/README.md) | Core 1 · Application 1 · Events 4 · Content 9 · Assets 9 |
| [SimulationCore](SimulationCore/README.md) | Simulation 3 · ECS 4 · World 4 · Physics 7 |
| GameplayAPI | Gameplay 14 · Combat 16 · GameplayFlow 29 · Quest 30 |
| WorldSystems | Environment 25 · Perception 25 · Navigation 31 · AI 32 · Water 40 |
| Networking | Network 12 · DistributedSimulation 36 |
| [Presentation](Presentation/README.md) | GodotBridge 0 · Client 5 · Animation 23 · Audio 24 |
| [Tools](Tools/README.md) | GodotTools 0 · Cooker 9 |
| [Programs](Programs/README.md) | RunicEditor 0 · RunicServer 1 · RunicClient 1 · RunicBot 38 — executables, not modules |

Modules with code: Core (CORE-1, CORE-3; CORE-2 and CORE-8 from slice 3; shape instances from slice 5; cached debug
meshes and the physics layer's geometry from slice 8) and Application (APP-1), from slice 1; Content (CNT-1) and
Assets (ASSET-1) from slice 9; Simulation (SIM-1, slice
3; SIM-2, slice 5; its phases on the worker pool, SIM-3, slice 10); Events (EVT-1), ECS (ECS-1, ECS-2) and World (WORLD-1), from slice 4; Physics (PHYS-1, PHYS-2) from
slice 7, which also gave Simulation its physics bridge (SIM-8), its debug view (PHYS-3) from slice 8 and Jolt on the worker pool (PHYS-4) from slice 10; Client
(CLI-1, slice 5; CLI-2, slice 7); GodotBridge (the slice 0 LibGodot host, GDB-1's GodotLayer, GDB-2's debug view,
GDB-3's presenter, GDB-4's physics geometry layer), GodotTools (GDT-2's project model, editor plugin and world dock,
slice 6; GDT-3's physics inspector and the editor's server view, slice 8; GDT-4's Cooker integration, slice 9) and the
Cooker (COOK-1, slice 9). Programs with code: RunicEditor (slice 0, on
the entry point since GDB-1, with a local world since GDT-2, running physics since GDT-3, cooking with `--runic-cook`
since GDT-4), RunicServer (APP-1, with the Simulation layer since SIM-1, walking on a cooked folder since slice 9, its parallel demo
since slice 10) and RunicClient (GDB-1; slice 5's moving entities; slice 7's walk on terrain far from the
origin, with its collision drawn since slice 8). The other groups, modules and programs are only on the `layout` branch so far.

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
- A module joins the build when its first package creates its folder (with its READMEs from the `layout` branch) and
  adds its `CMakeLists.txt`. [`CMakeLists.txt`](CMakeLists.txt) adds every catalogued module that has one (BUILD-2),
  its extra targets from their own `CMakeLists.txt`, the umbrella target of each group, and each program in
  `Programs/` that has one; no package edits it.

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
