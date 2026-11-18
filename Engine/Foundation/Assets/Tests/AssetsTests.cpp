// ASSET-1: asset handles, the cache, and the native cooked mesh, collision and cell formats. A
// cooked test mesh loads on client and server: this executable is built and run by both the
// headless build (the server's) and the debug build with presentation (the client's), and reads a
// golden file written once by the writer.

#include "AssetCache.hpp"
#include "AssetCatalog.hpp"
#include "CookedFormats.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <vector>

using namespace Runic;
using namespace Runic::Assets;
using Core::AssetId;
using Core::Vec3;
namespace fs = std::filesystem;

namespace {

// A unit square of two triangles facing up, with its normals: the golden file's mesh.
CookedMesh TestSquare() {
    CookedMesh mesh;
    mesh.positions = {{0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}};
    mesh.normals = {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.boundsMin = {0, 0, 0};
    mesh.boundsMax = {1, 0, 1};
    return mesh;
}

CookedCollision TestCollision() {
    CookedCollision collision;
    collision.meshes.push_back({{{0, 0, 0}, {0, 0, 1}, {1, 0, 1}}, {0, 1, 2}});
    CollisionShape mesh;
    mesh.kind = CollisionShapeKind::Mesh;
    mesh.mesh = 0;
    CollisionShape box;
    box.kind = CollisionShapeKind::Box;
    box.halfExtents = {0.5f, 1.0f, 2.0f};
    box.position = {0, 1, 0};
    box.rotation = glm::angleAxis(0.5f, Vec3(0, 1, 0));
    CollisionShape capsule;
    capsule.kind = CollisionShapeKind::Capsule;
    capsule.radius = 0.3f;
    capsule.halfHeight = 0.6f;
    collision.shapes = {mesh, box, capsule};
    return collision;
}

CookedCell TestCell() {
    CookedCell cell;
    cell.origin = {5'000'000.25, 120.5, -7'000'000.125}; // 64-bit: kept to the last bit
    CellPlacement rock;
    rock.id = 0x1234'5678'9abc'def0ull;
    rock.mesh = AssetId(11);
    rock.collision = AssetId(12);
    rock.position = {3, 0, -2};
    rock.scale = {2, 2, 2};
    cell.placements = {rock};
    CellCollider wall;
    wall.id = 99;
    wall.shape.kind = CollisionShapeKind::Box;
    wall.shape.halfExtents = {5, 2, 0.1f};
    cell.colliders = {wall};
    return cell;
}

std::string ErrorOf(const Core::Result<CookedMesh> &result) {
    return result ? std::string("(read)") : result.GetError().message;
}

bool Contains(const std::string &text, const std::string &part) {
    return text.find(part) != std::string::npos;
}

// A cooked folder in the temporary directory, removed afterwards.
class CookedFolder {
  public:
    explicit CookedFolder(std::string_view name)
        : m_Root(fs::temp_directory_path() / ("runic-assets-test-" + std::string(name))) {
        fs::remove_all(m_Root);
        fs::create_directories(m_Root);
    }
    ~CookedFolder() {
        std::error_code ignored;
        fs::remove_all(m_Root, ignored);
    }
    const fs::path &Root() const { return m_Root; }
    void Put(std::string_view file, const std::vector<std::byte> &bytes) {
        fs::create_directories((m_Root / file).parent_path());
        ASSERT_TRUE(WriteBytesToFile(m_Root / file, bytes));
    }
    void PutCatalog(const AssetCatalog &catalog) {
        const std::string text = catalog.Write();
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        Put(kCatalogFileName, bytes);
    }

  private:
    fs::path m_Root;
};

AssetEntry Entry(std::uint64_t id, AssetKind kind, const char *name, const char *file) {
    return {AssetId(id), kind, Content::ContentId::Of(name), file};
}

} // namespace

TEST(CookedFormats, AMeshRoundTrips) {
    const CookedMesh square = TestSquare();
    const auto read = ReadCookedMesh(WriteCookedMesh(square));
    ASSERT_TRUE(read) << read.GetError().message;
    EXPECT_EQ(read.Value().positions, square.positions);
    EXPECT_EQ(read.Value().normals, square.normals);
    EXPECT_EQ(read.Value().indices, square.indices);
    EXPECT_EQ(read.Value().boundsMax, square.boundsMax);
    EXPECT_EQ(PeekCookedKind(WriteCookedMesh(square)).Value(), AssetKind::Mesh);
}

TEST(CookedFormats, ACollisionRoundTripsWithEveryShapeKind) {
    const CookedCollision collision = TestCollision();
    const auto read = ReadCookedCollision(WriteCookedCollision(collision));
    ASSERT_TRUE(read) << read.GetError().message;
    ASSERT_EQ(read.Value().shapes.size(), 3u);
    EXPECT_EQ(read.Value().meshes[0].positions, collision.meshes[0].positions);
    EXPECT_EQ(read.Value().shapes[0].kind, CollisionShapeKind::Mesh);
    EXPECT_EQ(read.Value().shapes[1].halfExtents, collision.shapes[1].halfExtents);
    EXPECT_EQ(read.Value().shapes[1].rotation, collision.shapes[1].rotation);
    EXPECT_EQ(read.Value().shapes[2].radius, 0.3f);
    EXPECT_EQ(read.Value().shapes[2].halfHeight, 0.6f);
}

TEST(CookedFormats, ACellKeepsItsWorldOriginToTheLastBit) {
    const CookedCell cell = TestCell();
    const auto read = ReadCookedCell(WriteCookedCell(cell));
    ASSERT_TRUE(read) << read.GetError().message;
    EXPECT_EQ(read.Value().origin, cell.origin);
    ASSERT_EQ(read.Value().placements.size(), 1u);
    EXPECT_EQ(read.Value().placements[0].id, cell.placements[0].id);
    EXPECT_EQ(read.Value().placements[0].mesh, AssetId(11));
    EXPECT_EQ(read.Value().placements[0].collision, AssetId(12));
    EXPECT_EQ(read.Value().placements[0].scale, Vec3(2.0f));
    ASSERT_EQ(read.Value().colliders.size(), 1u);
    EXPECT_EQ(read.Value().colliders[0].shape.halfExtents, Vec3(5, 2, 0.1f));
}

TEST(CookedFormats, TheGoldenMeshFileReadsAndTheWriterStillWritesItByteForByte) {
    const auto golden = ReadBytesFromFile(fs::path(RUNIC_ASSETS_TEST_DATA) / "golden_square.rmesh");
    ASSERT_TRUE(golden) << golden.GetError().message;
    EXPECT_EQ(golden.Value(), WriteCookedMesh(TestSquare())); // the format did not change silently
    const auto read = ReadCookedMesh(golden.Value());
    ASSERT_TRUE(read) << read.GetError().message;
    EXPECT_EQ(read.Value().indices.size(), 6u);
}

TEST(CookedFormats, ADamagedFileSaysWhatIsWrong) {
    const std::vector<std::byte> good = WriteCookedMesh(TestSquare());
    auto damaged = [&](std::size_t at, std::byte value) {
        std::vector<std::byte> bytes = good;
        bytes[at] = value;
        return bytes;
    };
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(damaged(0, std::byte{'X'}))), "RUNA"));
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(damaged(4, std::byte{9}))), "version 9"));
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(WriteCookedCell(TestCell()))), "a cell"));
    EXPECT_TRUE(
        Contains(ErrorOf(ReadCookedMesh(damaged(good.size() - 1, std::byte{0x7f}))), "checksum"));
    std::vector<std::byte> truncated(good.begin(), good.end() - 4);
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(truncated)), "payload"));
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(std::vector<std::byte>(3))), "header"));

    // Sound bytes with unsound content: checked after the checksum, named.
    CookedMesh outside = TestSquare();
    outside.indices[4] = 9;
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(WriteCookedMesh(outside))), "index 9"));
    CookedMesh notANumber = TestSquare();
    notANumber.positions[1].y = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(WriteCookedMesh(notANumber))), "finite"));
    CookedMesh partial = TestSquare();
    partial.indices.pop_back();
    EXPECT_TRUE(Contains(ErrorOf(ReadCookedMesh(WriteCookedMesh(partial))), "multiple of 3"));
    CookedCollision badShape = TestCollision();
    badShape.shapes[0].mesh = 4;
    const auto collision = ReadCookedCollision(WriteCookedCollision(badShape));
    ASSERT_FALSE(collision);
    EXPECT_TRUE(Contains(collision.GetError().message, "mesh 4"));
    CookedCell badRotation = TestCell();
    badRotation.placements[0].rotation = Core::Quat(2.0f, 0.0f, 0.0f, 0.0f);
    const auto cell = ReadCookedCell(WriteCookedCell(badRotation));
    ASSERT_FALSE(cell);
    EXPECT_TRUE(Contains(cell.GetError().message, "unit"));
}

TEST(AssetCatalog, ReadsWhatItWritesAndFindsByIdOrName) {
    AssetCatalog catalog;
    EXPECT_TRUE(catalog.Add(Entry(0xa1, AssetKind::Mesh, "game:test_rock", "Meshes/a1.rmesh"),
                            Content::FromCode("a test")));
    EXPECT_TRUE(catalog.Add(
        Entry(0xa2, AssetKind::Collision, "game:test_rock/collision", "Collision/a2.rcol"),
        Content::FromCode("a test")));
    const AssetCatalog read = AssetCatalog::Read(catalog.Write(), "Build/Cooked/catalog.txt");
    EXPECT_FALSE(read.HasProblems());
    EXPECT_EQ(read.Count(), 2u);
    ASSERT_NE(read.Find(AssetId(0xa2)), nullptr);
    EXPECT_EQ(read.Find(AssetId(0xa2))->kind, AssetKind::Collision);
    EXPECT_EQ(read.Find("game:test_rock")->file, "Meshes/a1.rmesh");
    EXPECT_EQ(read.Find(AssetId(0xbad)), nullptr);
    EXPECT_EQ(AssetIdText(AssetId(0xa1)), "00000000000000a1");
    EXPECT_EQ(ParseAssetId("00000000000000a1").Value(), AssetId(0xa1));
}

TEST(AssetCatalog, ReportsEveryProblemWithItsLine) {
    const std::string text = "runic-assets 1\n"
                             "# a comment, then four assets and three mistakes\n"
                             "mesh\t00000000000000a1\tgame:test_rock\tMeshes/a1.rmesh\n"
                             "mesh\t00000000000000a2\tgame:test_rock\tMeshes/a2.rmesh\n"
                             "mesh\t00000000000000a1\tgame:test_tree\tMeshes/a3.rmesh\n"
                             "sound\t00000000000000a4\tgame:test_bell\tSounds/a4.ogg\n"
                             "mesh\t00000000000000a5\tgame:Test_Bush\tMeshes/a5.rmesh\n"
                             "mesh\t00000000000000a6\n";
    const AssetCatalog catalog = AssetCatalog::Read(text, "Build/Cooked/catalog.txt");
    EXPECT_EQ(catalog.Count(), 1u);
    std::vector<std::string> messages;
    for (const Content::ContentProblem &problem : catalog.Problems()) {
        messages.push_back(problem.message);
    }
    ASSERT_EQ(messages.size(), 5u);
    EXPECT_TRUE(Contains(messages[0], "catalog.txt:4")); // the name twice
    EXPECT_TRUE(Contains(messages[0], "catalog.txt:3"));
    EXPECT_TRUE(Contains(messages[1], "catalog.txt:5")); // the ID twice
    EXPECT_TRUE(Contains(messages[1], "00000000000000a1"));
    EXPECT_TRUE(Contains(messages[2], "catalog.txt:6")); // an unknown kind
    EXPECT_TRUE(Contains(messages[2], "sound"));
    EXPECT_TRUE(Contains(messages[3], "catalog.txt:7")); // a malformed name
    EXPECT_TRUE(Contains(messages[4], "catalog.txt:8")); // too few columns

    const AssetCatalog wrongHeader = AssetCatalog::Read("runic-assets 2\n", "catalog.txt");
    ASSERT_EQ(wrongHeader.Problems().size(), 1u);
    EXPECT_TRUE(Contains(wrongHeader.Problems()[0].message, "runic-assets 1"));
}

TEST(AssetCache, ACookedTestMeshLoadsOnceAndIsShared) {
    CookedFolder folder("shared");
    folder.Put("Meshes/a1.rmesh", WriteCookedMesh(TestSquare()));
    AssetCatalog catalog;
    catalog.Add(Entry(0xa1, AssetKind::Mesh, "game:test_square", "Meshes/a1.rmesh"),
                Content::FromCode("a test"));
    folder.PutCatalog(catalog);

    AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());
    const auto first = cache.LoadMesh(AssetId(0xa1));
    ASSERT_TRUE(first) << first.GetError().message;
    EXPECT_EQ(first.Value()->indices.size(), 6u);
    const auto again = cache.LoadMesh("game:test_square");
    ASSERT_TRUE(again);
    EXPECT_EQ(&again.Value().Get(), &first.Value().Get());
    const AssetCacheStats stats = cache.Stats();
    EXPECT_EQ(stats.loads, 1u);
    EXPECT_EQ(stats.hits, 1u);
    EXPECT_EQ(stats.resident, 1u);
    EXPECT_GT(stats.residentBytes, 0u);
}

TEST(AssetCache, CollectFreesWhatNoHandleHolds) {
    CookedFolder folder("collect");
    folder.Put("Meshes/a1.rmesh", WriteCookedMesh(TestSquare()));
    folder.Put("Collision/a2.rcol", WriteCookedCollision(TestCollision()));
    AssetCatalog catalog;
    catalog.Add(Entry(0xa1, AssetKind::Mesh, "game:test_square", "Meshes/a1.rmesh"),
                Content::FromCode("a test"));
    catalog.Add(
        Entry(0xa2, AssetKind::Collision, "game:test_square/collision", "Collision/a2.rcol"),
        Content::FromCode("a test"));
    folder.PutCatalog(catalog);
    AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());

    auto held = cache.LoadCollision(AssetId(0xa2));
    ASSERT_TRUE(held);
    {
        const auto dropped = cache.LoadMesh(AssetId(0xa1));
        ASSERT_TRUE(dropped);
    }
    EXPECT_EQ(cache.Collect(), 1u); // the mesh: no handle holds it
    EXPECT_EQ(cache.Stats().resident, 1u);
    EXPECT_EQ(held.Value()->shapes.size(), 3u); // still there
    ASSERT_TRUE(cache.LoadMesh(AssetId(0xa1)));
    EXPECT_EQ(cache.Stats().loads, 3u); // read again after it went
}

TEST(AssetCache, AnAssetThatCannotLoadSaysWhy) {
    CookedFolder folder("failures");
    folder.Put("Meshes/a1.rmesh", WriteCookedMesh(TestSquare()));
    std::vector<std::byte> damaged = WriteCookedMesh(TestSquare());
    damaged.back() = std::byte{0x55};
    folder.Put("Meshes/a3.rmesh", damaged);
    AssetCatalog catalog;
    catalog.Add(Entry(0xa1, AssetKind::Mesh, "game:test_square", "Meshes/a1.rmesh"),
                Content::FromCode("a test"));
    catalog.Add(Entry(0xa2, AssetKind::Mesh, "game:test_gone", "Meshes/a2.rmesh"),
                Content::FromCode("a test"));
    catalog.Add(Entry(0xa3, AssetKind::Mesh, "game:test_damaged", "Meshes/a3.rmesh"),
                Content::FromCode("a test"));
    folder.PutCatalog(catalog);
    AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());

    const auto why = [](const auto &result) {
        return result ? std::string("(loaded)") : result.GetError().message;
    };
    EXPECT_TRUE(Contains(why(cache.LoadMesh(AssetId(0xff))), "00000000000000ff"));
    EXPECT_TRUE(Contains(why(cache.LoadMesh("game:test_nothing")), "game:test_nothing"));
    EXPECT_TRUE(Contains(why(cache.LoadCollision(AssetId(0xa1))), "a mesh"));
    EXPECT_TRUE(Contains(why(cache.LoadMesh(AssetId(0xa2))), "Meshes/a2.rmesh"));
    EXPECT_TRUE(Contains(why(cache.LoadMesh(AssetId(0xa3))), "checksum"));
    EXPECT_EQ(cache.Stats().failures, 5u);

    AssetCache nothing(folder.Root() / "nowhere");
    const auto opened = nothing.Open();
    ASSERT_FALSE(opened);
    EXPECT_TRUE(Contains(opened.GetError().message, "catalog.txt"));
}

TEST(AssetCache, ThreadsLoadTheSameAndDifferentAssetsSafely) {
    CookedFolder folder("threads");
    AssetCatalog catalog;
    constexpr int kAssets = 8;
    for (int i = 0; i < kAssets; ++i) {
        const std::string file = "Meshes/m" + std::to_string(i) + ".rmesh";
        folder.Put(file, WriteCookedMesh(TestSquare()));
        catalog.Add({AssetId(0x100 + i), AssetKind::Mesh,
                     Content::ContentId::Of("game:test_mesh_" + std::to_string(i)), file},
                    Content::FromCode("a test"));
    }
    folder.PutCatalog(catalog);
    AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());

    std::atomic<int> loaded{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            for (int round = 0; round < 50; ++round) {
                const auto mesh = cache.LoadMesh(AssetId(0x100 + (t + round) % kAssets));
                loaded += mesh ? 1 : 0;
                if (round % 10 == 0) {
                    cache.Collect();
                }
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(loaded.load(), 200);
    EXPECT_EQ(cache.Stats().failures, 0u);
}
