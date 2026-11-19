// CORE-3 acceptance tests: colors and DebugGeometry, the layout debug vertices are uploaded in.

#include "AllocationCounter.hpp"
#include "RenderData/Color.hpp"
#include "RenderData/DebugGeometry.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

using namespace Runic::RenderData;
using Runic::Core::Vec3;
namespace Testing = Runic::Core::Testing;

namespace {

constexpr std::uint8_t kRed = 10;
constexpr std::uint8_t kGreen = 20;
constexpr std::uint8_t kBlue = 30;
constexpr std::uint8_t kAlpha = 40;
constexpr std::size_t kFloatsPerPosition = 3;
constexpr std::size_t kBytesPerColor = 4;

constexpr DebugGeometryCapacity kTinyCapacity{
    .lineVertices = 4, .triangleVertices = 3, .texts = 2, .textBytes = 8};

const Vec3 kA(1.0f, 2.0f, 3.0f);
const Vec3 kB(4.0f, 5.0f, 6.0f);
const Vec3 kC(7.0f, 8.0f, 9.0f);
constexpr const char *kLabel = "tick";

} // namespace

TEST(Color, PacksRedFirstInMemory) {
    const PackedColor color = PackABGR(kRed, kGreen, kBlue, kAlpha);
    std::array<std::uint8_t, sizeof(PackedColor)> bytes{};
    std::memcpy(bytes.data(), &color, sizeof color);
    EXPECT_EQ(bytes, (std::array<std::uint8_t, sizeof(PackedColor)>{kRed, kGreen, kBlue, kAlpha}))
        << "Godot's vertex colors are the bytes R, G, B, A";
    EXPECT_EQ(RedOf(color), kRed);
    EXPECT_EQ(GreenOf(color), kGreen);
    EXPECT_EQ(BlueOf(color), kBlue);
    EXPECT_EQ(AlphaOf(color), kAlpha);
    EXPECT_EQ(AlphaOf(PackABGR(kRed, kGreen, kBlue)), kOpaqueAlpha);
}

TEST(Color, WithAlphaKeepsTheColor) {
    const PackedColor faded = WithAlpha(kDebugHarmful, kHalfAlpha);
    EXPECT_EQ(AlphaOf(faded), kHalfAlpha);
    EXPECT_EQ(RedOf(faded), RedOf(kDebugHarmful));
    EXPECT_EQ(GreenOf(faded), GreenOf(kDebugHarmful));
    EXPECT_EQ(BlueOf(faded), BlueOf(kDebugHarmful));
}

TEST(DebugGeometry, StartsEmpty) {
    const DebugGeometry geometry(kTinyCapacity);
    EXPECT_TRUE(geometry.IsEmpty());
    EXPECT_EQ(geometry.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines), 0u);
    EXPECT_TRUE(geometry.Texts().empty());
    EXPECT_EQ(geometry.DroppedVertices(), 0u);
}

TEST(DebugGeometry, LinesAndTrianglesGoToTheirBatchAndPrimitive) {
    DebugGeometry geometry(kTinyCapacity);
    EXPECT_TRUE(geometry.AddLine(DebugBatch::WorldDepth, kA, kB, kDebugPredicted));
    EXPECT_TRUE(geometry.AddTriangle(DebugBatch::Overlay, kA, kB, kC, kDebugHarmful));
    EXPECT_EQ(geometry.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines),
              kVerticesPerLine);
    EXPECT_EQ(geometry.VertexCount(DebugBatch::Overlay, DebugPrimitive::Triangles),
              kVerticesPerTriangle);
    EXPECT_EQ(geometry.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines), 0u);
    EXPECT_EQ(geometry.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Triangles), 0u);
    EXPECT_EQ(geometry.Positions(DebugBatch::Overlay,
                                 DebugPrimitive::Triangles)[kVerticesPerTriangle - 1],
              kC);
    for (const PackedColor color : geometry.Colors(DebugBatch::WorldDepth, DebugPrimitive::Lines)) {
        EXPECT_EQ(color, kDebugPredicted);
    }
}

TEST(DebugGeometry, VerticesAreInGodotsUploadLayout) {
    // Godot's surface for vertex and color: 3 floats per position in one stream, 4 color bytes per
    // vertex in the attribute stream. GodotBridge checks the same layout against Godot itself
    // (GDB-2).
    static_assert(sizeof(Vec3) == kFloatsPerPosition * sizeof(float));
    static_assert(sizeof(PackedColor) == kBytesPerColor);
    DebugGeometry geometry(kTinyCapacity);
    geometry.AddLine(DebugBatch::WorldDepth, kA, kB, PackABGR(kRed, kGreen, kBlue, kAlpha));
    const auto positions = geometry.Positions(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    const std::array<float, kVerticesPerLine * kFloatsPerPosition> expected{kA.x, kA.y, kA.z,
                                                                            kB.x, kB.y, kB.z};
    std::array<float, kVerticesPerLine * kFloatsPerPosition> floats{};
    std::memcpy(floats.data(), positions.data(), sizeof floats);
    EXPECT_EQ(floats, expected) << "positions are packed float triples";
    const auto colors = geometry.Colors(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    std::array<std::uint8_t, kVerticesPerLine * kBytesPerColor> bytes{};
    std::memcpy(bytes.data(), colors.data(), sizeof bytes);
    EXPECT_EQ(bytes, (std::array<std::uint8_t, kVerticesPerLine * kBytesPerColor>{
                         kRed, kGreen, kBlue, kAlpha, kRed, kGreen, kBlue, kAlpha}));
}

TEST(DebugGeometry, AFullBudgetDropsAndCounts) {
    DebugGeometry geometry(kTinyCapacity);
    EXPECT_TRUE(geometry.AddLine(DebugBatch::WorldDepth, kA, kB, kDebugQuery));
    EXPECT_TRUE(geometry.AddLine(DebugBatch::WorldDepth, kB, kC, kDebugQuery));
    EXPECT_FALSE(geometry.AddLine(DebugBatch::WorldDepth, kA, kC, kDebugQuery))
        << "four vertices fit, not six";
    EXPECT_TRUE(geometry.AddLine(DebugBatch::Overlay, kA, kC, kDebugQuery))
        << "each batch has its own budget";
    EXPECT_TRUE(geometry.AddTriangle(DebugBatch::WorldDepth, kA, kB, kC, kDebugHarmful));
    EXPECT_FALSE(geometry.AddTriangle(DebugBatch::WorldDepth, kA, kB, kC, kDebugHarmful));
    EXPECT_EQ(geometry.DroppedVertices(), kVerticesPerLine + kVerticesPerTriangle);
    EXPECT_EQ(geometry.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines),
              kTinyCapacity.lineVertices);
}

TEST(DebugGeometry, TextIsCopiedIntoTheGeometry) {
    DebugGeometry geometry(kTinyCapacity);
    {
        std::string label = kLabel;
        EXPECT_TRUE(geometry.AddText(DebugBatch::Overlay, kA, label, kDebugText));
        label.assign(label.size(), '?');
    }
    ASSERT_EQ(geometry.Texts().size(), 1u);
    const DebugText &text = geometry.Texts().front();
    EXPECT_EQ(geometry.TextOf(text), kLabel);
    EXPECT_EQ(text.position, kA);
    EXPECT_EQ(text.batch, DebugBatch::Overlay);
    EXPECT_EQ(text.color, kDebugText);
    EXPECT_TRUE(geometry.AddText(DebugBatch::Overlay, kB, kLabel, kDebugText))
        << "the bytes still fit";
    EXPECT_FALSE(geometry.AddText(DebugBatch::Overlay, kC, kLabel, kDebugText))
        << "only two texts fit";
    EXPECT_EQ(geometry.DroppedTexts(), 1u);
}

TEST(DebugGeometry, ClearEmptiesWithoutAllocating) {
    DebugGeometry geometry(kTinyCapacity);
    std::size_t allocations = 0;
    bool refilled = true;
    {
        Testing::AllocationScope scope;
        geometry.AddLine(DebugBatch::WorldDepth, kA, kB, kDebugPhysics);
        geometry.AddText(DebugBatch::WorldDepth, kA, kLabel, kDebugText);
        geometry.Clear();
        refilled =
            geometry.IsEmpty() && geometry.AddLine(DebugBatch::WorldDepth, kB, kC, kDebugPhysics);
        allocations = scope.Count();
    }
    EXPECT_TRUE(refilled);
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(geometry.DroppedVertices(), 0u);
}
