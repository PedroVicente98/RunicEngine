#include <cstdio>

// BEGIN disposable dependency includes: the headless third parties of the Runic::Runtime bundle.
#include <flecs.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

// Jolt's umbrella header must precede its other headers.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Memory.h>
// END disposable dependency includes.

int main() {
    // Disposable checks: no window, renderer or simulation. Presentation is Godot (GodotBridge).
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
    void* allocation = JPH::Allocate(16);
    if (allocation == nullptr) {
        return 1;
    }
    JPH::Free(allocation);

#if defined(__AVX2__)
    const char* baseline = "x86-64-v3 (AVX2)";
#else
    const char* baseline = "none";
#endif
    std::printf("RunicSandbox ready (Flecs, GLM, Jolt; CPU baseline %s).\n", baseline);
    return 0;
}
