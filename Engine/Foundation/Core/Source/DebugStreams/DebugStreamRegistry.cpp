// The debug-stream registry (DebugStreamRegistry.hpp): per stream a ring of bytes holding whole
// records, each behind its size, under a short lock.

#include "DebugStreamRegistry.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "Assert.hpp"
#include "Log.hpp"

namespace Runic::Core {

namespace {

// A record is stored as its size, then its bytes.
using RecordSize = std::uint32_t;
constexpr std::size_t kRecordHeaderBytes = sizeof(RecordSize);

} // namespace

// One stream. The description is written once, before the stream is published through the count.
struct DebugStreamRegistry::Stream {
    std::string name;
    std::string help;
    std::atomic<bool> enabled{false};

    // Guarded by lock: the ring and the records it holds, [first, next).
    mutable std::mutex lock;
    std::vector<std::byte> ring;
    std::size_t head = 0;                    // where the oldest record starts
    std::size_t used = 0;                    // bytes of records held
    std::uint64_t first = kFirstDebugRecord; // the oldest record's sequence
    std::uint64_t next = kFirstDebugRecord;  // the sequence the next record gets
    std::uint64_t dropped = 0;
    // Where the record a reader took last starts, so a reader that takes records one after the
    // other does not walk the ring from its start each time. Valid while that record is held.
    mutable std::uint64_t hintSequence = 0;
    mutable std::size_t hintAt = 0;

    void CopyIn(std::size_t at, const void *source, std::size_t bytes) noexcept {
        if (bytes == 0) {
            return; // an empty record has no bytes, and its source may be null
        }
        const std::size_t start = at % ring.size();
        const std::size_t untilEnd = std::min(bytes, ring.size() - start);
        std::memcpy(ring.data() + start, source, untilEnd);
        std::memcpy(ring.data(), static_cast<const std::byte *>(source) + untilEnd,
                    bytes - untilEnd);
    }

    void CopyOut(std::size_t at, void *destination, std::size_t bytes) const noexcept {
        if (bytes == 0) {
            return;
        }
        const std::size_t start = at % ring.size();
        const std::size_t untilEnd = std::min(bytes, ring.size() - start);
        std::memcpy(destination, ring.data() + start, untilEnd);
        std::memcpy(static_cast<std::byte *>(destination) + untilEnd, ring.data(),
                    bytes - untilEnd);
    }

    RecordSize SizeAt(std::size_t at) const noexcept {
        RecordSize size = 0;
        CopyOut(at, &size, kRecordHeaderBytes);
        return size;
    }

    void DropOldest() noexcept {
        const std::size_t stored = kRecordHeaderBytes + SizeAt(head);
        head = (head + stored) % ring.size();
        used -= stored;
        ++first;
    }
};

struct DebugStreamRegistry::State {
    explicit State(std::uint32_t capacity)
        : streams(std::make_unique<Stream[]>(capacity)), capacity(capacity) {}

    std::unique_ptr<Stream[]> streams;
    const std::uint32_t capacity;
    std::atomic<std::uint32_t> count{0}; // streams [0, count) are registered
    std::mutex registration;
    bool reportedFull = false;
};

DebugStreamRegistry::DebugStreamRegistry(std::uint32_t capacity)
    : m_State(std::make_unique<State>(capacity)) {
    RUNIC_ASSERT(capacity > 0);
}

DebugStreamRegistry::~DebugStreamRegistry() = default;

DebugStreamId DebugStreamRegistry::Add(const DebugStreamSpec &spec) {
    RUNIC_ASSERT(!spec.name.empty());
    RUNIC_ASSERT(spec.capacityBytes > kRecordHeaderBytes);
    const std::lock_guard lock(m_State->registration);
    const std::uint32_t count = m_State->count.load(std::memory_order_relaxed);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (m_State->streams[index].name == spec.name) {
            return DebugStreamId(index);
        }
    }
    if (count == m_State->capacity) {
        if (!m_State->reportedFull) {
            m_State->reportedFull = true;
            RUNIC_LOG_ERROR("RUNIC: the debug-stream registry is full ({} streams); '{}' and "
                            "later streams are not kept",
                            m_State->capacity, spec.name);
        }
        return DebugStreamId();
    }
    Stream &stream = m_State->streams[count];
    stream.name = spec.name;
    stream.help = spec.help;
    stream.ring.resize(spec.capacityBytes);
    m_State->count.store(count + 1, std::memory_order_release);
    return DebugStreamId(count);
}

DebugStreamRegistry::Stream *DebugStreamRegistry::StreamAt(DebugStreamId id) const noexcept {
    return id.Index() < m_State->count.load(std::memory_order_acquire)
               ? &m_State->streams[id.Index()]
               : nullptr;
}

void DebugStreamRegistry::SetEnabled(DebugStreamId id, bool enabled) noexcept {
    if (Stream *stream = StreamAt(id)) {
        stream->enabled.store(enabled, std::memory_order_relaxed);
    }
}

bool DebugStreamRegistry::IsEnabled(DebugStreamId id) const noexcept {
    const Stream *stream = StreamAt(id);
    return stream != nullptr && stream->enabled.load(std::memory_order_relaxed);
}

void DebugStreamRegistry::Publish(DebugStreamId id, std::span<const std::byte> record) noexcept {
    Stream *stream = StreamAt(id);
    if (stream == nullptr || !stream->enabled.load(std::memory_order_relaxed)) {
        return;
    }
    const std::size_t stored = kRecordHeaderBytes + record.size();
    const std::lock_guard lock(stream->lock);
    if (stored > stream->ring.size()) {
        ++stream->dropped;
        return;
    }
    while (stream->ring.size() - stream->used < stored) {
        stream->DropOldest();
    }
    const auto size = static_cast<RecordSize>(record.size());
    const std::size_t at = stream->head + stream->used;
    stream->CopyIn(at, &size, kRecordHeaderBytes);
    stream->CopyIn(at + kRecordHeaderBytes, record.data(), record.size());
    stream->used += stored;
    ++stream->next;
}

void DebugStreamRegistry::Publish(DebugStreamId id, std::string_view text) noexcept {
    Publish(id, std::as_bytes(std::span<const char>(text.data(), text.size())));
}

DebugRecordRead DebugStreamRegistry::Read(DebugStreamId id, std::uint64_t cursor,
                                          std::span<std::byte> buffer) const noexcept {
    DebugRecordRead read;
    const Stream *stream = StreamAt(id);
    if (stream == nullptr) {
        return read;
    }
    const std::lock_guard lock(stream->lock);
    if (cursor >= stream->next) {
        return read;
    }
    // Walk from the oldest record to the one asked for; a cursor older than that reads the
    // oldest and learns how many it missed.
    const std::uint64_t sequence = std::max(cursor, stream->first);
    std::uint64_t walked = stream->first;
    std::size_t at = stream->head;
    if (stream->hintSequence >= stream->first && stream->hintSequence <= sequence) {
        walked = stream->hintSequence;
        at = stream->hintAt;
    }
    for (; walked < sequence; ++walked) {
        at = (at + kRecordHeaderBytes + stream->SizeAt(at)) % stream->ring.size();
    }
    stream->hintSequence = sequence;
    stream->hintAt = at;
    read.found = true;
    read.sequence = sequence;
    read.missed = sequence - std::max(cursor, kFirstDebugRecord);
    read.bytes = stream->SizeAt(at);
    stream->CopyOut(at + kRecordHeaderBytes, buffer.data(),
                    std::min<std::size_t>(read.bytes, buffer.size()));
    return read;
}

std::uint32_t DebugStreamRegistry::Count() const noexcept {
    return m_State->count.load(std::memory_order_acquire);
}

DebugStreamId DebugStreamRegistry::IdAt(std::uint32_t index) const noexcept {
    return index < Count() ? DebugStreamId(index) : DebugStreamId();
}

DebugStreamId DebugStreamRegistry::Find(std::string_view name) const noexcept {
    const std::uint32_t count = Count();
    for (std::uint32_t index = 0; index < count; ++index) {
        if (m_State->streams[index].name == name) {
            return DebugStreamId(index);
        }
    }
    return DebugStreamId();
}

DebugStreamInfo DebugStreamRegistry::Describe(DebugStreamId id) const noexcept {
    DebugStreamInfo info;
    const Stream *stream = StreamAt(id);
    RUNIC_ASSERT(stream != nullptr);
    if (stream == nullptr) {
        return info;
    }
    info.name = stream->name;
    info.help = stream->help;
    info.enabled = stream->enabled.load(std::memory_order_relaxed);
    const std::lock_guard lock(stream->lock);
    info.published = stream->next - kFirstDebugRecord;
    info.dropped = stream->dropped;
    return info;
}

} // namespace Runic::Core
