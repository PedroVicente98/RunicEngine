#pragma once

#include <bit>
#include <cstdint>

// Colors of render data: four 8-bit channels in one 32-bit value whose bytes in memory are red,
// green, blue and alpha, the layout of Godot's vertex colors, so debug vertices reach Godot with no
// conversion (slice 2 decision D5). Read as a number the value is 0xAABBGGRR, hence PackABGR
// (RunicDocs › Debug drawing (Engine)).

namespace Runic::RenderData {

static_assert(std::endian::native == std::endian::little,
              "PackedColor's byte order (red first in memory) assumes a little-endian CPU");

using PackedColor = std::uint32_t;

inline constexpr std::uint8_t kOpaqueAlpha = 255;
inline constexpr std::uint8_t kHalfAlpha = 128;

// Where each channel sits in a PackedColor.
inline constexpr unsigned kRedShift = 0;
inline constexpr unsigned kGreenShift = 8;
inline constexpr unsigned kBlueShift = 16;
inline constexpr unsigned kAlphaShift = 24;
inline constexpr PackedColor kChannelMask = 0xFFu;

constexpr PackedColor PackABGR(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                               std::uint8_t alpha = kOpaqueAlpha) noexcept {
    return (PackedColor{alpha} << kAlphaShift) | (PackedColor{blue} << kBlueShift) |
           (PackedColor{green} << kGreenShift) | (PackedColor{red} << kRedShift);
}

constexpr std::uint8_t RedOf(PackedColor color) noexcept {
    return static_cast<std::uint8_t>((color >> kRedShift) & kChannelMask);
}
constexpr std::uint8_t GreenOf(PackedColor color) noexcept {
    return static_cast<std::uint8_t>((color >> kGreenShift) & kChannelMask);
}
constexpr std::uint8_t BlueOf(PackedColor color) noexcept {
    return static_cast<std::uint8_t>((color >> kBlueShift) & kChannelMask);
}
constexpr std::uint8_t AlphaOf(PackedColor color) noexcept {
    return static_cast<std::uint8_t>((color >> kAlphaShift) & kChannelMask);
}

// The same color with another opacity: fills with opacity keep their meaning's color.
constexpr PackedColor WithAlpha(PackedColor color, std::uint8_t alpha) noexcept {
    return (color & ~(kChannelMask << kAlphaShift)) | (PackedColor{alpha} << kAlphaShift);
}

// The debug palette (RunicDocs › Debug drawing (Engine) › What to visualize): a color means the
// same in every system's debug drawing.

// Predicted or local state: green.
inline constexpr PackedColor kDebugPredicted = PackABGR(80, 220, 120);
// The server's authoritative state: orange.
inline constexpr PackedColor kDebugAuthoritative = PackABGR(255, 176, 48);
// Damage or a harmful volume: red.
inline constexpr PackedColor kDebugHarmful = PackABGR(235, 64, 52);
// Healing or a beneficial volume: cyan.
inline constexpr PackedColor kDebugBeneficial = PackABGR(64, 214, 230);
// Physics bounds and the broadphase: blue.
inline constexpr PackedColor kDebugPhysics = PackABGR(90, 150, 255);
// Queries and rays: magenta.
inline constexpr PackedColor kDebugQuery = PackABGR(230, 80, 230);
// Inactive or past geometry: dim gray.
inline constexpr PackedColor kDebugInactive = PackABGR(110, 110, 110);
// Labels: near white.
inline constexpr PackedColor kDebugText = PackABGR(240, 240, 240);

} // namespace Runic::RenderData
