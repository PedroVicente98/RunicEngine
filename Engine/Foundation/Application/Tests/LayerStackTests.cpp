#include "LayerStack.hpp"
#include "TestLayers.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace Runic::Application;
using Runic::Application::Testing::Journal;
using Runic::Application::Testing::NullContext;
using Runic::Application::Testing::RecordingLayer;
using Lines = std::vector<std::string>;

namespace {

std::unique_ptr<RecordingLayer> Make(const std::string &name, Journal &journal) {
    return std::make_unique<RecordingLayer>(name, journal);
}

AppEvent Key() { return AppEvent{KeyEvent{65, 0, true, false}}; }
AppEvent Click() { return AppEvent{MouseButtonEvent{MouseButton::Left, true, 10.0f, 20.0f, 0}}; }

} // namespace

TEST(LayerStack, NothingChangesBeforeTheSafePoint) {
    Journal journal;
    NullContext context;
    LayerStack stack;
    stack.PushLayer(Make("A", journal));
    stack.PushLayer(Make("B", journal));
    EXPECT_EQ(stack.LayerCount(), 0u);
    EXPECT_TRUE(stack.HasPendingChanges());
    EXPECT_TRUE(journal.lines.empty());

    ASSERT_TRUE(stack.ApplyPendingChanges(context));
    EXPECT_FALSE(stack.HasPendingChanges());
    EXPECT_EQ(stack.Names(), (Lines{"A", "B"}));
    EXPECT_EQ(journal.lines, (Lines{"A attach", "B attach"}));
    stack.DetachAll();
}

TEST(LayerStack, UpdatesAndRendersBottomUpWithOverlaysAboveEveryLayer) {
    Journal journal;
    NullContext context;
    LayerStack stack;
    stack.PushOverlay(Make("Overlay", journal)); // pushed first, still above every layer
    stack.PushLayer(Make("A", journal));
    stack.PushLayer(Make("B", journal));
    ASSERT_TRUE(stack.ApplyPendingChanges(context));
    EXPECT_EQ(stack.Names(), (Lines{"A", "B", "Overlay"}));

    journal.Clear();
    stack.UpdateBottomUp(FrameTime{});
    stack.RenderBottomUp();
    EXPECT_EQ(journal.lines, (Lines{"A update", "B update", "Overlay update", "A render",
                                    "B render", "Overlay render"}));
    stack.DetachAll();
}

TEST(LayerStack, EventsGoTopDownUntilHandled) {
    Journal journal;
    NullContext context;
    LayerStack stack;
    auto b = Make("B", journal);
    b->handlesKeys = true;
    stack.PushLayer(Make("A", journal));
    stack.PushLayer(std::move(b));
    stack.PushLayer(Make("C", journal));
    stack.PushOverlay(Make("Overlay", journal));
    ASSERT_TRUE(stack.ApplyPendingChanges(context));

    journal.Clear();
    AppEvent key = Key();
    stack.DispatchEvent(key);
    EXPECT_TRUE(key.handled);
    EXPECT_EQ(journal.lines,
              (Lines{"Overlay event Key", "C event Key", "B event Key"})); // A never sees it

    journal.Clear();
    AppEvent click = Click();
    stack.DispatchEvent(click);
    EXPECT_FALSE(click.handled);
    EXPECT_EQ(journal.lines, (Lines{"Overlay event MouseButton", "C event MouseButton",
                                    "B event MouseButton", "A event MouseButton"}));
    stack.DetachAll();
}

TEST(LayerStack, PopAndReplaceWaitForTheSafePoint) {
    Journal journal;
    NullContext context;
    LayerStack stack;
    auto a = Make("A", journal);
    auto b = Make("B", journal);
    auto c = Make("C", journal);
    const Layer &bRef = *b;
    const Layer &cRef = *c;
    stack.PushLayer(std::move(a));
    stack.PushLayer(std::move(b));
    stack.PushLayer(std::move(c));
    ASSERT_TRUE(stack.ApplyPendingChanges(context));

    journal.Clear();
    stack.PopLayer(bRef);
    stack.ReplaceLayer(cRef, Make("D", journal));
    stack.UpdateBottomUp(FrameTime{}); // the frame still runs on the old stack
    EXPECT_EQ(journal.lines, (Lines{"A update", "B update", "C update"}));

    journal.Clear();
    ASSERT_TRUE(stack.ApplyPendingChanges(context));
    EXPECT_EQ(journal.lines,
              (Lines{"B detach", "C detach", "D attach"})); // a replaced layer detaches first
    EXPECT_EQ(stack.Names(), (Lines{"A", "D"}));
    stack.DetachAll();
}

TEST(LayerStack, ChangesAskedWhileApplyingWaitForTheNextSafePoint) {
    Journal journal;
    NullContext context;
    LayerStack stack;
    auto a = Make("A", journal);
    a->onAttach = [&stack, &journal](LayerContext &) { stack.PushLayer(Make("Late", journal)); };
    stack.PushLayer(std::move(a));
    ASSERT_TRUE(stack.ApplyPendingChanges(context));
    EXPECT_EQ(stack.Names(), (Lines{"A"}));
    EXPECT_TRUE(stack.HasPendingChanges());
    ASSERT_TRUE(stack.ApplyPendingChanges(context));
    EXPECT_EQ(stack.Names(), (Lines{"A", "Late"}));
    stack.DetachAll();
}

TEST(LayerStack, AFailedAttachIsReportedAndTheOtherChangesStillApply) {
    Journal journal;
    NullContext context;
    LayerStack stack;
    auto a = Make("A", journal);
    a->refuseAttach = true;
    stack.PushLayer(std::move(a));
    stack.PushLayer(Make("B", journal));
    const Runic::Core::Result<void> result = stack.ApplyPendingChanges(context);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().message, "A failed to attach: refused");
    EXPECT_EQ(stack.Names(), (Lines{"B"}));
    stack.DetachAll();
}

TEST(LayerStack, DetachAllGoesTopDown) {
    Journal journal;
    NullContext context;
    LayerStack stack;
    stack.PushLayer(Make("A", journal));
    stack.PushLayer(Make("B", journal));
    stack.PushOverlay(Make("Overlay", journal));
    ASSERT_TRUE(stack.ApplyPendingChanges(context));
    journal.Clear();
    stack.DetachAll();
    EXPECT_EQ(journal.lines, (Lines{"Overlay detach", "B detach", "A detach"}));
    EXPECT_EQ(stack.LayerCount() + stack.OverlayCount(), 0u);
}

TEST(AppEvents, ArePlainValuesWithNames) {
    static_assert(std::is_trivially_copyable_v<AppEvent>,
                  "an event is copied into the queue without allocation");
    EXPECT_STREQ(AppEventName(Key()), "Key");
    EXPECT_STREQ(AppEventName(Click()), "MouseButton");
    EXPECT_STREQ(AppEventName(AppEvent{QuitRequestEvent{}}), "QuitRequest");
    EXPECT_STREQ(AppEventName(AppEvent{WindowResizeEvent{1280, 720}}), "WindowResize");
    EXPECT_EQ(Click().As<MouseButtonEvent>()->x, 10.0f);
    EXPECT_EQ(Key().As<MouseButtonEvent>(), nullptr);
}
