// The debug-draw API (DebugDraw.hpp): each shape is CPU-side geometry generation into lines
// (RunicDocs › Debug drawing (Engine) › Shapes). The shapes follow the RunicDocs listings, with
// GLM's vector functions.

#include "RenderData/DebugDraw.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include <glm/ext/scalar_constants.hpp>

namespace Runic::RenderData {

namespace {

using Core::Vec3;

// A vector shorter than this has no direction.
constexpr float kDegenerateLength = 1.0e-6f;
// Above this |cos|, a helper axis is too close to the normal to build a basis from.
constexpr float kParallelLimit = 0.99f;
// An arrow's head is a quarter of its length, never longer than 35 cm, and its half width is 45 %
// of the head's length.
constexpr float kArrowHeadShare = 0.25f;
constexpr float kArrowHeadMaxLengthM = 0.35f;
constexpr float kArrowHeadWidthRatio = 0.45f;
constexpr float kHalf = 0.5f;
constexpr float kPi = glm::pi<float>();
constexpr float kTwoPi = glm::two_pi<float>();

// Angles in a plane are measured from its first axis.
constexpr float kStartAngle = 0.0f;

const Vec3 kOrigin(0.0f);
const Vec3 kAxisX(1.0f, 0.0f, 0.0f);
const Vec3 kAxisY(0.0f, 1.0f, 0.0f);
const Vec3 kAxisZ(0.0f, 0.0f, 1.0f);

constexpr std::size_t kBoxCorners = 8;
constexpr std::size_t kEdgeEnds = 2;
using EdgeTable = std::array<std::array<std::uint8_t, kEdgeEnds>, kBoxLines>;
// AABB corners: 0-3 at min.z counter-clockwise from min, 4-7 the same at max.z.
constexpr EdgeTable kAabbEdges{{{0, 1},
                                {1, 2},
                                {2, 3},
                                {3, 0},
                                {4, 5},
                                {5, 6},
                                {6, 7},
                                {7, 4},
                                {0, 4},
                                {1, 5},
                                {2, 6},
                                {3, 7}}};
// OBB corners: x varies fastest, then y, then z (0-1 / 2-3 at -z, 4-5 / 6-7 at +z).
constexpr EdgeTable kObbEdges{{{0, 1},
                               {0, 2},
                               {1, 3},
                               {2, 3},
                               {4, 5},
                               {4, 6},
                               {5, 7},
                               {6, 7},
                               {0, 4},
                               {1, 5},
                               {2, 6},
                               {3, 7}}};
constexpr int kNegative = -1;
constexpr int kPositive = 1;
constexpr int kCornerStep = kPositive - kNegative;

bool TryNormalize(Vec3 v, Vec3 &out) noexcept {
    const float length = glm::length(v);
    if (length <= kDegenerateLength) {
        return false;
    }
    out = v / length;
    return true;
}

// An orthonormal basis around an axis: the key to circles in any orientation.
void MakeBasis(Vec3 normal, Vec3 &tangent, Vec3 &bitangent) noexcept {
    const Vec3 n = glm::normalize(normal);
    const Vec3 helper = std::abs(n.y) < kParallelLimit ? kAxisY : kAxisX; // not parallel to n
    tangent = glm::normalize(glm::cross(helper, n));
    bitangent = glm::cross(n, tangent);
}

// segments lines along the arc from startRadians to endRadians, in the plane of axisX and axisY.
void DrawArcInPlane(DebugDraw &draw, Vec3 center, Vec3 axisX, Vec3 axisY, float radius,
                    float startRadians, float endRadians, PackedColor color,
                    std::uint32_t segments) {
    axisX = glm::normalize(axisX);
    axisY = glm::normalize(axisY);
    const auto pointAt = [&](float angle) {
        return center + axisX * (std::cos(angle) * radius) + axisY * (std::sin(angle) * radius);
    };
    Vec3 previous = pointAt(startRadians);
    for (std::uint32_t i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 current = pointAt(startRadians + (endRadians - startRadians) * t);
        draw.DrawLine(previous, current, color);
        previous = current;
    }
}

// A point on the ring of radius around center, at angle in the basis (tangent, bitangent).
Vec3 RingPoint(Vec3 center, Vec3 tangent, Vec3 bitangent, float radius, float angle) noexcept {
    return center + tangent * (std::cos(angle) * radius) + bitangent * (std::sin(angle) * radius);
}

} // namespace

DebugDraw::DebugDraw(DebugGeometry &out, DebugBatch batch) noexcept : m_Out(&out), m_Batch(batch) {}

DebugBatch DebugDraw::Batch() const noexcept { return m_Batch; }

void DebugDraw::DrawLine(Vec3 a, Vec3 b, PackedColor color) {
    m_Out->AddLine(m_Batch, a, b, color);
}

void DebugDraw::DrawRay(Vec3 origin, Vec3 direction, float length, PackedColor color) {
    Vec3 unit;
    if (length <= 0.0f || !TryNormalize(direction, unit)) {
        return;
    }
    DrawLine(origin, origin + unit * length, color);
}

void DebugDraw::DrawArrow(Vec3 start, Vec3 end, PackedColor color) {
    const Vec3 delta = end - start;
    const float length = glm::length(delta);
    if (length <= kDegenerateLength) {
        return;
    }
    const Vec3 direction = delta / length;
    Vec3 right;
    Vec3 up;
    MakeBasis(direction, right, up);
    const float headLength = std::min(kArrowHeadShare * length, kArrowHeadMaxLengthM);
    const float headWidth = headLength * kArrowHeadWidthRatio;
    const Vec3 base = end - direction * headLength;
    DrawLine(start, end, color);
    DrawLine(end, base + right * headWidth, color);
    DrawLine(end, base - right * headWidth, color);
    DrawLine(end, base + up * headWidth, color);
    DrawLine(end, base - up * headWidth, color);
}

void DebugDraw::DrawAABB(Vec3 min, Vec3 max, PackedColor color) {
    const std::array<Vec3, kBoxCorners> corners{
        Vec3(min.x, min.y, min.z), Vec3(max.x, min.y, min.z), Vec3(max.x, max.y, min.z),
        Vec3(min.x, max.y, min.z), Vec3(min.x, min.y, max.z), Vec3(max.x, min.y, max.z),
        Vec3(max.x, max.y, max.z), Vec3(min.x, max.y, max.z)};
    for (const auto &edge : kAabbEdges) {
        DrawLine(corners[edge.front()], corners[edge.back()], color);
    }
}

void DebugDraw::DrawOBB(Vec3 center, Vec3 axisX, Vec3 axisY, Vec3 axisZ, Vec3 halfExtent,
                        PackedColor color) {
    axisX = glm::normalize(axisX);
    axisY = glm::normalize(axisY);
    axisZ = glm::normalize(axisZ);
    std::array<Vec3, kBoxCorners> corners{};
    std::size_t index = 0;
    for (int z = kNegative; z <= kPositive; z += kCornerStep) {
        for (int y = kNegative; y <= kPositive; y += kCornerStep) {
            for (int x = kNegative; x <= kPositive; x += kCornerStep) {
                corners[index++] = center + axisX * (halfExtent.x * static_cast<float>(x)) +
                                   axisY * (halfExtent.y * static_cast<float>(y)) +
                                   axisZ * (halfExtent.z * static_cast<float>(z));
            }
        }
    }
    for (const auto &edge : kObbEdges) {
        DrawLine(corners[edge.front()], corners[edge.back()], color);
    }
}

void DebugDraw::DrawCircle(Vec3 center, Vec3 normal, float radius, PackedColor color,
                           std::uint32_t segments) {
    Vec3 unit;
    if (radius <= 0.0f || !TryNormalize(normal, unit)) {
        return;
    }
    Vec3 tangent;
    Vec3 bitangent;
    MakeBasis(unit, tangent, bitangent);
    DrawArcInPlane(*this, center, tangent, bitangent, radius, kStartAngle, kTwoPi, color,
                   CircleLines(segments));
}

void DebugDraw::DrawSphere(Vec3 center, float radius, PackedColor color, std::uint32_t segments) {
    DrawCircle(center, kAxisX, radius, color, segments);
    DrawCircle(center, kAxisY, radius, color, segments);
    DrawCircle(center, kAxisZ, radius, color, segments);
}

void DebugDraw::DrawCylinder(Vec3 a, Vec3 b, float radius, PackedColor color,
                             std::uint32_t segments) {
    Vec3 axis;
    if (radius <= 0.0f || !TryNormalize(b - a, axis)) {
        return;
    }
    Vec3 tangent;
    Vec3 bitangent;
    MakeBasis(axis, tangent, bitangent);
    const std::uint32_t ring = std::max(segments, kMinCylinderSegments);
    DrawCircle(a, axis, radius, color, ring);
    DrawCircle(b, axis, radius, color, ring);
    for (std::uint32_t i = 0; i < kCylinderSideLines; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kCylinderSideLines);
        const Vec3 radial = RingPoint(kOrigin, tangent, bitangent, radius, angle);
        DrawLine(a + radial, b + radial, color);
    }
}

void DebugDraw::DrawCapsule(Vec3 a, Vec3 b, float radius, PackedColor color,
                            std::uint32_t segments) {
    if (radius <= 0.0f) {
        return;
    }
    Vec3 axis;
    if (!TryNormalize(b - a, axis)) {
        DrawSphere(a, radius, color, segments);
        return;
    }
    Vec3 tangent;
    Vec3 bitangent;
    MakeBasis(axis, tangent, bitangent);
    const std::uint32_t ring = std::max(segments, kMinCapsuleSegments);
    const std::uint32_t halfCircle = ring / kHalvesPerCircle;

    DrawCircle(a, axis, radius, color, ring); // the equator rings at both cap centers
    DrawCircle(b, axis, radius, color, ring);
    DrawLine(a + tangent * radius, b + tangent * radius, color);
    DrawLine(a - tangent * radius, b - tangent * radius, color);
    DrawLine(a + bitangent * radius, b + bitangent * radius, color);
    DrawLine(a - bitangent * radius, b - bitangent * radius, color);

    DrawArcInPlane(*this, a, tangent, axis, radius, kPi, kTwoPi, color, halfCircle); // bottom cap
    DrawArcInPlane(*this, a, bitangent, axis, radius, kPi, kTwoPi, color, halfCircle);
    DrawArcInPlane(*this, b, tangent, axis, radius, kStartAngle, kPi, color, halfCircle); // top cap
    DrawArcInPlane(*this, b, bitangent, axis, radius, kStartAngle, kPi, color, halfCircle);
}

void DebugDraw::DrawCone(Vec3 apex, Vec3 direction, float length, float halfAngleRadians,
                         PackedColor color, std::uint32_t segments) {
    Vec3 axis;
    if (length <= 0.0f || !TryNormalize(direction, axis)) {
        return;
    }
    const float radius = std::tan(halfAngleRadians) * length;
    const Vec3 baseCenter = apex + axis * length;
    Vec3 tangent;
    Vec3 bitangent;
    MakeBasis(axis, tangent, bitangent);
    DrawCircle(baseCenter, axis, radius, color, std::max(segments, kMinConeSegments));
    for (std::uint32_t i = 0; i < kConeSpokes; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kConeSpokes);
        DrawLine(apex, RingPoint(baseCenter, tangent, bitangent, radius, angle), color);
    }
}

void DebugDraw::DrawArcSector(Vec3 origin, Vec3 forward, Vec3 up, float innerRadius,
                              float outerRadius, float angleRadians, PackedColor color,
                              std::uint32_t segments) {
    Vec3 normal;
    Vec3 flatForward;
    if (outerRadius <= 0.0f || angleRadians <= 0.0f || !TryNormalize(up, normal) ||
        !TryNormalize(forward - normal * glm::dot(forward, normal), flatForward)) {
        return; // no size, no angle, or forward along up: nothing to draw
    }
    const Vec3 right = glm::normalize(glm::cross(normal, flatForward));
    const float halfAngle = angleRadians * kHalf;
    const std::uint32_t arc = std::max(segments, kMinArcSegments);
    const bool hasInner = innerRadius > kNoInnerRadius;
    const auto pointAt = [&](float radius, float angle) {
        return origin + (flatForward * std::cos(angle) + right * std::sin(angle)) * radius;
    };

    Vec3 previousOuter = pointAt(outerRadius, -halfAngle);
    Vec3 previousInner = hasInner ? pointAt(innerRadius, -halfAngle) : origin;
    for (std::uint32_t i = 1; i <= arc; ++i) {
        const float angle =
            -halfAngle + angleRadians * (static_cast<float>(i) / static_cast<float>(arc));
        const Vec3 outer = pointAt(outerRadius, angle);
        DrawLine(previousOuter, outer, color);
        previousOuter = outer;
        if (hasInner) {
            const Vec3 inner = pointAt(innerRadius, angle);
            DrawLine(previousInner, inner, color);
            previousInner = inner;
        }
    }
    const Vec3 outerStart = pointAt(outerRadius, -halfAngle);
    const Vec3 outerEnd = pointAt(outerRadius, halfAngle);
    if (hasInner) {
        DrawLine(pointAt(innerRadius, -halfAngle), outerStart, color);
        DrawLine(pointAt(innerRadius, halfAngle), outerEnd, color);
    } else {
        DrawLine(origin, outerStart, color);
        DrawLine(origin, outerEnd, color);
    }
}

void DebugDraw::DrawArcVolume(Vec3 origin, Vec3 forward, Vec3 up, float innerRadius,
                              float outerRadius, float angleRadians, float height,
                              PackedColor color, std::uint32_t segments) {
    Vec3 normal;
    Vec3 flatForward;
    if (outerRadius <= 0.0f || angleRadians <= 0.0f || !TryNormalize(up, normal) ||
        !TryNormalize(forward - normal * glm::dot(forward, normal), flatForward)) {
        return;
    }
    const Vec3 offset = normal * (height * kHalf);
    const Vec3 bottom = origin - offset;
    const Vec3 top = origin + offset;
    DrawArcSector(bottom, flatForward, normal, innerRadius, outerRadius, angleRadians, color,
                  segments);
    DrawArcSector(top, flatForward, normal, innerRadius, outerRadius, angleRadians, color,
                  segments);

    const Vec3 right = glm::normalize(glm::cross(normal, flatForward));
    const float halfAngle = angleRadians * kHalf;
    const auto radial = [&](float radius, float angle) {
        return (flatForward * std::cos(angle) + right * std::sin(angle)) * radius;
    };
    for (const float angle : {-halfAngle, halfAngle}) {
        DrawLine(bottom + radial(outerRadius, angle), top + radial(outerRadius, angle), color);
        if (innerRadius > kNoInnerRadius) {
            DrawLine(bottom + radial(innerRadius, angle), top + radial(innerRadius, angle), color);
        }
    }
}

void DebugDraw::DrawGridXZ(float halfSize, float spacing, PackedColor color) {
    // Each line is placed from its index, so no error builds up along the grid.
    const std::uint32_t linesPerDirection = GridLines(halfSize, spacing) / kGridDirections;
    for (std::uint32_t i = 0; i < linesPerDirection; ++i) {
        const float at = -halfSize + spacing * static_cast<float>(i);
        DrawLine(Vec3(at, kGridHeightM, -halfSize), Vec3(at, kGridHeightM, halfSize), color);
        DrawLine(Vec3(-halfSize, kGridHeightM, at), Vec3(halfSize, kGridHeightM, at), color);
    }
}

void DebugDraw::DrawText(Vec3 position, std::string_view text, PackedColor color) {
    m_Out->AddText(m_Batch, position, text, color);
}

void DebugDraw::FillTriangle(Vec3 a, Vec3 b, Vec3 c, PackedColor colorWithAlpha) {
    m_Out->AddTriangle(m_Batch, a, b, c, colorWithAlpha);
}

} // namespace Runic::RenderData
