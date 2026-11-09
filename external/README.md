# external

Code this repository did not write. Nothing here is edited by hand.

| Folder | What | How it gets here | In git |
| --- | --- | --- | --- |
| `godot/` | Godot 4.7.2-stable, built as libgodot (editor and template targets) | Shallow submodule: `git submodule update --init --depth 1 external/godot external/godot-cpp` | Submodule pin |
| `godot-cpp/` | godot-cpp 10.0.0-stable, compiled into the Runic executables | The same command | Submodule pin |
| `flecs-4.0.4/`, `glm-1.0.1/`, `JoltPhysics-5.2.0/` | The transitional `Runic::Runtime` bundle (headless) | Downloaded on configure by [cmake/Dependencies.cmake](../cmake/Dependencies.cmake) | Ignored |
| `googletest-1.17.0/` | The C++ test framework, only when tests are on | Downloaded by [cmake/RunicTesting.cmake](../cmake/RunicTesting.cmake) | Ignored |
| `scons-venv/` | Pinned SCons 4.10.1 (hash-checked wheel) that builds libgodot | Created by [scripts/godot/build_libgodot.py](../scripts/godot/build_libgodot.py) on first use | Ignored |

Only Godot and godot-cpp are submodules: they are large source trees with release tags and Godot is built with
its own tool (SCons). Every other library is a pinned archive (URL and SHA-256) that the build downloads; each
folder name carries its version, so changing a pin downloads a new tree instead of silently reusing an old one.
Generated objects and libraries stay in the selected `build/` directory. `.gitignore` ignores everything here
except this file and the two submodules.
