// The client camera (ClientCamera.hpp): a 64-bit eye and target, and a lens. The view's rotation
// is worked out from the direction between them in double precision, so a camera far from the
// world's origin turns as exactly as one near it.

#include "ClientCamera.hpp"

#include <cmath>

#include <glm/ext/quaternion_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Assert.hpp"
#include "WorldComponents.hpp"

namespace Runic::Client {

namespace {

const Core::Vec3 kWorldUp(0.0f, 1.0f, 0.0f);
// When the view is this close to straight up or down, +Y cannot be its up: the camera's top
// points along -Z instead, as a map seen from above.
constexpr float kNearlyVertical = 0.9999f;
const Core::Vec3 kUpWhenVertical(0.0f, 0.0f, -1.0f);

// The unit direction of a world-space difference, computed in double precision.
Core::Vec3 DirectionOf(Core::WorldDelta delta) noexcept {
    const double length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    return {static_cast<float>(delta.x / length), static_cast<float>(delta.y / length),
            static_cast<float>(delta.z / length)};
}

} // namespace

void ClientCamera::LookAt(Core::WorldPosition eye, Core::WorldPosition target) noexcept {
    RUNIC_ASSERT_MSG(eye != target, "a camera cannot look at its own eye");
    m_Eye = eye;
    m_Target = target;
    m_Followed = ECS::Entity();
}

void ClientCamera::Follow(ECS::Entity entity, Core::Vec3 eyeOffset,
                          Core::Vec3 lookOffset) noexcept {
    RUNIC_ASSERT_MSG(!entity.IsNull(), "a camera cannot follow no entity");
    RUNIC_ASSERT_MSG(eyeOffset != lookOffset, "a camera cannot look at its own eye");
    m_Followed = entity;
    m_EyeOffset = eyeOffset;
    m_LookOffset = lookOffset;
}

void ClientCamera::StopFollowing() noexcept { m_Followed = ECS::Entity(); }

void ClientCamera::SetLens(const CameraLens &lens) noexcept {
    RUNIC_ASSERT_MSG(lens.nearPlaneM > 0.0f && lens.farPlaneM > lens.nearPlaneM,
                     "a lens needs 0 < near < far");
    m_Lens = lens;
}

void ClientCamera::Update(const ECS::EntityWorld &world) noexcept {
    if (m_Followed.IsNull()) {
        return;
    }
    const World::WorldTransform *transform =
        world.IsAlive(m_Followed) ? world.TryGet<World::WorldTransform>(m_Followed) : nullptr;
    if (transform == nullptr) {
        m_Followed = ECS::Entity();
        return;
    }
    m_Eye = transform->position + m_EyeOffset;
    m_Target = transform->position + m_LookOffset;
}

RenderData::Camera ClientCamera::ViewFrom(Core::WorldPosition origin) const noexcept {
    RenderData::Camera camera;
    camera.position = Core::ToLocal(m_Eye - origin);
    const Core::Vec3 forward = DirectionOf(m_Target - m_Eye);
    const bool vertical = std::abs(glm::dot(forward, kWorldUp)) > kNearlyVertical;
    camera.rotation = glm::quatLookAt(forward, vertical ? kUpWhenVertical : kWorldUp);
    camera.verticalFovRadians = m_Lens.verticalFovRadians;
    camera.nearPlaneM = m_Lens.nearPlaneM;
    camera.farPlaneM = m_Lens.farPlaneM;
    return camera;
}

} // namespace Runic::Client
