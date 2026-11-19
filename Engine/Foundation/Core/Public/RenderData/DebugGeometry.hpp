#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "Math.hpp"
#include "RenderData/Color.hpp"

// DebugGeometry: what debug drawing produces, as plain lines, triangles and text (RunicDocs › Debug
// drawing (Engine)). Per batch and primitive it keeps a positions array and a colors array, the two
// vertex streams Godot uploads (3 floats, then 4 color bytes per vertex), so the upload is a copy
// with no per-vertex work (slice 2 decision D5). Positions are camera-relative floats: the frame's
// origin is the one 64-bit value. The arrays are allocated once, at construction; after that
// nothing allocates, and an add past the capacity is dropped and counted (the vertex budget).

namespace Runic::RenderData {

// Depth-tested geometry, hidden behind what is in front of it, or overlay geometry, always visible.
enum class DebugBatch : std::uint8_t { WorldDepth, Overlay };
inline constexpr std::size_t kDebugBatchCount = 2;

enum class DebugPrimitive : std::uint8_t { Lines, Triangles };
inline constexpr std::size_t kDebugPrimitiveCount = 2;

inline constexpr std::uint32_t kVerticesPerLine = 2;
inline constexpr std::uint32_t kVerticesPerTriangle = 3;

// How much one DebugGeometry holds. Line and triangle vertices count per batch.
inline constexpr std::uint32_t kDefaultLineVertexCapacity = 65'536;
inline constexpr std::uint32_t kDefaultTriangleVertexCapacity = 16'384;
inline constexpr std::uint32_t kDefaultTextCapacity = 256;
inline constexpr std::uint32_t kDefaultTextByteCapacity = 16'384;

struct DebugGeometryCapacity {
    std::uint32_t lineVertices = kDefaultLineVertexCapacity;
    std::uint32_t triangleVertices = kDefaultTriangleVertexCapacity;
    std::uint32_t texts = kDefaultTextCapacity;
    std::uint32_t textBytes = kDefaultTextByteCapacity;
};

// One label: where it is (camera-relative), its color and batch, and its bytes inside the
// geometry's text storage.
struct DebugText {
    Core::Vec3 position{0.0f};
    PackedColor color = kDebugText;
    DebugBatch batch = DebugBatch::WorldDepth;
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

class DebugGeometry {
  public:
    explicit DebugGeometry(const DebugGeometryCapacity &capacity = {});
    DebugGeometry(const DebugGeometry &) = delete;
    DebugGeometry &operator=(const DebugGeometry &) = delete;

    // Empties every array and resets the dropped counts; the capacity stays, so nothing is freed or
    // allocated.
    void Clear() noexcept;

    // Each returns false, and counts what it dropped, when the batch's array (or the text storage)
    // is full.
    bool AddLine(DebugBatch batch, Core::Vec3 a, Core::Vec3 b, PackedColor color) noexcept;
    bool AddTriangle(DebugBatch batch, Core::Vec3 a, Core::Vec3 b, Core::Vec3 c,
                     PackedColor color) noexcept;
    bool AddText(DebugBatch batch, Core::Vec3 position, std::string_view text,
                 PackedColor color) noexcept;

    // The vertex streams of one batch and primitive: positions[i] and colors[i] are vertex i. Lines
    // are vertex pairs, triangles vertex triples.
    std::span<const Core::Vec3> Positions(DebugBatch batch,
                                          DebugPrimitive primitive) const noexcept;
    std::span<const PackedColor> Colors(DebugBatch batch, DebugPrimitive primitive) const noexcept;
    std::uint32_t VertexCount(DebugBatch batch, DebugPrimitive primitive) const noexcept;

    std::span<const DebugText> Texts() const noexcept;
    std::string_view TextOf(const DebugText &text) const noexcept; // valid until the next Clear

    std::uint32_t DroppedVertices() const noexcept; // since the last Clear
    std::uint32_t DroppedTexts() const noexcept;    // since the last Clear
    bool IsEmpty() const noexcept;                  // no vertex and no text
    const DebugGeometryCapacity &Capacity() const noexcept;

  private:
    struct Stream {
        std::vector<Core::Vec3> positions;
        std::vector<PackedColor> colors;
        std::uint32_t capacity = 0;
    };
    Stream &StreamOf(DebugBatch batch, DebugPrimitive primitive) noexcept;
    const Stream &StreamOf(DebugBatch batch, DebugPrimitive primitive) const noexcept;

    DebugGeometryCapacity m_Capacity;
    Stream m_Streams[kDebugBatchCount][kDebugPrimitiveCount];
    std::vector<DebugText> m_Texts;
    std::vector<char> m_TextBytes;
    std::uint32_t m_DroppedVertices = 0;
    std::uint32_t m_DroppedTexts = 0;
};

} // namespace Runic::RenderData
