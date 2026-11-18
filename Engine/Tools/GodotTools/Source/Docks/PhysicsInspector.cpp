// The physics inspector's model (PhysicsInspector.hpp).

#include "PhysicsInspector.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

#include <glm/gtc/quaternion.hpp>

#include "InspectorText.hpp"
#include "PhysicsBridge.hpp"
#include "RenderData/DebugDraw.hpp"

namespace Runic::GodotTools {

namespace {

using Physics::BodyHandle;
using Physics::MotionType;
using Physics::ShapeKind;

constexpr std::string_view kBodyLabelFormat = "body {}";
constexpr std::string_view kStaticMotion = "static";
constexpr std::string_view kKinematicMotion = "kinematic";
constexpr std::string_view kDynamicMotion = "dynamic";
constexpr std::string_view kBoxKind = "box";
constexpr std::string_view kSphereKind = "sphere";
constexpr std::string_view kCapsuleKind = "capsule";
constexpr std::string_view kMeshKindFormat = "mesh, {} triangles";
constexpr std::string_view kPositionFormat = "{:.3f}, {:.3f}, {:.3f}";
constexpr std::string_view kHalfExtentsFormat = "{:.3f}, {:.3f}, {:.3f} m";
constexpr std::string_view kVelocityFormat = "{:.3f}, {:.3f}, {:.3f} m/s";
constexpr std::string_view kRotationFormat = "w {:.3f}, x {:.3f}, y {:.3f}, z {:.3f}";
constexpr std::string_view kCoefficientFormat = "{:.2f}";
constexpr std::string_view kAwakeState = "awake";
constexpr std::string_view kAsleepState = "asleep";
constexpr std::string_view kStaticState = "never moves";
constexpr std::string_view kYes = "yes";
constexpr std::string_view kNo = "no";
constexpr std::string_view kNoOwner = "none";

// BodyDesc::owner of a body nobody owns.
constexpr std::uint64_t kUnowned = 0;
// Half the last digit shown: smaller values show as 0.000, never as -0.000.
constexpr double kShownZero = 0.0005;
// A ray's segment fits in the sphere around its middle whose radius is half its length.
constexpr double kHalf = 0.5;
// A mesh's bounds are not centred on its body (Physics' ShapeInfo has their size only): its
// selection is an arrow coming down onto the body's origin.
constexpr float kMeshMarkerHeightM = 5.0f;

const Core::Vec3 kAxisX(1.0f, 0.0f, 0.0f);
const Core::Vec3 kAxisY(0.0f, 1.0f, 0.0f);
const Core::Vec3 kAxisZ(0.0f, 0.0f, 1.0f);

double Shown(double value) noexcept { return std::abs(value) < kShownZero ? 0.0 : value; }

std::string_view MotionName(MotionType motion) noexcept {
    switch (motion) {
    case MotionType::Static:
        return kStaticMotion;
    case MotionType::Kinematic:
        return kKinematicMotion;
    case MotionType::Dynamic:
        return kDynamicMotion;
    }
    return kStaticMotion;
}

std::string KindName(const Physics::ShapeInfo &shape) {
    switch (shape.kind) {
    case ShapeKind::Box:
        return std::string(kBoxKind);
    case ShapeKind::Sphere:
        return std::string(kSphereKind);
    case ShapeKind::Capsule:
        return std::string(kCapsuleKind);
    case ShapeKind::Mesh:
        return std::format(kMeshKindFormat, shape.triangles);
    }
    return std::string(kBoxKind);
}

std::string_view StateName(const Physics::BodyInfo &body) noexcept {
    if (body.motion == MotionType::Static) {
        return kStaticState;
    }
    return body.active ? kAwakeState : kAsleepState;
}

std::string Vector(std::string_view format, double x, double y, double z) {
    return std::vformat(format, std::make_format_args(x, y, z));
}

// Where a ray from origin along unit direction first meets the box of half sizes half centred on
// the origin (slabs); t is 0 when the ray starts inside.
bool RayMeetsBox(glm::dvec3 origin, glm::dvec3 direction, glm::dvec3 half, double &t) {
    double enter = 0.0;
    double leave = std::numeric_limits<double>::max();
    for (int axis = 0; axis < glm::dvec3::length(); ++axis) {
        if (direction[axis] == 0.0) {
            if (std::abs(origin[axis]) > half[axis]) {
                return false;
            }
            continue;
        }
        double near = (-half[axis] - origin[axis]) / direction[axis];
        double far = (half[axis] - origin[axis]) / direction[axis];
        if (near > far) {
            std::swap(near, far);
        }
        enter = std::max(enter, near);
        leave = std::min(leave, far);
        if (enter > leave) {
            return false;
        }
    }
    t = enter;
    return true;
}

// Where a ray from origin along unit direction first meets the sphere of radius centred on the
// origin; t is 0 when the ray starts inside.
bool RayMeetsSphere(glm::dvec3 origin, glm::dvec3 direction, double radius, double &t) {
    const double along = glm::dot(origin, direction);
    const double outside = glm::dot(origin, origin) - radius * radius;
    if (outside > 0.0 && along > 0.0) {
        return false; // outside and pointing away
    }
    const double discriminant = along * along - outside;
    if (discriminant < 0.0) {
        return false;
    }
    t = std::max(0.0, -along - std::sqrt(discriminant));
    return true;
}

} // namespace

PhysicsSummary PhysicsInspector::Summary() const {
    PhysicsSummary summary;
    const Simulation::PhysicsBridge *bridge = m_Runtime.FindPhysicsBridge();
    if (bridge == nullptr) {
        return summary;
    }
    const Physics::PhysicsStats stats = bridge->World().Stats();
    const Simulation::PhysicsBridgeStats owned = bridge->Stats();
    summary.enabled = true;
    summary.bodies = stats.bodies;
    summary.activeBodies = stats.activeBodies;
    summary.shapes = stats.shapes;
    summary.characters = stats.characters;
    summary.steps = stats.steps;
    summary.entityBodies = owned.bodies + owned.characters;
    return summary;
}

std::vector<BodyHandle> PhysicsInspector::Bodies(Core::WorldPosition center, double radiusM,
                                                 std::uint32_t limit) const {
    std::vector<BodyHandle> bodies;
    const Simulation::PhysicsBridge *bridge = m_Runtime.FindPhysicsBridge();
    if (bridge == nullptr) {
        return bodies;
    }
    bridge->World().FindBodies(center, radiusM, bodies);
    std::sort(bodies.begin(), bodies.end(),
              [](BodyHandle a, BodyHandle b) { return a.Value() < b.Value(); });
    if (bodies.size() > limit) {
        bodies.resize(limit);
    }
    return bodies;
}

InspectedBody PhysicsInspector::Inspect(BodyHandle body) const {
    InspectedBody inspected;
    inspected.body = body;
    inspected.label = std::format(kBodyLabelFormat, body.Value());
    const Simulation::PhysicsBridge *bridge = m_Runtime.FindPhysicsBridge();
    if (bridge == nullptr || !body.IsValid() || !bridge->World().IsBody(body)) {
        return inspected;
    }
    const Physics::PhysicsWorld &world = bridge->World();
    const Physics::BodyInfo info = world.DescribeBody(body);
    const Physics::ShapeInfo shape = world.DescribeShape(info.shape);
    inspected.shape = std::string(shape.name);
    inspected.motion = std::string(MotionName(info.motion));

    const auto add = [&](std::string_view name, std::string value) {
        inspected.properties.push_back({std::string(name), std::move(value)});
    };
    add(kBodyShapeProperty, std::string(shape.name));
    add(kBodyKindProperty, KindName(shape));
    add(kBodyHalfExtentsProperty, Vector(kHalfExtentsFormat, Shown(shape.halfExtents.x),
                                         Shown(shape.halfExtents.y), Shown(shape.halfExtents.z)));
    add(kBodyMotionProperty, inspected.motion);
    add(kBodyLayerProperty, std::string(world.Layers().Describe(info.layer).name));
    add(kBodyPositionProperty, Vector(kPositionFormat, Shown(info.pose.position.x),
                                      Shown(info.pose.position.y), Shown(info.pose.position.z)));
    const double w = Shown(info.pose.rotation.w);
    const double x = Shown(info.pose.rotation.x);
    const double y = Shown(info.pose.rotation.y);
    const double z = Shown(info.pose.rotation.z);
    add(kBodyRotationProperty, std::vformat(kRotationFormat, std::make_format_args(w, x, y, z)));
    add(kBodyVelocityProperty, Vector(kVelocityFormat, Shown(info.linearVelocity.x),
                                      Shown(info.linearVelocity.y), Shown(info.linearVelocity.z)));
    const double friction = Shown(info.friction);
    const double restitution = Shown(info.restitution);
    add(kBodyFrictionProperty, std::vformat(kCoefficientFormat, std::make_format_args(friction)));
    add(kBodyRestitutionProperty,
        std::vformat(kCoefficientFormat, std::make_format_args(restitution)));
    add(kBodyStateProperty, std::string(StateName(info)));
    add(kBodySensorProperty, std::string(info.sensor ? kYes : kNo));
    add(kBodyOwnerProperty,
        info.owner == kUnowned ? std::string(kNoOwner) : Detail::EntityLabel(info.owner));
    return inspected;
}

BodyHandle PhysicsInspector::Pick(Core::WorldPosition origin, Core::Vec3 direction,
                                  double maxDistanceM) const {
    const Simulation::PhysicsBridge *bridge = m_Runtime.FindPhysicsBridge();
    const double length = glm::length(glm::dvec3(direction));
    if (bridge == nullptr || length <= 0.0 || maxDistanceM <= 0.0) {
        return {};
    }
    const Physics::PhysicsWorld &world = bridge->World();
    const glm::dvec3 unit = glm::dvec3(direction) / length;
    const double half = maxDistanceM * kHalf;
    const Core::WorldPosition middle = origin + Core::WorldDelta{unit.x, unit.y, unit.z} * half;
    std::vector<BodyHandle> candidates;
    world.FindBodies(middle, half, candidates);

    BodyHandle nearest;
    double nearestT = maxDistanceM;
    for (const BodyHandle candidate : candidates) {
        const Physics::BodyInfo body = world.DescribeBody(candidate);
        const Physics::ShapeInfo shape = world.DescribeShape(body.shape);
        if (shape.kind == ShapeKind::Mesh) {
            continue;
        }
        // The ray in the body's own space, in doubles: the world is far from its origin.
        const Core::WorldDelta offset = origin - body.pose.position;
        const glm::dquat toBody = glm::conjugate(glm::dquat(body.pose.rotation));
        const glm::dvec3 from = toBody * glm::dvec3(offset.x, offset.y, offset.z);
        const glm::dvec3 along = toBody * unit;
        double t = 0.0;
        const bool meets = shape.kind == ShapeKind::Sphere
                               ? RayMeetsSphere(from, along, shape.halfExtents.x, t)
                               : RayMeetsBox(from, along, glm::dvec3(shape.halfExtents), t);
        if (meets && t < nearestT) {
            nearest = candidate;
            nearestT = t;
        }
    }
    return nearest;
}

BodyHandle PhysicsInspector::Selected() const {
    const Simulation::PhysicsBridge *bridge = m_Runtime.FindPhysicsBridge();
    if (bridge == nullptr || !m_Selected.IsValid() || !bridge->World().IsBody(m_Selected)) {
        return {};
    }
    return m_Selected;
}

ServerViewStats PhysicsInspector::DrawServerView(Core::WorldPosition origin,
                                                 const ServerViewSettings &settings,
                                                 RenderData::DebugGeometry &out,
                                                 RenderData::DebugMeshLibrary *meshes) {
    ServerViewStats stats;
    Simulation::PhysicsBridge *bridge = m_Runtime.FindPhysicsBridge();
    if (bridge == nullptr) {
        return stats;
    }
    Physics::PhysicsWorld &world = bridge->World();
    Physics::PhysicsDebugSettings debug;
    debug.radiusM = settings.radiusM;
    debug.faces = settings.faces;
    debug.velocities = settings.velocities;
    debug.batch =
        settings.overVisuals ? RenderData::DebugBatch::Overlay : RenderData::DebugBatch::WorldDepth;
    stats.physics = world.ExtractDebugGeometry(origin, debug, out, meshes);

    const BodyHandle selected = Selected();
    if (!selected.IsValid()) {
        return stats;
    }
    const Physics::BodyInfo body = world.DescribeBody(selected);
    const Core::WorldDelta offset = body.pose.position - origin;
    const double reach = Core::kMaxLocalOffsetM;
    if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z >= reach * reach) {
        return stats; // too far from the frame's origin to stand in its floats
    }
    const Physics::ShapeInfo shape = world.DescribeShape(body.shape);
    const Core::Vec3 center = Core::ToLocal(offset);
    const std::size_t before =
        out.VertexCount(RenderData::DebugBatch::Overlay, RenderData::DebugPrimitive::Lines);
    RenderData::DebugDraw draw(out, RenderData::DebugBatch::Overlay);
    if (shape.kind == ShapeKind::Mesh) {
        draw.DrawArrow(center + kAxisY * kMeshMarkerHeightM, center, kSelectedBodyColor);
    } else {
        const Core::Quat &rotation = body.pose.rotation;
        draw.DrawOBB(center, rotation * kAxisX, rotation * kAxisY, rotation * kAxisZ,
                     shape.halfExtents + Core::Vec3(kSelectionPaddingM), kSelectedBodyColor);
    }
    stats.selectionDrawn = out.VertexCount(RenderData::DebugBatch::Overlay,
                                           RenderData::DebugPrimitive::Lines) > before;
    return stats;
}

} // namespace Runic::GodotTools
