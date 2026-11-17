// Component registration that needs no world: the type indices and the naming convention.

#include "ComponentInfo.hpp"

#include <atomic>

#include "ComponentNames.hpp"

namespace Runic::ECS::Detail {

namespace {

constexpr char kNamespaceSeparator = ':';
constexpr char kWordSeparator = '_';

bool IsLower(char character) noexcept { return character >= 'a' && character <= 'z'; }
bool IsDigit(char character) noexcept { return character >= '0' && character <= '9'; }

// A lowercase letter, then lowercase letters, digits and underscores.
bool IsWord(std::string_view word) noexcept {
    if (word.empty() || !IsLower(word.front())) {
        return false;
    }
    for (const char character : word) {
        if (!IsLower(character) && !IsDigit(character) && character != kWordSeparator) {
            return false;
        }
    }
    return true;
}

std::atomic<std::uint32_t> g_NextTypeIndex{0};

} // namespace

std::uint32_t NextTypeIndex() noexcept {
    return g_NextTypeIndex.fetch_add(1, std::memory_order_relaxed);
}

bool IsNamespacedName(std::string_view name) noexcept {
    const std::size_t separator = name.find(kNamespaceSeparator);
    return separator != std::string_view::npos && IsWord(name.substr(0, separator)) &&
           IsWord(name.substr(separator + 1));
}

} // namespace Runic::ECS::Detail
