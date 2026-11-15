#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

#include "Math.hpp"
#include "RenderData/Color.hpp"
#include "RenderData/DebugGeometry.hpp"

// The debug-draw API (RunicDocs › Debug drawing (Engine)): shapes generated on the CPU into
// DebugGeometry. Nothing about a capsule, a cone or an attack arc reaches Godot: every shape
// becomes lines, a filled triangle becomes a triangle, and text stays text. Positions are
// camera-relative floats; angles are radians; sizes are metres.

namespace Runic::RenderData {

// Segments are a budget: a 32-segment circle is 32 lines. Round shapes take their segment count as
// an argument, raised to each shape's minimum.
inline constexpr std::uint32_t kDefaultCircleSegments = 32;
// Spheres, cylinders, capsules, cones and arcs.
inline constexpr std::uint32_t kDefaultRoundSegments = 24;
inline constexpr std::uint32_t kMinCircleSegments = 3;
inline constexpr std::uint32_t kMinCylinderSegments = 4;
inline constexpr std::uint32_t kMinCapsuleSegments = 8;
inline constexpr std::uint32_t kMinConeSegments = 4;
inline constexpr std::uint32_t kMinArcSegments = 2;

// The parts each shape is made of.
inline constexpr std::uint32_t kArrowLines = 5;    // the shaft and four head lines
inline constexpr std::uint32_t kBoxLines = 12;     // AABB and OBB edges
inline constexpr std::uint32_t kSphereCircles = 3; // three great circles
inline constexpr std::uint32_t kEndRings = 2;      // a cylinder's and a capsule's ring at each end
inline constexpr std::uint32_t kCylinderSideLines = 8;  // lines joining a cylinder's rings
inline constexpr std::uint32_t kCapsuleSideLines = 4;   // lines joining a capsule's rings
inline constexpr std::uint32_t kCapsuleHalfCircles = 4; // two half circles over each cap
inline constexpr std::uint32_t kHalvesPerCircle = 2;    // a half circle gets half the segments
inline constexpr std::uint32_t kConeSpokes = 8;         // lines from the apex to the base
inline constexpr std::uint32_t kArcEdgeLines = 2;       // an arc sector's two straight edges
inline constexpr std::uint32_t kArcVolumeLayers = 2;    // an arc volume's bottom and top sectors

// What a shape costs, in lines, for a segment count: what DebugDraw draws when the shape is not
// degenerate.
constexpr std::uint32_t CircleLines(std::uint32_t segments) noexcept {
    return segments < kMinCircleSegments ? kMinCircleSegments : segments;
}
constexpr std::uint32_t SphereLines(std::uint32_t segments) noexcept {
    return kSphereCircles * CircleLines(segments);
}
constexpr std::uint32_t CylinderLines(std::uint32_t segments) noexcept {
    const std::uint32_t ring = segments < kMinCylinderSegments ? kMinCylinderSegments : segments;
    return kEndRings * ring + kCylinderSideLines;
}
constexpr std::uint32_t CapsuleLines(std::uint32_t segments) noexcept {
    const std::uint32_t ring = segments < kMinCapsuleSegments ? kMinCapsuleSegments : segments;
    return kEndRings * ring + kCapsuleSideLines + kCapsuleHalfCircles * (ring / kHalvesPerCircle);
}
constexpr std::uint32_t ConeLines(std::uint32_t segments) noexcept {
    return (segments < kMinConeSegments ? kMinConeSegments : segments) + kConeSpokes;
}
// With an inner radius, the sector has an inner arc as well as the outer one.
constexpr std::uint32_t ArcSectorLines(std::uint32_t segments, bool hasInnerRadius) noexcept {
    const std::uint32_t arc = segments < kMinArcSegments ? kMinArcSegments : segments;
    return (hasInnerRadius ? arc + arc : arc) + kArcEdgeLines;
}
// Both sectors, and a vertical line at each outer corner (and at each inner corner).
constexpr std::uint32_t ArcVolumeLines(std::uint32_t segments, bool hasInnerRadius) noexcept {
    return kArcVolumeLayers * ArcSectorLines(segments, hasInnerRadius) + kArcEdgeLines +
           (hasInnerRadius ? kArcEdgeLines : 0);
}
// The XZ grid's lines run along X and along Z, one of each every spacing metres from -halfSize to
// +halfSize.
inline constexpr std::uint32_t kGridDirections = 2;
// So the line at +halfSize survives float rounding.
inline constexpr float kGridRoundingSlack = 1.0e-4f;
inline std::uint32_t GridLines(float halfSize, float spacing) noexcept {
    if (halfSize <= 0.0f || spacing <= 0.0f) {
        return 0;
    }
    const auto steps = static_cast<std::uint32_t>(
        std::floor((halfSize + halfSize) / spacing + kGridRoundingSlack));
    return kGridDirections * (steps + 1);
}

class DebugDraw {
  public:
    // Draws into one batch of out: depth-tested (WorldDepth) or always visible (Overlay).
    explicit DebugDraw(DebugGeometry &out, DebugBatch batch = DebugBatch::WorldDepth) noexcept;

    DebugBatch Batch() const noexcept;

    void DrawLine(Core::Vec3 a, Core::Vec3 b, PackedColor color);
    void DrawRay(Core::Vec3 origin, Core::Vec3 direction, float length, PackedColor color);
    void DrawArrow(Core::Vec3 start, Core::Vec3 end, PackedColor color);
    void DrawAABB(Core::Vec3 min, Core::Vec3 max, PackedColor color);
    // An oriented box: axes are its local X, Y and Z (normalized here), halfExtent its half sizes
    // along them.
    void DrawOBB(Core::Vec3 center, Core::Vec3 axisX, Core::Vec3 axisY, Core::Vec3 axisZ,
                 Core::Vec3 halfExtent, PackedColor color);
    void DrawCircle(Core::Vec3 center, Core::Vec3 normal, float radius, PackedColor color,
                    std::uint32_t segments = kDefaultCircleSegments);
    // Three great circles show the center and the radius.
    void DrawSphere(Core::Vec3 center, float radius, PackedColor color,
                    std::uint32_t segments = kDefaultRoundSegments);
    void DrawCylinder(Core::Vec3 a, Core::Vec3 b, float radius, PackedColor color,
                      std::uint32_t segments = kDefaultRoundSegments);
    // a and b are the centers of the two caps; the capsule reaches radius beyond both.
    void DrawCapsule(Core::Vec3 a, Core::Vec3 b, float radius, PackedColor color,
                     std::uint32_t segments = kDefaultRoundSegments);
    void DrawCone(Core::Vec3 apex, Core::Vec3 direction, float length, float halfAngleRadians,
                  PackedColor color, std::uint32_t segments = kDefaultRoundSegments);
    // A melee swing: the annular sector between innerRadius (0 for a pie slice) and outerRadius,
    // centered on forward in the plane whose normal is up, spanning angleRadians.
    void DrawArcSector(Core::Vec3 origin, Core::Vec3 forward, Core::Vec3 up, float innerRadius,
                       float outerRadius, float angleRadians, PackedColor color,
                       std::uint32_t segments = kDefaultRoundSegments);
    // The arc sector at the bottom and the top of a volume height tall, centered on origin, with
    // its corners joined.
    void DrawArcVolume(Core::Vec3 origin, Core::Vec3 forward, Core::Vec3 up, float innerRadius,
                       float outerRadius, float angleRadians, float height, PackedColor color,
                       std::uint32_t segments = kDefaultRoundSegments);
    // Lines along X and Z every spacing metres, from -halfSize to +halfSize, at height 0.
    void DrawGridXZ(float halfSize, float spacing, PackedColor color);
    void DrawText(Core::Vec3 position, std::string_view text, PackedColor color);
    // A filled triangle; its opacity is the color's alpha.
    void FillTriangle(Core::Vec3 a, Core::Vec3 b, Core::Vec3 c, PackedColor colorWithAlpha);

  private:
    DebugGeometry *m_Out;
    DebugBatch m_Batch;
};

} // namespace Runic::RenderData
