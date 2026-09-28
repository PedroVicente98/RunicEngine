# RunicEngine

C++20 MMORPG engine foundation. This is **step 0.0: build setup only**.
The sandbox checks dependency headers and linkage; no simulation, window,
renderer, input system, or gameplay is implemented.

## Debug and Release

Requires CMake 3.24+, Ninja, C/C++ compilers, and the Linux X11/OpenGL development
libraries used by GLFW/bgfx. From this repository:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
./build/debug/bin/RunicSandbox

cmake --preset release
cmake --build --preset release --parallel 2
./build/release/bin/RunicSandbox
```

Both executables check Flecs **C++ bindings**, GLM, Jolt, GLFW, bgfx, and ImGui,
then print `RunicSandbox ready.`. The check needs no display or GPU context.
Debug includes symbols; Release is optimized.

## VS Code / Cursor

Open **Runic.code-workspace** from this repository to see all three sibling
repositories and use the repository-owned launch tasks. Opening RunicEngine
directly also works. Opening only the parent directory does not load a nested
`.vscode/launch.json`.

In Run and Debug, select **Debug** or **Release** and press F5. Each launch
configures and builds its matching preset first. The configurations use the
C/C++ extension's debugger and `/usr/bin/gdb` on Linux. Source stepping is intended
for Debug; optimized Release may skip variables/lines.

The presets write `compile_commands.json`; `.clangd` selects the Debug database.
CMake supplies the include paths, definitions, library locations, and transitive
OS libraries. No manual editor include paths or global linker directories are needed.

## Files and boundaries

- `CMakeLists.txt`: project, two build choices, helper includes, sandbox.
- `cmake/Dependencies.cmake`: every third-party pin, option, source, and link target,
  separated into library sections.
- `cmake/CompilerWarnings.cmake`: warnings for Runic targets only.
- `cmake/Targets.cmake`: C++20/include root and headless target boundaries.
- `cmake/Sandbox.cmake`: the single disposable executable.
- `apps/Sandbox/Main.cpp`: two marked dependency-check blocks. Delete both blocks
  to leave a plain `main()`; no library implementation relies on them.
- `include/RunicEngine/`, `src/`: empty Core, Simulation, Physics, Platform,
  Input, Renderer, Debug, and UI directories for later slices.
- `tests/`: reserved; there are no duplicate bootstrap test executables.
- `ThirdParty/`: downloaded source trees, ignored by Git.

Future headers use `<RunicEngine/Module/Header.hpp>`. Platform and Input remain
separate, and simulation must stay independent of presentation. bgfx remains the
renderer dependency; the supplied Vulkan roadmap is not implemented.

## Dependencies

Sources are fetched only when their local ThirdParty directory is missing;
existing source trees are reused across build configurations. Downloads use
pinned revisions and SHA-256 checks. Keep downloaded trees unmodified. To update
a dependency, update its pin/hash and remove its old source directory before
configuring again. Configure shared-source build trees sequentially.

| Dependency | Pin |
| --- | --- |
| Flecs | 4.0.4, C++ API in `flecs.h` |
| GLM | 1.0.1 |
| Jolt | 5.2.0 |
| GLFW | 3.4 |
| ImGui | 1.91.8, core sources only |
| bgfx.cmake | `0fb9ec06bdaa7c3ae31ce6a3521e211183a0cead`, matching bx/bimg/bgfx revisions in CMake |

`Runic::Core` is an interface include/C++20 target with no dummy version source.
`Runic::Runtime` adds Flecs, GLM, and Jolt. `Runic::Presentation` adds only the
client dependencies. The sandbox links both groups. Their libraries keep their
own licenses; RunicEngine is MIT licensed.

RunicGame consumes this sibling repository through `RUNIC_ENGINE_DIR`.
For a server-only game build, use a separate build directory:

```sh
cmake -S ../RunicGame -B ../RunicGame/build/headless \
  -DRUNIC_GAME_BUILD_CLIENT=OFF -DRUNIC_ENGINE_ENABLE_PRESENTATION=OFF
cmake --build ../RunicGame/build/headless --target RunicGameServer --parallel 2
```

An engine-only headless build can disable both `RUNIC_BUILD_SANDBOX` and
`RUNIC_ENGINE_ENABLE_PRESENTATION`. No third-party-free build mode or engine
systems are introduced.
