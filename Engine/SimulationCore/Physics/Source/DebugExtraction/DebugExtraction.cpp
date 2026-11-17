// The debug view and inspection (PhysicsWorld.hpp, PHYS-3): the bodies and characters near a point
// through Jolt's debug interface into Core debug geometry, large shapes as cached meshes, and the
// plain descriptions tools read.

// Jolt.h comes before any other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "Assert.hpp"
#include "DebugExtraction/DebugCapture.hpp"
#include "PhysicsWorld/WorldState.hpp"

namespace Runic::Physics {

namespace Detail {

namespace {

#ifdef JPH_DEBUG_RENDERER
// The velocity arrow shows where a body will be in a second; its head is this long.
constexpr float kVelocityArrowSeconds = 1.0f;
constexpr float kArrowHeadM = 0.15f;
// A body slower than this has no arrow.
constexpr float kStillMps = 1e-3f;
constexpr std::uint8_t kOpaque = 255;
// Shapes are drawn at their own size.
const JPH::Vec3 kUnitScale = JPH::Vec3::sOne();
constexpr bool kWireframe = true;
constexpr bool kFaces = false;
constexpr bool kNoMaterialColors = false;
#endif

[[maybe_unused]] RenderData::PackedColor WithAlpha(RenderData::PackedColor color,
                                                   std::uint8_t alpha) noexcept {
    return RenderData::PackABGR(RenderData::RedOf(color), RenderData::GreenOf(color),
                                RenderData::BlueOf(color), alpha);
}

[[maybe_unused]] Core::Vec3 Down(JPH::RVec3Arg point, JPH::RVec3Arg origin) noexcept {
    const JPH::RVec3 local = point - origin;
    return {static_cast<float>(local.GetX()), static_cast<float>(local.GetY()),
            static_cast<float>(local.GetZ())};
}

#ifdef JPH_DEBUG_RENDERER
JPH::Color JoltColor(RenderData::PackedColor color) noexcept {
    return JPH::Color(RenderData::RedOf(color), RenderData::GreenOf(color),
                      RenderData::BlueOf(color), RenderData::AlphaOf(color));
}
#endif

[[maybe_unused]] RenderData::PackedColor ColorOf(const JPH::Body &body) noexcept {
    if (body.IsSensor()) {
        return kSensorBodyColor;
    }
    if (body.IsStatic()) {
        return kStaticBodyColor;
    }
    return body.IsActive() ? kMovingBodyColor : kSleepingBodyColor;
}

// The bodies whose broad-phase bounds overlap the box of half-size radius around center, in handle
// order.
void BodiesNear(const JPH::PhysicsSystem &system, Core::WorldPosition center, double radius,
                std::vector<JPH::BodyID> &out) {
    const JPH::Vec3 half = JPH::Vec3::sReplicate(static_cast<float>(radius));
    const JPH::Vec3 middle(static_cast<float>(center.x), static_cast<float>(center.y),
                           static_cast<float>(center.z));
    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    system.GetBroadPhaseQuery().CollideAABox(JPH::AABox(middle - half, middle + half), collector);
    out.assign(collector.mHits.begin(), collector.mHits.end());
    std::ranges::sort(out, [](JPH::BodyID a, JPH::BodyID b) {
        return a.GetIndexAndSequenceNumber() < b.GetIndexAndSequenceNumber();
    });
}

} // namespace

#ifdef JPH_DEBUG_RENDERER

class DebugCapture final : public JPH::DebugRendererSimple {
  public:
    // Into a frame's geometry, relative to its origin.
    void Begin(RenderData::DebugGeometry &out, JPH::RVec3Arg origin, RenderData::DebugBatch batch) {
        m_Geometry = &out;
        m_Mesh = nullptr;
        m_Origin = origin;
        m_Batch = batch;
        SetCameraPos(origin);
    }
    // Into a mesh in a shape's own space (drawn at the identity transform).
    void Begin(RenderData::DebugMesh &mesh) {
        m_Geometry = nullptr;
        m_Mesh = &mesh;
        m_Origin = JPH::RVec3::sZero();
    }

    // The color of what is drawn next. Jolt multiplies a shape's color by its geometry's white
    // (rounding 255 down to 254), so the capture paints with the color it was told instead.
    void Paint(RenderData::PackedColor color) noexcept { m_Color = color; }

    void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg) override {
        const RenderData::PackedColor packed = WithAlpha(m_Color, kOpaque);
        const Core::Vec3 a = Down(from, m_Origin);
        const Core::Vec3 b = Down(to, m_Origin);
        if (m_Geometry != nullptr) {
            m_Geometry->AddLine(m_Batch, a, b, packed);
        } else if (m_Mesh != nullptr) {
            m_Mesh->linePositions.insert(m_Mesh->linePositions.end(), {a, b});
            m_Mesh->lineColors.insert(m_Mesh->lineColors.end(), {packed, packed});
        }
    }

    void DrawTriangle(JPH::RVec3Arg first, JPH::RVec3Arg second, JPH::RVec3Arg third, JPH::ColorArg,
                      ECastShadow) override {
        const RenderData::PackedColor packed = WithAlpha(m_Color, kDebugFaceAlpha);
        const Core::Vec3 a = Down(first, m_Origin);
        const Core::Vec3 b = Down(second, m_Origin);
        const Core::Vec3 c = Down(third, m_Origin);
        if (m_Geometry != nullptr) {
            m_Geometry->AddTriangle(m_Batch, a, b, c, packed);
        } else if (m_Mesh != nullptr) {
            m_Mesh->trianglePositions.insert(m_Mesh->trianglePositions.end(), {a, b, c});
            m_Mesh->triangleColors.insert(m_Mesh->triangleColors.end(), {packed, packed, packed});
        }
    }

    void DrawText3D(JPH::RVec3Arg, const JPH::string_view &, JPH::ColorArg, float) override {
        // Physics writes no text: tools describe bodies through BodyInfo.
    }

  private:
    RenderData::DebugGeometry *m_Geometry = nullptr;
    RenderData::DebugMesh *m_Mesh = nullptr;
    JPH::RVec3 m_Origin = JPH::RVec3::sZero();
    RenderData::DebugBatch m_Batch = RenderData::DebugBatch::WorldDepth;
    RenderData::PackedColor m_Color = kStaticBodyColor;
};

void DebugCaptureDeleter::operator()(DebugCapture *capture) const noexcept { delete capture; }

#else

class DebugCapture {};
void DebugCaptureDeleter::operator()(DebugCapture *capture) const noexcept { delete capture; }

#endif // JPH_DEBUG_RENDERER

} // namespace Detail

PhysicsDebugStats PhysicsWorld::ExtractDebugGeometry(Core::WorldPosition origin,
                                                     const PhysicsDebugSettings &settings,
                                                     RenderData::DebugGeometry &out,
                                                     RenderData::DebugMeshLibrary *meshes) {
    PhysicsDebugStats stats;
#ifdef JPH_DEBUG_RENDERER
    stats.available = true;
    State &state = *m_State;
    if (state.debug == nullptr) {
        state.debug = Detail::DebugCapturePtr(new Detail::DebugCapture());
    }
    Detail::DebugCapture &capture = *state.debug;
    const JPH::RVec3 at = Detail::ToJolt(origin);

    if (settings.bodies) {
        Detail::BodiesNear(state.system, origin, settings.radiusM, state.debugBodies);
        const JPH::BodyLockInterfaceNoLock &locks = state.system.GetBodyLockInterfaceNoLock();
        for (const JPH::BodyID id : state.debugBodies) {
            const JPH::BodyLockRead lock(locks, id);
            if (!lock.Succeeded()) {
                continue;
            }
            const JPH::Body &body = lock.GetBody();
            const JPH::Shape &shape = *body.GetShape();
            const RenderData::PackedColor color = Detail::ColorOf(body);
            const JPH::RMat44 transform = body.GetCenterOfMassTransform();
            const ShapeId registered(static_cast<std::uint32_t>(shape.GetUserData()));
            const bool large = meshes != nullptr && registered.IsValid() &&
                               shape.GetStats().mNumTriangles >= settings.largeShapeTriangles;
            if (large) {
                const Core::DebugMeshId mesh(settings.meshIdBase + registered.Value());
                if (!meshes->Contains(mesh)) {
                    auto geometry = std::make_shared<RenderData::DebugMesh>();
                    capture.Begin(*geometry);
                    capture.Paint(color);
                    shape.Draw(&capture, JPH::RMat44::sIdentity(), Detail::kUnitScale,
                               Detail::JoltColor(color), Detail::kNoMaterialColors, Detail::kFaces);
                    shape.Draw(&capture, JPH::RMat44::sIdentity(), Detail::kUnitScale,
                               Detail::JoltColor(color), Detail::kNoMaterialColors,
                               Detail::kWireframe);
                    meshes->Put(mesh, std::move(geometry));
                    ++stats.meshesAdded;
                }
                RenderData::DebugMeshInstance instance;
                instance.mesh = mesh;
                instance.batch = settings.batch;
                instance.position = Detail::Down(transform.GetTranslation(), at);
                instance.rotation = Detail::FromJolt(body.GetRotation());
                if (out.AddMeshInstance(instance)) {
                    ++stats.meshInstances;
                }
            } else {
                capture.Begin(out, at, settings.batch);
                capture.Paint(color);
                if (settings.faces) {
                    shape.Draw(&capture, transform, Detail::kUnitScale, Detail::JoltColor(color),
                               Detail::kNoMaterialColors, Detail::kFaces);
                }
                shape.Draw(&capture, transform, Detail::kUnitScale, Detail::JoltColor(color),
                           Detail::kNoMaterialColors, Detail::kWireframe);
                ++stats.bodies;
            }
            capture.Begin(out, at, settings.batch);
            if (settings.boundingBoxes) {
                capture.Paint(kMovingBodyColor);
                capture.DrawWireBox(transform, shape.GetLocalBounds(),
                                    Detail::JoltColor(kMovingBodyColor));
            }
            const JPH::Vec3 velocity = body.GetLinearVelocity();
            if (settings.velocities && velocity.Length() > Detail::kStillMps) {
                const JPH::RVec3 from = transform.GetTranslation();
                capture.Paint(kVelocityColor);
                capture.DrawArrow(from, from + velocity * Detail::kVelocityArrowSeconds,
                                  Detail::JoltColor(kVelocityColor), Detail::kArrowHeadM);
            }
        }
    }

    if (settings.characters) {
        capture.Begin(out, at, settings.batch);
        capture.Paint(kCharacterColor);
        const double reach = settings.radiusM;
        for (const Detail::CharacterSlot &slot : state.characters.Slots()) {
            if (slot.character == nullptr) {
                continue;
            }
            const JPH::CharacterVirtual &character = *slot.character;
            const JPH::RVec3 feet = character.GetPosition();
            const JPH::RVec3 offset = feet - at;
            if (std::abs(offset.GetX()) > reach || std::abs(offset.GetY()) > reach ||
                std::abs(offset.GetZ()) > reach) {
                continue;
            }
            const JPH::RMat44 transform = character.GetCenterOfMassTransform();
            if (settings.faces) {
                character.GetShape()->Draw(&capture, transform, Detail::kUnitScale,
                                           Detail::JoltColor(kCharacterColor),
                                           Detail::kNoMaterialColors, Detail::kFaces);
            }
            character.GetShape()->Draw(&capture, transform, Detail::kUnitScale,
                                       Detail::JoltColor(kCharacterColor),
                                       Detail::kNoMaterialColors, Detail::kWireframe);
            ++stats.characters;
        }
    }
#else
    (void)origin;
    (void)settings;
    (void)out;
    (void)meshes;
#endif
    return stats;
}

void PhysicsWorld::FindBodies(Core::WorldPosition center, double radiusM,
                              std::vector<BodyHandle> &out) const {
    std::vector<JPH::BodyID> found;
    Detail::BodiesNear(m_State->system, center, radiusM, found);
    out.clear();
    out.reserve(found.size());
    for (const JPH::BodyID id : found) {
        out.push_back(Detail::ToHandle(id));
    }
}

BodyInfo PhysicsWorld::DescribeBody(BodyHandle body) const {
    RUNIC_ASSERT_MSG(IsBody(body), "the handle names no body");
    const JPH::BodyLockRead lock(m_State->system.GetBodyLockInterfaceNoLock(),
                                 Detail::ToBodyId(body));
    const JPH::Body &jolt = lock.GetBody();
    BodyInfo info;
    info.body = body;
    info.shape = ShapeId(static_cast<std::uint32_t>(jolt.GetShape()->GetUserData()));
    switch (jolt.GetMotionType()) {
    case JPH::EMotionType::Static:
        info.motion = MotionType::Static;
        break;
    case JPH::EMotionType::Kinematic:
        info.motion = MotionType::Kinematic;
        break;
    case JPH::EMotionType::Dynamic:
        info.motion = MotionType::Dynamic;
        break;
    }
    info.layer = Detail::FromJolt(jolt.GetObjectLayer());
    info.pose = {Detail::FromJolt(jolt.GetPosition()), Detail::FromJolt(jolt.GetRotation())};
    info.linearVelocity = Detail::FromJolt(jolt.GetLinearVelocity());
    info.friction = jolt.GetFriction();
    info.restitution = jolt.GetRestitution();
    info.active = jolt.IsActive();
    info.sensor = jolt.IsSensor();
    info.owner = jolt.GetUserData();
    return info;
}

ShapeInfo PhysicsWorld::DescribeShape(ShapeId shape) const {
    RUNIC_ASSERT_MSG(shape.IsValid() && shape.Value() <= m_State->shapes.size(),
                     "the shape is not one of this world's");
    const Detail::NamedShape &named = m_State->shapes[shape.Value() - 1u];
    ShapeInfo info;
    info.id = shape;
    info.name = named.name;
    info.kind = named.kind;
    info.triangles = static_cast<std::uint32_t>(named.shape->GetStats().mNumTriangles);
    info.halfExtents = Detail::FromJolt(named.shape->GetLocalBounds().GetExtent());
    return info;
}

} // namespace Runic::Physics
