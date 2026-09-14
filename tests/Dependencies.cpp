// Link smoke only: no ECS systems, physics world, or simulation.
#include <flecs.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Memory.h>

int main() {
    JPH::RegisterDefaultAllocator();
    flecs::world world;
    const glm::vec3 unit{1.0F, 0.0F, 0.0F};
    const auto entity = world.entity().set<glm::vec3>(unit);
    const auto *stored = entity.get<glm::vec3>();
    return stored != nullptr && glm::dot(*stored, unit) == 1.0F ? 0 : 1;
}
