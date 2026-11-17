// The slice 8 Core addition: cached debug meshes. A library the writer and the reader of a frame
// stream share, and mesh instances a DebugGeometry lists per frame.

#include "RenderData/DebugMeshes.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "AllocationCounter.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "RenderData/RenderFrameExchange.hpp"

using namespace Runic;
using namespace Runic::RenderData;
using Core::DebugMeshId;
using Core::Vec3;

namespace {

std::shared_ptr<const DebugMesh> Triangle(float size) {
    auto mesh = std::make_shared<DebugMesh>();
    mesh->trianglePositions = {Vec3(0.0f), Vec3(size, 0.0f, 0.0f), Vec3(0.0f, 0.0f, size)};
    mesh->triangleColors = {kDebugPhysics, kDebugPhysics, kDebugPhysics};
    mesh->linePositions = {Vec3(0.0f), Vec3(size, 0.0f, 0.0f)};
    mesh->lineColors = {kDebugInactive, kDebugInactive};
    return mesh;
}

} // namespace

TEST(DebugMeshLibrary, KeepsMeshesByIdWithARevisionPerChange) {
    DebugMeshLibrary library;
    EXPECT_EQ(library.Revision(), 0u);
    library.Put(DebugMeshId(7), Triangle(1.0f));
    library.Put(DebugMeshId(3), Triangle(2.0f));
    EXPECT_EQ(library.Count(), 2u);
    EXPECT_TRUE(library.Contains(DebugMeshId(7)));
    EXPECT_EQ(library.Revision(), 2u);

    std::vector<DebugMeshLibrary::Entry> entries;
    EXPECT_EQ(library.Snapshot(entries), 2u);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].id, DebugMeshId(3)); // by ID
    EXPECT_EQ(entries[0].revision, 2u);
    EXPECT_EQ(entries[1].revision, 1u);
    EXPECT_EQ(entries[0].mesh->VertexCount(), 5u);

    // Replacing a mesh gives it the new revision; the reader still holding the old one keeps it.
    const std::shared_ptr<const DebugMesh> old = entries[1].mesh;
    library.Put(DebugMeshId(7), Triangle(5.0f));
    library.Snapshot(entries);
    EXPECT_EQ(entries[1].revision, 3u);
    EXPECT_EQ(old->trianglePositions[1].x, 1.0f);

    library.Remove(DebugMeshId(3));
    library.Remove(DebugMeshId(99)); // not there: ignored, no change
    EXPECT_EQ(library.Revision(), 4u);
    EXPECT_FALSE(library.Contains(DebugMeshId(3)));
    EXPECT_EQ(library.Snapshot(entries), 4u);
    EXPECT_EQ(entries.size(), 1u);
}

TEST(DebugMeshLibrary, IsSharedBetweenAWriterAndAReaderThread) {
    DebugMeshLibrary library;
    constexpr int kMeshes = 2000;
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (int i = 1; i <= kMeshes; ++i) {
            library.Put(DebugMeshId(static_cast<std::uint64_t>(i)), Triangle(1.0f));
            if (i % 3 == 0) {
                library.Remove(DebugMeshId(static_cast<std::uint64_t>(i - 1)));
            }
        }
        done = true;
    });
    std::vector<DebugMeshLibrary::Entry> entries;
    std::uint64_t seen = 0;
    while (!done) {
        const std::uint64_t revision = library.Snapshot(entries);
        EXPECT_GE(revision, seen); // revisions only move forward
        seen = revision;
        for (const auto &entry : entries) {
            EXPECT_EQ(entry.mesh->trianglePositions.size(), 3u);
        }
    }
    writer.join();
    EXPECT_EQ(library.Count(), static_cast<std::size_t>(kMeshes - kMeshes / 3));
}

TEST(DebugGeometry, ListsMeshInstancesUpToItsCapacity) {
    DebugGeometryCapacity capacity;
    capacity.meshInstances = 2;
    DebugGeometry geometry(capacity);
    EXPECT_TRUE(geometry.IsEmpty());
    DebugMeshInstance instance;
    instance.mesh = DebugMeshId(5);
    instance.position = Vec3(1.0f, 2.0f, 3.0f);
    {
        Core::Testing::AllocationScope scope;
        EXPECT_TRUE(geometry.AddMeshInstance(instance));
        instance.batch = DebugBatch::Overlay;
        EXPECT_TRUE(geometry.AddMeshInstance(instance));
        EXPECT_FALSE(geometry.AddMeshInstance(instance)); // full: dropped and counted
        EXPECT_EQ(scope.Count(), 0u);
    }
    EXPECT_FALSE(geometry.IsEmpty());
    ASSERT_EQ(geometry.MeshInstances().size(), 2u);
    EXPECT_EQ(geometry.MeshInstances()[1].batch, DebugBatch::Overlay);
    EXPECT_EQ(geometry.DroppedMeshInstances(), 1u);
    geometry.Clear();
    EXPECT_TRUE(geometry.IsEmpty());
    EXPECT_EQ(geometry.DroppedMeshInstances(), 0u);
}

TEST(RenderFrame, CarriesThePhysicsLayersGeometryApartAndTheExchangeItsMeshes) {
    RenderFrameExchange exchange;
    RenderFrame &frame = exchange.WriteFrame();
    frame.physics.Clear();
    frame.debug.Clear();
    EXPECT_TRUE(
        frame.physics.AddLine(DebugBatch::WorldDepth, Vec3(0.0f), Vec3(1.0f), kDebugPhysics));
    exchange.Meshes().Put(DebugMeshId(1), Triangle(1.0f));
    frame.physics.AddMeshInstance({DebugMeshId(1), DebugBatch::WorldDepth, Vec3(0.0f), {}});
    exchange.Publish();
    const RenderFrame *read = exchange.ReadNewest();
    ASSERT_NE(read, nullptr);
    EXPECT_TRUE(read->debug.IsEmpty());
    EXPECT_EQ(read->physics.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines), 2u);
    EXPECT_EQ(read->physics.MeshInstances().size(), 1u);
    EXPECT_TRUE(exchange.Meshes().Contains(read->physics.MeshInstances()[0].mesh));
}
