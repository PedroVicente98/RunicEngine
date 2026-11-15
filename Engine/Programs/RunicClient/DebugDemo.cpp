// RunicClient's debug demo (DebugDemo.hpp).

#include "DebugDemo.hpp"

#include "RenderData/Camera.hpp"
#include "RenderData/DebugDraw.hpp"
#include "Time.hpp"

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

constexpr std::string_view kLayerName = "engine:debug_demo";
constexpr float kTwoPi = glm::two_pi<float>();
const Vec3 kUp(0.0f, 1.0f, 0.0f);
const Vec3 kOrigin(0.0f);

// The camera circles the scene once every 40 s, looking at its center.
constexpr float kCameraOrbitPeriodS = 40.0f;
constexpr float kCameraOrbitRadiusM = 16.0f;
constexpr float kCameraHeightM = 8.0f;
const Vec3 kSceneCenter(0.0f, 1.0f, 0.0f);

constexpr float kGridHalfSizeM = 10.0f;
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

} // namespace

DebugDemoLayer::DebugDemoLayer(RenderData::RenderFrameExchange &frames)
    : Layer(std::string(kLayerName)), m_Frames(frames) {}

void DebugDemoLayer::OnUpdate(const Application::FrameTime &time) {
    const auto seconds = static_cast<float>(Core::ToSeconds(time.sinceStart));
    RenderData::RenderFrame &frame = m_Frames.WriteFrame();
    frame.debug.Clear();
    frame.origin = Core::WorldPosition{};

    const float orbit = kTwoPi * seconds / kCameraOrbitPeriodS;
    const Vec3 eye(std::cos(orbit) * kCameraOrbitRadiusM, kCameraHeightM,
                   std::sin(orbit) * kCameraOrbitRadiusM);
    frame.camera = RenderData::LookAt(eye, kSceneCenter, kUp);

    DebugDraw depth(frame.debug);
    depth.DrawGridXZ(kGridHalfSizeM, kGridSpacingM, RenderData::kDebugInactive);

    const float walk = kTwoPi * seconds / kWalkPeriodS;
    const Vec3 feet(std::cos(walk) * kWalkRadiusM, 0.0f, std::sin(walk) * kWalkRadiusM);
    const Vec3 facing(-std::sin(walk), 0.0f, std::cos(walk)); // along the circle
    const Vec3 chest = Above(feet, kChestHeightM);
    const Vec3 top = Above(feet, kCapsuleHeightM);
    depth.DrawCapsule(Above(feet, kCapsuleRadiusM), Above(feet, kCapsuleHeightM - kCapsuleRadiusM),
                      kCapsuleRadiusM, RenderData::kDebugPredicted);
    depth.DrawRay(chest, facing, kRayLengthM, RenderData::kDebugQuery);
    depth.DrawArcSector(chest, facing, kUp, kArcInnerRadiusM, kArcOuterRadiusM, kArcAngleRadians,
                        RenderData::kDebugHarmful);

    depth.DrawSphere(kSphereCenter, kSphereRadiusM, RenderData::kDebugBeneficial);
    depth.DrawAABB(kBoxMin, kBoxMax, RenderData::kDebugPhysics);
    depth.DrawCylinder(kCylinderBase, kCylinderTop, kCylinderRadiusM,
                       RenderData::kDebugAuthoritative);
    depth.DrawCone(kConeApex, kConeDirection, kConeLengthM, kConeHalfAngleRadians,
                   RenderData::kDebugHarmful);
    depth.DrawArcVolume(kVolumeOrigin, kVolumeForward, kUp, 0.0f, kVolumeOuterRadiusM,
                        kVolumeAngleRadians, kVolumeHeightM, RenderData::kDebugHarmful);
    depth.DrawOBB(kTiltedBoxCenter, kTiltedBoxAxisX, kTiltedBoxAxisY, kTiltedBoxAxisZ,
                  kTiltedBoxHalfExtent, RenderData::kDebugPhysics);
    FillSquareXZ(depth, kRedSquareMin, kRedSquareMax,
                 RenderData::WithAlpha(RenderData::kDebugHarmful, RenderData::kHalfAlpha));
    FillSquareXZ(depth, kCyanSquareMin, kCyanSquareMax,
                 RenderData::WithAlpha(RenderData::kDebugBeneficial, kQuarterAlpha));

    depth.DrawText(Above(top, kLabelLiftM), kCapsuleLabel, RenderData::kDebugText);
    depth.DrawText(Above(kSphereCenter, kSphereRadiusM + kLabelLiftM), kSphereLabel,
                   RenderData::kDebugText);
    depth.DrawText(Above(kBoxMax, kLabelLiftM), kBoxLabel, RenderData::kDebugText);
    depth.DrawText(Above(kCylinderTop, kLabelLiftM), kCylinderLabel, RenderData::kDebugText);
    depth.DrawText(Above(kConeApex, kLabelLiftM), kConeLabel, RenderData::kDebugText);
    depth.DrawText(Above(kVolumeOrigin, kVolumeHeightM), kVolumeLabel, RenderData::kDebugText);
    depth.DrawText(Above(kTiltedBoxCenter, kTiltedBoxHalfExtent.y + kLabelLiftM), kTiltedBoxLabel,
                   RenderData::kDebugText);
    depth.DrawText(Above(kCyanSquareMax, kLabelLiftM), kFillsLabel, RenderData::kDebugText);

    DebugDraw overlay(frame.debug, DebugBatch::Overlay);
    overlay.DrawArrow(kOrigin, kAxisX * kAxisLengthM, RenderData::kDebugHarmful);
    overlay.DrawArrow(kOrigin, kAxisY * kAxisLengthM, RenderData::kDebugPredicted);
    overlay.DrawArrow(kOrigin, kAxisZ * kAxisLengthM, RenderData::kDebugPhysics);
    char counter[kFrameLabelBytes];
    const auto written = std::format_to_n(counter, static_cast<std::ptrdiff_t>(kFrameLabelBytes),
                                          kFrameLabelFormat, time.frame + 1);
    const auto length = std::min(static_cast<std::size_t>(written.size), kFrameLabelBytes);
    overlay.DrawText(Above(top, kLabelLiftM + kLabelLiftM), std::string_view(counter, length),
                     RenderData::kDebugAuthoritative);

    m_Frames.Publish();
}

} // namespace Runic::Programs
