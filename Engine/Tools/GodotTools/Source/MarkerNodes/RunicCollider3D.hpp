#pragma once

#include <godot_cpp/classes/editor_node3d_gizmo_plugin.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "CookedFormats.hpp"

// RunicCollider3D (GDT-4; RunicDocs › World authoring and cooking › How collision is marked): a
// box, sphere or capsule placed in the map for invisible walls, stairs and the like. It exists only
// in the editor (the Cooker turns it into a cell's collider; Godot's physics is not in the build)
// and shows as a wireframe gizmo. Its size is in its own space: the node's scale multiplies it when
// it cooks. Private to Runic::GodotTools; its Godot class is public in the editor, so a scene can
// hold it.

namespace Runic::GodotTools::Detail {

class RunicCollider3D : public godot::Node3D {
    GDCLASS(RunicCollider3D, godot::Node3D)

  public:
    enum Shape : int64_t { ShapeBox = 0, ShapeSphere = 1, ShapeCapsule = 2 };

    void SetShape(int64_t shape);
    int64_t GetShape() const noexcept { return m_Shape; }
    void SetSize(godot::Vector3 size); // a box's full size
    godot::Vector3 GetSize() const noexcept { return m_Size; }
    void SetRadius(godot::real_t radius); // a sphere's or a capsule's
    godot::real_t GetRadius() const noexcept { return m_Radius; }
    void SetHeight(godot::real_t height); // a capsule's full height, caps included
    godot::real_t GetHeight() const noexcept { return m_Height; }

    // The collider as the Cooker takes it, at a pose relative to the cooked scene's root (its scale
    // baked into the sizes, the absolute values of its parts).
    Assets::CollisionShape ToCookedShape(const godot::Transform3D &relative) const;

  protected:
    static void _bind_methods();

  private:
    int64_t m_Shape = ShapeBox;
    godot::Vector3 m_Size{1.0f, 1.0f, 1.0f};
    godot::real_t m_Radius = 0.5f;
    godot::real_t m_Height = 2.0f;
};

// Draws a RunicCollider3D in the editor's 3D viewports: its box, its sphere's three circles, or its
// capsule's rings and sides.
class RunicColliderGizmo : public godot::EditorNode3DGizmoPlugin {
    GDCLASS(RunicColliderGizmo, godot::EditorNode3DGizmoPlugin)

  public:
    // Makes its material (after the editor is up).
    void Prepare();
    bool _has_gizmo(godot::Node3D *node) const override;
    godot::String _get_gizmo_name() const override;
    void _redraw(const godot::Ref<godot::EditorNode3DGizmo> &gizmo) override;

  protected:
    static void _bind_methods() {}
};

} // namespace Runic::GodotTools::Detail
