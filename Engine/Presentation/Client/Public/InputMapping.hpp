#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "AppEvent.hpp"
#include "Math.hpp"
#include "PlainIds.hpp"
#include "Result.hpp"
#include "WorldSpace.hpp"

// Input mapping (CLI-2): the keys and mouse buttons the GodotLayer forwards as AppEvents (only the
// input Godot's UI did not consume) become actions the Game names, and command builders turn
// actions into commands. The Engine names no action and binds no key: actions, bindings and
// presets are the Game's data.
//
// A binding holds while every key of its chord is down and at least its modifiers are held. When
// two bindings hold at once and one is more specific (its keys and its modifiers include the
// other's), only the more specific one counts: Shift+1 is its own action and not also 1's, and the
// chord Q+E is not also Q's or E's, while W still walks with Shift held when nothing binds
// Shift+W. An action is held while one of its bindings counts; its presses and releases since the
// last frame are counted, so a tap shorter than a frame is not lost.

namespace Runic::Client {

struct ActionTag;
// An action of an InputMap: what a binding does ("game:jump"). 0 is none.
using ActionId = Core::Id<ActionTag, std::uint32_t>;

// What a binding presses: a key (the window layer's key code, KeyEvent::key) or a mouse button.
enum class InputDevice : std::uint8_t { Key, MouseButton };

struct InputKey {
    InputDevice device = InputDevice::Key;
    std::uint32_t code = Application::kNoKey;

    friend constexpr bool operator==(InputKey, InputKey) noexcept = default;
};

constexpr InputKey KeyInput(std::uint32_t key) noexcept { return {InputDevice::Key, key}; }
constexpr InputKey MouseInput(Application::MouseButton button) noexcept {
    return {InputDevice::MouseButton, static_cast<std::uint32_t>(button)};
}

// The most keys one chord holds.
inline constexpr std::size_t kMaxChordKeys = 4;

struct InputBinding {
    ActionId action;
    std::array<InputKey, kMaxChordKeys> keys{};
    std::uint8_t keyCount = 0;
    std::uint32_t modifiers = Application::kNoModifiers; // Application::kModifierShift and others
};

// One key, with the modifiers it needs.
InputBinding Bind(ActionId action, InputKey key,
                  std::uint32_t modifiers = Application::kNoModifiers) noexcept;
// Keys held together (two to kMaxChordKeys), with the modifiers they need.
InputBinding Chord(ActionId action, std::span<const InputKey> keys,
                   std::uint32_t modifiers = Application::kNoModifiers) noexcept;

// A named set of bindings the Game defines ("game:default", "game:left_handed"); the player picks
// one.
struct InputPreset {
    std::string name;
    std::vector<InputBinding> bindings;
};

class InputMap {
  public:
    // ---- Actions and presets (setup) ---------------------------------------------------------
    // Adds an action under a unique "namespace:name".
    Core::Result<ActionId> AddAction(std::string_view name);
    ActionId FindAction(std::string_view name) const noexcept;
    std::string_view ActionName(ActionId action) const;
    std::uint32_t ActionCount() const noexcept;

    // Adds a preset under a unique "namespace:name". Fails when a binding names no action of this
    // map, has no key or more than kMaxChordKeys, or holds the same key twice.
    Core::Result<void> AddPreset(InputPreset preset);
    // Makes a preset the one that maps input, releasing every action first.
    Core::Result<void> UsePreset(std::string_view name);
    // The active preset's name, empty before UsePreset.
    std::string_view ActivePreset() const noexcept;
    std::span<const InputBinding> Bindings() const noexcept;

    // ---- Input -------------------------------------------------------------------------------
    // Takes a key or mouse-button event (anything else is ignored). A repeat is not a press.
    void Feed(const Application::AppEvent &event);
    // Lets go of every key and releases every held action (focus lost, a menu took over).
    void ReleaseAll();

    // ---- What the actions did ----------------------------------------------------------------
    bool IsHeld(ActionId action) const noexcept;
    // Presses and releases since the last EndFrame.
    std::uint32_t Presses(ActionId action) const noexcept;
    std::uint32_t Releases(ActionId action) const noexcept;
    bool WasPressed(ActionId action) const noexcept { return Presses(action) > 0; }
    bool WasReleased(ActionId action) const noexcept { return Releases(action) > 0; }
    // Forgets this frame's presses and releases; held actions stay held.
    void EndFrame() noexcept;

  private:
    struct ActionState {
        std::string name;
        bool held = false;
        std::uint32_t presses = 0;
        std::uint32_t releases = 0;
    };

    const InputPreset *Active() const noexcept;
    // Works out which bindings count from the keys down and the modifiers held, and updates the
    // actions.
    void Remap();

    std::vector<ActionState> m_Actions; // ActionId n is m_Actions[n - 1]
    std::vector<InputPreset> m_Presets;
    std::size_t m_Active = 0;     // m_Presets[m_Active - 1]; 0 before UsePreset
    std::vector<InputKey> m_Down; // keys down now
    std::uint32_t m_Modifiers = Application::kNoModifiers;
    // Scratch for Remap, kept so mapping an event allocates nothing once warm.
    std::vector<std::uint32_t> m_Holding; // indices of the bindings that hold
    std::vector<bool> m_Wanted;           // per action: held after this event
};

// ---- Movement -------------------------------------------------------------------------------

// The four actions that move on the ground, in the camera's frame.
struct MovementActions {
    ActionId forward;
    ActionId back;
    ActionId left;
    ActionId right;
};

// The direction they ask for: x to the right, y forward; length 1 (diagonals included) or 0.
Core::Vec2 MovementDirection(const InputMap &map, const MovementActions &actions) noexcept;
// That direction on the world's ground plane, turned to where a camera at eye looking at target
// faces: horizontal, length 1 or 0. A camera looking straight down faces world -Z.
Core::Vec3 WorldMovement(Core::Vec2 direction, Core::WorldPosition eye,
                         Core::WorldPosition target) noexcept;

} // namespace Runic::Client
