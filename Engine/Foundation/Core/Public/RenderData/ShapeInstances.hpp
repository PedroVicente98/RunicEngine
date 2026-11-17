#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Math.hpp"
#include "PlainIds.hpp"
#include "RenderData/Color.hpp"
#include "WorldSpace.hpp"

// Shape instances: debug shapes the presenter keeps between frames (slice 5, before CORE-4's mesh
// instances). Debug geometry is redrawn from nothing every frame; a shape instance is created once
// by its RenderInstanceId, moved only when it changes and freed when it leaves the frame, so a
// frame of shapes that did not change costs the presenter nothing (RunicDocs › Render data,
// extraction and world UI › The presenter; GDB-3's Done when).
//
// Change is measured in the world: a shape changes when its place in the world, its rotation, its
// size, its color or its kind changes. A new frame origin changes no shape, even though every
// position in the frame, being relative to the origin, is a different number.
//
// The writer (ShapeInstanceWriter, owned by one extractor) stamps each list with a revision and the
// revision of the list it wrote before it (its base), and lists which instances changed since the
// base. A reader that applied the base needs only those; a reader that applied another revision,
// because it skipped a frame, compares the whole list by ID. A list whose revision the reader
// already applied needs nothing.

namespace Runic::RenderData {

// The unit shape an instance scales. A capsule keeps round ends: its size is its diameter (x and
// z) and its full height (y), never scaled into an ellipsoid.
enum class ShapeKind : std::uint8_t { Box, Sphere, Capsule, Cylinder };
inline constexpr std::size_t kShapeKindCount = 4;

// The shape instances one frame holds unless its capacity says otherwise.
inline constexpr std::uint32_t kDefaultShapeInstanceCapacity = 65'536;

// One shape, as the presenter receives it.
struct ShapeInstance {
    Core::RenderInstanceId id;
    ShapeKind shape = ShapeKind::Box;
    PackedColor color = kDebugInactive;
    Core::Vec3 position{0.0f}; // its center, relative to the frame's origin
    Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    // Metres along its own axes: a box's full extents; a sphere's, a capsule's and a cylinder's
    // diameter in x and z and full height in y.
    Core::Vec3 size{1.0f};
};

// The shape instances of one frame. Only a ShapeInstanceWriter fills it.
class ShapeInstanceList {
  public:
    explicit ShapeInstanceList(std::uint32_t capacity = kDefaultShapeInstanceCapacity);
    ShapeInstanceList(const ShapeInstanceList &) = delete;
    ShapeInstanceList &operator=(const ShapeInstanceList &) = delete;

    std::span<const ShapeInstance> Instances() const noexcept;
    // The writer's revision of this list: two lists with the same revision hold the same shapes
    // in the same places. 0 is the empty list a writer starts from.
    std::uint64_t Revision() const noexcept;
    // The revision of the list its writer wrote before this one.
    std::uint64_t BaseRevision() const noexcept;
    // True when shapes were added, removed or reordered since the base: the indices of the base no
    // longer name the same shapes, and a reader compares by ID.
    bool LayoutChanged() const noexcept;
    // When the layout did not change: the indices of the shapes that changed since the base, in
    // ascending order.
    std::span<const std::uint32_t> Changed() const noexcept;
    // Shapes the writer could not add this frame: past the capacity, or too far from the origin
    // for a 32-bit position (Core::kMaxLocalOffsetM).
    std::uint32_t Dropped() const noexcept;
    std::uint32_t Capacity() const noexcept;

  private:
    friend class ShapeInstanceWriter;

    std::vector<ShapeInstance> m_Instances;
    std::vector<std::uint32_t> m_Changed;
    std::uint32_t m_Capacity = 0;
    std::uint64_t m_Revision = 0;
    std::uint64_t m_BaseRevision = 0;
    bool m_LayoutChanged = false;
    std::uint32_t m_Dropped = 0;
};

// Writes one frame's ShapeInstanceList after another and works out what changed between them. One
// writer per stream of frames, used by one thread; it allocates only when it is made.
class ShapeInstanceWriter {
  public:
    explicit ShapeInstanceWriter(std::uint32_t capacity = kDefaultShapeInstanceCapacity);
    ShapeInstanceWriter(const ShapeInstanceWriter &) = delete;
    ShapeInstanceWriter &operator=(const ShapeInstanceWriter &) = delete;

    // Starts filling list, the shape list of a frame whose origin is origin: empties it. The list
    // is used until End.
    void Begin(ShapeInstanceList &list, Core::WorldPosition origin) noexcept;
    // Adds a shape at its place in the world. False, and counted as dropped, when the list is full
    // or the place is too far from the origin.
    bool Add(Core::RenderInstanceId id, ShapeKind shape, Core::WorldPosition position,
             Core::Quat rotation, Core::Vec3 size, PackedColor color) noexcept;
    // Finishes the list: stamps its revision, its base and what changed, and keeps its shapes to
    // compare the next list with.
    void End() noexcept;

    // The revision of the last list ended; 0 before the first.
    std::uint64_t Revision() const noexcept;
    std::uint32_t Capacity() const noexcept;

  private:
    // A shape as the writer compares it: in world terms.
    struct Kept {
        Core::RenderInstanceId id;
        ShapeKind shape = ShapeKind::Box;
        PackedColor color = kDebugInactive;
        Core::WorldPosition position;
        Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Core::Vec3 size{1.0f};
    };

    std::vector<Kept> m_Previous; // the last list ended
    std::vector<Kept> m_Current;  // the list being written
    std::uint32_t m_Capacity = 0;
    std::uint64_t m_Revision = 0;
    ShapeInstanceList *m_List = nullptr;
    Core::WorldPosition m_Origin;
    bool m_LayoutChanged = false;
};

} // namespace Runic::RenderData
