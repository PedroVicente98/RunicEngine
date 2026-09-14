# RunicEngine

The public, MIT-licensed C++20 MMORPG simulation and reusable gameplay framework.
RunicEngine will simulate a local authoritative world, including in a headless
server. RunicFabric connects and coordinates simulations; the proprietary RunicGame
supplies game rules, definitions, balance, and content. This engine depends on
neither repository. Future networking integration belongs behind narrow adapters.

This repository currently contains only build boundaries and disposable link
smokes. No engine systems, simulation loop, or gameplay are implemented.

## Targets and boundaries

| CMake target / directory | Current role |
| --- | --- |
| `Runic::Core` / `src/Core` | Static C++20 library with a version function |
| `Runic::Runtime` / `src/Runtime` | Core, Flecs C++ bindings, GLM, and Jolt |
| `Runic::Presentation` | Optional GLFW/bgfx/ImGui dependency group for clients and tools |
| `src/World`, `src/Gameplay`, `src/Editor` | Short boundary notes; no speculative libraries or classes |
| `tests` | Public-header/link checks; graphics smoke creates no window |

Future simulation uses a fixed tick independent of render frames. Runtime must
remain free of presentation dependencies. CMake targets define module boundaries;
a LayerStack only orders runtime updates/events. World chunks are spatial data,
with ownership assigned to processes separately; one process can own many chunks.
Generic data-driven action combat belongs here, with spatial targeting by default
and explicit entity targets where needed. Actual game definitions belong in the
game. Local simulation events remain local; generic distributed events belong in
Fabric and proprietary distributed events belong in the game.

## Build

Requires CMake 3.24+, C and C++20 compilers, and a native build tool. Linux is the
initial platform; no Windows-specific project files are required. From this repo:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Fresh standalone builds fetch and build all six dependencies by default. Headless
consumers can set `RUNIC_ENGINE_ENABLE_PRESENTATION=OFF`; the engine defaults this
option OFF when included as a subdirectory. RunicGame selects it for client builds.
`RUNIC_ENGINE_BUILD_TESTS` defaults ON standalone and OFF as a subdirectory.
With **Ninja** installed, `cmake --preset debug`, `cmake --build --preset debug`,
and `ctest --preset debug` provide the equivalent Debug workflow and compile database.
Presets also update caches created by the earlier bootstrap with dependencies OFF.

## Third-party dependencies and C++ usage

`cmake/RunicDependencies.cmake` uses FetchContent with fixed releases/commits,
SHA-256 archive checks, and TLS verification. Sources and outputs stay inside
the ignored build directory; no third-party sources are vendored into Git.

| Dependency | Pin | Enabled by |
| --- | --- | --- |
| [Flecs](https://github.com/SanderMertens/flecs/tree/v4.0.4) | v4.0.4 | `RUNIC_ENGINE_FETCH_DEPENDENCIES=ON` (default); C++ bindings |
| [GLM](https://github.com/g-truc/glm/tree/1.0.1) | 1.0.1 | Same; header-only |
| [Jolt](https://github.com/jrouwe/JoltPhysics/tree/v5.2.0) | v5.2.0 | Same; static library |
| [GLFW](https://github.com/glfw/glfw/tree/3.4) | 3.4 | Also `RUNIC_ENGINE_ENABLE_PRESENTATION=ON` |
| [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.91.8) | v1.91.8 | Same; core sources only |
| [bgfx.cmake](https://github.com/bkaradzic/bgfx.cmake/tree/0fb9ec06bdaa7c3ae31ce6a3521e211183a0cead) | `0fb9ec06bdaa7c3ae31ce6a3521e211183a0cead` | Same |

bgfx uses the CMake integration's matching source revisions: bgfx
`dd38b306c85472d0c89bb025970da7c2d42838d4`, bimg
`ddbeeae05779f84f97694553eb41605a60f86f0a`, and bx
`f86bece7967be1b8a7fd39262cdc8ce99d123c3b`. Separate archives avoid downloading Git
history. Wrapper installation is disabled; archive builds lack its Git-derived
version metadata. ImGui has a small source-list target because it provides no
upstream CMake target. Renderer/platform backends, shader tools, examples, and
wrapper systems are deferred. Dependencies keep their own upstream licenses.

Flecs code uses the **C++ API** from `<flecs.h>` (`flecs::world`, typed components,
queries, and systems). Its header bindings use the same `flecs::flecs_static`
library; no separate C++ binary is needed. The dependency smoke creates a world
and round-trips a typed component through that API without implementing systems.

Link the Runic targets to inherit headers, compile definitions, static library
locations, and platform linker requirements:

| Consumer target | Available dependency headers |
| --- | --- |
| `Runic::Runtime` | `<flecs.h>`, `<glm/glm.hpp>`, `<Jolt/Jolt.h>` |
| `Runic::Presentation` | `<GLFW/glfw3.h>`, `<bgfx/bgfx.h>`, `<imgui.h>` |

```cmake
target_link_libraries(MyServer PRIVATE Runic::Runtime)
target_link_libraries(MyClient PRIVATE Runic::Runtime Runic::Presentation)
```

CMake resolves library files and transitive OS libraries from these targets;
consumers do not need global include/link search paths or hand-written `-I`/`-L`
flags. `Runic::Presentation` exports `GLFW_INCLUDE_NONE` so GLFW does not select
OpenGL headers for bgfx consumers. Include `<Jolt/Jolt.h>` before other Jolt headers.
Editor tooling can read `build/debug/compile_commands.json` for the actual include
paths and compiler definitions.

For the headless dependencies and their link smoke:

```sh
cmake -S . -B build/dependencies \
  -DRUNIC_ENGINE_FETCH_DEPENDENCIES=ON -DRUNIC_ENGINE_ENABLE_PRESENTATION=OFF
cmake --build build/dependencies --parallel 2
ctest --test-dir build/dependencies --output-on-failure
```

Ninja presets `dependencies` (headless) and `presentation` (all six) provide these
configurations; `debug` also builds all six. For a dependency-free infrastructure
smoke only, explicitly set both `RUNIC_ENGINE_FETCH_DEPENDENCIES=OFF` and
`RUNIC_ENGINE_ENABLE_PRESENTATION=OFF`. Initial fetching requires internet access; extracted source
overrides can use CMake's `FETCHCONTENT_SOURCE_DIR_<UPPERCASE_NAME>` cache variables.
See the declaration names in the dependency file. Use `FETCHCONTENT_FULLY_DISCONNECTED`
only after all required sources are populated. Pins and hashes should be updated
together and reverified.

The Linux presentation build defaults to X11 and needs X11/Xrandr/Xinerama/Xcursor/Xi
headers and OpenGL development libraries. Wayland can later be enabled through
`GLFW_BUILD_WAYLAND` and `BGFX_WITH_WAYLAND` with its development dependencies.
Server-only builds do not need these packages. Even in a combined graphics build,
the game server links only Runtime and Fabric. To avoid graphics acquisition and
configuration altogether, keep presentation OFF in a separate server build tree.

Jolt's debug renderer/profiler, forced optimization flags, IPO, and optional x86
instruction extensions are disabled. This establishes a portable link baseline;
physics budgets, determinism, and simplified MMO collision usage must be decided
with a real simulation slice, without assuming mass rigid-body simulation.

Bootstrap verification passed on Linux with GCC 16.1.1 and CMake 4.4.2: the default
build and all six dependencies compile/link, and all three engine smoke
tests pass without a display. The pinned Flecs/GLM CMake files emit deprecation
warnings on this CMake version; bimg's bundled codecs emit compiler warnings.
These are upstream warnings, not build failures. Rendering/window behavior and
non-Linux platforms have not been tested.

## Repository workflow

This is an independent repository beside RunicFabric and RunicGame; the parent
is only a workspace folder. RunicGame uses a sibling default and configurable
`RUNIC_ENGINE_DIR` with `add_subdirectory()` and an explicit binary directory.
No remote or submodule URL is assumed. Once a real remote exists:

```sh
git remote add origin <actual-RunicEngine-remote-URL>
git push -u origin main
```

Future acquisition may use a deliberate packaging superproject/dependency layout
with submodules, FetchContent from tagged releases, or installed packages. Package
exports/install support are deferred. Do not duplicate the current sibling checkout.
The next slice is one map, one player, fixed-tick movement, headless simulation,
and minimal client rendering.
