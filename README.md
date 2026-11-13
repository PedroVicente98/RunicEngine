# RunicEngine

RunicEngine is an open-source (MIT) C++20 engine of generic mechanisms for a server-authoritative MMORPG: a headless multi-rate simulation on Flecs and Jolt, 64-bit world space, gameplay and combat toolkits, AI, water and environment, realtime replication and distributed simulation, client presentation, and the RunicEditor (the Godot editor through LibGodot) and the Cooker.

It is one of three repositories. RunicGame (proprietary) builds its game on it; RunicPlatform (MIT) supplies the realtime transport, services and infrastructure and implements the Engine's interfaces. The Engine contains no game rules or content and depends on neither.

## Status

Slice 0 (Build skeleton) is done: the module helpers and their checks, the v4 module catalog, GoogleTest, presets,
CI, the x86-64-v3 baseline, Godot 4.7.2 and godot-cpp 10.0.0 pinned, libgodot built from source in the editor and
template targets, and the RunicEditor opening the Godot editor through LibGodot with a test plugin (GDT-1). Every
module and subsystem folder exists with a README saying what it will hold ([Engine/README.md](Engine/README.md)).
Slice 1 (Application and layers) is next. The build skeleton is described in [cmake/README.md](cmake/README.md); the
plan and the status of every slice and package are in RunicDocs (Development).

## Build

Requires CMake 3.24+, Ninja, a C++20 compiler (GCC 13+ or Clang 18+), Python 3.8+ and git. Godot is built with
SCons, which the build installs into `external/scons-venv` (pinned and hash-checked) on first use; clang with lld
is used for it when available. No X11, Wayland, Vulkan or audio development packages are needed: Godot loads those
libraries at runtime.

```sh
git submodule update --init --depth 1 external/godot external/godot-cpp

cmake --preset debug
cmake --build --preset debug        # the first build also compiles libgodot (several minutes)
ctest --preset debug                # module, build, checks and Godot spike tests

./build/debug/bin/RunicEditor                                                  # project manager
./build/debug/bin/RunicEditor --editor --path build/debug/godot/SpikeProject   # editor + test plugin
./build/debug/bin/RunicEditor --headless --editor --path build/debug/godot/SpikeProject --quit-after 120
```

Headless build (no Godot, no graphics library configured), as the server and the Platform use the Engine:

```sh
cmake --preset headless && cmake --build --preset headless && ctest --preset headless
```

| Dependency | Pin |
| --- | --- |
| Godot | 4.7.2-stable (submodule `external/godot`), built as libgodot: editor and template targets, module lists in `scripts/godot/` |
| godot-cpp | 10.0.0-stable (submodule `external/godot-cpp`), API 4.7 |
| GoogleTest | 1.17.0 (tests only) |
| Flecs | 4.0.4 (transitional `Runic::Runtime` bundle; 4.2 is decided and set by ECS-1) |
| GLM | 1.0.1 |
| Jolt Physics | 5.2.0 with SIMD from the CPU baseline (5.6 in double precision is decided and set by PHYS-1) |

Third-party archives are downloaded on configure into versioned folders in `external/` (git-ignored) with SHA-256
checks and reused afterwards. Editors: open `Runic.code-workspace` for the Engine, Platform, Game and Docs folders;
`.vscode/` has build and test tasks and launch configurations for the RunicEditor and the tests.

## Documentation

The architecture, the roadmap and the status of every package live in one place, the RunicDocs vault
([pedrovicente98.github.io/RunicDocs](https://pedrovicente98.github.io/RunicDocs/)): this repository keeps no copy.
Next to the code:

- [Engine/README.md](Engine/README.md) — every group, module and subsystem folder, and the package that fills it;
  each module's own `README.md` and `ARCHITECTURE.md`
- [cmake/README.md](cmake/README.md) — the build helpers, the checks, the presets and CI
- [CLAUDE.md](CLAUDE.md) — rules for Claude Code sessions in this repository
- `docs/` will hold the generated API documentation (Doxygen, later)

## License

MIT. Third-party libraries keep their own licenses.
