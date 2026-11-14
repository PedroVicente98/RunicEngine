#include "AllocationCounter.hpp"
#include "Log.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

using namespace Runic::Core;

namespace {

// What the capturing sink saw last; it copies into a fixed array, so the sink itself never
// allocates.
struct Captured {
    int lines = 0;
    LogLevel level = LogLevel::Trace;
    std::array<char, 2 * kMaxLogLineBytes> text{};
    std::size_t size = 0;
    const char *file = nullptr;
    int line = 0;

    std::string_view Text() const { return {text.data(), size}; }
};

void Capture(const LogRecord &record, void *user) {
    auto &captured = *static_cast<Captured *>(user);
    ++captured.lines;
    captured.level = record.level;
    captured.size = std::min(record.message.size(), captured.text.size());
    std::copy_n(record.message.data(), captured.size, captured.text.data());
    captured.file = record.file;
    captured.line = record.line;
}

// Installs the capturing sink for one test, then restores the default sink and level.
class LogTest : public ::testing::Test {
  protected:
    void SetUp() override {
        SetLogSink(&Capture, &m_Captured);
        SetMinimumLogLevel(LogLevel::Trace);
    }
    void TearDown() override {
        SetLogSink(nullptr, nullptr);
        SetMinimumLogLevel(LogLevel::Info);
    }

    Captured m_Captured;
};

} // namespace

TEST_F(LogTest, SinkReceivesLevelMessageAndLocation) {
    const int line = __LINE__ + 1;
    RUNIC_LOG_WARNING("layer {} took {} us", "engine:probe_a", 42);
    EXPECT_EQ(m_Captured.lines, 1);
    EXPECT_EQ(m_Captured.level, LogLevel::Warning);
    EXPECT_EQ(m_Captured.Text(), "layer engine:probe_a took 42 us");
    EXPECT_EQ(m_Captured.line, line);
    ASSERT_NE(m_Captured.file, nullptr);
    EXPECT_NE(std::string_view(m_Captured.file).find("LogTests.cpp"), std::string_view::npos);
}

TEST_F(LogTest, LinesBelowTheMinimumLevelAreDropped) {
    SetMinimumLogLevel(LogLevel::Warning);
    EXPECT_EQ(MinimumLogLevel(), LogLevel::Warning);
    EXPECT_FALSE(IsLogLevelEnabled(LogLevel::Info));
    EXPECT_TRUE(IsLogLevelEnabled(LogLevel::Fatal));
    RUNIC_LOG_INFO("dropped");
    RUNIC_LOG_ERROR("kept");
    EXPECT_EQ(m_Captured.lines, 1);
    EXPECT_EQ(m_Captured.level, LogLevel::Error);
    EXPECT_EQ(m_Captured.Text(), "kept");
}

TEST_F(LogTest, LongLinesAreCutWithoutAllocating) {
    const std::string longText(2 * kMaxLogLineBytes, 'x'); // allocated before the measured scope
    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        RUNIC_LOG_INFO("{}", longText);
        allocations = scope.Count();
    }
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(m_Captured.size, kMaxLogLineBytes);
    EXPECT_TRUE(m_Captured.Text().ends_with("..."));
    EXPECT_EQ(m_Captured.Text().substr(0, 8), "xxxxxxxx");
}

TEST_F(LogTest, FinishedLinesAreCutTheSameWay) {
    const std::string longText(1000, 'y');
    WriteLog(LogLevel::Error, longText, "Somewhere.cpp", 7);
    EXPECT_EQ(m_Captured.level, LogLevel::Error);
    EXPECT_EQ(m_Captured.size, kMaxLogLineBytes);
    EXPECT_TRUE(m_Captured.Text().ends_with("..."));
    EXPECT_EQ(m_Captured.line, 7);
}

TEST(LogDefaultSink, WritesTheLevelAndMessageToStderr) {
    testing::internal::CaptureStderr();
    RUNIC_LOG_ERROR("default sink {}", 1);
    const std::string written = testing::internal::GetCapturedStderr();
    EXPECT_NE(written.find("Error: default sink 1"), std::string::npos) << written;
}
