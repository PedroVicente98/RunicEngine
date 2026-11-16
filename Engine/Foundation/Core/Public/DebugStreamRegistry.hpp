#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

// The debug-stream registry: named streams where modules publish traces, each record a run of
// bytes whose meaning the publisher and its tool share. The Diagnostics layer reads the streams
// and sends them to the RunicEditor's live attach (RunicDocs › Modules, work packages › CORE-2,
// NET-7). A stream nobody listens to is disabled, and publishing to it costs one atomic load.
// Like the metrics registry, it is an object the program owns, not a global.

namespace Runic::Core {

// Streams a registry holds unless its constructor is given another capacity.
inline constexpr std::uint32_t kDefaultDebugStreamCapacity = 64;

// The bytes of records a stream keeps unless its spec says otherwise. When they are full the
// oldest records make room for the new one.
inline constexpr std::uint32_t kDefaultDebugStreamBytes = 64 * 1024;

struct DebugStreamSpec {
    std::string_view name; // unique in the registry
    std::string_view help; // one line for the tool that lists streams
    std::uint32_t capacityBytes = kDefaultDebugStreamBytes;
};

// The handle of a registered stream. A default handle, or one a full registry gave, is invalid:
// publishing through it does nothing.
class DebugStreamId {
  public:
    constexpr DebugStreamId() noexcept = default;
    constexpr bool IsValid() const noexcept { return m_Index != kInvalidIndex; }
    constexpr std::uint32_t Index() const noexcept { return m_Index; }

    friend constexpr bool operator==(const DebugStreamId &,
                                     const DebugStreamId &) noexcept = default;

  private:
    friend class DebugStreamRegistry;

    static constexpr std::uint32_t kInvalidIndex = 0xFFFF'FFFF;

    constexpr explicit DebugStreamId(std::uint32_t index) noexcept : m_Index(index) {}

    std::uint32_t m_Index = kInvalidIndex;
};

// The first record a stream ever holds has this sequence; each later record has the next number.
inline constexpr std::uint64_t kFirstDebugRecord = 1;

// What a reader gets from one Read.
struct DebugRecordRead {
    bool found = false;         // false: no record at or after the cursor yet
    std::uint64_t sequence = 0; // the record's sequence; the next cursor is sequence + 1
    std::uint32_t bytes = 0;    // the record's size; more than the buffer means it was cut short
    std::uint64_t missed = 0;   // records between the cursor and this one that had been dropped
};

// A stream as a tool lists it. The texts stay valid while the registry lives.
struct DebugStreamInfo {
    std::string_view name;
    std::string_view help;
    bool enabled = false;
    std::uint64_t published = 0; // records accepted since the registry was created
    std::uint64_t dropped = 0;   // records refused because they were larger than the stream
};

class DebugStreamRegistry {
  public:
    explicit DebugStreamRegistry(std::uint32_t capacity = kDefaultDebugStreamCapacity);
    ~DebugStreamRegistry();

    DebugStreamRegistry(const DebugStreamRegistry &) = delete;
    DebugStreamRegistry &operator=(const DebugStreamRegistry &) = delete;

    // Registration, at start: thread-safe, and it allocates the stream's bytes. Adding a name
    // that exists returns the existing stream. A full registry logs an error and returns an
    // invalid handle. Streams start disabled.
    DebugStreamId Add(const DebugStreamSpec &spec);

    // Turned on by the tool that wants the stream, from any thread.
    void SetEnabled(DebugStreamId id, bool enabled) noexcept;
    // One atomic load: a publisher asks before it builds a costly record.
    bool IsEnabled(DebugStreamId id) const noexcept;

    // Appends a record, from any thread, without allocating. A disabled stream keeps nothing. A
    // record larger than the stream is refused and counted.
    void Publish(DebugStreamId id, std::span<const std::byte> record) noexcept;
    void Publish(DebugStreamId id, std::string_view text) noexcept;

    // Copies the oldest record at or after cursor into buffer. Each reader keeps its own cursor
    // (it starts at kFirstDebugRecord and continues at sequence + 1), so several readers see every
    // record. Thread-safe.
    DebugRecordRead Read(DebugStreamId id, std::uint64_t cursor,
                         std::span<std::byte> buffer) const noexcept;

    std::uint32_t Count() const noexcept;
    DebugStreamId IdAt(std::uint32_t index) const noexcept;
    DebugStreamId Find(std::string_view name) const noexcept;
    DebugStreamInfo Describe(DebugStreamId id) const noexcept;

  private:
    struct Stream;
    struct State;

    Stream *StreamAt(DebugStreamId id) const noexcept;

    std::unique_ptr<State> m_State;
};

} // namespace Runic::Core
