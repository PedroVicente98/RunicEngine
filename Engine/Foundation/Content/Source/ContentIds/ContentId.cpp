// Content IDs (ContentId.hpp).

#include "ContentId.hpp"

#include <format>

#include "Assert.hpp"

namespace Runic::Content {

namespace {

// 64-bit FNV-1a.
constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ull;

constexpr std::string_view kNotAnIdFormat = "'{}' is not a content ID: {}";

bool IsNameCharacter(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

Core::Error NotAnId(std::string_view text, std::string_view why) {
    return Core::Fail(std::vformat(kNotAnIdFormat, std::make_format_args(text, why)));
}

} // namespace

std::string_view NamespaceText(ContentNamespace ns) noexcept {
    return ns == ContentNamespace::Engine ? kEngineNamespace : kGameNamespace;
}

std::uint64_t HashText(std::string_view text) noexcept {
    std::uint64_t hash = kFnvOffsetBasis;
    for (const char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= kFnvPrime;
    }
    return hash;
}

Core::Result<ContentId> ContentId::Parse(std::string_view text) {
    if (text.empty()) {
        return NotAnId(text, "it is empty");
    }
    if (text.size() > kMaxContentIdLength) {
        return NotAnId(text, std::format("it is longer than {} characters", kMaxContentIdLength));
    }
    const std::size_t separator = text.find(kNamespaceSeparator);
    if (separator == std::string_view::npos) {
        return NotAnId(text, std::format("it has no namespace ('{}:' or '{}:')", kEngineNamespace,
                                         kGameNamespace));
    }
    const std::string_view space = text.substr(0, separator);
    ContentId id;
    if (space == kEngineNamespace) {
        id.m_Namespace = ContentNamespace::Engine;
    } else if (space == kGameNamespace) {
        id.m_Namespace = ContentNamespace::Game;
    } else {
        return NotAnId(text, std::format("'{}' is not a namespace ('{}' or '{}')", space,
                                         kEngineNamespace, kGameNamespace));
    }
    const std::size_t nameStart = separator + 1;
    if (nameStart == text.size()) {
        return NotAnId(text, "it has no name after the namespace");
    }
    bool partStarted = false;
    for (std::size_t i = nameStart; i < text.size(); ++i) {
        const char c = text[i];
        if (c == kNamePartSeparator) {
            if (!partStarted) {
                return NotAnId(text, std::format("an empty part ends at {}", i));
            }
            partStarted = false;
            continue;
        }
        if (!IsNameCharacter(c)) {
            return NotAnId(text,
                           std::format("'{}' at {} is not a lowercase letter, a digit, '_' or "
                                       "'{}' between parts",
                                       c, i, kNamePartSeparator));
        }
        partStarted = true;
    }
    if (!partStarted) {
        return NotAnId(text, std::format("an empty part ends at {}", text.size()));
    }
    id.m_Text = std::string(text);
    id.m_Hash = HashText(text);
    id.m_NameStart = static_cast<std::uint32_t>(nameStart);
    return id;
}

ContentId ContentId::Of(std::string_view text) {
    auto id = Parse(text);
    RUNIC_ASSERT_MSG(static_cast<bool>(id), "a content ID written in code is malformed");
    return id.Value();
}

std::string_view ContentId::Name() const noexcept {
    return std::string_view(m_Text).substr(m_NameStart);
}

} // namespace Runic::Content
