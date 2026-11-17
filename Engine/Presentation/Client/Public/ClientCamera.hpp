#pragma once

#include "Entity.hpp"
#include "EntityWorld.hpp"
#include "Math.hpp"
#include "RenderData/Camera.hpp"
#include "WorldSpace.hpp"

// The client camera (CLI-1): where the player's eye is in the world, where it looks and through
// which lens. The Presentation layer makes every frame's origin and RenderData::Camera from it, so
// the camera is the one 64-bit place a frame is relative to (RunicDocs › Render data › RenderData
// in Core › Camera-relative). It either stays where it was put or follows an entity.

namespace Runic::Client {

struct CameraLens {
    float verticalFovRadians = RenderData::kDefaultVerticalFovRadians;
    float nearPlaneM = RenderData::kDefaultNearPlaneM;
    float farPlaneM = RenderData::kDefaultFarPlaneM; // also the draw distance
};

// Where a new camera is and where it looks: 10 m back and 5 m up from the world's origin.
inline constexpr Core::WorldPosition kDefaultEye{0.0, 5.0, 10.0};
inline constexpr Core::WorldPosition kDefaultTarget{0.0, 0.0, 0.0};

class ClientCamera {
  public:
    // Puts the eye at eye, looking at target, with +Y up; stops following. target differs from
    // eye.
    void LookAt(Core::WorldPosition eye, Core::WorldPosition target) noexcept;
    // From the next Update on, the eye is the entity's position plus eyeOffset and looks at its
    // position plus lookOffset (world axes: the entity's rotation does not turn the camera). The
    // offsets differ.
    void Follow(ECS::Entity entity, Core::Vec3 eyeOffset, Core::Vec3 lookOffset) noexcept;
    void StopFollowing() noexcept;
    bool IsFollowing() const noexcept { return !m_Followed.IsNull(); }
    ECS::Entity Followed() const noexcept { return m_Followed; }

    void SetLens(const CameraLens &lens) noexcept;
    const CameraLens &Lens() const noexcept { return m_Lens; }

    // Takes the followed entity's place from world. An entity that is gone or has no
    // World::WorldTransform ends the following and leaves the camera where it was.
    void Update(const ECS::EntityWorld &world) noexcept;

    Core::WorldPosition Eye() const noexcept { return m_Eye; }
    Core::WorldPosition Target() const noexcept { return m_Target; }
    // The camera as seen from a frame whose origin is origin: its position relative to it, its
    // rotation (looking down -Z, as in Godot) and its lens.
    RenderData::Camera ViewFrom(Core::WorldPosition origin) const noexcept;

  private:
    Core::WorldPosition m_Eye = kDefaultEye;
    Core::WorldPosition m_Target = kDefaultTarget;
    ECS::Entity m_Followed;
    Core::Vec3 m_EyeOffset{0.0f};
    Core::Vec3 m_LookOffset{0.0f};
    CameraLens m_Lens;
};

} // namespace Runic::Client
