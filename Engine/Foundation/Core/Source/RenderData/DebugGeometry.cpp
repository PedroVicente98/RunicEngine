// DebugGeometry (DebugGeometry.hpp): arrays allocated once, adds bounded by the capacity.

#include "RenderData/DebugGeometry.hpp"

namespace Runic::RenderData {

namespace {

constexpr std::size_t IndexOf(DebugBatch batch) noexcept { return static_cast<std::size_t>(batch); }
constexpr std::size_t IndexOf(DebugPrimitive primitive) noexcept {
    return static_cast<std::size_t>(primitive);
}

} // namespace

DebugGeometry::DebugGeometry(const DebugGeometryCapacity &capacity) : m_Capacity(capacity) {
    for (auto &batch : m_Streams) {
        batch[IndexOf(DebugPrimitive::Lines)].capacity = capacity.lineVertices;
        batch[IndexOf(DebugPrimitive::Triangles)].capacity = capacity.triangleVertices;
        for (Stream &stream : batch) {
            stream.positions.reserve(stream.capacity);
            stream.colors.reserve(stream.capacity);
        }
    }
    m_Texts.reserve(capacity.texts);
    m_TextBytes.reserve(capacity.textBytes);
}

void DebugGeometry::Clear() noexcept {
    for (auto &batch : m_Streams) {
        for (Stream &stream : batch) {
            stream.positions.clear();
            stream.colors.clear();
        }
    }
    m_Texts.clear();
    m_TextBytes.clear();
    m_DroppedVertices = 0;
    m_DroppedTexts = 0;
}

bool DebugGeometry::AddLine(DebugBatch batch, Core::Vec3 a, Core::Vec3 b,
                            PackedColor color) noexcept {
    Stream &stream = StreamOf(batch, DebugPrimitive::Lines);
    if (stream.positions.size() + kVerticesPerLine > stream.capacity) {
        m_DroppedVertices += kVerticesPerLine;
        return false;
    }
    stream.positions.push_back(a);
    stream.positions.push_back(b);
    stream.colors.push_back(color);
    stream.colors.push_back(color);
    return true;
}

bool DebugGeometry::AddTriangle(DebugBatch batch, Core::Vec3 a, Core::Vec3 b, Core::Vec3 c,
                                PackedColor color) noexcept {
    Stream &stream = StreamOf(batch, DebugPrimitive::Triangles);
    if (stream.positions.size() + kVerticesPerTriangle > stream.capacity) {
        m_DroppedVertices += kVerticesPerTriangle;
        return false;
    }
    stream.positions.push_back(a);
    stream.positions.push_back(b);
    stream.positions.push_back(c);
    stream.colors.push_back(color);
    stream.colors.push_back(color);
    stream.colors.push_back(color);
    return true;
}

bool DebugGeometry::AddText(DebugBatch batch, Core::Vec3 position, std::string_view text,
                            PackedColor color) noexcept {
    if (m_Texts.size() >= m_Capacity.texts ||
        m_TextBytes.size() + text.size() > m_Capacity.textBytes) {
        ++m_DroppedTexts;
        return false;
    }
    const auto offset = static_cast<std::uint32_t>(m_TextBytes.size());
    m_TextBytes.insert(m_TextBytes.end(), text.begin(), text.end());
    m_Texts.push_back(
        DebugText{position, color, batch, offset, static_cast<std::uint32_t>(text.size())});
    return true;
}

std::span<const Core::Vec3> DebugGeometry::Positions(DebugBatch batch,
                                                     DebugPrimitive primitive) const noexcept {
    return StreamOf(batch, primitive).positions;
}

std::span<const PackedColor> DebugGeometry::Colors(DebugBatch batch,
                                                   DebugPrimitive primitive) const noexcept {
    return StreamOf(batch, primitive).colors;
}

std::uint32_t DebugGeometry::VertexCount(DebugBatch batch,
                                         DebugPrimitive primitive) const noexcept {
    return static_cast<std::uint32_t>(StreamOf(batch, primitive).positions.size());
}

std::span<const DebugText> DebugGeometry::Texts() const noexcept { return m_Texts; }

std::string_view DebugGeometry::TextOf(const DebugText &text) const noexcept {
    return std::string_view(m_TextBytes.data() + text.offset, text.length);
}

std::uint32_t DebugGeometry::DroppedVertices() const noexcept { return m_DroppedVertices; }
std::uint32_t DebugGeometry::DroppedTexts() const noexcept { return m_DroppedTexts; }

bool DebugGeometry::IsEmpty() const noexcept {
    for (const auto &batch : m_Streams) {
        for (const Stream &stream : batch) {
            if (!stream.positions.empty()) {
                return false;
            }
        }
    }
    return m_Texts.empty();
}

const DebugGeometryCapacity &DebugGeometry::Capacity() const noexcept { return m_Capacity; }

DebugGeometry::Stream &DebugGeometry::StreamOf(DebugBatch batch,
                                               DebugPrimitive primitive) noexcept {
    return m_Streams[IndexOf(batch)][IndexOf(primitive)];
}

const DebugGeometry::Stream &DebugGeometry::StreamOf(DebugBatch batch,
                                                     DebugPrimitive primitive) const noexcept {
    return m_Streams[IndexOf(batch)][IndexOf(primitive)];
}

} // namespace Runic::RenderData
