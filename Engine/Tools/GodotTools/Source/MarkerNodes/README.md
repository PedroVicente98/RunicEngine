# Runic::GodotTools › Source/MarkerNodes

Marker nodes (RunicVolume3D, RunicSpawnArea3D, RunicInteractable3D, RunicRoute3D, RunicCollider3D) and spawn-area previews; they exist only in the editor, the Cooker writes placements.

- **Filled by:** GDT-4 (slice 9), GDT-7 (slice 21)
- **Status:** GDT-4 (slice 9) wrote `RunicCollider3D` (a box, sphere or capsule collider marker) and its gizmo; the other marker nodes are GDT-7's (slice 21)

Private to `Runic::GodotTools`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotTools` (or `Runic::GodotTools::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
