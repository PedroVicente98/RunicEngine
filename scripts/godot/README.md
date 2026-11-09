# Godot as a library: build files

Godot enters the Engine as a library (LibGodot) in two targets, built from the pinned source in `external/godot`
(RunicDocs › Godot integration (Engine) › Building Godot). The build calls these files through
[cmake/RunicGodot.cmake](../../cmake/RunicGodot.cmake); you rarely run them by hand.

| File | What |
| --- | --- |
| `runic_editor.py` | SCons profile, editor target: the RunicEditor, the development GameClient, the Cooker on build machines |
| `runic_template.py` | SCons profile, template target: the shipping GameClient (physics, navigation and XR disabled) |
| `runic_client.gdbuild` | Build profile of the template target: classes the client never uses |
| `build_libgodot.py` | Runs SCons with a profile and stages the library next to the executables |
| `list_modules.py` | Lists the modules of a Godot checkout and their dependencies |

## Pins

`external/godot` is Godot **4.7.2-stable** and `external/godot-cpp` is godot-cpp **10.0.0-stable** (API 4.7), both
shallow submodules:

```sh
git submodule update --init --depth 1 external/godot external/godot-cpp
```

## By hand

```sh
python3 scripts/godot/build_libgodot.py --godot external/godot --target editor --out build/debug/bin
python3 scripts/godot/build_libgodot.py --godot external/godot --target template_release --out build/debug/bin
```

`--jobs N` sets the SCons jobs (default: half the CPUs; Godot's build is memory hungry), `--compiler llvm|gcc`
the Linux compiler (default: clang with lld when available), `--cache DIR` the SCons cache, `--prebuilt DIR`
copies built libraries instead of building. SCons comes from the PATH, `$SCONS`, or `external/scons-venv`,
which the script creates on first use with the pinned, hash-checked SCons wheel.

## Updating Godot

Godot is never forked; a new minor version is a decision with tests:

1. Move both submodules to the new release tags.
2. Run `python3 scripts/godot/list_modules.py external/godot` and review both profiles: module names and
   dependencies change between versions. GDScript stays off until slices 20–21.
3. Update `RUNIC_GODOT_API_VERSION` (godot-cpp's API) and `compatibility_minimum` in project editor libraries.
4. Rebuild and run `ctest --preset godot`.
