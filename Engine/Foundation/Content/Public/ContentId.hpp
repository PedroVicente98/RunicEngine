#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "Result.hpp"

// Content IDs (CNT-1; RunicDocs › Repositories, build targets and processes: "Namespaced ContentId
// (engine:, game:)"). Everything data names — an item, an ability, an asset, a component, a system
// — has one: the namespace says who defined it (the Engine, or the game built on it), the name what
// it is. Text in files and code, a 64-bit hash for tables. The Engine's own IDs are engine:..., the
// game's game:...; the public repositories never hold a game: ID except in tests' neutral
// fixtures.

namespace Runic::Content {

enum class ContentNamespace : std::uint8_t {
    Engine, // engine:
    Game,   // game:
};

inline constexpr std::string_view kEngineNamespace = "engine";
inline constexpr std::string_view kGameNamespace = "game";
inline constexpr char kNamespaceSeparator = ':';
// Between the parts of a name: game:rocks/granite_01.
inline constexpr char kNamePartSeparator = '/';
// The longest ID, namespace and separator included.
inline constexpr std::size_t kMaxContentIdLength = 128;

std::string_view NamespaceText(ContentNamespace ns) noexcept;

// "namespace:name": the namespace engine or game; the name one or more parts separated by '/', each
// part of lowercase ASCII letters, digits and '_'. Compared and ordered by its text; hashed with
// 64-bit FNV-1a of its text, the same in every process and on every platform. The default ID is
// empty and not valid.
class ContentId {
  public:
    ContentId() = default;

    // The ID text names, or why it is not one (the first fault, with its position).
    static Core::Result<ContentId> Parse(std::string_view text);
    // For IDs written in code, known to be well formed: asserts otherwise.
    static ContentId Of(std::string_view text);

    bool IsValid() const noexcept { return !m_Text.empty(); }
    ContentNamespace Namespace() const noexcept { return m_Namespace; }
    // The part after the namespace: rocks/granite_01.
    std::string_view Name() const noexcept;
    std::string_view Text() const noexcept { return m_Text; }
    std::uint64_t Hash() const noexcept { return m_Hash; }

    friend bool operator==(const ContentId &a, const ContentId &b) noexcept {
        return a.m_Hash == b.m_Hash && a.m_Text == b.m_Text;
    }
    friend std::strong_ordering operator<=>(const ContentId &a, const ContentId &b) noexcept {
        return a.m_Text <=> b.m_Text;
    }

  private:
    std::string m_Text;
    std::uint64_t m_Hash = 0;
    std::uint32_t m_NameStart = 0;
    ContentNamespace m_Namespace = ContentNamespace::Engine;
};

// 64-bit FNV-1a, the hash ContentId uses: Hash of an ID's text equals the ID's Hash().
std::uint64_t HashText(std::string_view text) noexcept;

} // namespace Runic::Content

template <> struct std::hash<Runic::Content::ContentId> {
    std::size_t operator()(const Runic::Content::ContentId &id) const noexcept {
        return static_cast<std::size_t>(id.Hash());
    }
};
