// CORE-3 acceptance tests: every debug shape becomes lines and triangles, with the cost its
// line-count function says.

#include "AllocationCounter.hpp"
#include "RenderData/Camera.hpp"
#include "RenderData/DebugDraw.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

using namespace Runic::RenderData;
using Runic::Core::Vec3;
namespace Testing = Runic::Core::Testing;

namespace {

constexpr std::uint32_t kSegments = 16;
constexpr float kRadiusM = 2.0f;
constexpr float kLengthM = 3.0f;
constexpr float kHalfAngleRadians = 0.5f;
constexpr float kInnerRadiusM = 1.0f;
constexpr float kOuterRadiusM = 4.0f;
constexpr float kSwingRadians = 2.0f;
constexpr float kHeightM = 2.0f;
constexpr float kGridHalfSizeM = 5.0f;
constexpr float kGridSpacingM = 1.0f;
constexpr float kToleranceM = 1.0e-4f;
constexpr const char *kLabel = "engine:test_label";

const Vec3 kOrigin(0.0f);
const Vec3 kCenter(1.0f, 2.0f, 3.0f);
const Vec3 kUp(0.0f, 1.0f, 0.0f);
const Vec3 kForward(0.0f, 0.0f, -1.0f);
const Vec3 kRight(1.0f, 0.0f, 0.0f);
const Vec3 kTop(1.0f, 5.0f, 3.0f);
const Vec3 kHalfExtent(1.0f, 2.0f, 3.0f);

std::uint32_t LinesIn(const DebugGeometry &geometry, DebugBatch batch = DebugBatch::WorldDepth) {
    return geometry.VertexCount(batch, DebugPrimitive::Lines) / kVerticesPerLine;
}

struct ShapeCase {
    const char *name;
    std::function<void(DebugDraw &)> draw;
    std::uint32_t expectedLines;
};

std::vector<ShapeCase> EveryShape() {
    return {
        {"line", [](DebugDraw &d) { d.DrawLine(kOrigin, kCenter, kDebugQuery); }, 1},
        {"ray", [](DebugDraw &d) { d.DrawRay(kOrigin, kForward, kLengthM, kDebugQuery); }, 1},
        {"arrow", [](DebugDraw &d) { d.DrawArrow(kOrigin, kCenter, kDebugQuery); }, kArrowLines},
        {"aabb", [](DebugDraw &d) { d.DrawAABB(kOrigin, kCenter, kDebugPhysics); }, kBoxLines},
        {"obb",
         [](DebugDraw &d) {
             d.DrawOBB(kCenter, kRight, kUp, kForward, kHalfExtent, kDebugPhysics);
         },
         kBoxLines},
        {"circle",
         [](DebugDraw &d) { d.DrawCircle(kCenter, kUp, kRadiusM, kDebugBeneficial, kSegments); },
         CircleLines(kSegments)},
        {"sphere",
         [](DebugDraw &d) { d.DrawSphere(kCenter, kRadiusM, kDebugBeneficial, kSegments); },
         SphereLines(kSegments)},
        {"cylinder",
         [](DebugDraw &d) { d.DrawCylinder(kCenter, kTop, kRadiusM, kDebugPhysics, kSegments); },
         CylinderLines(kSegments)},
        {"capsule",
         [](DebugDraw &d) { d.DrawCapsule(kCenter, kTop, kRadiusM, kDebugPredicted, kSegments); },
         CapsuleLines(kSegments)},
        {"cone",
         [](DebugDraw &d) {
             d.DrawCone(kCenter, kForward, kLengthM, kHalfAngleRadians, kDebugHarmful, kSegments);
         },
         ConeLines(kSegments)},
        {"pie sector",
         [](DebugDraw &d) {
             d.DrawArcSector(kOrigin, kForward, kUp, 0.0f, kOuterRadiusM, kSwingRadians,
                             kDebugHarmful, kSegments);
         },
         ArcSectorLines(kSegments, false)},
        {"annular sector",
         [](DebugDraw &d) {
             d.DrawArcSector(kOrigin, kForward, kUp, kInnerRadiusM, kOuterRadiusM, kSwingRadians,
                             kDebugHarmful, kSegments);
         },
         ArcSectorLines(kSegments, true)},
        {"arc volume",
         [](DebugDraw &d) {
             d.DrawArcVolume(kOrigin, kForward, kUp, kInnerRadiusM, kOuterRadiusM, kSwingRadians,
                             kHeightM, kDebugHarmful, kSegments);
         },
         ArcVolumeLines(kSegments, true)},
        {"grid", [](DebugDraw &d) { d.DrawGridXZ(kGridHalfSizeM, kGridSpacingM, kDebugInactive); },
         GridLines(kGridHalfSizeM, kGridSpacingM)},
    };
}

} // namespace

TEST(DebugDraw, EveryShapeBecomesLines) {
    for (const ShapeCase &shape : EveryShape()) {
        DebugGeometry geometry;
        DebugDraw draw(geometry);
        shape.draw(draw);
        EXPECT_EQ(LinesIn(geometry), shape.expectedLines) << shape.name;
        EXPECT_EQ(geometry.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Triangles), 0u)
            << shape.name;
        EXPECT_EQ(geometry.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines), 0u)
            << shape.name;
        EXPECT_TRUE(geometry.Texts().empty()) << shape.name;
    }
}

TEST(DebugDraw, TheGridCountsBothDirections) {
    // 5 m each side at 1 m spacing: 11 lines along X and 11 along Z.
    constexpr std::uint32_t kLinesPerDirection = 11;
    EXPECT_EQ(GridLines(kGridHalfSizeM, kGridSpacingM), kGridDirections * kLinesPerDirection);
}

TEST(DebugDraw, FilledTrianglesKeepTheirOpacity) {
    DebugGeometry geometry;
    DebugDraw draw(geometry);
    const PackedColor halfRed = WithAlpha(kDebugHarmful, kHalfAlpha);
    draw.FillTriangle(kOrigin, kCenter, kTop, halfRed);
    ASSERT_EQ(geometry.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Triangles),
              kVerticesPerTriangle);
    for (const PackedColor color :
         geometry.Colors(DebugBatch::WorldDepth, DebugPrimitive::Triangles)) {
        EXPECT_EQ(AlphaOf(color), kHalfAlpha);
    }
    EXPECT_EQ(LinesIn(geometry), 0u);
}

TEST(DebugDraw, TheBatchDecidesWhereVerticesGo) {
    DebugGeometry geometry;
    DebugDraw overlay(geometry, DebugBatch::Overlay);
    EXPECT_EQ(overlay.Batch(), DebugBatch::Overlay);
    overlay.DrawAABB(kOrigin, kCenter, kDebugQuery);
    overlay.DrawText(kCenter, kLabel, kDebugText);
    EXPECT_EQ(LinesIn(geometry, DebugBatch::Overlay), kBoxLines);
    EXPECT_EQ(LinesIn(geometry, DebugBatch::WorldDepth), 0u);
    ASSERT_EQ(geometry.Texts().size(), 1u);
    EXPECT_EQ(geometry.Texts().front().batch, DebugBatch::Overlay);
    EXPECT_EQ(geometry.TextOf(geometry.Texts().front()), kLabel);
}

TEST(DebugDraw, DegenerateShapesDrawNothing) {
    DebugGeometry geometry;
    DebugDraw draw(geometry);
    draw.DrawRay(kOrigin, kOrigin, kLengthM, kDebugQuery);                    // no direction
    draw.DrawArrow(kCenter, kCenter, kDebugQuery);                            // no length
    draw.DrawCircle(kCenter, kUp, 0.0f, kDebugBeneficial);                    // no radius
    draw.DrawCylinder(kCenter, kCenter, kRadiusM, kDebugPhysics);             // no axis
    draw.DrawCone(kCenter, kForward, 0.0f, kHalfAngleRadians, kDebugHarmful); // no length
    draw.DrawArcSector(kOrigin, kUp, kUp, 0.0f, kOuterRadiusM, kSwingRadians,
                       kDebugHarmful);                     // forward is up
    draw.DrawGridXZ(kGridHalfSizeM, 0.0f, kDebugInactive); // no spacing
    EXPECT_TRUE(geometry.IsEmpty());
}

TEST(DebugDraw, ACapsuleWithoutLengthIsASphere) {
    DebugGeometry geometry;
    DebugDraw draw(geometry);
    draw.DrawCapsule(kCenter, kCenter, kRadiusM, kDebugPredicted, kSegments);
    EXPECT_EQ(LinesIn(geometry), SphereLines(kSegments));
}

TEST(DebugDraw, ShapesLieWhereTheySay) {
    DebugGeometry circle;
    DebugDraw(circle).DrawCircle(kCenter, kUp, kRadiusM, kDebugBeneficial, kSegments);
    for (const Vec3 &point : circle.Positions(DebugBatch::WorldDepth, DebugPrimitive::Lines)) {
        EXPECT_NEAR(glm::length(point - kCenter), kRadiusM, kToleranceM);
        EXPECT_NEAR(point.y, kCenter.y, kToleranceM)
            << "a circle with normal +Y stays in its plane";
    }
    DebugGeometry grid;
    DebugDraw(grid).DrawGridXZ(kGridHalfSizeM, kGridSpacingM, kDebugInactive);
    for (const Vec3 &point : grid.Positions(DebugBatch::WorldDepth, DebugPrimitive::Lines)) {
        EXPECT_EQ(point.y, 0.0f);
        EXPECT_LE(std::abs(point.x), kGridHalfSizeM + kToleranceM);
        EXPECT_LE(std::abs(point.z), kGridHalfSizeM + kToleranceM);
    }
    DebugGeometry ray;
    DebugDraw(ray).DrawRay(kCenter, kForward + kForward, kLengthM, kDebugQuery);
    const auto rayEnds = ray.Positions(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    ASSERT_EQ(rayEnds.size(), kVerticesPerLine);
    EXPECT_NEAR(glm::length(rayEnds.back() - rayEnds.front()), kLengthM, kToleranceM)
        << "the direction is normalized: the ray is length long";
}

TEST(DebugDraw, AFrameOfEveryShapeNeverAllocates) {
    const std::vector<ShapeCase> shapes = EveryShape();
    DebugGeometry geometry;
    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        DebugDraw depth(geometry);
        DebugDraw overlay(geometry, DebugBatch::Overlay);
        for (const ShapeCase &shape : shapes) {
            shape.draw(depth);
            shape.draw(overlay);
        }
        depth.FillTriangle(kOrigin, kCenter, kTop, kDebugHarmful);
        overlay.DrawText(kCenter, kLabel, kDebugText);
        allocations = scope.Count();
    }
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(geometry.DroppedVertices(), 0u);
}

TEST(Camera, LookAtFacesTheTarget) {
    const Vec3 eye(0.0f, kHeightM, kLengthM);
    const Camera camera = LookAt(eye, kOrigin, kUp);
    const Vec3 looking = camera.rotation * kForward; // a camera looks down its local -Z
    const Vec3 expected = glm::normalize(kOrigin - eye);
    EXPECT_NEAR(glm::length(looking - expected), 0.0f, kToleranceM);
    EXPECT_EQ(camera.position, eye);
    EXPECT_EQ(camera.verticalFovRadians, kDefaultVerticalFovRadians);
}
