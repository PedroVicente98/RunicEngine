#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// What a component is to the registry: its name, its layout and how its values are built, copied,
// moved and destroyed. One registration feeds every consumer: the world stores values with it and
// tools list components from it (RunicDocs › GameplayAPI › Registries and tools).

namespace Runic::ECS {

// Writes one component value as text, for tools such as the RunicEditor's ECS inspector.
using ComponentFormatter = void (*)(const void *component, std::string &out);

// What a registration may add to the type itself.
struct ComponentOptions {
    std::string_view description;        // one line for tools
    ComponentFormatter format = nullptr; // how tools show a value; none shows the name only
};

// A registered component as tools see it. The texts stay valid while the world lives.
struct ComponentInfo {
    std::string_view name;
    std::string_view description;
    std::size_t size = 0;
    std::size_t alignment = 0;
    ComponentFormatter format = nullptr;
};

namespace Detail {

// How a world builds, copies, moves and destroys runs of one component type. A null function
// means the bytes are enough (a trivial type).
struct ComponentLifecycle {
    void (*construct)(void *first, std::int32_t count) = nullptr;
    void (*destroy)(void *first, std::int32_t count) = nullptr;
    void (*copy)(void *to, const void *from, std::int32_t count) = nullptr;
    void (*move)(void *to, void *from, std::int32_t count) = nullptr;
};

template <class T> ComponentLifecycle LifecycleOf() noexcept {
    ComponentLifecycle lifecycle;
    if (!std::is_trivially_default_constructible_v<T>) {
        lifecycle.construct = [](void *first, std::int32_t count) {
            std::uninitialized_default_construct_n(static_cast<T *>(first), count);
        };
    }
    if (!std::is_trivially_destructible_v<T>) {
        lifecycle.destroy = [](void *first, std::int32_t count) {
            std::destroy_n(static_cast<T *>(first), count);
        };
    }
    if (!std::is_trivially_copy_assignable_v<T>) {
        lifecycle.copy = [](void *to, const void *from, std::int32_t count) {
            for (std::int32_t index = 0; index < count; ++index) {
                static_cast<T *>(to)[index] = static_cast<const T *>(from)[index];
            }
        };
    }
    if (!std::is_trivially_move_assignable_v<T>) {
        lifecycle.move = [](void *to, void *from, std::int32_t count) {
            for (std::int32_t index = 0; index < count; ++index) {
                static_cast<T *>(to)[index] = std::move(static_cast<T *>(from)[index]);
            }
        };
    }
    return lifecycle;
}

// One number per component type, the same in every world of the process: the index of the type
// in each world's table of registered components.
std::uint32_t NextTypeIndex() noexcept;

template <class T> std::uint32_t TypeIndexOf() noexcept {
    static const std::uint32_t index = NextTypeIndex();
    return index;
}

// What the world needs to register a type.
struct ComponentRegistration {
    std::uint32_t typeIndex = 0;
    std::string_view name;
    ComponentOptions options;
    std::size_t size = 0;
    std::size_t alignment = 0;
    ComponentLifecycle lifecycle;
};

} // namespace Detail

} // namespace Runic::ECS
