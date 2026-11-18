#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "Entity.hpp"
#include "EntityQuery.hpp"
#include "EntityWorld.hpp"
#include "PerWorker.hpp"
#include "SimulationRuntime.hpp"
#include "WorkerPool.hpp"

// Work inside a phase split across the simulation's worker pool (SIM-3; RunicDocs › Simulation core
// clock and threads › Threading model: "the work inside a phase is split across workers, and
// nobody starts the next phase until every worker finishes the current one").
//
// ParallelEach runs a system's per-entity work over a query's entities in chunks across the pool,
// and returns when every chunk has run: its end is a barrier. The chunks depend only on how many
// entities match and on the chunk size, never on the worker count, and the entities are visited in
// the query's order. A chunk's body writes only the components of the entity it is given; what it
// does to anything else (damage to another entity, an event, a spawn) goes into a ParallelOutput,
// which keeps one buffer per worker without a lock and hands everything over in chunk order when
// the system merges it on the driving thread. So the result is the same, to the bit, with one
// worker and with many: nothing depends on which worker ran which chunk, or when.
//
// While chunks run nothing may change the world's shape (the loop holds every such change back,
// as a phase does) and no chunk may call the world, the event hub or the runtime: the world
// belongs to the driving thread, and a chunk reaches only the component values handed to it.

namespace Runic::Simulation {

// Entities in one chunk when a system has no reason to choose: big enough that handing a chunk to
// a worker costs little next to its work, small enough that a few thousand entities spread across
// every worker.
inline constexpr std::uint32_t kDefaultEntitiesPerChunk = 256;

// Where a chunk's body runs.
struct ParallelChunk {
    // The runtime's parallel loops are numbered in the order they run, from 0.
    std::uint64_t loop = 0;
    // The chunk's place among its loop's chunks, and its entities' positions in the loop
    // [begin, end).
    std::uint32_t chunk = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    // The worker running it: two chunks that run at the same time never share one.
    Core::WorkerIndex worker = Core::kDrivingWorker;
};

namespace Detail {

// A run of entities stored together, gathered on the driving thread before the chunks run.
struct EntityRun {
    std::uint32_t first = 0; // the position of its first entity in the loop
    std::uint32_t count = 0;
    const ECS::Entity *entities = nullptr;
    std::array<void *, ECS::kMaxQueryTerms> columns{};
};

// The runtime's state for its parallel loops: the runs of the loop that runs (kept between loops,
// so a loop allocates nothing once they have grown), its number and the next one's.
struct ParallelLoops {
    std::vector<EntityRun> runs;
    std::uint64_t current = 0;
    std::uint64_t next = 0;
    bool running = false;
};

struct ParallelAccess {
    // Starts a loop, numbered and with no runs yet: on the driving thread, never inside another
    // loop's chunk (asserted).
    static ParallelLoops &Begin(SimulationRuntime &runtime);
    static void End(SimulationRuntime &runtime) noexcept;
    static bool IsRunning(const SimulationRuntime &runtime) noexcept;
};

template <class... Ts, class Body, std::size_t... Columns>
void VisitRows(const EntityRun &run, std::uint32_t row, std::uint32_t rows,
               const ParallelChunk &chunk, Body &body, std::index_sequence<Columns...>) {
    for (std::uint32_t i = row; i < row + rows; ++i) {
        body(chunk, run.entities[i], static_cast<Ts *>(run.columns[Columns])[i]...);
    }
}

} // namespace Detail

// Calls body(const ParallelChunk &, ECS::Entity, Ts &...) once for every entity of the query, in
// chunks of entitiesPerChunk entities across the runtime's worker pool, and returns when all have
// run. With one worker the chunks run in order on the calling thread. Called on the driving thread:
// in a system, or between steps.
template <class... Ts, class Body>
void ParallelEach(SimulationRuntime &runtime, const ECS::EntityQuery<Ts...> &query,
                  std::uint32_t entitiesPerChunk, Body &&body) {
    RUNIC_ASSERT_MSG(entitiesPerChunk > 0, "a parallel loop needs at least one entity per chunk");
    // Nothing that moves entities applies before every chunk has run: the gathered runs stay put.
    const ECS::EntityWorld::DeferScope hold(runtime.Entities());
    Detail::ParallelLoops &loops = Detail::ParallelAccess::Begin(runtime);
    const std::uint64_t loop = loops.current;
    std::uint32_t total = 0;
    query.EachChunk([&](std::span<const ECS::Entity> entities, std::span<Ts>... values) {
        if (entities.empty()) {
            return;
        }
        Detail::EntityRun run;
        run.first = total;
        run.count = static_cast<std::uint32_t>(entities.size());
        run.entities = entities.data();
        std::size_t column = 0;
        ((run.columns[column++] = const_cast<void *>(static_cast<const void *>(values.data()))),
         ...);
        loops.runs.push_back(run);
        total += run.count;
    });
    const std::vector<Detail::EntityRun> &runs = loops.runs;
    runtime.Workers().ParallelFor(
        total, entitiesPerChunk, [&](const Core::ChunkRange &range, Core::WorkerIndex worker) {
            const ParallelChunk chunk{loop, range.chunk, range.begin, range.end, worker};
            // The run holding the chunk's first entity: the last that starts at or before it.
            auto run = std::upper_bound(runs.begin(), runs.end(), range.begin,
                                        [](std::uint32_t index, const Detail::EntityRun &next) {
                                            return index < next.first;
                                        }) -
                       1;
            for (std::uint32_t index = range.begin; index < range.end; ++run) {
                const std::uint32_t row = index - run->first;
                const std::uint32_t rows = std::min(run->count - row, range.end - index);
                Detail::VisitRows<Ts...>(*run, row, rows, chunk, body,
                                         std::index_sequence_for<Ts...>{});
                index += rows;
            }
        });
    Detail::ParallelAccess::End(runtime);
}

// What the chunks of parallel loops produce for others, kept per worker and handed over in a fixed
// order: by loop, then by chunk, then in the order each chunk pushed. Made at setup, like a query,
// for one runtime; it keeps its memory between merges.
template <class T> class ParallelOutput {
  public:
    explicit ParallelOutput(SimulationRuntime &runtime)
        : m_Runtime(&runtime), m_Buffers(runtime.Workers().WorkerCount()) {}

    ParallelOutput(const ParallelOutput &) = delete;
    ParallelOutput &operator=(const ParallelOutput &) = delete;

    // From a chunk's body: appends to the buffer of the worker running the chunk. No lock.
    void Push(const ParallelChunk &chunk, const T &value) {
        m_Buffers[chunk.worker].push_back(Record{chunk.loop, chunk.chunk, value});
    }
    void Push(const ParallelChunk &chunk, T &&value) {
        m_Buffers[chunk.worker].push_back(Record{chunk.loop, chunk.chunk, std::move(value)});
    }

    // On the driving thread, after the loops: calls apply(T &) for every value pushed since the
    // last merge, in loop order, then chunk order, then push order; then empties the buffers.
    template <class Apply> void Merge(Apply &&apply);

    // Values waiting for the next merge.
    std::size_t Size() const noexcept;

  private:
    struct Record {
        std::uint64_t loop = 0;
        std::uint32_t chunk = 0;
        T value;
    };
    // The records one chunk pushed: contiguous in its worker's buffer, since a chunk runs on one
    // worker from start to end.
    struct Pushed {
        std::uint64_t loop = 0;
        std::uint32_t chunk = 0;
        Core::WorkerIndex worker = Core::kDrivingWorker;
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    SimulationRuntime *m_Runtime;
    Core::PerWorker<std::vector<Record>> m_Buffers;
    std::vector<Pushed> m_Order;
};

template <class T> template <class Apply> void ParallelOutput<T>::Merge(Apply &&apply) {
    RUNIC_ASSERT_MSG(!Detail::ParallelAccess::IsRunning(*m_Runtime),
                     "a parallel output was merged while a parallel loop runs");
    m_Order.clear();
    for (Core::WorkerIndex worker = 0; worker < m_Buffers.Workers(); ++worker) {
        const std::vector<Record> &records = m_Buffers[worker];
        for (std::size_t i = 0; i < records.size(); ++i) {
            const bool sameChunk =
                !m_Order.empty() && m_Order.back().worker == worker && m_Order.back().end == i &&
                m_Order.back().loop == records[i].loop && m_Order.back().chunk == records[i].chunk;
            if (sameChunk) {
                ++m_Order.back().end;
            } else {
                m_Order.push_back(Pushed{records[i].loop, records[i].chunk, worker, i, i + 1});
            }
        }
    }
    std::sort(m_Order.begin(), m_Order.end(), [](const Pushed &x, const Pushed &y) {
        return x.loop != y.loop ? x.loop < y.loop : x.chunk < y.chunk;
    });
    for (const Pushed &pushed : m_Order) {
        std::vector<Record> &records = m_Buffers[pushed.worker];
        for (std::size_t i = pushed.begin; i < pushed.end; ++i) {
            apply(records[i].value);
        }
    }
    for (Core::WorkerIndex worker = 0; worker < m_Buffers.Workers(); ++worker) {
        m_Buffers[worker].clear();
    }
}

template <class T> std::size_t ParallelOutput<T>::Size() const noexcept {
    std::size_t size = 0;
    for (Core::WorkerIndex worker = 0; worker < m_Buffers.Workers(); ++worker) {
        size += m_Buffers[worker].size();
    }
    return size;
}

} // namespace Runic::Simulation
