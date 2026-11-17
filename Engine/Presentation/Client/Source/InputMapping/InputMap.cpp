// Input mapping (InputMapping.hpp): bindings that hold, the more specific of them winning, and the
// actions they hold.

#include "InputMapping.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <format>
#include <utility>

#include "Assert.hpp"

namespace Runic::Client {

namespace {

constexpr char kNamespaceSeparator = ':';
constexpr float kNoMovement = 0.0f;
constexpr float kFullMovement = 1.0f;
// A camera looking straight down has no heading: it faces world -Z.
constexpr float kStraightDownFacingZ = -1.0f;
constexpr std::size_t kFirstKey = 0;
constexpr std::uint8_t kOneKey = 1;

bool Contains(const InputBinding &binding, InputKey key) noexcept {
    const auto end = binding.keys.begin() + binding.keyCount;
    return std::find(binding.keys.begin(), end, key) != end;
}

// Whether `wide` holds every key and modifier of `narrow`, and more: the more specific binding.
bool MoreSpecific(const InputBinding &wide, const InputBinding &narrow) noexcept {
    if ((wide.modifiers & narrow.modifiers) != narrow.modifiers) {
        return false;
    }
    for (std::uint8_t i = 0; i < narrow.keyCount; ++i) {
        if (!Contains(wide, narrow.keys[i])) {
            return false;
        }
    }
    const int wider = wide.keyCount + std::popcount(wide.modifiers);
    const int narrower = narrow.keyCount + std::popcount(narrow.modifiers);
    return wider > narrower;
}

// The key an event presses or lets go, if it is a key or a mouse button.
bool KeyOf(const Application::AppEvent &event, InputKey &key, bool &pressed, bool &repeat,
           std::uint32_t &modifiers) noexcept {
    if (const auto *keyEvent = event.As<Application::KeyEvent>()) {
        key = KeyInput(keyEvent->key);
        pressed = keyEvent->pressed;
        repeat = keyEvent->repeat;
        modifiers = keyEvent->modifiers;
        return keyEvent->key != Application::kNoKey;
    }
    if (const auto *button = event.As<Application::MouseButtonEvent>()) {
        key = MouseInput(button->button);
        pressed = button->pressed;
        repeat = false;
        modifiers = button->modifiers;
        return true;
    }
    return false;
}

} // namespace

InputBinding Bind(ActionId action, InputKey key, std::uint32_t modifiers) noexcept {
    InputBinding binding;
    binding.action = action;
    binding.keys[kFirstKey] = key;
    binding.keyCount = kOneKey;
    binding.modifiers = modifiers;
    return binding;
}

InputBinding Chord(ActionId action, std::span<const InputKey> keys,
                   std::uint32_t modifiers) noexcept {
    InputBinding binding;
    binding.action = action;
    binding.keyCount = static_cast<std::uint8_t>(std::min(keys.size(), kMaxChordKeys));
    std::copy_n(keys.begin(), binding.keyCount, binding.keys.begin());
    binding.modifiers = modifiers;
    return binding;
}

Core::Result<ActionId> InputMap::AddAction(std::string_view name) {
    if (name.find(kNamespaceSeparator) == std::string_view::npos) {
        return Core::Fail(std::format("the action name '{}' is not namespace:name", name));
    }
    if (FindAction(name).IsValid()) {
        return Core::Fail(std::format("an action is already called '{}'", name));
    }
    m_Actions.push_back({std::string(name)});
    m_Wanted.resize(m_Actions.size());
    return ActionId(static_cast<std::uint32_t>(m_Actions.size()));
}

ActionId InputMap::FindAction(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < m_Actions.size(); ++i) {
        if (m_Actions[i].name == name) {
            return ActionId(static_cast<std::uint32_t>(i + 1));
        }
    }
    return ActionId();
}

std::string_view InputMap::ActionName(ActionId action) const {
    RUNIC_ASSERT_MSG(action.IsValid() && action.Value() <= m_Actions.size(),
                     "the action is not one of this map's");
    return m_Actions[action.Value() - 1].name;
}

std::uint32_t InputMap::ActionCount() const noexcept {
    return static_cast<std::uint32_t>(m_Actions.size());
}

Core::Result<void> InputMap::AddPreset(InputPreset preset) {
    if (preset.name.find(kNamespaceSeparator) == std::string::npos) {
        return Core::Fail(std::format("the preset name '{}' is not namespace:name", preset.name));
    }
    for (const InputPreset &existing : m_Presets) {
        if (existing.name == preset.name) {
            return Core::Fail(std::format("a preset is already called '{}'", preset.name));
        }
    }
    for (std::size_t i = 0; i < preset.bindings.size(); ++i) {
        const InputBinding &binding = preset.bindings[i];
        if (!binding.action.IsValid() || binding.action.Value() > m_Actions.size()) {
            return Core::Fail(
                std::format("preset '{}': binding {} names no action of this map", preset.name, i));
        }
        if (binding.keyCount == 0 || binding.keyCount > kMaxChordKeys) {
            return Core::Fail(std::format("preset '{}': binding {} needs 1 to {} keys", preset.name,
                                          i, kMaxChordKeys));
        }
        for (std::uint8_t a = 0; a < binding.keyCount; ++a) {
            for (std::uint8_t b = a + 1; b < binding.keyCount; ++b) {
                if (binding.keys[a] == binding.keys[b]) {
                    return Core::Fail(
                        std::format("preset '{}': binding {} holds a key twice", preset.name, i));
                }
            }
        }
    }
    m_Holding.reserve(std::max(m_Holding.capacity(), preset.bindings.size()));
    m_Presets.push_back(std::move(preset));
    return {};
}

Core::Result<void> InputMap::UsePreset(std::string_view name) {
    for (std::size_t i = 0; i < m_Presets.size(); ++i) {
        if (m_Presets[i].name == name) {
            ReleaseAll();
            m_Active = i + 1;
            return {};
        }
    }
    return Core::Fail(std::format("there is no preset called '{}'", name));
}

std::string_view InputMap::ActivePreset() const noexcept {
    const InputPreset *active = Active();
    return active == nullptr ? std::string_view() : std::string_view(active->name);
}

std::span<const InputBinding> InputMap::Bindings() const noexcept {
    const InputPreset *active = Active();
    return active == nullptr ? std::span<const InputBinding>() : active->bindings;
}

const InputPreset *InputMap::Active() const noexcept {
    return m_Active == 0 ? nullptr : &m_Presets[m_Active - 1];
}

void InputMap::Feed(const Application::AppEvent &event) {
    InputKey key;
    bool pressed = false;
    bool repeat = false;
    std::uint32_t modifiers = Application::kNoModifiers;
    if (!KeyOf(event, key, pressed, repeat, modifiers) || repeat) {
        return;
    }
    m_Modifiers = modifiers;
    const auto down = std::find(m_Down.begin(), m_Down.end(), key);
    if (pressed && down == m_Down.end()) {
        m_Down.push_back(key);
    } else if (!pressed && down != m_Down.end()) {
        m_Down.erase(down);
    }
    Remap();
}

void InputMap::ReleaseAll() {
    m_Down.clear();
    m_Modifiers = Application::kNoModifiers;
    Remap();
}

void InputMap::Remap() {
    // The bindings that hold: every key down, and at least their modifiers.
    m_Holding.clear();
    const std::span<const InputBinding> bindings = Bindings();
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        const InputBinding &binding = bindings[i];
        if ((m_Modifiers & binding.modifiers) != binding.modifiers) {
            continue;
        }
        bool allDown = true;
        for (std::uint8_t k = 0; k < binding.keyCount && allDown; ++k) {
            allDown = std::find(m_Down.begin(), m_Down.end(), binding.keys[k]) != m_Down.end();
        }
        if (allDown) {
            m_Holding.push_back(static_cast<std::uint32_t>(i));
        }
    }
    // Of those, the ones no more specific holding binding covers hold their actions.
    std::fill(m_Wanted.begin(), m_Wanted.end(), false);
    for (const std::uint32_t candidate : m_Holding) {
        const InputBinding &binding = bindings[candidate];
        const bool covered = std::ranges::any_of(m_Holding, [&](std::uint32_t other) {
            return other != candidate && MoreSpecific(bindings[other], binding);
        });
        if (!covered) {
            m_Wanted[binding.action.Value() - 1] = true;
        }
    }
    for (std::size_t i = 0; i < m_Actions.size(); ++i) {
        ActionState &action = m_Actions[i];
        if (m_Wanted[i] && !action.held) {
            ++action.presses;
        } else if (!m_Wanted[i] && action.held) {
            ++action.releases;
        }
        action.held = m_Wanted[i];
    }
}

bool InputMap::IsHeld(ActionId action) const noexcept {
    return action.IsValid() && action.Value() <= m_Actions.size() &&
           m_Actions[action.Value() - 1].held;
}

std::uint32_t InputMap::Presses(ActionId action) const noexcept {
    return action.IsValid() && action.Value() <= m_Actions.size()
               ? m_Actions[action.Value() - 1].presses
               : 0;
}

std::uint32_t InputMap::Releases(ActionId action) const noexcept {
    return action.IsValid() && action.Value() <= m_Actions.size()
               ? m_Actions[action.Value() - 1].releases
               : 0;
}

void InputMap::EndFrame() noexcept {
    for (ActionState &action : m_Actions) {
        action.presses = 0;
        action.releases = 0;
    }
}

Core::Vec2 MovementDirection(const InputMap &map, const MovementActions &actions) noexcept {
    const auto axis = [&map](ActionId positive, ActionId negative) {
        return (map.IsHeld(positive) ? kFullMovement : kNoMovement) -
               (map.IsHeld(negative) ? kFullMovement : kNoMovement);
    };
    const Core::Vec2 direction(axis(actions.right, actions.left),
                               axis(actions.forward, actions.back));
    const float length = glm::length(direction);
    return length > kNoMovement ? direction / length : Core::Vec2(kNoMovement);
}

Core::Vec3 WorldMovement(Core::Vec2 direction, Core::WorldPosition eye,
                         Core::WorldPosition target) noexcept {
    if (direction == Core::Vec2(kNoMovement)) {
        return Core::Vec3(kNoMovement);
    }
    // The camera's heading on the ground plane; one looking straight down faces -Z.
    Core::Vec3 forward(static_cast<float>(target.x - eye.x), kNoMovement,
                       static_cast<float>(target.z - eye.z));
    const float reach = glm::length(forward);
    forward = reach > kNoMovement ? forward / reach
                                  : Core::Vec3(kNoMovement, kNoMovement, kStraightDownFacingZ);
    // Right of forward with +Y up.
    const Core::Vec3 right(-forward.z, kNoMovement, forward.x);
    const Core::Vec3 moving = direction.x * right + direction.y * forward;
    const float length = glm::length(moving);
    return length > kNoMovement ? moving / length : Core::Vec3(kNoMovement);
}

} // namespace Runic::Client
