// The components that place things in the world (WorldComponents.hpp).

#include "WorldComponents.hpp"

#include <format>
#include <string>

namespace Runic::World {

namespace {

constexpr std::string_view kWorldTransformDescription =
    "Where an entity is (64-bit metres, real Y) and how it is turned";
constexpr std::string_view kRegionDescription =
    "An authored content area: its world and its 64-bit origin";

void FormatTransform(const void *component, std::string &out) {
    const auto &transform = *static_cast<const WorldTransform *>(component);
    out = std::format("({:.2f}, {:.2f}, {:.2f}) m", transform.position.x, transform.position.y,
                      transform.position.z);
}

void FormatRegion(const void *component, std::string &out) {
    const auto &region = *static_cast<const Region *>(component);
    out = std::format("world {}, origin ({:.2f}, {:.2f}, {:.2f}) m", region.world.Value(),
                      region.frame.origin.x, region.frame.origin.y, region.frame.origin.z);
}

} // namespace

void RegisterWorldComponents(ECS::EntityWorld &world) {
    ECS::ComponentOptions transform;
    transform.description = kWorldTransformDescription;
    transform.format = &FormatTransform;
    world.RegisterComponent<WorldTransform>(kWorldTransformName, transform);

    ECS::ComponentOptions region;
    region.description = kRegionDescription;
    region.format = &FormatRegion;
    world.RegisterComponent<Region>(kRegionName, region);
}

} // namespace Runic::World
