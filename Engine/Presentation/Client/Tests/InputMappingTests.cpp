// CLI-2: input mapping, actions and the command builder. Key presses become movement commands; a
// modifier plus a key is its own action; input the UI consumed (or a layer above handled) never
// reaches the actions.

#include "InputLayer.hpp"
#include "InputMapping.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "CommandLine.hpp"
#include "CommandQueue.hpp"
#include "LayerStack.hpp"
#include "SimulationRuntime.hpp"

using namespace Runic;
using namespace Runic::Client;
using Application::AppEvent;
using Application::KeyEvent;
using Application::kModifierShift;
using Application::MouseButton;
using Application::MouseButtonEvent;

namespace {

// Godot's key numbering: printable keys are their upper-case code points.
constexpr std::uint32_t kKeyW = 'W';
constexpr std::uint32_t kKeyA = 'A';
constexpr std::uint32_t kKeyS = 'S';
constexpr std::uint32_t kKeyD = 'D';
constexpr std::uint32_t kKeyQ = 'Q';
constexpr std::uint32_t kKeyE = 'E';
constexpr std::uint32_t kKey1 = '1';

AppEvent Press(std::uint32_t key, std::uint32_t modifiers = Application::kNoModifiers) {
    return AppEvent{KeyEvent{key, modifiers, true, false}};
}
AppEvent Release(std::uint32_t key, std::uint32_t modifiers = Application::kNoModifiers) {
    return AppEvent{KeyEvent{key, modifiers, false, false}};
}

// The movement actions on W, A, S and D, in a preset.
void AddWalking(InputMap &map, MovementActions &move) {
    move.forward = map.AddAction("engine:test_forward").Value();
    move.back = map.AddAction("engine:test_back").Value();
    move.left = map.AddAction("engine:test_left").Value();
    move.right = map.AddAction("engine:test_right").Value();
    InputPreset preset{"engine:test_wasd",
                       {Bind(move.forward, KeyInput(kKeyW)), Bind(move.back, KeyInput(kKeyS)),
                        Bind(move.left, KeyInput(kKeyA)), Bind(move.right, KeyInput(kKeyD))}};
    ASSERT_TRUE(map.AddPreset(preset).HasValue());
    ASSERT_TRUE(map.UsePreset("engine:test_wasd").HasValue());
}

// A command the test's world takes: walk this way.
struct WalkCommand {
    Core::Vec3 direction{0.0f};
};

// A LayerContext that only counts what layers ask of it.
class QuietContext final : public Application::LayerContext {
  public:
    const Application::CommandLine &Command() const override { return m_Command; }
    std::uint64_t FramesRun() const override { return 0; }
    void PostEvent(const AppEvent &) override {}
    void RequestStop(int) override {}
    void PushLayer(std::unique_ptr<Application::Layer>) override {}
    void PushOverlay(std::unique_ptr<Application::Layer>) override {}
    void PopLayer(const Application::Layer &) override {}
    void ReplaceLayer(const Application::Layer &, std::unique_ptr<Application::Layer>) override {}

  private:
    Application::CommandLine m_Command;
};

// A layer that stands for Godot's UI above the Input layer: it handles the key it owns.
class KeyTakingLayer final : public Application::Layer {
  public:
    explicit KeyTakingLayer(std::uint32_t key) : Layer("engine:test_ui"), m_Key(key) {}
    void OnEvent(AppEvent &event) override {
        if (const auto *key = event.As<KeyEvent>(); key != nullptr && key->key == m_Key) {
            event.handled = true;
        }
    }

  private:
    std::uint32_t m_Key;
};

} // namespace

TEST(InputMapping, KeyPressesBecomeMovementCommands) {
    // The Input layer's builder turns the movement actions into a command each update; the
    // simulation applies it in its Commands phase.
    Simulation::SimulationRuntime runtime;
    Simulation::CommandQueue<WalkCommand> walks(runtime, "engine:test_walks");
    std::vector<Core::Vec3> applied;
    runtime.AddSystem({"engine:test_apply_walks", Simulation::ScheduleSlot::Hz30Realtime,
                       [&](const Simulation::TickContext &) {
                           walks.Drain(
                               [&](const WalkCommand &walk) { applied.push_back(walk.direction); });
                       },
                       Simulation::RealtimePhase::Commands});

    InputLayer input;
    MovementActions move;
    AddWalking(input.Map(), move);
    // The camera looks along +X from the west: forward is +X, right is +Z.
    const Core::WorldPosition eye{-10.0, 5.0, 0.0};
    const Core::WorldPosition target{0.0, 0.0, 0.0};
    input.AddCommandBuilder(
        "engine:test_walk", [&](const InputMap &map, const Application::FrameTime &) {
            walks.Push({WorldMovement(MovementDirection(map, move), eye, target)});
        });

    AppEvent forward = Press(kKeyW);
    input.OnEvent(forward);
    input.OnUpdate({});
    AppEvent right = Press(kKeyD);
    input.OnEvent(right);
    input.OnUpdate({});
    AppEvent stop = Release(kKeyW);
    input.OnEvent(stop);
    AppEvent stopToo = Release(kKeyD);
    input.OnEvent(stopToo);
    input.OnUpdate({});
    runtime.RunMasterTick(); // the realtime slot runs on master tick 0

    ASSERT_EQ(applied.size(), 3u);
    EXPECT_NEAR(applied[0].x, 1.0f, 1e-5f); // W: forward, +X
    EXPECT_NEAR(applied[0].z, 0.0f, 1e-5f);
    const float diagonal = 1.0f / std::sqrt(2.0f);
    EXPECT_NEAR(applied[1].x, diagonal, 1e-5f); // W and D: forward and right, length 1
    EXPECT_NEAR(applied[1].z, diagonal, 1e-5f);
    EXPECT_NEAR(applied[1].y, 0.0f, 1e-6f);
    EXPECT_EQ(applied[2], Core::Vec3(0.0f)); // released: stand
}

TEST(InputMapping, AModifierPlusAKeyMapsToItsOwnAction) {
    InputMap map;
    MovementActions move;
    AddWalking(map, move);
    const ActionId first = map.AddAction("engine:test_skill_1").Value();
    const ActionId sixth = map.AddAction("engine:test_skill_6").Value();
    InputPreset skills{"engine:test_skills",
                       {Bind(first, KeyInput(kKey1)), Bind(sixth, KeyInput(kKey1), kModifierShift),
                        Bind(move.forward, KeyInput(kKeyW))}};
    ASSERT_TRUE(map.AddPreset(skills).HasValue());
    ASSERT_TRUE(map.UsePreset("engine:test_skills").HasValue());

    map.Feed(Press(kKey1, kModifierShift));
    EXPECT_TRUE(map.WasPressed(sixth));
    EXPECT_FALSE(map.WasPressed(first)); // the more specific binding wins
    map.Feed(Release(kKey1, kModifierShift));
    map.EndFrame();

    map.Feed(Press(kKey1));
    EXPECT_TRUE(map.WasPressed(first));
    EXPECT_FALSE(map.WasPressed(sixth));
    map.Feed(Release(kKey1));
    map.EndFrame();

    // Nothing binds Shift+W: W walks with Shift held.
    map.Feed(Press(kKeyW, kModifierShift));
    EXPECT_TRUE(map.IsHeld(move.forward));
}

TEST(InputMapping, AChordIsItsOwnActionAndNotAlsoItsKeys) {
    InputMap map;
    const ActionId q = map.AddAction("engine:test_q").Value();
    const ActionId e = map.AddAction("engine:test_e").Value();
    const ActionId both = map.AddAction("engine:test_q_and_e").Value();
    const std::array<InputKey, 2> chord{KeyInput(kKeyQ), KeyInput(kKeyE)};
    ASSERT_TRUE(
        map.AddPreset({"engine:test_chords",
                       {Bind(q, KeyInput(kKeyQ)), Bind(e, KeyInput(kKeyE)), Chord(both, chord)}})
            .HasValue());
    ASSERT_TRUE(map.UsePreset("engine:test_chords").HasValue());

    map.Feed(Press(kKeyQ));
    EXPECT_TRUE(map.IsHeld(q));
    map.Feed(Press(kKeyE)); // the chord completes: it takes over from Q
    EXPECT_TRUE(map.IsHeld(both));
    EXPECT_FALSE(map.IsHeld(q));
    EXPECT_TRUE(map.WasReleased(q));
    EXPECT_FALSE(map.WasPressed(e));
    map.Feed(Release(kKeyQ)); // E alone again
    EXPECT_FALSE(map.IsHeld(both));
    EXPECT_TRUE(map.IsHeld(e));
}

TEST(InputMapping, ATapShorterThanAFrameIsNotLostAndRepeatsAreNotPresses) {
    InputMap map;
    MovementActions move;
    AddWalking(map, move);
    map.Feed(Press(kKeyW));
    map.Feed(Release(kKeyW));
    EXPECT_FALSE(map.IsHeld(move.forward));
    EXPECT_EQ(map.Presses(move.forward), 1u);
    EXPECT_EQ(map.Releases(move.forward), 1u);
    map.EndFrame();
    EXPECT_EQ(map.Presses(move.forward), 0u);

    map.Feed(Press(kKeyW));
    map.Feed(AppEvent{KeyEvent{kKeyW, Application::kNoModifiers, true, true}}); // a repeat
    map.Feed(AppEvent{KeyEvent{kKeyW, Application::kNoModifiers, true, true}});
    EXPECT_EQ(map.Presses(move.forward), 1u);
    EXPECT_TRUE(map.IsHeld(move.forward));
}

TEST(InputMapping, MouseButtonsBindLikeKeys) {
    InputMap map;
    const ActionId attack = map.AddAction("engine:test_attack").Value();
    const ActionId heavy = map.AddAction("engine:test_heavy_attack").Value();
    ASSERT_TRUE(map.AddPreset({"engine:test_mouse",
                               {Bind(attack, MouseInput(MouseButton::Left)),
                                Bind(heavy, MouseInput(MouseButton::Left), kModifierShift)}})
                    .HasValue());
    ASSERT_TRUE(map.UsePreset("engine:test_mouse").HasValue());
    map.Feed(AppEvent{MouseButtonEvent{MouseButton::Left, true, 10.0f, 10.0f, kModifierShift}});
    EXPECT_TRUE(map.IsHeld(heavy));
    EXPECT_FALSE(map.IsHeld(attack));
    map.Feed(AppEvent{MouseButtonEvent{MouseButton::Left, false, 10.0f, 10.0f, kModifierShift}});
    EXPECT_FALSE(map.IsHeld(heavy));
}

TEST(InputMapping, InputALayerAboveHandledNeverReachesTheActions) {
    // In the client the GodotLayer posts only what Godot's UI did not consume; a layer above the
    // Input layer that handles an event stops it the same way.
    Application::LayerStack stack;
    auto owned = std::make_unique<InputLayer>();
    InputLayer &input = *owned;
    MovementActions move;
    AddWalking(input.Map(), move);
    stack.PushLayer(std::move(owned));
    stack.PushLayer(std::make_unique<KeyTakingLayer>(kKeyS)); // the "UI" takes S
    QuietContext context;
    ASSERT_TRUE(stack.ApplyPendingChanges(context).HasValue());

    AppEvent taken = Press(kKeyS);
    stack.DispatchEvent(taken);
    AppEvent reaching = Press(kKeyW);
    stack.DispatchEvent(reaching);
    EXPECT_FALSE(input.Map().IsHeld(move.back));
    EXPECT_TRUE(input.Map().IsHeld(move.forward));
    stack.DetachAll();
}

TEST(InputMapping, LosingFocusReleasesEveryAction) {
    InputLayer input;
    MovementActions move;
    AddWalking(input.Map(), move);
    AppEvent forward = Press(kKeyW);
    input.OnEvent(forward);
    input.OnUpdate({});
    AppEvent unfocused{Application::WindowFocusEvent{false}};
    input.OnEvent(unfocused);
    EXPECT_FALSE(input.Map().IsHeld(move.forward));
    EXPECT_TRUE(input.Map().WasReleased(move.forward));
    // The release Godot gave its UI never comes; pressing W again is a new press.
    AppEvent again = Press(kKeyW);
    input.OnEvent(again);
    EXPECT_TRUE(input.Map().WasPressed(move.forward));
}

TEST(InputMapping, PresetsAreTheGamesAndAreChecked) {
    InputMap map;
    MovementActions move;
    AddWalking(map, move);
    EXPECT_FALSE(map.AddAction("engine:test_forward").HasValue()); // taken
    EXPECT_FALSE(map.AddAction("forward").HasValue());             // no namespace
    EXPECT_EQ(map.FindAction("engine:test_back"), move.back);
    EXPECT_EQ(map.ActionName(move.left), "engine:test_left");

    EXPECT_FALSE(map.AddPreset({"engine:test_wasd", {}}).HasValue()); // taken
    EXPECT_FALSE(map.AddPreset({"engine:test_ghost", {Bind(ActionId(99), KeyInput(kKeyW))}})
                     .HasValue()); // no such action
    InputBinding empty;
    empty.action = move.forward;
    EXPECT_FALSE(map.AddPreset({"engine:test_empty", {empty}}).HasValue()); // no key
    const std::array<InputKey, 2> twice{KeyInput(kKeyW), KeyInput(kKeyW)};
    EXPECT_FALSE(map.AddPreset({"engine:test_twice", {Chord(move.forward, twice)}}).HasValue());
    EXPECT_FALSE(map.UsePreset("engine:test_missing").HasValue());

    // Another preset: the arrows of a left-handed player (any key codes the Game picks).
    constexpr std::uint32_t kKeyI = 'I';
    ASSERT_TRUE(map.AddPreset({"engine:test_left_handed", {Bind(move.forward, KeyInput(kKeyI))}})
                    .HasValue());
    map.Feed(Press(kKeyW));
    ASSERT_TRUE(map.IsHeld(move.forward));
    ASSERT_TRUE(map.UsePreset("engine:test_left_handed").HasValue());
    EXPECT_FALSE(map.IsHeld(move.forward)); // switching releases everything
    EXPECT_EQ(map.ActivePreset(), "engine:test_left_handed");
    map.Feed(Press(kKeyI));
    EXPECT_TRUE(map.IsHeld(move.forward));
}

TEST(InputMapping, MovementFollowsTheCamerasHeading) {
    InputMap map;
    MovementActions move;
    AddWalking(map, move);
    map.Feed(Press(kKeyW));
    map.Feed(Press(kKeyA));
    const Core::Vec2 asked = MovementDirection(map, move);
    EXPECT_NEAR(asked.x, -1.0f / std::sqrt(2.0f), 1e-6f); // left
    EXPECT_NEAR(asked.y, 1.0f / std::sqrt(2.0f), 1e-6f);  // and forward
    map.Feed(Press(kKeyS));                               // forward and back cancel
    EXPECT_EQ(MovementDirection(map, move), Core::Vec2(-1.0f, 0.0f));

    // A camera far out looking along -Z: forward is -Z, right is +X.
    const Core::WorldPosition eye{5'000'000.0, 10.0, -7'000'000.0};
    const Core::WorldPosition target{5'000'000.0, 0.0, -7'000'010.0};
    const Core::Vec3 forward = WorldMovement(Core::Vec2(0.0f, 1.0f), eye, target);
    EXPECT_NEAR(forward.z, -1.0f, 1e-6f);
    const Core::Vec3 right = WorldMovement(Core::Vec2(1.0f, 0.0f), eye, target);
    EXPECT_NEAR(right.x, 1.0f, 1e-6f);
    EXPECT_EQ(WorldMovement(Core::Vec2(0.0f), eye, target), Core::Vec3(0.0f));
}
