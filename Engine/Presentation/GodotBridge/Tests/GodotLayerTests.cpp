#include "ApplicationHost.hpp"
#include "GodotLayer.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using Runic::Application::Application;
using Runic::Application::ApplicationHost;
using Runic::Application::ApplicationSpec;
using Runic::Application::CommandLine;
using Runic::Application::LoopPolicy;
using Runic::GodotBridge::GodotLayer;
using Runic::GodotBridge::GodotLayerSpec;

TEST(GodotLayer, KnowsHeadlessFromTheArguments) {
    GodotLayerSpec windowed;
    windowed.arguments = {"RunicClient", "--path", "Project"};
    EXPECT_FALSE(GodotLayer(windowed).IsHeadless());
    GodotLayerSpec headless;
    headless.arguments = {"RunicClient", "--headless"};
    EXPECT_TRUE(GodotLayer(headless).IsHeadless());
}

TEST(GodotLayer, AMissingLibraryStopsTheApplicationWithItsReason) {
    // No Godot instance is created: the library is missing, so this runs without libgodot.
    GodotLayerSpec spec;
    spec.arguments = {"RunicClient", "--headless"};
    spec.libraryPath = "/nonexistent/libgodot.so";
    auto application =
        std::make_unique<Application>(ApplicationSpec{"test", LoopPolicy::PacedByTopLayer(), 0});
    application->PushLayer(std::make_unique<GodotLayer>(spec));
    ApplicationHost host(std::move(application), CommandLine({"RunicClient"}));
    const Runic::Core::Result<void> started = host.Start();
    ASSERT_FALSE(started);
    EXPECT_NE(started.GetError().message.find("Godot could not start"), std::string::npos)
        << started.GetError().message;
    EXPECT_NE(started.GetError().message.find("/nonexistent/libgodot.so"), std::string::npos);
    EXPECT_EQ(host.ExitCode(), 1);
    host.Shutdown();
}
