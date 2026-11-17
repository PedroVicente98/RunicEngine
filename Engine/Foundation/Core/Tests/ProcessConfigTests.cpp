#include "ProcessConfig.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Runic::Core;

namespace {

// The settings of an imaginary server, one of each kind and a few more to tell the sources apart.
struct TestSettings {
    explicit TestSettings(ProcessConfig &config)
        : workers(config.DeclareInt("simulation.workers", 1, IntRange{1, 64}, "Worker threads")),
          port(
              config.DeclareInt("network.port", 7777, IntRange{1, 65535}, "The port to listen on")),
          offset(config.DeclareInt("test.offset", 0, IntRange{-100, 100}, "A signed value")),
          overlay(config.DeclareBool("development.overlay", false, "Show the overlay")),
          strict(config.DeclareBool("development.strict", true, "Stop on a warning")),
          dataPath(config.DeclareText("paths.data", "Data", "Where cooked data is")),
          name(config.DeclareText("server.name", "engine:test_server", "The server's name")) {}

    IntSetting workers;
    IntSetting port;
    IntSetting offset;
    BoolSetting overlay;
    BoolSetting strict;
    TextSetting dataPath;
    TextSetting name;
};

std::vector<std::string> ProblemLines(const ProcessConfig &config) {
    std::vector<std::string> lines;
    for (const ConfigProblem &problem : config.Problems()) {
        lines.push_back(problem.source + ": " + problem.message);
    }
    return lines;
}

} // namespace

TEST(ProcessConfig, ASettingKeepsItsDefaultWhenNoSourceSetsIt) {
    ProcessConfig config;
    const TestSettings settings(config);
    EXPECT_FALSE(config.IsLoaded());

    ASSERT_TRUE(config.Load({}));

    EXPECT_TRUE(config.IsLoaded());
    EXPECT_EQ(config.Get(settings.workers), 1);
    EXPECT_EQ(config.Get(settings.port), 7777);
    EXPECT_FALSE(config.Get(settings.overlay));
    EXPECT_TRUE(config.Get(settings.strict));
    EXPECT_EQ(config.Get(settings.dataPath), "Data");
    EXPECT_EQ(config.SourceOf(settings.workers), ConfigSource::Default);
    EXPECT_EQ(config.SourceOf(settings.dataPath), ConfigSource::Default);
    EXPECT_TRUE(config.Problems().empty());
}

TEST(ProcessConfig, EachSourceWinsOverTheOnesBeforeIt) {
    ProcessConfig config;
    const TestSettings settings(config);

    // workers is set by all three, port by the file and the environment, the path by the file
    // alone, and the name by nothing.
    const std::vector<std::string> environment = {"RUNIC_CFG_SIMULATION_WORKERS=3",
                                                  "RUNIC_CFG_NETWORK_PORT=9000"};
    const std::vector<std::string> arguments = {"RunicServer", "--set", "simulation.workers=4"};
    ConfigInputs inputs;
    inputs.fileText = "simulation.workers = 2\nnetwork.port = 8000\npaths.data = /srv/data\n";
    inputs.fileName = "server.cfg";
    inputs.environment = environment;
    inputs.arguments = arguments;

    ASSERT_TRUE(config.Load(inputs));

    EXPECT_EQ(config.Get(settings.workers), 4);
    EXPECT_EQ(config.SourceOf(settings.workers), ConfigSource::CommandLine);
    EXPECT_EQ(config.Get(settings.port), 9000);
    EXPECT_EQ(config.SourceOf(settings.port), ConfigSource::Environment);
    EXPECT_EQ(config.Get(settings.dataPath), "/srv/data");
    EXPECT_EQ(config.SourceOf(settings.dataPath), ConfigSource::File);
    EXPECT_EQ(config.Get(settings.name), "engine:test_server");
    EXPECT_EQ(config.SourceOf(settings.name), ConfigSource::Default);
}

TEST(ProcessConfig, EverySourceAloneSetsEveryKind) {
    {
        ProcessConfig config;
        const TestSettings settings(config);
        ConfigInputs inputs;
        inputs.fileText = "test.offset = -42\ndevelopment.overlay = true\nserver.name = a b\n";
        ASSERT_TRUE(config.Load(inputs));
        EXPECT_EQ(config.Get(settings.offset), -42);
        EXPECT_TRUE(config.Get(settings.overlay));
        EXPECT_EQ(config.Get(settings.name), "a b");
    }
    {
        ProcessConfig config;
        const TestSettings settings(config);
        const std::vector<std::string> environment = {"RUNIC_CFG_TEST_OFFSET=-42",
                                                      "RUNIC_CFG_DEVELOPMENT_STRICT=false",
                                                      "RUNIC_CFG_SERVER_NAME=a=b c"};
        ConfigInputs inputs;
        inputs.environment = environment;
        ASSERT_TRUE(config.Load(inputs));
        EXPECT_EQ(config.Get(settings.offset), -42);
        EXPECT_FALSE(config.Get(settings.strict));
        EXPECT_EQ(config.Get(settings.name), "a=b c"); // everything after the first '='
        EXPECT_EQ(config.SourceOf(settings.name), ConfigSource::Environment);
    }
    {
        ProcessConfig config;
        const TestSettings settings(config);
        const std::vector<std::string> arguments = {
            "--set", "test.offset=7", "--set", "development.overlay=true", "--set", "paths.data="};
        ConfigInputs inputs;
        inputs.arguments = arguments;
        ASSERT_TRUE(config.Load(inputs));
        EXPECT_EQ(config.Get(settings.offset), 7);
        EXPECT_TRUE(config.Get(settings.overlay));
        EXPECT_EQ(config.Get(settings.dataPath), ""); // an empty text is a value
        EXPECT_EQ(config.SourceOf(settings.dataPath), ConfigSource::CommandLine);
    }
}

TEST(ProcessConfig, AFileIsKeyValueLinesWithCommentsAndBlankLines) {
    ProcessConfig config;
    const TestSettings settings(config);
    ConfigInputs inputs;
    inputs.fileText = "# The test server\r\n"
                      "\r\n"
                      "   simulation.workers=8   \r\n"
                      "\t# indented comment\n"
                      "server.name =   engine:test # not a comment here  \n"
                      "paths.data = C:\\Runic Data\\cooked"; // no line end after the last line
    ASSERT_TRUE(config.Load(inputs));
    EXPECT_EQ(config.Get(settings.workers), 8);
    EXPECT_EQ(config.Get(settings.name), "engine:test # not a comment here");
    EXPECT_EQ(config.Get(settings.dataPath), "C:\\Runic Data\\cooked");
}

TEST(ProcessConfig, AnUnknownKeyIsReportedWithItsSource) {
    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> environment = {"RUNIC_CFG_SIMULATION_WORKER=2", "PATH=/usr/bin",
                                                  "RUNIC_ENGINE_DIR=/somewhere"};
    const std::vector<std::string> arguments = {"--quit-after", "10", "--set", "network.prot=1"};
    ConfigInputs inputs;
    inputs.fileText = "simulation.workers = 2\nsimulation.wokers = 3\n";
    inputs.fileName = "server.cfg";
    inputs.environment = environment;
    inputs.arguments = arguments;

    const Result<void> loaded = config.Load(inputs);

    ASSERT_FALSE(loaded);
    EXPECT_EQ(ProblemLines(config),
              (std::vector<std::string>{
                  "server.cfg:2: unknown setting 'simulation.wokers'",
                  "environment RUNIC_CFG_SIMULATION_WORKER: names no setting",
                  "command line --set network.prot=1: unknown setting 'network.prot'"}));
    // The error carries the same lines, so a program that only logs it says everything.
    const std::string &message = loaded.GetError().message;
    EXPECT_NE(message.find("3 configuration problem(s)"), std::string::npos);
    for (const std::string &line : ProblemLines(config)) {
        EXPECT_NE(message.find(line), std::string::npos) << line;
    }
    // What no problem touched is still resolved.
    EXPECT_EQ(config.Get(settings.workers), 2);
}

TEST(ProcessConfig, ABadValueIsReportedWithItsSource) {
    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> environment = {"RUNIC_CFG_DEVELOPMENT_OVERLAY=yes",
                                                  "RUNIC_CFG_NETWORK_PORT=70000"};
    const std::vector<std::string> arguments = {"--set", "test.offset=1.5",
                                                "--set", "development.strict=TRUE",
                                                "--set", "network.port=99999999999999999999"};
    ConfigInputs inputs;
    inputs.fileText = "simulation.workers = many\nsimulation.workers.count = 2\ntest.offset =\n";
    inputs.fileName = "server.cfg";
    inputs.environment = environment;
    inputs.arguments = arguments;

    ASSERT_FALSE(config.Load(inputs));

    EXPECT_EQ(
        ProblemLines(config),
        (std::vector<std::string>{
            "server.cfg:1: simulation.workers: 'many' is not a whole number",
            "server.cfg:2: unknown setting 'simulation.workers.count'",
            "server.cfg:3: test.offset: '' is not a whole number",
            "environment RUNIC_CFG_DEVELOPMENT_OVERLAY: development.overlay: 'yes' is not true "
            "or false",
            "environment RUNIC_CFG_NETWORK_PORT: network.port: 70000 is outside 1 to 65535",
            "command line --set test.offset=1.5: test.offset: '1.5' is not a whole number",
            "command line --set development.strict=TRUE: development.strict: 'TRUE' is not true "
            "or false",
            "command line --set network.port=99999999999999999999: network.port: "
            "99999999999999999999 is outside 1 to 65535"}));
    // A bad value changes nothing: the setting keeps what an earlier source or its default gave.
    EXPECT_EQ(config.Get(settings.workers), 1);
    EXPECT_EQ(config.SourceOf(settings.workers), ConfigSource::Default);
    EXPECT_EQ(config.Get(settings.port), 7777);
    EXPECT_TRUE(config.Get(settings.strict));
}

TEST(ProcessConfig, MalformedLinesAndFlagsAreReported) {
    {
        ProcessConfig config;
        const TestSettings settings(config);
        const std::vector<std::string> arguments = {"--set", "simulation.workers", "--set", "=3",
                                                    "--set"};
        ConfigInputs inputs;
        inputs.fileText = "simulation.workers 2\n= 5\nnetwork.port = 1\nnetwork.port = 2\n";
        inputs.arguments = arguments;
        ASSERT_FALSE(config.Load(inputs));
        EXPECT_EQ(ProblemLines(config),
                  (std::vector<std::string>{
                      "configuration file:1: 'simulation.workers 2' is not a 'key = value' line",
                      "configuration file:2: '= 5' is not a 'key = value' line",
                      "configuration file:4: network.port is already set on line 3",
                      "command line --set simulation.workers: is not 'key=value'",
                      "command line --set =3: is not 'key=value'",
                      "command line --set: needs 'key=value'"}));
        EXPECT_EQ(config.Get(settings.port), 1); // the first line stands
    }
    {
        ProcessConfig config;
        const TestSettings settings(config);
        const std::vector<std::string> arguments = {"--config"};
        const std::vector<std::string> environment = {"RUNIC_CFG_NETWORK_PORT"};
        ConfigInputs inputs;
        inputs.arguments = arguments;
        inputs.environment = environment;
        ASSERT_FALSE(config.Load(inputs));
        EXPECT_EQ(ProblemLines(config),
                  (std::vector<std::string>{
                      "environment RUNIC_CFG_NETWORK_PORT: has no value",
                      "command line --config: needs the path of a configuration file"}));
    }
}

TEST(ProcessConfig, TheLastSetOnTheCommandLineWins) {
    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> arguments = {"--set",    "simulation.workers=2",
                                                "--config", "ignored-by-load.cfg",
                                                "--set",    "simulation.workers=6"};
    ConfigInputs inputs;
    inputs.arguments = arguments;
    ASSERT_TRUE(config.Load(inputs));
    EXPECT_EQ(config.Get(settings.workers), 6);
}

TEST(ProcessConfig, DescribeListsEverySettingWithItsValueAndSource) {
    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> arguments = {"--set", "development.overlay=true"};
    ConfigInputs inputs;
    inputs.fileText = "simulation.workers = 12\n";
    inputs.arguments = arguments;
    ASSERT_TRUE(config.Load(inputs));

    ASSERT_EQ(config.Count(), 7u);
    const SettingInfo workers = config.Describe(settings.workers.Index());
    EXPECT_EQ(workers.key, "simulation.workers");
    EXPECT_EQ(workers.help, "Worker threads");
    EXPECT_EQ(workers.kind, SettingKind::Int);
    EXPECT_EQ(workers.source, ConfigSource::File);
    EXPECT_EQ(workers.value, "12");
    EXPECT_EQ(workers.defaultValue, "1");
    const SettingInfo overlay = config.Describe(settings.overlay.Index());
    EXPECT_EQ(overlay.kind, SettingKind::Bool);
    EXPECT_EQ(overlay.source, ConfigSource::CommandLine);
    EXPECT_EQ(overlay.value, "true");
    EXPECT_EQ(overlay.defaultValue, "false");
    const SettingInfo name = config.Describe(settings.name.Index());
    EXPECT_EQ(name.kind, SettingKind::Text);
    EXPECT_EQ(name.value, "engine:test_server");
    EXPECT_EQ(name.source, ConfigSource::Default);

    EXPECT_EQ(ConfigSourceName(ConfigSource::Default), "default");
    EXPECT_EQ(ConfigSourceName(ConfigSource::File), "file");
    EXPECT_EQ(ConfigSourceName(ConfigSource::Environment), "environment");
    EXPECT_EQ(ConfigSourceName(ConfigSource::CommandLine), "command line");
    EXPECT_EQ(ProcessConfig::EnvironmentName("simulation.max_catch_up"),
              "RUNIC_CFG_SIMULATION_MAX_CATCH_UP");
}

#if !defined(_WIN32)

#include <unistd.h> // getpid

namespace {

// A configuration file in the test's temporary folder, removed with the fixture.
class ProcessConfigFile : public testing::Test {
  protected:
    void SetUp() override {
        m_Path = std::filesystem::temp_directory_path() /
                 ("runic_config_test_" + std::to_string(::getpid()) + ".cfg");
    }
    void TearDown() override {
        std::filesystem::remove(m_Path);
        ::unsetenv("RUNIC_CFG_NETWORK_PORT");
        ::unsetenv("RUNIC_CFG_NO_SUCH_SETTING");
    }
    void Write(const std::string &text) const {
        std::ofstream file(m_Path, std::ios::binary);
        file << text;
    }

    std::filesystem::path m_Path;
};

} // namespace

TEST_F(ProcessConfigFile, LoadForProcessReadsTheFileTheEnvironmentAndTheCommandLine) {
    Write("simulation.workers = 2\nnetwork.port = 8000\npaths.data = /srv/data\n");
    ::setenv("RUNIC_CFG_NETWORK_PORT", "9000", 1);

    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> arguments = {"RunicServer", "--config", m_Path.string(), "--set",
                                                "simulation.workers=4"};
    const Result<void> loaded = config.LoadForProcess(arguments);

    ASSERT_TRUE(loaded) << loaded.GetError().message;
    EXPECT_EQ(config.Get(settings.workers), 4);
    EXPECT_EQ(config.Get(settings.port), 9000);
    EXPECT_EQ(config.Get(settings.dataPath), "/srv/data");
    EXPECT_EQ(config.SourceOf(settings.dataPath), ConfigSource::File);
}

TEST_F(ProcessConfigFile, AProblemInTheProcessSourcesNamesTheFileAndTheVariable) {
    Write("simulation.workers = 0\n");
    ::setenv("RUNIC_CFG_NO_SUCH_SETTING", "1", 1);

    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> arguments = {"--config", m_Path.string()};
    ASSERT_FALSE(config.LoadForProcess(arguments));

    EXPECT_EQ(
        ProblemLines(config),
        (std::vector<std::string>{m_Path.string() + ":1: simulation.workers: 0 is outside 1 to 64",
                                  "environment RUNIC_CFG_NO_SUCH_SETTING: names no setting"}));
}

TEST_F(ProcessConfigFile, AFileThatCannotBeReadIsReported) {
    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> arguments = {"--config", m_Path.string()}; // never written
    ASSERT_FALSE(config.LoadForProcess(arguments));
    EXPECT_EQ(ProblemLines(config),
              (std::vector<std::string>{"command line --config " + m_Path.string() +
                                        ": the file cannot be read"}));
    EXPECT_EQ(config.Get(settings.workers), 1);
}

TEST_F(ProcessConfigFile, NoFlagMeansNoFile) {
    ProcessConfig config;
    const TestSettings settings(config);
    const std::vector<std::string> arguments = {"RunicServer", "--quit-after", "60"};
    ASSERT_TRUE(config.LoadForProcess(arguments));
    EXPECT_EQ(config.SourceOf(settings.workers), ConfigSource::Default);
}

#endif // !defined(_WIN32)

#if RUNIC_ASSERTS_ENABLED

TEST(ProcessConfigDeathTest, DeclaringAKeyTwiceIsAProgrammingError) {
    ProcessConfig config;
    config.DeclareBool("test.flag", false, "");
    EXPECT_DEATH(config.DeclareInt("test.flag", 0, IntRange{0, 1}, ""), "declared twice");
}

TEST(ProcessConfigDeathTest, DeclaringAfterLoadIsAProgrammingError) {
    ProcessConfig config;
    ASSERT_TRUE(config.Load({}));
    EXPECT_DEATH(config.DeclareBool("test.flag", false, ""), "after the configuration was loaded");
}

TEST(ProcessConfigDeathTest, AKeyMustBeLowercaseWordsJoinedByDots) {
    ProcessConfig config;
    EXPECT_DEATH(config.DeclareBool("Test.Flag", false, ""), "lowercase words");
    EXPECT_DEATH(config.DeclareBool("test..flag", false, ""), "lowercase words");
    EXPECT_DEATH(config.DeclareBool("test.", false, ""), "lowercase words");
}

TEST(ProcessConfigDeathTest, ADefaultOutsideItsRangeIsAProgrammingError) {
    ProcessConfig config;
    EXPECT_DEATH(config.DeclareInt("test.count", 0, IntRange{1, 4}, ""), "outside its range");
}

#endif // RUNIC_ASSERTS_ENABLED
