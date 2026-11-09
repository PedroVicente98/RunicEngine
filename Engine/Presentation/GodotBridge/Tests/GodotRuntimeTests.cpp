#include "GodotRuntime.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using Runic::GodotBridge::GodotRuntime;
using Runic::GodotBridge::GodotTarget;

TEST(GodotRuntime, LibraryFileNameNamesTheTarget) {
    const std::string editor = GodotRuntime::LibraryFileName(GodotTarget::Editor);
    const std::string shipping = GodotRuntime::LibraryFileName(GodotTarget::Template);
    EXPECT_NE(editor.find(".editor."), std::string::npos) << editor;
    EXPECT_NE(shipping.find(".template_release."), std::string::npos) << shipping;
#if defined(_WIN32)
    EXPECT_EQ(editor.rfind(".dll"), editor.size() - 4);
#elif defined(__linux__)
    EXPECT_EQ(editor.rfind("libgodot.linuxbsd.", 0), 0u);
    EXPECT_EQ(editor.rfind(".so"), editor.size() - 3);
#endif
}

TEST(GodotRuntime, DefaultLibraryIsNextToTheExecutable) {
    const std::filesystem::path directory(GodotRuntime::ExecutableDirectory());
    EXPECT_TRUE(std::filesystem::is_directory(directory));
    const std::filesystem::path library(GodotRuntime::DefaultLibraryPath(GodotTarget::Editor));
    EXPECT_EQ(library.parent_path(), directory);
}

TEST(GodotRuntime, LoadReportsAMissingLibrary) {
    GodotRuntime runtime;
    EXPECT_FALSE(runtime.Load("/nonexistent/libgodot.so"));
    EXPECT_FALSE(runtime.IsLoaded());
    EXPECT_NE(runtime.LastError().find("/nonexistent/libgodot.so"), std::string::npos) << runtime.LastError();
}

TEST(GodotRuntime, CreateNeedsALoadedLibrary) {
    GodotRuntime runtime;
    EXPECT_FALSE(runtime.Create({"host"}));
    EXPECT_FALSE(runtime.IsCreated());
    EXPECT_FALSE(runtime.LastError().empty());
}

TEST(GodotRuntime, IterateBeforeStartAsksToQuit) {
    GodotRuntime runtime;
    EXPECT_FALSE(runtime.Start());
    EXPECT_TRUE(runtime.Iterate());
    EXPECT_EQ(runtime.FrameCount(), 0u);
}

#if !defined(_WIN32)
TEST(GodotRuntime, CommandLineKeepsEveryArgumentUnchanged) {
    std::string a = "host", b = "--path", c = "My Game/Project", d = "--headless";
    std::vector<char*> argv = {a.data(), b.data(), c.data(), d.data()};
    const auto arguments = GodotRuntime::CommandLine(static_cast<int>(argv.size()), argv.data());
    EXPECT_EQ(arguments, (std::vector<std::string>{"host", "--path", "My Game/Project", "--headless"}));
}
#endif

// Slice 0 runnable result, at the Engine level: Godot starts headless as a library and exits cleanly.
// It runs the editor library without --editor, the way the development GameClient runs a project folder.
// Runs when the build staged the library next to this test (Runic::LibGodotEditor).
TEST(GodotRuntime, StartsGodotHeadlessAndExits) {
    const std::string library = GodotRuntime::DefaultLibraryPath(GodotTarget::Editor);
    if (!std::filesystem::exists(library)) {
        GTEST_SKIP() << "libgodot is not built: " << library;
    }
    GodotRuntime runtime;
    ASSERT_TRUE(runtime.Load(library)) << runtime.LastError();
    const std::vector<std::string> arguments = {GodotRuntime::ExecutableDirectory() + "/RunicGodotBridgeTests",
                                                "--headless", "--path", RUNIC_GODOT_SPIKE_PROJECT,
                                                "--quit-after", "10"};
    ASSERT_TRUE(runtime.Create(arguments)) << runtime.LastError();
    ASSERT_TRUE(runtime.Start()) << runtime.LastError();
    bool quit = false;
    while (!quit && runtime.FrameCount() < 600) {
        quit = runtime.Iterate();
    }
    EXPECT_TRUE(quit) << "Godot did not ask to quit after --quit-after 10";
    EXPECT_GE(runtime.FrameCount(), 1u);
    runtime.Destroy();
    EXPECT_FALSE(runtime.IsCreated());

    // LibGodot never creates a second instance in the same process.
    EXPECT_FALSE(runtime.Create(arguments));
}

// The shipping library is a valid LibGodot library. It cannot run a project folder: export templates are
// built with disable_path_overrides, so they refuse --path and only run the .pck exported next to the
// client (COOK-4, slice 22). That is the shipping lock, kept on.
TEST(GodotRuntime, LoadsTheTemplateLibrary) {
    const std::string library = GodotRuntime::DefaultLibraryPath(GodotTarget::Template);
    if (!std::filesystem::exists(library)) {
        GTEST_SKIP() << "libgodot is not built: " << library;
    }
    GodotRuntime runtime;
    EXPECT_TRUE(runtime.Load(library)) << runtime.LastError();
    EXPECT_TRUE(runtime.IsLoaded());
}
