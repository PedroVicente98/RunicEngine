// CNT-1: content IDs in the engine: and game: namespaces, registries and lookup. Duplicate and
// unknown IDs are reported with their source.

#include "ContentId.hpp"
#include "ContentRegistry.hpp"

#include <gtest/gtest.h>

#include <string>
#include <unordered_set>
#include <vector>

using namespace Runic::Content;

namespace {

struct TestItem {
    int weight = 0;
};

bool Contains(const std::string &text, const std::string &part) {
    return text.find(part) != std::string::npos;
}

} // namespace

TEST(ContentId, ParsesTheEngineAndGameNamespaces) {
    const auto engine = ContentId::Parse("engine:test_ability");
    ASSERT_TRUE(engine);
    EXPECT_EQ(engine.Value().Namespace(), ContentNamespace::Engine);
    EXPECT_EQ(engine.Value().Name(), "test_ability");
    EXPECT_EQ(engine.Value().Text(), "engine:test_ability");

    const auto game = ContentId::Parse("game:rocks/granite_01");
    ASSERT_TRUE(game);
    EXPECT_EQ(game.Value().Namespace(), ContentNamespace::Game);
    EXPECT_EQ(game.Value().Name(), "rocks/granite_01");
    EXPECT_EQ(NamespaceText(ContentNamespace::Game), "game");
    EXPECT_FALSE(ContentId().IsValid());
}

TEST(ContentId, SaysWhyTextIsNotAContentId) {
    const auto reason = [](const char *text) {
        const auto id = ContentId::Parse(text);
        return id ? std::string("(parsed)") : id.GetError().message;
    };
    EXPECT_TRUE(Contains(reason(""), "empty"));
    EXPECT_TRUE(Contains(reason("test_ability"), "namespace"));
    EXPECT_TRUE(Contains(reason("mod:test_ability"), "'mod'"));
    EXPECT_TRUE(Contains(reason("engine:"), "name"));
    EXPECT_TRUE(Contains(reason("engine:Test"), "'T' at 7"));
    EXPECT_TRUE(Contains(reason("engine:a b"), "' ' at 8"));
    EXPECT_TRUE(Contains(reason("game:rocks//granite"), "empty part"));
    EXPECT_TRUE(Contains(reason("game:/rocks"), "empty part"));
    EXPECT_TRUE(Contains(reason("game:rocks/"), "empty part"));
    EXPECT_TRUE(Contains(reason("engine:a:b"), "':' at 8"));
    EXPECT_TRUE(
        Contains(reason(("game:" + std::string(kMaxContentIdLength, 'a')).c_str()), "longer than"));
    // The message names the text, so a report reads on its own.
    EXPECT_TRUE(Contains(reason("engine:Test"), "engine:Test"));
}

TEST(ContentId, HashesItsTextTheSameEverywhere) {
    // 64-bit FNV-1a: fixed values, so a cooked file written on one machine reads on another.
    EXPECT_EQ(HashText(""), 0xcbf29ce484222325ull);
    EXPECT_EQ(HashText("a"), 0xaf63dc4c8601ec8cull);
    const ContentId id = ContentId::Of("engine:test_ability");
    EXPECT_EQ(id.Hash(), HashText("engine:test_ability"));
    EXPECT_EQ(std::hash<ContentId>()(id), static_cast<std::size_t>(id.Hash()));

    std::unordered_set<ContentId> ids{ContentId::Of("game:a"), ContentId::Of("game:b")};
    EXPECT_EQ(ids.count(ContentId::Of("game:a")), 1u);
    EXPECT_LT(ContentId::Of("engine:z"), ContentId::Of("game:a"));
    EXPECT_EQ(ContentId::Of("game:a"), ContentId::Parse("game:a").Value());
}

TEST(ContentRegistry, FindsWhatWasAddedByIdOrText) {
    ContentRegistry<TestItem> items("item");
    EXPECT_TRUE(items.Add(ContentId::Of("game:test_sword"), {3}, FromFile("Data/items.json", 4)));
    EXPECT_TRUE(items.Add("game:test_shield", {5}, FromFile("Data/items.json", 9)));
    EXPECT_EQ(items.Count(), 2u);
    ASSERT_NE(items.Find("game:test_sword"), nullptr);
    EXPECT_EQ(items.Find(ContentId::Of("game:test_shield"))->weight, 5);
    EXPECT_EQ(items.Find("game:test_bow"), nullptr);
    EXPECT_EQ(items.Find("not an id"), nullptr);
    EXPECT_EQ(items.SourceOf(ContentId::Of("game:test_shield"))->ToText(), "Data/items.json:9");
    EXPECT_FALSE(items.HasProblems()); // lookups report nothing

    std::vector<std::string> order;
    items.ForEach([&](const ContentId &id, const TestItem &, const ContentSource &) {
        order.emplace_back(id.Text());
    });
    EXPECT_EQ(order, (std::vector<std::string>{"game:test_sword", "game:test_shield"}));
}

TEST(ContentRegistry, ADuplicateIsReportedWithBothSourcesAndTheFirstStays) {
    ContentRegistry<TestItem> items("item");
    items.Add("game:test_sword", {3}, FromFile("Data/items.json", 4));
    EXPECT_FALSE(items.Add("game:test_sword", {7}, FromFile("Data/more_items.json", 12)));
    EXPECT_EQ(items.Find("game:test_sword")->weight, 3);

    ASSERT_EQ(items.Problems().size(), 1u);
    const ContentProblem &problem = items.Problems().front();
    EXPECT_EQ(problem.kind, ContentProblemKind::Duplicate);
    EXPECT_EQ(problem.id, "game:test_sword");
    EXPECT_EQ(problem.source, FromFile("Data/more_items.json", 12));
    EXPECT_EQ(problem.firstSource, FromFile("Data/items.json", 4));
    EXPECT_TRUE(Contains(problem.message, "item game:test_sword"));
    EXPECT_TRUE(Contains(problem.message, "Data/more_items.json:12"));
    EXPECT_TRUE(Contains(problem.message, "Data/items.json:4"));
}

TEST(ContentRegistry, AnUnknownIdIsReportedWithWhereItWasReferenced) {
    ContentRegistry<TestItem> items("item");
    items.Add("game:test_sword", {3}, FromCode("engine code"));
    EXPECT_NE(items.Resolve("game:test_sword", FromFile("Data/quests.json", 20)), nullptr);
    EXPECT_EQ(items.Resolve("game:test_swrod", FromFile("Data/quests.json", 40)), nullptr);
    EXPECT_EQ(items.Resolve(ContentId::Of("game:test_bow"), FromCode("a test")), nullptr);

    ASSERT_EQ(items.Problems().size(), 2u);
    const ContentProblem &typo = items.Problems()[0];
    EXPECT_EQ(typo.kind, ContentProblemKind::Unknown);
    EXPECT_EQ(typo.id, "game:test_swrod");
    EXPECT_EQ(typo.source.ToText(), "Data/quests.json:40");
    EXPECT_TRUE(Contains(typo.message, "Data/quests.json:40"));
    EXPECT_TRUE(Contains(typo.message, "item game:test_swrod"));
    EXPECT_EQ(items.Problems()[1].source.ToText(), "a test");

    items.ClearProblems();
    EXPECT_FALSE(items.HasProblems());
}

TEST(ContentRegistry, AMalformedIdIsReportedWithItsSource) {
    ContentRegistry<TestItem> items("item");
    EXPECT_FALSE(items.Add("game:Test_Sword", {1}, FromFile("Data/items.json", 2)));
    EXPECT_EQ(items.Resolve("sword", FromFile("Data/quests.json", 3)), nullptr);
    ASSERT_EQ(items.Problems().size(), 2u);
    EXPECT_EQ(items.Problems()[0].kind, ContentProblemKind::Malformed);
    EXPECT_EQ(items.Problems()[0].id, "game:Test_Sword");
    EXPECT_TRUE(Contains(items.Problems()[0].message, "Data/items.json:2"));
    EXPECT_TRUE(Contains(items.Problems()[0].message, "'T' at 5"));
    EXPECT_EQ(items.Problems()[1].source.ToText(), "Data/quests.json:3");
    EXPECT_EQ(items.Count(), 0u);
}

TEST(ContentRegistry, DefinitionsKeepTheirAddressAsTheRegistryGrows) {
    ContentRegistry<TestItem> items("item");
    items.Add("game:test_first", {1}, FromCode("a test"));
    const TestItem *first = items.Find("game:test_first");
    for (int i = 0; i < 1000; ++i) {
        items.Add("game:test_item_" + std::to_string(i), {i}, FromCode("a test"));
    }
    EXPECT_EQ(items.Find("game:test_first"), first);
    EXPECT_EQ(items.Find("game:test_item_999")->weight, 999);
}

TEST(ContentSource, NamesAFileItsLineOrWhatMadeIt) {
    EXPECT_EQ(FromFile("Data/items.json", 12).ToText(), "Data/items.json:12");
    EXPECT_EQ(FromFile("Content/test.tscn").ToText(), "Content/test.tscn");
    EXPECT_EQ(FromCode("engine code").ToText(), "engine code");
    EXPECT_EQ(ContentSource{}.ToText(), "an unknown source");
}
