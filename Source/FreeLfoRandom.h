// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <cstdint>
#include <limits>

namespace ljuno
{
// A dedicated stream for Free Sample & Hold. Other synth randomness (noise,
// voice drift, effects and Triggered S&H) must not be reseeded on host Play.
// Stable note-dependent streams keep polyphonic Free S&H voices independent
// without coupling them to the noise generator or voice-allocation order.
inline std::uint32_t freeLfoVoiceSeed (std::uint32_t baseSeed, int note) noexcept
{
    const auto noteValue = static_cast<std::uint32_t> (note + 1);
    const auto mixed = baseSeed ^ (noteValue * 0x9e3779b9u);
    return mixed != 0u ? mixed : baseSeed;
}

inline float nextFreeLfoSampleHold (std::uint32_t& rngState) noexcept
{
    auto x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x;
    return static_cast<float> (x) / static_cast<float> (UINT32_MAX) * 2.0f - 1.0f;
}
}
