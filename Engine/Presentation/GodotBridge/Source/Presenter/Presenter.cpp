// The presenter (Presenter.hpp; RunicDocs › Client runtime › Presenter origin): a frame's shape
// instances become RenderingServer instances, applied by what changed, in Godot's space around the
// presenter's own origin.

#include "Presenter/Presenter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "Assert.hpp"
#include "GodotLayer/SceneRoot.hpp"
#include "Log.hpp"

namespace Runic::GodotBridge::Detail {

namespace {

using godot::RenderingServer;
using RenderData::ShapeInstance;
using RenderData::ShapeKind;

// On a match by ID, a place that differs by less than this is where the presenter already put the
// shape: the difference is the rounding of two frame origins, not a move.
constexpr double kSamePlaceM = 0.001;
constexpr double kSamePlaceSqM = kSamePlaceM * kSamePlaceM;
constexpr double kRebaseDistanceSqM = kRebaseDistanceM * kRebaseDistanceM;

// The unit shapes: 1 m across and 1 m high, few faces (debug shapes, possibly many of them).
const godot::Vector3 kUnitBox(1.0f, 1.0f, 1.0f);
constexpr float kUnitRadiusM = 0.5f;
constexpr float kUnitHeightM = 1.0f;
constexpr int kRoundSegments = 16;
constexpr int kSphereRings = 8;
constexpr int kCylinderRings = 1;
constexpr int kCapsuleRings = 4;
// A size below a millimetre is drawn as a millimetre, so no scale is zero.
constexpr float kSmallestSizeM = 0.001f;
// Capsule meshes are kept per size, to the millimetre.
constexpr float kMillimetresPerMetre = 1000.0f;
constexpr int kCapsuleKeyShift = 32;
constexpr float kHalf = 0.5f;

constexpr float kChannelMax = 255.0f;
constexpr std::uint8_t kOpaque = 255;

godot::Color ToColor(RenderData::PackedColor color) {
    return godot::Color(
        RenderData::RedOf(color) / kChannelMax, RenderData::GreenOf(color) / kChannelMax,
        RenderData::BlueOf(color) / kChannelMax, RenderData::AlphaOf(color) / kChannelMax);
}

// Godot's coordinates of a place relative to the presenter's origin. A shape is within the draw
// distance of the camera (at most Core::kMaxLocalOffsetM) and the camera within kRebaseDistanceM
// of the origin, so the offset stays under 14 km, where a float's step is a millimetre: the one
// conversion down to 32 bits that ToLocal's tighter limit would refuse.
Core::Vec3 ToPresenterSpace(Core::WorldDelta delta) noexcept {
    return {static_cast<float>(delta.x), static_cast<float>(delta.y), static_cast<float>(delta.z)};
}

godot::Transform3D TransformOf(Core::Quat rotation, Core::Vec3 scale, Core::Vec3 position) {
    const godot::Quaternion turn(rotation.x, rotation.y, rotation.z, rotation.w);
    return godot::Transform3D(godot::Basis(turn, godot::Vector3(scale.x, scale.y, scale.z)),
                              godot::Vector3(position.x, position.y, position.z));
}

Core::Vec3 AtLeastSmallest(Core::Vec3 size) noexcept {
    return {std::max(size.x, kSmallestSizeM), std::max(size.y, kSmallestSizeM),
            std::max(size.z, kSmallestSizeM)};
}

std::uint32_t Millimetres(float metres) {
    return static_cast<std::uint32_t>(std::lround(metres * kMillimetresPerMetre));
}

} // namespace

Presenter::~Presenter() { Destroy(); }

void Presenter::Create() {
    godot::Window *root = SceneRoot();
    if (m_Created || root == nullptr || RenderingServer::get_singleton() == nullptr) {
        return;
    }
    m_Scenario = root->get_world_3d()->get_scenario();

    godot::Ref<godot::BoxMesh> box;
    box.instantiate();
    box->set_size(kUnitBox);
    godot::Ref<godot::SphereMesh> sphere;
    sphere.instantiate();
    sphere->set_radius(kUnitRadiusM);
    sphere->set_height(kUnitHeightM);
    sphere->set_radial_segments(kRoundSegments);
    sphere->set_rings(kSphereRings);
    godot::Ref<godot::CylinderMesh> cylinder;
    cylinder.instantiate();
    cylinder->set_top_radius(kUnitRadiusM);
    cylinder->set_bottom_radius(kUnitRadiusM);
    cylinder->set_height(kUnitHeightM);
    cylinder->set_radial_segments(kRoundSegments);
    cylinder->set_rings(kCylinderRings);
    m_UnitMeshes[static_cast<std::size_t>(ShapeKind::Box)] = box;
    m_UnitMeshes[static_cast<std::size_t>(ShapeKind::Sphere)] = sphere;
    m_UnitMeshes[static_cast<std::size_t>(ShapeKind::Cylinder)] = cylinder;
    // Capsules have no unit mesh: MeshFor makes one per size.
    m_Created = true;
}

void Presenter::Destroy() {
    if (!m_Created) {
        return;
    }
    RUNIC_LOG_INFO("RUNIC: presenter stopped at frame {}: {} instances, {} rebases", m_Stats.frame,
                   m_Stats.instances, m_Stats.rebases);
    if (RenderingServer *server = RenderingServer::get_singleton()) {
        for (Slot &slot : m_Slots) {
            if (slot.live) {
                server->free_rid(slot.instance);
            }
        }
    }
    m_Slots.clear();
    m_FreeSlots.clear();
    m_SlotOfId.clear();
    m_SlotOfIndex.clear();
    for (godot::Ref<godot::PrimitiveMesh> &mesh : m_UnitMeshes) {
        mesh.unref();
    }
    m_Capsules.clear();
    m_Materials.clear();
    m_Created = false;
}

Core::Vec3 Presenter::Apply(const RenderData::RenderFrame &frame) {
    m_Stats.frame = frame.number;
    m_Stats.created = 0;
    m_Stats.updated = 0;
    m_Stats.freed = 0;
    m_Stats.serverCalls = 0;
    m_Stats.comparedById = false;
    if (!m_Created) {
        return Core::Vec3(0.0f);
    }
    const Core::TimePoint start = Core::MonotonicNow();

    if (!m_HasOrigin) {
        m_Origin = frame.origin;
        m_HasOrigin = true;
    } else if (Core::DistanceSq(frame.origin, m_Origin) > kRebaseDistanceSqM) {
        Rebase(frame.origin);
    }

    const RenderData::ShapeInstanceList &shapes = frame.shapes;
    if (!m_HasApplied || shapes.Revision() != m_AppliedRevision) {
        if (m_HasApplied && !shapes.LayoutChanged() && shapes.BaseRevision() == m_AppliedRevision) {
            ApplyChanged(shapes, frame.origin);
        } else {
            Reconcile(shapes, frame.origin);
        }
        m_AppliedRevision = shapes.Revision();
        m_HasApplied = true;
    }
    m_Stats.instances = static_cast<std::uint32_t>(m_SlotOfId.size());
    m_Stats.origin = m_Origin;
    m_Stats.applyTime = Core::MonotonicNow() - start;
    return Core::ToLocal(frame.origin - m_Origin);
}

void Presenter::Rebase(Core::WorldPosition origin) {
    // One large, rare shift: every instance moves by the same amount in Godot's space.
    m_Origin = origin;
    ++m_Stats.rebases;
    for (Slot &slot : m_Slots) {
        if (slot.live) {
            Place(slot);
        }
    }
}

void Presenter::ApplyChanged(const RenderData::ShapeInstanceList &shapes,
                             Core::WorldPosition origin) {
    const auto instances = shapes.Instances();
    for (const std::uint32_t index : shapes.Changed()) {
        const std::uint32_t slot = m_SlotOfIndex[index];
        if (slot != kNoSlot) {
            Write(m_Slots[slot], instances[index], origin, true);
        }
    }
}

void Presenter::Reconcile(const RenderData::ShapeInstanceList &shapes, Core::WorldPosition origin) {
    m_Stats.comparedById = true;
    ++m_Pass;
    const auto instances = shapes.Instances();
    m_SlotOfIndex.assign(instances.size(), kNoSlot);
    for (std::size_t index = 0; index < instances.size(); ++index) {
        const ShapeInstance &shape = instances[index];
        const auto found = m_SlotOfId.find(shape.id.Value());
        std::uint32_t slot = kNoSlot;
        if (found == m_SlotOfId.end()) {
            slot = NewSlot(shape, origin);
        } else {
            slot = found->second;
            if (m_Slots[slot].seen == m_Pass) {
                RUNIC_ASSERT_MSG(false, "two instances of one frame share a RenderInstanceId");
                continue; // without asserts: the first one wins
            }
            Write(m_Slots[slot], shape, origin, false);
        }
        m_Slots[slot].seen = m_Pass;
        m_SlotOfIndex[index] = slot;
    }
    // What the frame no longer has leaves Godot.
    RenderingServer *server = RenderingServer::get_singleton();
    for (std::uint32_t slot = 0; slot < m_Slots.size(); ++slot) {
        Slot &unseen = m_Slots[slot];
        if (!unseen.live || unseen.seen == m_Pass) {
            continue;
        }
        server->free_rid(unseen.instance);
        ++m_Stats.serverCalls;
        m_SlotOfId.erase(unseen.id.Value());
        unseen = Slot{};
        m_FreeSlots.push_back(slot);
        ++m_Stats.freed;
    }
}

std::uint32_t Presenter::NewSlot(const ShapeInstance &shape, Core::WorldPosition origin) {
    std::uint32_t index = 0;
    if (m_FreeSlots.empty()) {
        index = static_cast<std::uint32_t>(m_Slots.size());
        m_Slots.emplace_back();
    } else {
        index = m_FreeSlots.back();
        m_FreeSlots.pop_back();
    }
    Slot &slot = m_Slots[index];
    RenderingServer *server = RenderingServer::get_singleton();
    slot.id = shape.id;
    slot.shape = shape.shape;
    slot.size = shape.size;
    slot.color = shape.color;
    slot.rotation = shape.rotation;
    slot.world = origin + shape.position;
    slot.mesh = MeshFor(shape.shape, shape.size, slot.scale);
    slot.instance = server->instance_create2(slot.mesh, m_Scenario);
    server->instance_geometry_set_material_override(slot.instance, MaterialFor(shape.color));
    m_Stats.serverCalls += 2;
    slot.live = true;
    Place(slot);
    m_SlotOfId.emplace(shape.id.Value(), index);
    ++m_Stats.created;
    return index;
}

void Presenter::Write(Slot &slot, const ShapeInstance &shape, Core::WorldPosition origin,
                      bool exact) {
    RenderingServer *server = RenderingServer::get_singleton();
    const Core::WorldPosition world = origin + shape.position;
    bool place =
        slot.rotation != shape.rotation ||
        (exact ? world != slot.world : Core::DistanceSq(world, slot.world) > kSamePlaceSqM);
    bool changed = false;
    if (slot.shape != shape.shape || slot.size != shape.size) {
        const godot::RID mesh = MeshFor(shape.shape, shape.size, slot.scale);
        if (mesh != slot.mesh) {
            server->instance_set_base(slot.instance, mesh);
            ++m_Stats.serverCalls;
            slot.mesh = mesh;
        }
        slot.shape = shape.shape;
        slot.size = shape.size;
        place = true; // the scale is part of the transform
        changed = true;
    }
    if (slot.color != shape.color) {
        server->instance_geometry_set_material_override(slot.instance, MaterialFor(shape.color));
        ++m_Stats.serverCalls;
        slot.color = shape.color;
        changed = true;
    }
    if (place) {
        slot.world = world;
        slot.rotation = shape.rotation;
        Place(slot);
        changed = true;
    }
    if (changed) {
        ++m_Stats.updated;
    }
}

void Presenter::Place(Slot &slot) {
    slot.position = ToPresenterSpace(slot.world - m_Origin);
    RenderingServer::get_singleton()->instance_set_transform(
        slot.instance, TransformOf(slot.rotation, slot.scale, slot.position));
    ++m_Stats.serverCalls;
}

godot::RID Presenter::MeshFor(ShapeKind shape, Core::Vec3 size, Core::Vec3 &scale) {
    const Core::Vec3 drawn = AtLeastSmallest(size);
    if (shape != ShapeKind::Capsule) {
        scale = drawn;
        return m_UnitMeshes[static_cast<std::size_t>(shape)]->get_rid();
    }
    // A capsule keeps round ends: a mesh of its own diameter (x) and height, at least a sphere;
    // a different depth (z) only flattens it.
    const float radius = drawn.x * kHalf;
    const float height = std::max(drawn.y, drawn.x);
    const std::uint64_t key =
        (static_cast<std::uint64_t>(Millimetres(radius)) << kCapsuleKeyShift) | Millimetres(height);
    auto found = m_Capsules.find(key);
    if (found == m_Capsules.end()) {
        godot::Ref<godot::CapsuleMesh> capsule;
        capsule.instantiate();
        capsule->set_radius(radius);
        capsule->set_height(height);
        capsule->set_radial_segments(kRoundSegments);
        capsule->set_rings(kCapsuleRings);
        found = m_Capsules.emplace(key, capsule).first;
    }
    scale = Core::Vec3(1.0f, 1.0f, drawn.z / drawn.x);
    return found->second->get_rid();
}

godot::RID Presenter::MaterialFor(RenderData::PackedColor color) {
    auto found = m_Materials.find(color);
    if (found == m_Materials.end()) {
        godot::Ref<godot::StandardMaterial3D> material;
        material.instantiate();
        material->set_albedo(ToColor(color));
        if (RenderData::AlphaOf(color) < kOpaque) {
            material->set_transparency(godot::BaseMaterial3D::TRANSPARENCY_ALPHA);
        }
        found = m_Materials.emplace(color, material).first;
    }
    return found->second->get_rid();
}

bool Presenter::ReadBack(Core::RenderInstanceId id, PresentedInstance &out) const {
    const auto found = m_SlotOfId.find(id.Value());
    if (found == m_SlotOfId.end()) {
        return false;
    }
    const Slot &slot = m_Slots[found->second];
    out.shape = slot.shape;
    out.color = slot.color;
    out.position = slot.position;
    out.rotation = slot.rotation;
    out.scale = slot.scale;
    return true;
}

} // namespace Runic::GodotBridge::Detail
