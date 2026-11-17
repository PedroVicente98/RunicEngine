#pragma once

#include <cstdint>
#include <string_view>

#include "EntityQuery.hpp"
#include "EntityWorld.hpp"
#include "ExtractorRegistry.hpp"
#include "Math.hpp"
#include "PlainIds.hpp"
#include "RenderData/Color.hpp"
#include "RenderData/ShapeInstances.hpp"
#include "WorldComponents.hpp"

// Entities as debug shapes: the first extractor (RunicDocs › Modules, work packages: CLI-1). An
// entity with a WorldTransform, a DebugShape and a RenderIdentity is drawn as a shape instance in
// every frame it is within the draw distance; the presenter keeps the instance between frames by
// the entity's RenderInstanceId and moves it only when the entity moves.

namespace Runic::Client {

// How an entity looks as a debug shape. The center sits at the entity's position plus offset,
// turned with the entity; size is ShapeInstance's (a capsule's diameter and full height).
struct DebugShape {
    RenderData::ShapeKind shape = RenderData::ShapeKind::Capsule;
    Core::Vec3 size{1.0f};
    Core::Vec3 offset{0.0f};
    RenderData::PackedColor color = RenderData::kDebugInactive;
};

// The ID every frame gives what an entity draws, so the presenter finds its instance again. In a
// connected client it comes from the entity's GlobalEntityId; a local world gives its own.
struct RenderIdentity {
    Core::RenderInstanceId id;
};

// The names the components are registered under.
inline constexpr std::string_view kDebugShapeName = "engine:debug_shape";
inline constexpr std::string_view kRenderIdentityName = "engine:render_identity";

// Registers DebugShape and RenderIdentity (and World's components) in a world. Calling it again
// does nothing.
void RegisterClientComponents(ECS::EntityWorld &world);

inline constexpr std::string_view kDebugShapeExtractorName = "engine:debug_shapes";

// What the debug-shape extractor wrote into the last frame.
struct DebugShapeStats {
    std::uint32_t drawn = 0;              // shapes in the frame
    std::uint32_t beyondDrawDistance = 0; // entities left out because they are too far
    std::uint32_t dropped = 0;            // entities the frame had no room for
    std::uint64_t revision = 0;           // the frame's shape revision
};

// Writes every drawable entity of a world into the frame's shape list. It is the list's one
// writer: register one per frame stream.
class DebugShapeExtractor final : public Extractor {
  public:
    // world must outlive the extractor; RegisterClientComponents is called on it.
    explicit DebugShapeExtractor(
        ECS::EntityWorld &world,
        std::uint32_t capacity = RenderData::kDefaultShapeInstanceCapacity);

    void Extract(const ExtractionView &view, RenderData::RenderFrame &frame) override;

    DebugShapeStats Stats() const noexcept { return m_Stats; }

  private:
    ECS::EntityQuery<const World::WorldTransform, const DebugShape, const RenderIdentity> m_Shapes;
    RenderData::ShapeInstanceWriter m_Writer;
    DebugShapeStats m_Stats;
};

} // namespace Runic::Client
