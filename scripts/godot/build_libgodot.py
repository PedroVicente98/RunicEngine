#!/usr/bin/env python3
"""Build libgodot from the pinned Godot source with the Runic profiles, and stage it next to the executables.

Called by cmake/RunicGodot.cmake (one custom command per Godot target); it can also run by hand:

    python3 scripts/godot/build_libgodot.py --godot external/godot --target editor --out build/debug/bin

Targets (RunicDocs › Godot integration › Executables and Godot targets):
    editor            RunicEditor, the development GameClient, the Cooker on build machines
    template_release  the shipping GameClient
    template_debug    a shipping-like GameClient with Godot's debug checks

SCons comes from --scons, $SCONS, the Engine's external/scons-venv (created on first use with the pinned,
hash-checked SCons wheel), or the PATH, in that order. SCons is incremental: a second run only checks what
changed. With --prebuilt the libraries are copied from a folder instead of being built (CI artifacts).
"""

import argparse
import glob
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import venv
from pathlib import Path

HERE = Path(__file__).resolve().parent
ENGINE = HERE.parent.parent
SCONS_VERSION = "4.10.1"
SCONS_WHEEL_SHA256 = "bd9d1c52f908d874eba92a8c0c0a8dcf2ed9f3b88ab956d0fce1da479c4e7126"


def log(message: str) -> None:
    print(f"[libgodot] {message}", flush=True)


def host_platform() -> str:
    if sys.platform.startswith("linux") or "bsd" in sys.platform:
        return "linuxbsd"
    if sys.platform == "win32":
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    raise SystemExit(f"[libgodot] unsupported host platform: {sys.platform}")


def host_arch() -> str:
    machine = platform.machine().lower()
    return {"amd64": "x86_64", "x86_64": "x86_64", "arm64": "arm64", "aarch64": "arm64"}.get(machine, machine)


def library_glob(plat: str, target: str, arch: str) -> str:
    if plat == "windows":
        return f"godot.windows.{target}.{arch}*.dll"
    if plat == "macos":
        return f"libgodot.macos.{target}.{arch}*.dylib"
    return f"libgodot.{plat}.{target}.{arch}*.so"


def canonical_name(plat: str, target: str, arch: str) -> str:
    """The name the hosts look for next to the executable (no compiler suffix such as .llvm)."""
    if plat == "windows":
        return f"godot.windows.{target}.{arch}.dll"
    if plat == "macos":
        return f"libgodot.macos.{target}.{arch}.dylib"
    return f"libgodot.{plat}.{target}.{arch}.so"


def bootstrap_scons() -> Path:
    env_dir = ENGINE / "external" / "scons-venv"
    bindir = env_dir / ("Scripts" if sys.platform == "win32" else "bin")
    scons = bindir / ("scons.exe" if sys.platform == "win32" else "scons")
    if scons.exists():
        return scons
    log(f"creating {env_dir} with SCons {SCONS_VERSION} (pinned, hash-checked)")
    venv.EnvBuilder(with_pip=True, clear=False).create(env_dir)
    requirements = env_dir / "requirements.txt"
    requirements.write_text(f"scons=={SCONS_VERSION} --hash=sha256:{SCONS_WHEEL_SHA256}\n")
    python = bindir / ("python.exe" if sys.platform == "win32" else "python3")
    subprocess.check_call([str(python), "-m", "pip", "install", "--quiet", "--disable-pip-version-check",
                           "--require-hashes", "-r", str(requirements)])
    return scons


def find_scons(explicit: str | None) -> list[str]:
    if explicit:
        return [explicit]
    if os.environ.get("SCONS"):
        return [os.environ["SCONS"]]
    local = ENGINE / "external" / "scons-venv" / ("Scripts" if sys.platform == "win32" else "bin") / \
        ("scons.exe" if sys.platform == "win32" else "scons")
    if local.exists():
        return [str(local)]
    on_path = shutil.which("scons")
    if on_path:
        return [on_path]
    return [str(bootstrap_scons())]


def compiler_args(choice: str, plat: str) -> list[str]:
    if plat != "linuxbsd":
        return []
    if choice == "auto":
        choice = "llvm" if shutil.which("clang++") and shutil.which("ld.lld") else "gcc"
    if choice == "llvm":
        return ["use_llvm=yes", "linker=lld"]
    return []


def same_file(a: Path, b: Path) -> bool:
    if not b.exists() or a.stat().st_size != b.stat().st_size:
        return False
    def digest(p: Path) -> str:
        h = hashlib.sha256()
        with p.open("rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        return h.hexdigest()
    return digest(a) == digest(b)


def stage(source: Path, out_dir: Path, name: str) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    destination = out_dir / name
    if same_file(source, destination):
        destination.touch()   # up to date for the build system, no copy
        log(f"{destination} is up to date")
    else:
        shutil.copy2(source, destination)
        destination.touch()
        log(f"staged {source.name} -> {destination}")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--godot", required=True, help="Godot source checkout (external/godot)")
    parser.add_argument("--target", required=True, choices=["editor", "template_release", "template_debug"])
    parser.add_argument("--out", required=True, help="folder where the library is staged (the executables' bin/)")
    parser.add_argument("--jobs", type=int, default=0, help="parallel SCons jobs (default: half the CPUs)")
    parser.add_argument("--compiler", default="auto", choices=["auto", "gcc", "llvm"],
                        help="Linux only: llvm = clang with lld (faster); auto picks llvm when available")
    parser.add_argument("--cache", default="", help="SCons cache folder (CI)")
    parser.add_argument("--scons", default=None, help="path to scons")
    parser.add_argument("--prebuilt", default="", help="copy the library from this folder instead of building")
    parser.add_argument("--extra", action="append", default=[], help="extra SCons argument (repeatable)")
    args = parser.parse_args()

    godot = Path(args.godot).resolve()
    out_dir = Path(args.out).resolve()
    plat, arch = host_platform(), host_arch()
    name = canonical_name(plat, args.target, arch)

    if args.prebuilt:
        candidates = sorted(Path(args.prebuilt).glob(library_glob(plat, args.target, arch)))
        if not candidates:
            raise SystemExit(f"[libgodot] no {library_glob(plat, args.target, arch)} in {args.prebuilt}")
        stage(candidates[-1], out_dir, name)
        return 0

    if not (godot / "SConstruct").exists():
        raise SystemExit(f"[libgodot] {godot} is not a Godot checkout; run "
                         "`git submodule update --init --depth 1 external/godot` in RunicEngine")

    profile = HERE / ("runic_editor.py" if args.target == "editor" else "runic_template.py")
    command = find_scons(args.scons) + [f"platform={plat}", f"target={args.target}", f"profile={profile}"]
    if args.target != "editor":
        command.append(f"build_profile={HERE / 'runic_client.gdbuild'}")
    command += compiler_args(args.compiler, plat)
    if args.cache:
        command.append(f"cache_path={Path(args.cache).resolve()}")
    command += args.extra
    jobs = args.jobs if args.jobs > 0 else max(1, (os.cpu_count() or 2) // 2)
    command.append(f"-j{jobs}")

    log("running: " + " ".join(command))
    log("a first build of the editor target takes several minutes; later runs only check what changed")
    result = subprocess.run(command, cwd=godot)
    if result.returncode != 0:
        return result.returncode

    built = sorted(glob.glob(str(godot / "bin" / library_glob(plat, args.target, arch))), key=os.path.getmtime)
    if not built:
        raise SystemExit(f"[libgodot] SCons finished but no {library_glob(plat, args.target, arch)} is in {godot / 'bin'}")
    stage(Path(built[-1]), out_dir, name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
