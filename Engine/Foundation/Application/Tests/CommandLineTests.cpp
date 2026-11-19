#include "CommandLine.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using Runic::Application::CommandLine;

#if !defined(_WIN32)
TEST(CommandLine, KeepsEveryArgumentUnchanged) {
    std::string a = "RunicClient", b = "--path", c = "My Game/Project", d = "--headless";
    std::vector<char *> argv = {a.data(), b.data(), c.data(), d.data()};
    const CommandLine command = CommandLine::FromMain(static_cast<int>(argv.size()), argv.data());
    EXPECT_EQ(command.Arguments(),
              (std::vector<std::string>{"RunicClient", "--path", "My Game/Project", "--headless"}));
}
#endif

TEST(CommandLine, FindsFlagsAndTheirValues) {
    const CommandLine command(
        {"--headless", "--path", "My Game/Project", "--name=probe", "--headless-not"});
    EXPECT_FALSE(command.Has("--headless")); // argv[0] is the executable, never a flag
    EXPECT_TRUE(command.Has("--headless-not"));
    EXPECT_EQ(command.ValueOf("--path"), "My Game/Project");
    EXPECT_EQ(command.ValueOf("--name"), "probe");
    EXPECT_FALSE(command.ValueOf("--missing"));
}

TEST(CommandLine, ReadsQuitAfter) {
    EXPECT_EQ(CommandLine({"exe", "--quit-after", "120"}).QuitAfterFrames(), 120u);
    EXPECT_EQ(CommandLine({"exe", "--quit-after=60"}).QuitAfterFrames(), 60u);
    EXPECT_FALSE(CommandLine({"exe", "--quit-after", "soon"}).QuitAfterFrames());
    EXPECT_FALSE(CommandLine({"exe", "--quit-after"}).QuitAfterFrames());
    EXPECT_FALSE(CommandLine({"exe"}).QuitAfterFrames());
}
