# RunicEngine

RunicEngine is an open-source (MIT) C++20 engine of generic mechanisms for a server-authoritative MMORPG: a headless multi-rate simulation on Flecs and Jolt, 64-bit world space, gameplay and combat toolkits, AI, water and environment, realtime replication and distributed simulation, client presentation, and the Editor and Cooker tools.

It is one of three repositories. RunicGame (proprietary) builds its game on it; RunicPlatform (MIT) supplies the realtime transport, services and infrastructure and implements the Engine's interfaces. The Engine contains no game rules or content and depends on neither.

## Status

Bootstrap only. The build skeleton, pinned third-party dependencies and a disposable dependency check (`RunicSandbox`) exist; **no Engine module is implemented yet (0 of 27)**. The next work is slice 0 (BUILD-1: module helper, CI, test framework, headless checks, CPU baseline) and slice 1 (CORE-1, APP-1, APP-2). Details: [docs/current-state.md](docs/current-state.md).

## Build

Requires CMake 3.24+, Ninja, a C++20 compiler, and on Linux the X11 and OpenGL development libraries that GLFW and bgfx use.

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
./build/debug/bin/RunicSandbox          # prints "RunicSandbox ready."

cmake --preset release
cmake --build --preset release --parallel 2
./build/release/bin/RunicSandbox
```

Headless engine-only build (no GLFW, bgfx or ImGui configured):

```sh
cmake -S . -B build/headless -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DRUNIC_BUILD_SANDBOX=OFF -DRUNIC_ENGINE_ENABLE_PRESENTATION=OFF
cmake --build build/headless --parallel 2
```

Third-party sources are downloaded on the first configure into `ThirdParty/` (git-ignored) from pinned archives with SHA-256 checks, and reused afterwards. To change a pin, update its URL and hash in `cmake/Dependencies.cmake` and delete the old source folder.

| Dependency | Pin today |
| --- | --- |
| Flecs | 4.0.4 (C++ API) |
| GLM | 1.0.1 |
| Jolt Physics | 5.2.0 (moving to 5.6.0 in double precision with the AVX2 path in BUILD-1 and PHYS-1) |
| GLFW | 3.4 (client and tools builds only) |
| bgfx, bx, bimg | bgfx.cmake `0fb9ec06bdaa7c3ae31ce6a3521e211183a0cead` with matching revisions (client and tools builds only) |
| Dear ImGui | 1.91.8, core only (client development builds and tools only) |

Today's targets: `Runic::Core` (bootstrap include root), `Runic::Runtime` (headless bundle: Flecs, GLM, Jolt) and `Runic::Presentation` (client bundle: GLFW, bgfx, ImGui). RunicGame links the two bundles until it moves to per-module targets.

Editors: open `Runic.code-workspace` to see all three sibling repositories; the `.vscode` launch configurations build the matching preset first, and `.clangd` reads the Debug compile database.

## Documentation

- [docs/README.md](docs/README.md) — documentation index
- [docs/architecture.md](docs/architecture.md) — how the Engine is organized
- [docs/modules/README.md](docs/modules/README.md) — module catalog and contracts
- [docs/current-state.md](docs/current-state.md) — what exists and what comes next
- [CLAUDE.md](CLAUDE.md) — rules for Claude Code sessions in this repository

## License

MIT. Third-party libraries keep their own licenses.
