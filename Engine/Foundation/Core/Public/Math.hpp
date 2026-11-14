#pragma once

// The Engine's 32-bit math vocabulary on GLM, Core's approved public exception (ARCHITECTURE.md ›
// Allowed dependencies). Directions, velocities, sizes, rotations, local offsets and shapes use
// these types; a place in the world is a WorldPosition (WorldSpace.hpp).
//
// Runic::Core defines GLM_FORCE_EXPLICIT_CTOR for every target that links it, so a 64-bit GLM
// vector becomes a 32-bit one only through an explicit conversion.
#if !defined(GLM_FORCE_EXPLICIT_CTOR)
#error "Math.hpp needs Runic::Core's public definition GLM_FORCE_EXPLICIT_CTOR: link Runic::Core"
#endif

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Runic::Core {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Quat = glm::quat;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

} // namespace Runic::Core
