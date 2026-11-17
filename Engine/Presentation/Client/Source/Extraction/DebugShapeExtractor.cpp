// The debug-shape extractor (DebugShapes.hpp): one query over the drawable entities, culled by the
// draw distance, written through a ShapeInstanceWriter that works out what changed.

#include "DebugShapes.hpp"

#include "Assert.hpp"

namespace Runic::Client {

namespace {

constexpr std::string_view kDrawableQueryName = "engine:debug_shapes";

} // namespace

void RegisterClientComponents(ECS::EntityWorld &world) {
    World::RegisterWorldComponents(world);
    if (!world.IsRegistered<DebugShape>()) {
        world.RegisterComponent<DebugShape>(kDebugShapeName);
    }
    if (!world.IsRegistered<RenderIdentity>()) {
        world.RegisterComponent<RenderIdentity>(kRenderIdentityName);
    }
}

namespace {

ECS::EntityWorld &Registered(ECS::EntityWorld &world) {
    RegisterClientComponents(world);
    return world;
}

} // namespace

DebugShapeExtractor::DebugShapeExtractor(ECS::EntityWorld &world, std::uint32_t capacity)
    : m_Shapes(Registered(world)
                   .MakeQuery<const World::WorldTransform, const DebugShape, const RenderIdentity>(
                       kDrawableQueryName)),
      m_Writer(capacity) {}

void DebugShapeExtractor::Extract(const ExtractionView &view, RenderData::RenderFrame &frame) {
    const double drawDistanceSq =
        static_cast<double>(view.drawDistanceM) * static_cast<double>(view.drawDistanceM);
    DebugShapeStats stats;
    m_Writer.Begin(frame.shapes, view.origin);
    m_Shapes.Each([&](ECS::Entity, const World::WorldTransform &transform, const DebugShape &shape,
                      const RenderIdentity &identity) {
        const Core::WorldPosition center = transform.position + transform.rotation * shape.offset;
        if (Core::DistanceSq(center, view.camera) > drawDistanceSq) {
            ++stats.beyondDrawDistance;
            return;
        }
        if (m_Writer.Add(identity.id, shape.shape, center, transform.rotation, shape.size,
                         shape.color)) {
            ++stats.drawn;
        }
    });
    m_Writer.End();
    stats.dropped = frame.shapes.Dropped();
    stats.revision = frame.shapes.Revision();
    m_Stats = stats;
}

} // namespace Runic::Client
