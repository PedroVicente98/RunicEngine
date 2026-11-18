# tests/godot/fixtures/test_region

The first cooker proof's test region (GDT-4, slice 9): a mesh "made in Blender" that Godot imports, a scene that
places it with a collider marker, cooked by the Runic Cooker and walked on.

- `make_test_region.py` writes `test_region.glb` (run it again after changing it, and commit both). No Blender runs
  in CI, so the script writes what Blender's glTF exporter writes for the same objects: metres, Y up, one node and one
  mesh per object, flat normals. Ground, Ramp, Platform and Rock are visual meshes whose collision is the mesh itself
  (the import preset's default); `Rock-rcol` is Rock's collision under the naming convention: never drawn, cooked as
  Rock's collision instead of its visual mesh.
- `test_region.tscn` instances the glTF under `Geometry` and adds `Wall`, a RunicCollider3D box standing on the
  platform at x = 4.

The editor tests copy both into a scratch project's `Content/TestRegion/`; the CTests copy them into the project New
made (`godot/NewProjectCheck`), cook it with the RunicEditor and walk a character on the result with the RunicServer.
