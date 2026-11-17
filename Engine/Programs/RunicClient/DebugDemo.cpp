// RunicClient's debug demo (DebugDemo.hpp).

#include "DebugDemo.hpp"

#include "RenderData/DebugDraw.hpp"
#include "WorldSpace.hpp"

#include <glm/ext/scalar_constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace Runic::Programs {

namespace {

using Core::Vec3;
using RenderData::DebugBatch;
using RenderData::DebugDraw;

constexpr float kTwoPi = glm::two_pi<float>();
const Vec3 kUp(0.0f, 1.0f, 0.0f);
const Vec3 kOrigin(0.0f);

// The demo's animation follows the frame number at the rate Godot usually draws.
constexpr float kFramesPerSecond = 60.0f;

constexpr float kGridHalfSizeM = 20.0f;
constexpr float kGridSpacingM = 1.0f;

// A capsule the size of a person walks a 3 m circle every 12 s, with its facing ray and a melee
// arc in front of it.
constexpr float kWalkPeriodS = 12.0f;
constexpr float kWalkRadiusM = 3.0f;
constexpr float kCapsuleRadiusM = 0.4f;
constexpr float kCapsuleHeightM = 1.8f;
constexpr float kChestHeightM = 1.2f;
constexpr float kRayLengthM = 2.5f;
constexpr float kArcInnerRadiusM = 0.6f;
constexpr float kArcOuterRadiusM = 2.0f;
constexpr float kArcAngleRadians = 1.6f;

// Fixed shapes around the scene.
const Vec3 kSphereCenter(6.0f, 1.5f, -4.0f);
constexpr float kSphereRadiusM = 1.5f;
const Vec3 kBoxMin(-7.0f, 0.0f, -6.0f);
const Vec3 kBoxMax(-5.0f, 2.0f, -4.0f);
const Vec3 kCylinderBase(-6.0f, 0.0f, 5.0f);
const Vec3 kCylinderTop(-6.0f, 3.0f, 5.0f);
constexpr float kCylinderRadiusM = 1.0f;
const Vec3 kConeApex(6.0f, 3.0f, 5.0f);
const Vec3 kConeDirection(0.0f, -1.0f, 0.0f);
constexpr float kConeLengthM = 3.0f;
constexpr float kConeHalfAngleRadians = 0.45f;
const Vec3 kVolumeOrigin(0.0f, 1.0f, -7.0f);
const Vec3 kVolumeForward(0.0f, 0.0f, 1.0f);
constexpr float kVolumeOuterRadiusM = 3.0f;
constexpr float kVolumeAngleRadians = 1.2f;
constexpr float kVolumeHeightM = 2.0f;
const Vec3 kTiltedBoxCenter(0.0f, 1.0f, 7.0f);
const Vec3 kTiltedBoxAxisX(1.0f, 0.0f, 1.0f);
const Vec3 kTiltedBoxAxisY(0.0f, 1.0f, 0.0f);
const Vec3 kTiltedBoxAxisZ(-1.0f, 0.0f, 1.0f);
const Vec3 kTiltedBoxHalfExtent(1.0f, 1.0f, 0.5f);

// Two filled squares on the ground overlap, so their opacity shows.
constexpr float kFillHeightM = 0.01f; // just above the grid
const Vec3 kRedSquareMin(1.0f, kFillHeightM, 1.0f);
const Vec3 kRedSquareMax(4.0f, kFillHeightM, 4.0f);
const Vec3 kCyanSquareMin(2.5f, kFillHeightM, 2.5f);
const Vec3 kCyanSquareMax(5.5f, kFillHeightM, 5.5f);
constexpr std::uint8_t kQuarterAlpha = 64;

// Always on top: the world axes at the origin, and the frame counter above the capsule.
constexpr float kAxisLengthM = 1.5f;
const Vec3 kAxisX(1.0f, 0.0f, 0.0f);
const Vec3 kAxisY(0.0f, 1.0f, 0.0f);
const Vec3 kAxisZ(0.0f, 0.0f, 1.0f);
constexpr float kLabelLiftM = 0.6f;
constexpr std::size_t kFrameLabelBytes = 32;
constexpr std::string_view kFrameLabelFormat = "frame {}";

constexpr std::string_view kCapsuleLabel = "capsule";
constexpr std::string_view kSphereLabel = "sphere";
constexpr std::string_view kBoxLabel = "box";
constexpr std::string_view kCylinderLabel = "cylinder";
constexpr std::string_view kConeLabel = "cone";
constexpr std::string_view kVolumeLabel = "arc volume";
constexpr std::string_view kTiltedBoxLabel = "oriented box";
constexpr std::string_view kFillsLabel = "fills with opacity";

void FillSquareXZ(DebugDraw &draw, Vec3 min, Vec3 max, RenderData::PackedColor color) {
    const Vec3 a(min.x, min.y, min.z);
    const Vec3 b(max.x, min.y, min.z);
    const Vec3 c(max.x, min.y, max.z);
    const Vec3 d(min.x, min.y, max.z);
    draw.FillTriangle(a, b, c, color);
    draw.FillTriangle(a, c, d, color);
}

Vec3 Above(Vec3 point, float height) { return point + kUp * height; }

// DrawGridXZ draws around the frame's origin, at height kGridHeightM: its lines, moved by shift.
void DrawGridAt(DebugDraw &draw, Vec3 shift) {
    const auto lines = static_cast<int>(kGridHalfSizeM / kGridSpacingM);
    for (int i = -lines; i <= lines; ++i) {
        const float along = static_cast<float>(i) * kGridSpacingM;
        draw.DrawLine(shift + Vec3(along, RenderData::kGridHeightM, -kGridHalfSizeM),
                      shift + Vec3(along, RenderData::kGridHeightM, kGridHalfSizeM),
                      RenderData::kDebugInactive);
        draw.DrawLine(shift + Vec3(-kGridHalfSizeM, RenderData::kGridHeightM, along),
                      shift + Vec3(kGridHalfSizeM, RenderData::kGridHeightM, along),
                      RenderData::kDebugInactive);
    }
}

} // namespace

void DebugDemoExtractor::Extract(const Client::ExtractionView &view,
                                 RenderData::RenderFrame &frame) {
    // The scene sits around the world's origin; the frame is relative to the camera.
    const Vec3 shift = Core::ToLocal(Core::WorldPosition{} - view.origin);
    const auto seconds = static_cast<float>(view.frame) / kFramesPerSecond;

    DebugDraw depth(frame.debug);
    // The grid is drawn around the origin of the frame: move it to the world's.
    DrawGridAt(depth, shift);

    const float walk = kTwoPi * seconds / kWalkPeriodS;
    const Vec3 feet = shift + Vec3(std::cos(walk) * kWalkRadiusM, RenderData::kGridHeightM,
                                   std::sin(walk) * kWalkRadiusM); // on the grid
    const Vec3 facing(-std::sin(walk), 0.0f, std::cos(walk));      // along the circle
    const Vec3 chest = Above(feet, kChestHeightM);
    const Vec3 top = Above(feet, kCapsuleHeightM);
    depth.DrawCapsule(Above(feet, kCapsuleRadiusM), Above(feet, kCapsuleHeightM - kCapsuleRadiusM),
                      kCapsuleRadiusM, RenderData::kDebugPredicted);
    depth.DrawRay(chest, facing, kRayLengthM, RenderData::kDebugQuery);
    depth.DrawArcSector(chest, facing, kUp, kArcInnerRadiusM, kArcOuterRadiusM, kArcAngleRadians,
                        RenderData::kDebugHarmful);

    depth.DrawSphere(shift + kSphereCenter, kSphereRadiusM, RenderData::kDebugBeneficial);
    depth.DrawAABB(shift + kBoxMin, shift + kBoxMax, RenderData::kDebugPhysics);
    depth.DrawCylinder(shift + kCylinderBase, shift + kCylinderTop, kCylinderRadiusM,
                       RenderData::kDebugAuthoritative);
    depth.DrawCone(shift + kConeApex, kConeDirection, kConeLengthM, kConeHalfAngleRadians,
                   RenderData::kDebugHarmful);
    depth.DrawArcVolume(shift + kVolumeOrigin, kVolumeForward, kUp, RenderData::kNoInnerRadius,
                        kVolumeOuterRadiusM, kVolumeAngleRadians, kVolumeHeightM,
                        RenderData::kDebugHarmful);
    depth.DrawOBB(shift + kTiltedBoxCenter, kTiltedBoxAxisX, kTiltedBoxAxisY, kTiltedBoxAxisZ,
                  kTiltedBoxHalfExtent, RenderData::kDebugPhysics);
    FillSquareXZ(depth, shift + kRedSquareMin, shift + kRedSquareMax,
                 RenderData::WithAlpha(RenderData::kDebugHarmful, RenderData::kHalfAlpha));
    FillSquareXZ(depth, shift + kCyanSquareMin, shift + kCyanSquareMax,
                 RenderData::WithAlpha(RenderData::kDebugBeneficial, kQuarterAlpha));

    depth.DrawText(Above(top, kLabelLiftM), kCapsuleLabel, RenderData::kDebugText);
    depth.DrawText(shift + Above(kSphereCenter, kSphereRadiusM + kLabelLiftM), kSphereLabel,
                   RenderData::kDebugText);
    depth.DrawText(shift + Above(kBoxMax, kLabelLiftM), kBoxLabel, RenderData::kDebugText);
    depth.DrawText(shift + Above(kCylinderTop, kLabelLiftM), kCylinderLabel,
                   RenderData::kDebugText);
    depth.DrawText(shift + Above(kConeApex, kLabelLiftM), kConeLabel, RenderData::kDebugText);
    depth.DrawText(shift + Above(kVolumeOrigin, kVolumeHeightM), kVolumeLabel,
                   RenderData::kDebugText);
    depth.DrawText(shift + Above(kTiltedBoxCenter, kTiltedBoxHalfExtent.y + kLabelLiftM),
                   kTiltedBoxLabel, RenderData::kDebugText);
    depth.DrawText(shift + Above(kCyanSquareMax, kLabelLiftM), kFillsLabel, RenderData::kDebugText);

    DebugDraw overlay(frame.debug, DebugBatch::Overlay);
    overlay.DrawArrow(shift + kOrigin, shift + kAxisX * kAxisLengthM, RenderData::kDebugHarmful);
    overlay.DrawArrow(shift + kOrigin, shift + kAxisY * kAxisLengthM, RenderData::kDebugPredicted);
    overlay.DrawArrow(shift + kOrigin, shift + kAxisZ * kAxisLengthM, RenderData::kDebugPhysics);
    char counter[kFrameLabelBytes];
    const auto written = std::format_to_n(counter, static_cast<std::ptrdiff_t>(kFrameLabelBytes),
                                          kFrameLabelFormat, view.frame);
    const auto length = std::min(static_cast<std::size_t>(written.size), kFrameLabelBytes);
    overlay.DrawText(Above(top, kLabelLiftM + kLabelLiftM), std::string_view(counter, length),
                     RenderData::kDebugAuthoritative);
}

} // namespace Runic::Programs
