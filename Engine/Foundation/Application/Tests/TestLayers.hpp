#pragma once

// Test layers and a test clock for RunicApplicationTests.

#include "Application.hpp"
#include "CommandLine.hpp"
#include "Layer.hpp"
#include "LoopPolicy.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Runic::Application::Testing {

// The calls the recording layers make, in order: "A attach", "B event", "A update"...
struct Journal {
    std::vector<std::string> lines;

    void Add(const std::string &line) { lines.push_back(line); }
    void Clear() { lines.clear(); }
};

// A layer that writes every call to a journal; it can refuse to attach, handle key events, and run
// a step in OnUpdate.
class RecordingLayer : public Layer {
  public:
    RecordingLayer(std::string name, Journal &journal)
        : Layer(std::move(name)), m_Journal(journal) {}

    bool refuseAttach = false;
    bool handlesKeys = false;
    std::function<void(LayerContext &, const FrameTime &)> onUpdate;
    std::function<void(LayerContext &)> onAttach;

    Core::Result<void> OnAttach(LayerContext &context) override {
        m_Context = &context;
        m_Journal.Add(Name() + " attach");
        if (refuseAttach) {
            return Core::Fail("refused");
        }
        if (onAttach) {
            onAttach(context);
        }
        return {};
    }
    void OnDetach() override { m_Journal.Add(Name() + " detach"); }
    void OnEvent(AppEvent &event) override {
        m_Journal.Add(Name() + " event " + AppEventName(event));
        if (handlesKeys && event.Is<KeyEvent>()) {
            event.handled = true;
        }
    }
    void OnUpdate(const FrameTime &time) override {
        m_Journal.Add(Name() + " update");
        if (onUpdate) {
            onUpdate(*m_Context, time);
        }
    }
    void OnRender() override { m_Journal.Add(Name() + " render"); }

  private:
    Journal &m_Journal;
    LayerContext *m_Context = nullptr;
};

// A layer that only counts, so a measured frame allocates nothing of its own.
class CountingLayer final : public Layer {
  public:
    explicit CountingLayer(std::string name) : Layer(std::move(name)) {}

    std::size_t events = 0;
    std::size_t updates = 0;

    void OnEvent(AppEvent &) override { ++events; }
    void OnUpdate(const FrameTime &) override { ++updates; }
};

// A context for LayerStack tests that run without a host.
class NullContext final : public LayerContext {
  public:
    const CommandLine &Command() const override { return m_Command; }
    std::uint64_t FramesRun() const override { return 0; }
    void PostEvent(const AppEvent &) override {}
    void RequestStop(int) override {}
    void PushLayer(std::unique_ptr<Layer>) override {}
    void PushOverlay(std::unique_ptr<Layer>) override {}
    void PopLayer(const Layer &) override {}
    void ReplaceLayer(const Layer &, std::unique_ptr<Layer>) override {}

  private:
    CommandLine m_Command;
};

// A clock that never waits: it jumps to each deadline and records the sleeps; Now() advances by a
// fixed step.
class TestClock final : public LoopClock {
  public:
    struct Record {
        std::vector<Core::Duration> sleeps; // deadlines, relative to the start
    };

    TestClock(Record &record, Core::Duration step) : m_Record(record), m_Step(step) {}

    Core::TimePoint Now() override {
        const Core::TimePoint now = m_Now;
        m_Now += m_Step;
        return now;
    }
    void SleepUntil(Core::TimePoint deadline) override {
        m_Record.sleeps.push_back(deadline - Core::TimePoint{});
        if (deadline > m_Now) {
            m_Now = deadline;
        }
    }

  private:
    Record &m_Record;
    Core::Duration m_Step;
    Core::TimePoint m_Now{};
};

} // namespace Runic::Application::Testing
