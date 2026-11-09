#!/usr/bin/env python3
"""List the modules of a Godot checkout and their dependencies, to review the Runic module lists.

Run it at every Godot update, before touching runic_editor.py or runic_template.py (module names and
dependencies change between Godot versions; RunicDocs › Godot integration › Building Godot):

    python3 scripts/godot/list_modules.py external/godot

Adapted from the LibGodot reference project (runic-libgodot-reference/godot_build/list_modules.py).
"""

import re
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else "external/godot")
version_file = root / "version.py"
if not version_file.exists():
    raise SystemExit(f"{root} is not a Godot checkout (no version.py)")
version = version_file.read_text()
major = re.search(r"major = (\d+)", version).group(1)
minor = re.search(r"minor = (\d+)", version).group(1)
patch = re.search(r"patch = (\d+)", version)
label = f"{major}.{minor}" + (f".{patch.group(1)}" if patch else "")
print(f"Godot {label} — modules in {root / 'modules'}\n")

dep_call = re.compile(r'module_add_dependencies\("(\w+)",\s*\[([^\]]*)\](?:,\s*(True))?\)')
for config in sorted((root / "modules").glob("*/config.py")):
    name = config.parent.name
    text = config.read_text()
    required, optional = [], []
    for _, deps, is_optional in dep_call.findall(text):
        names = [d.strip().strip('"') for d in deps.split(",") if d.strip()]
        (optional if is_optional else required).extend(names)
    flags = []
    if "def is_enabled" in text and "return False" in text.split("def is_enabled")[1][:200]:
        flags.append("off by default")
    if "return env.editor_build" in text:
        flags.append("editor only")
    line = f"{name:22}"
    if required:
        can_build = text.split("def can_build")[1][:120] if "def can_build" in text else ""
        when = " (editor builds)" if "if env.editor_build" in can_build else ""
        line += f" requires {', '.join(required)}{when}"
    if optional:
        line += f" | optional {', '.join(optional)}"
    if flags:
        line += f" | {', '.join(flags)}"
    print(line)
