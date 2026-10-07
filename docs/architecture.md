# RunicEngine architecture

## Role

RunicEngine provides **mechanisms that can affect authoritative gameplay**, plus
the client/tooling mechanisms that present and author them. It is reusable by a
different game: concrete RunicGame rules, numbers, content, and presentation never
appear here — not even in tests (use `engine:`/test-namespaced data).
[system/01](system/01-architecture-overview.md)

It must also:

- run **headless**: the authoritative server links no GLFW, bgfx, RmlUi, ImGui,
  client audio, or editor code [J.14.6];
- run **without production infrastructure**: no Agones, Kubernetes, database,
  gRPC, or NATS dependency anywhere in Engine [J.11.2];
- expose networking only through the narrow `NetworkAdapter` seam; transport is
  RunicPlatform's job [J.4.23].

## Target repository layout

```text
RunicEngine/
├── Engine/
│   └── <Module>/                # one per catalog entry, see modules/README.md
│       ├── Public/              # flat include root: <Module>*.hpp, namespace Runic::<Module>
│       ├── Source/              # implementation + private backends (Jolt/, Bgfx/, Flecs/, …)
│       ├── Tests/
│       ├── ARCHITECTURE.md      # module contract (seeded from docs/modules/<Module>.md)
│       └── CMakeLists.txt       # add_library(Runic<Module>) + alias Runic::<Module>
├── apps/                        # executables (sandbox, headless host, client shell, editor, cooker)
├── cmake/                       # shared CMake helpers, dependency pins, warnings
├── ThirdParty/                  # downloaded sources (git-ignored)
└── docs/
```

The `Engine/` root and per-module layout come from [J.2.2] and [J.3]. The `apps/`,
`cmake/`, and `ThirdParty/` folders already exist from the bootstrap. Migration
from today's `include/RunicEngine/` + `src/` scaffold is tracked in
[current-state.md](current-state.md) and [OQ-8](system/11-open-questions.md#oq-8).

## Module groups

| Group | Modules | Linked by |
| --- | --- | --- |
| Foundation | Core, Application, Events, Content, ECS | everything |
| Authoritative world | World, Physics, Simulation, Navigation, Water | server and client (prediction) |
| Gameplay frameworks | Gameplay, Combat, AI, GameplayFlow, Quest | server (client only as needed for prediction/presentation state) |
| Distribution seam | DistributedSimulation, NetworkAdapter | server and client |
| Presentation | Assets, Renderer, Client, Animation, Audio, UI | client, editor (Assets also server) |
| Tools | Cooker, Editor | tools only — never server/client runtime |

Dependency direction, the full allowed-dependency table, and forbidden edges:
[system/03-dependency-rules.md](system/03-dependency-rules.md).

## Executables (target)

| Executable | Purpose | Phase | Links |
| --- | --- | --- | --- |
| `RunicSandbox` | Disposable dependency check (exists today) | bootstrap | Runtime + Presentation bundles |
| Headless host | Engine-only headless runtime for tests/proofs | 0–1 | headless modules only |
| Client shell | Engine-only client for local proofs (phase 2–3) | 0–2 | client modules |
| Editor | Developer tooling | 0+ | Editor (leaf) |
| `RunicCooker` | Offline content cooking | 3 | Cooker + tool libraries |

The real game executables (`RunicGameClient`, `RunicGameServer`) live in
RunicGame and compose Engine modules with Game code.

## Third-party placement

| Library | Pin | Owner module | Visibility |
| --- | --- | --- | --- |
| Flecs (C++ API) | 4.0.4 | ECS | private (public only if deliberately chosen) |
| GLM | 1.0.1 | Core | public (foundational math) |
| Jolt | 5.2.0 | Physics; Cooker (cooking) | private |
| GLFW | 3.4 | Application OS/window adapter (proposal, [OQ-4](system/11-open-questions.md#oq-4)) | private, client/editor only |
| bgfx / bx / bimg (bgfx.cmake `0fb9ec06…`) | pinned | Renderer | private |
| Dear ImGui (core only) | 1.91.8 | Editor / debug tooling (proposal, [OQ-6](system/11-open-questions.md#oq-6)) | private, never headless |
| RmlUi | — | UI | private (to add in phase 2) |
| BehaviorTree.CPP | — | AI | private (to add in phase 11) |
| Detour / Recast | — | Navigation / Cooker | private / tool-only |
| fastgltf, meshoptimizer, MikkTSpace, KTX, Zstd | — | Cooker | tool-only |

All pins, hashes, and options live in `cmake/Dependencies.cmake`; sources are
fetched into `ThirdParty/` and never edited. To change a pin, update pin + hash
and delete the old source folder.

## Build conventions (current)

- CMake ≥ 3.24, C++20, Ninja presets `debug` and `release`
  (`compile_commands.json` exported; `.clangd` uses `build/debug`).
- Warnings via `runic_enable_warnings(target)` (`-Wall -Wextra -Wpedantic` / `/W4
  /permissive-`) on Runic targets only.
- Formatting: `.clang-format` (LLVM base, 4-space indent, 100 columns).
- Options: `RUNIC_BUILD_SANDBOX` (default: top-level),
  `RUNIC_ENGINE_ENABLE_PRESENTATION` (default: = sandbox). Presentation
  dependencies are not even configured when it is OFF.

```sh
cmake --preset debug && cmake --build --preset debug --parallel 2
./build/debug/bin/RunicSandbox          # prints "RunicSandbox ready."
```

## Key engine-wide rules (summary)

1. Simulation owns the Flecs world, the 60 Hz clock and 30/20/10 Hz domains,
   phases, command ingress, deferred mutation, and physics timing. Features
   *register* into it; it never depends on them.
2. Gameplay facts go through `Events` and are never "handled-away".
3. Physics answers geometry; Combat/Game decide meaning.
4. AI emits intents; it never mutates combat/physics state directly.
5. Scripted behavior (quests, bosses, cutscenes) uses `GameplayFlow`; restrictions
   are source-owned and released by one termination routine.
6. Renderer only observes presentation data; nothing else issues bgfx calls.
7. Local Flecs IDs never leave the process; cross-process identity is
   `GlobalEntityId`.
8. Backend types stay in `Source/`.
