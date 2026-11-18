#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "Math.hpp"

// Little-endian bytes for the cooked formats: a writer that appends and a reader that never reads
// past its end (it fails instead, and stays failed). Private to Runic::Assets.

namespace Runic::Assets::Detail {

inline constexpr unsigned kBitsPerByte = 8;

class ByteWriter {
  public:
    void U8(std::uint8_t value) { m_Bytes.push_back(static_cast<std::byte>(value)); }
    void U16(std::uint16_t value) { Unsigned(value, sizeof(value)); }
    void U32(std::uint32_t value) { Unsigned(value, sizeof(value)); }
    void U64(std::uint64_t value) { Unsigned(value, sizeof(value)); }
    void F32(float value) { U32(std::bit_cast<std::uint32_t>(value)); }
    void F64(double value) { U64(std::bit_cast<std::uint64_t>(value)); }
    void Vec3(const Core::Vec3 &v) {
        F32(v.x);
        F32(v.y);
        F32(v.z);
    }
    void Quat(const Core::Quat &q) {
        F32(q.w);
        F32(q.x);
        F32(q.y);
        F32(q.z);
    }
    std::vector<std::byte> &Bytes() noexcept { return m_Bytes; }

  private:
    void Unsigned(std::uint64_t value, std::size_t bytes) {
        for (std::size_t i = 0; i < bytes; ++i) {
            m_Bytes.push_back(static_cast<std::byte>((value >> (kBitsPerByte * i)) & 0xffu));
        }
    }
    std::vector<std::byte> m_Bytes;
};

class ByteReader {
  public:
    explicit ByteReader(std::span<const std::byte> bytes) noexcept : m_Bytes(bytes) {}

    bool Ok() const noexcept { return m_Ok; }
    std::size_t Remaining() const noexcept { return m_Bytes.size() - m_At; }
    std::size_t Offset() const noexcept { return m_At; }
    // Whether count elements of size bytes are still there (false fails the reader).
    bool Has(std::uint64_t count, std::size_t size) noexcept {
        if (!m_Ok || (size != 0 && count > Remaining() / size)) {
            m_Ok = false;
        }
        return m_Ok;
    }

    std::uint8_t U8() noexcept { return static_cast<std::uint8_t>(Unsigned(sizeof(std::uint8_t))); }
    std::uint16_t U16() noexcept {
        return static_cast<std::uint16_t>(Unsigned(sizeof(std::uint16_t)));
    }
    std::uint32_t U32() noexcept {
        return static_cast<std::uint32_t>(Unsigned(sizeof(std::uint32_t)));
    }
    std::uint64_t U64() noexcept { return Unsigned(sizeof(std::uint64_t)); }
    float F32() noexcept { return std::bit_cast<float>(U32()); }
    double F64() noexcept { return std::bit_cast<double>(U64()); }
    Core::Vec3 Vec3() noexcept {
        const float x = F32();
        const float y = F32();
        const float z = F32();
        return {x, y, z};
    }
    Core::Quat Quat() noexcept {
        const float w = F32();
        const float x = F32();
        const float y = F32();
        const float z = F32();
        return {w, x, y, z};
    }

  private:
    std::uint64_t Unsigned(std::size_t bytes) noexcept {
        if (!Has(1, bytes)) {
            return 0;
        }
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < bytes; ++i) {
            value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(m_Bytes[m_At + i]))
                     << (kBitsPerByte * i);
        }
        m_At += bytes;
        return value;
    }

    std::span<const std::byte> m_Bytes;
    std::size_t m_At = 0;
    bool m_Ok = true;
};

} // namespace Runic::Assets::Detail
