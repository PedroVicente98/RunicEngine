# Content

What Godot imports: assets, the world and region scenes, UI scenes.

## Collision (the Runic Cooker)

Godot imports meshes; the Runic Cooker cooks their collision when the project cooks (Project › Tools › Runic: cook
the project, or before each export). Godot's own physics is not in the game, so Godot's collision shapes and the
`-col` name suffixes do nothing here. How a mesh says what collision it has:

1. **The file's preset.** In the Import dock, **Runic › Collision** for a glTF: *The mesh itself* (the default:
   rocks, buildings, terrain modelled to be walked on), *None* (grass, banners, anything you walk through), or
   *Collision meshes (-rcol)*. *Convex hull* and *Simplified mesh* are listed but not cooked yet.
2. **Collision meshes, named by the convention.** Beside a detailed mesh, model its simple collision in Blender and
   name the object after it with `-rcol`: `Rock` and `Rock-rcol` (several pieces: `Rock-rcol1`, `Rock-rcol2`). Make it
   a child of `Rock` or its sibling. It is never drawn, and it is `Rock`'s collision whatever the file's preset.
3. **RunicCollider3D.** For invisible walls, stairs and the like, add a RunicCollider3D (a box, sphere or capsule) to
   the scene. It is drawn only in the editor.

Units are metres, Y up, scale 1: Blender's glTF exporter converts from Blender's Z up. Scale a placement freely
(each scale cooks its own collision), but not by a negative number. Every asset gets its ID from the UID Godot gives
its file: keep the `.import` files in version control.
