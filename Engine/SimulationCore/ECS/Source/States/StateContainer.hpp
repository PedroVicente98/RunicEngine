#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// The compact per-entity state container: the content flags and the values of one entity. A
// content flag is one bit and a value one number, both found by the slot their declaration was
// given. The container grows to the highest slot its entity uses, so an entity pays for the
// states it has, not for every state the game declares.

namespace Runic::ECS::Detail {

struct StateContainer {
    std::vector<std::uint64_t> flags; // content flags, one bit per slot
    std::vector<std::uint64_t> given; // which values were given, one bit per slot
    std::vector<std::int64_t> values; // by slot

    bool Flag(std::uint32_t slot) const noexcept { return Bit(flags, slot); }
    void SetFlag(std::uint32_t slot, bool value) { SetBit(flags, slot, value); }

    bool HasValue(std::uint32_t slot) const noexcept { return Bit(given, slot); }
    std::int64_t Value(std::uint32_t slot) const noexcept {
        return HasValue(slot) ? values[slot] : kNoValue;
    }
    void SetValue(std::uint32_t slot, std::int64_t value) {
        if (values.size() <= slot) {
            values.resize(static_cast<std::size_t>(slot) + 1, kNoValue);
        }
        values[slot] = value;
        SetBit(given, slot, true);
    }
    void ClearValue(std::uint32_t slot) {
        if (HasValue(slot)) {
            values[slot] = kNoValue;
            SetBit(given, slot, false);
        }
    }

  private:
    static constexpr std::uint32_t kBitsPerWord = 64;
    static constexpr std::int64_t kNoValue = 0;

    static bool Bit(const std::vector<std::uint64_t> &words, std::uint32_t slot) noexcept {
        const std::size_t word = slot / kBitsPerWord;
        return word < words.size() && ((words[word] >> (slot % kBitsPerWord)) & 1u) != 0;
    }

    static void SetBit(std::vector<std::uint64_t> &words, std::uint32_t slot, bool value) {
        const std::size_t word = slot / kBitsPerWord;
        if (words.size() <= word) {
            if (!value) {
                return; // a bit beyond the end is already clear
            }
            words.resize(word + 1);
        }
        const std::uint64_t mask = std::uint64_t{1} << (slot % kBitsPerWord);
        words[word] = value ? (words[word] | mask) : (words[word] & ~mask);
    }
};

} // namespace Runic::ECS::Detail
