// RunicCollider3D and its gizmo (RunicCollider3D.hpp).

#include "MarkerNodes/RunicCollider3D.hpp"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/editor_node3d_gizmo.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>

#include <glm/ext/scalar_constants.hpp>

namespace Runic::GodotTools::Detail {

namespace {

constexpr const char *kShapeProperty = "shape";
constexpr const char *kSizeProperty = "size";
constexpr const char *kRadiusProperty = "radius";
constexpr const char *kHeightProperty = "height";
constexpr const char *kShapeHint = "Box,Sphere,Capsule";
constexpr const char *kGizmoName = "RunicCollider3D";
constexpr const char *kGizmoMaterial = "runic_collider";
// Like Godot's own collision shapes: a light blue the viewport's grid does not hide.
const godot::Color kGizmoColor(0.5f, 0.7f, 1.0f);
// Smallest sizes the inspector accepts.
constexpr godot::real_t kMinimumSizeM = 0.001f;
constexpr int kCircleSegments = 32;
constexpr godot::real_t kHalf = 0.5f;
constexpr godot::real_t kTwoPi = glm::two_pi<godot::real_t>();
constexpr godot::real_t kPi = glm::pi<godot::real_t>();
constexpr godot::real_t kNoLength = 0.0f;
constexpr godot::real_t kUp = 1.0f;
constexpr godot::real_t kDown = -1.0f;
constexpr const char *kSetShapeMethod = "set_shape";
constexpr const char *kGetShapeMethod = "get_shape";
constexpr const char *kSetSizeMethod = "set_size";
constexpr const char *kGetSizeMethod = "get_size";
constexpr const char *kSetRadiusMethod = "set_radius";
constexpr const char *kGetRadiusMethod = "get_radius";
constexpr const char *kSetHeightMethod = "set_height";
constexpr const char *kGetHeightMethod = "get_height";

// A circle (or, with a sweep of pi, the half from u through v) around center in the plane of u and
// v.
void Circle(godot::PackedVector3Array &lines, godot::Vector3 center, godot::Vector3 u,
            godot::Vector3 v, godot::real_t radius, godot::real_t sweep = kTwoPi) {
    const auto segments = static_cast<godot::real_t>(kCircleSegments);
    for (int i = 0; i < kCircleSegments; ++i) {
        const godot::real_t a = sweep * static_cast<godot::real_t>(i) / segments;
        const godot::real_t b = sweep * static_cast<godot::real_t>(i + 1) / segments;
        lines.push_back(center + (u * std::cos(a) + v * std::sin(a)) * radius);
        lines.push_back(center + (u * std::cos(b) + v * std::sin(b)) * radius);
    }
}

} // namespace

void RunicCollider3D::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD(kSetShapeMethod, kShapeProperty), &RunicCollider3D::SetShape);
    ClassDB::bind_method(D_METHOD(kGetShapeMethod), &RunicCollider3D::GetShape);
    ClassDB::bind_method(D_METHOD(kSetSizeMethod, kSizeProperty), &RunicCollider3D::SetSize);
    ClassDB::bind_method(D_METHOD(kGetSizeMethod), &RunicCollider3D::GetSize);
    ClassDB::bind_method(D_METHOD(kSetRadiusMethod, kRadiusProperty), &RunicCollider3D::SetRadius);
    ClassDB::bind_method(D_METHOD(kGetRadiusMethod), &RunicCollider3D::GetRadius);
    ClassDB::bind_method(D_METHOD(kSetHeightMethod, kHeightProperty), &RunicCollider3D::SetHeight);
    ClassDB::bind_method(D_METHOD(kGetHeightMethod), &RunicCollider3D::GetHeight);
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, kShapeProperty, godot::PROPERTY_HINT_ENUM,
                                     kShapeHint),
                 kSetShapeMethod, kGetShapeMethod);
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::VECTOR3, kSizeProperty), kSetSizeMethod,
                 kGetSizeMethod);
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, kRadiusProperty), kSetRadiusMethod,
                 kGetRadiusMethod);
    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, kHeightProperty), kSetHeightMethod,
                 kGetHeightMethod);
}

void RunicCollider3D::SetShape(int64_t shape) {
    m_Shape = std::clamp<int64_t>(shape, ShapeBox, ShapeCapsule);
    update_gizmos();
}

void RunicCollider3D::SetSize(godot::Vector3 size) {
    m_Size = godot::Vector3(std::max(size.x, kMinimumSizeM), std::max(size.y, kMinimumSizeM),
                            std::max(size.z, kMinimumSizeM));
    update_gizmos();
}

void RunicCollider3D::SetRadius(godot::real_t radius) {
    m_Radius = std::max(radius, kMinimumSizeM);
    update_gizmos();
}

void RunicCollider3D::SetHeight(godot::real_t height) {
    m_Height = std::max(height, kMinimumSizeM);
    update_gizmos();
}

Assets::CollisionShape RunicCollider3D::ToCookedShape(const godot::Transform3D &relative) const {
    const godot::Vector3 scale = relative.basis.get_scale().abs();
    const godot::Quaternion rotation = relative.basis.orthonormalized().get_rotation_quaternion();
    Assets::CollisionShape shape;
    shape.position = Core::Vec3(relative.origin.x, relative.origin.y, relative.origin.z);
    shape.rotation = Core::Quat(rotation.w, rotation.x, rotation.y, rotation.z);
    const godot::real_t across = std::max(scale.x, scale.z);
    switch (m_Shape) {
    case ShapeSphere:
        shape.kind = Assets::CollisionShapeKind::Sphere;
        shape.radius = m_Radius * std::max(across, scale.y);
        break;
    case ShapeCapsule:
        shape.kind = Assets::CollisionShapeKind::Capsule;
        shape.radius = m_Radius * across;
        shape.halfHeight = std::max(kNoLength, (m_Height * kHalf - m_Radius) * scale.y);
        break;
    default:
        shape.kind = Assets::CollisionShapeKind::Box;
        shape.halfExtents = Core::Vec3(m_Size.x * kHalf * scale.x, m_Size.y * kHalf * scale.y,
                                       m_Size.z * kHalf * scale.z);
        break;
    }
    return shape;
}

void RunicColliderGizmo::Prepare() { create_material(kGizmoMaterial, kGizmoColor); }

bool RunicColliderGizmo::_has_gizmo(godot::Node3D *node) const {
    return godot::Object::cast_to<RunicCollider3D>(node) != nullptr;
}

godot::String RunicColliderGizmo::_get_gizmo_name() const { return kGizmoName; }

void RunicColliderGizmo::_redraw(const godot::Ref<godot::EditorNode3DGizmo> &gizmo) {
    gizmo->clear();
    const auto *collider = godot::Object::cast_to<RunicCollider3D>(gizmo->get_node_3d());
    if (collider == nullptr) {
        return;
    }
    const godot::Vector3 x(1, 0, 0);
    const godot::Vector3 y(0, 1, 0);
    const godot::Vector3 z(0, 0, 1);
    godot::PackedVector3Array lines;
    switch (collider->GetShape()) {
    case RunicCollider3D::ShapeSphere:
        Circle(lines, godot::Vector3(), x, y, collider->GetRadius());
        Circle(lines, godot::Vector3(), y, z, collider->GetRadius());
        Circle(lines, godot::Vector3(), x, z, collider->GetRadius());
        break;
    case RunicCollider3D::ShapeCapsule: {
        const godot::real_t r = collider->GetRadius();
        const godot::real_t half = std::max(kNoLength, collider->GetHeight() * kHalf - r);
        for (const godot::real_t end : {half, -half}) {
            const godot::Vector3 away = y * (end >= kNoLength ? kUp : kDown);
            Circle(lines, y * end, x, z, r);
            // The cap's two half circles, bulging away from the cylinder.
            Circle(lines, y * end, x, away, r, kPi);
            Circle(lines, y * end, z, away, r, kPi);
        }
        for (const godot::Vector3 side : {x, -x, z, -z}) {
            lines.push_back(side * r + y * half);
            lines.push_back(side * r - y * half);
        }
        break;
    }
    default: {
        const godot::Vector3 h = collider->GetSize() * kHalf;
        const godot::Vector3 corners[8] = {{-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z},
                                           {-h.x, h.y, -h.z},  {-h.x, -h.y, h.z}, {h.x, -h.y, h.z},
                                           {h.x, h.y, h.z},    {-h.x, h.y, h.z}};
        constexpr int kEdges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                       {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto &edge : kEdges) {
            lines.push_back(corners[edge[0]]);
            lines.push_back(corners[edge[1]]);
        }
        break;
    }
    }
    gizmo->add_lines(lines, get_material(kGizmoMaterial, gizmo));
}

} // namespace Runic::GodotTools::Detail
