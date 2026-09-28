#include <iostream>

// BEGIN disposable dependency includes. Remove these with the check block below.
#include <flecs.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

// Jolt's umbrella header must precede its other headers.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Memory.h>

#include <GLFW/glfw3.h>
#include <bgfx/bgfx.h>
#include <imgui.h>
// END disposable dependency includes.

int main() {
    // BEGIN disposable dependency checks. No window, renderer, or simulation.
    {
        flecs::world world;
        const auto entity = world.entity();
        if (!entity.is_alive()) {
            return 1;
        }

        const glm::vec3 vector{1.0F, 2.0F, 3.0F};
        if (glm::dot(vector, vector) != 14.0F) {
            return 1;
        }

        JPH::RegisterDefaultAllocator();
        void *allocation = JPH::Allocate(16);
        if (allocation == nullptr) {
            return 1;
        }
        JPH::Free(allocation);

        if (glfwGetVersionString() == nullptr || bgfx::getSupportedRenderers(0, nullptr) == 0 ||
            !IMGUI_CHECKVERSION()) {
            return 1;
        }
    }
    // END disposable dependency checks. The rest of main works without them.

    std::cout << "RunicSandbox ready.\n";
    return 0;
}
