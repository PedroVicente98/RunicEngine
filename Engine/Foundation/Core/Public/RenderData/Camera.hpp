#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>

#include "Math.hpp"

// The camera a RenderFrame is seen through (slice 2 decision D3): its place relative to the frame's
// origin, the way it looks, and its projection. As in Godot, a camera looks down its local -Z axis
// with +Y up; the identity rotation looks along world -Z.

namespace Runic::RenderData {

inline constexpr float kRadiansPerDegree = glm::pi<float>() / 180.0f;

// Godot's own camera defaults.
inline constexpr float kDefaultVerticalFovDegrees = 75.0f;
inline constexpr float kDefaultVerticalFovRadians = kDefaultVerticalFovDegrees * kRadiansPerDegree;
inline constexpr float kDefaultNearPlaneM = 0.05f;
inline constexpr float kDefaultFarPlaneM = 4000.0f;

struct Camera {
    Core::Vec3 position{0.0f}; // metres, relative to the frame's origin
    Core::Quat rotation = glm::identity<Core::Quat>();
    float verticalFovRadians = kDefaultVerticalFovRadians;
    float nearPlaneM = kDefaultNearPlaneM;
    float farPlaneM = kDefaultFarPlaneM;
};

// A camera at eye that looks at target, with up as its up direction. target must differ from eye,
// and up must not be parallel to the view.
inline Camera LookAt(Core::Vec3 eye, Core::Vec3 target, Core::Vec3 up) noexcept {
    Camera camera;
    camera.position = eye;
    camera.rotation = glm::quatLookAt(glm::normalize(target - eye), up);
    return camera;
}

} // namespace Runic::RenderData
