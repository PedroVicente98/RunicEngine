// Link smoke only: no ECS systems, physics world, or simulation.
#include <flecs.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Memory.h>

int main() {
    JPH::RegisterDefaultAllocator();
    const auto *build = ecs_get_build_info();
    const glm::vec3 unit{1.0F, 0.0F, 0.0F};
    return build != nullptr && glm::dot(unit, unit) == 1.0F ? 0 : 1;
}
