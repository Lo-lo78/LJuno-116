// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace ljuno
{
// TEST133: Chip registers belong to a physical noise generator, not to a
// musical note/envelope. Keep one instance per voice register in SynthEngine:
// replacing, stealing or idling a Voice must not restart the SID/AY sequence.
// No allocation, random calls or background ticking are required.
struct ChipNoiseState
{
    std::uint32_t sidLeft = 0x7fffffu, sidRight = 0x5a5a5au;
    std::uint32_t ayLeft = 0x1ffffu, ayRight = 0x15555u;
    double sidClock = 0.0, ayClock = 0.0;
    float sidSampleLeft = 0.0f, sidSampleRight = 0.0f;
    float aySampleLeft = 0.0f, aySampleRight = 0.0f;

    // Exactly the old tick cap, fractional accumulator and output mapping.
    // A separate accumulator for each vintage chip also prevents switching
    // noise type from forcing the other chip to retrigger.
    static int advanceClock (double& clock, double increment) noexcept
    {
        clock += std::clamp (increment, 0.0, 8.0);
        const auto ticks = static_cast<int> (clock);
        clock -= ticks;
        return std::clamp (ticks, 0, 16);
    }

    static float tickSid (std::uint32_t& state) noexcept
    {
        state &= 0x7fffffu;
        if (state == 0)
            state = 0x7fffffu;
        const auto feedback = ((state >> 17u) ^ (state >> 22u)) & 1u;
        state = ((state << 1u) & 0x7fffffu) | feedback;
        const auto output = ((state >> 22u) & 1u) * 128u
                          + ((state >> 20u) & 1u) * 64u
                          + ((state >> 16u) & 1u) * 32u
                          + ((state >> 13u) & 1u) * 16u
                          + ((state >> 11u) & 1u) * 8u
                          + ((state >> 7u) & 1u) * 4u
                          + ((state >> 4u) & 1u) * 2u
                          + ((state >> 2u) & 1u);
        return static_cast<float> (output) / 127.5f - 1.0f;
    }

    static float tickAy (std::uint32_t& state) noexcept
    {
        state &= 0x1ffffu;
        if (state == 0)
            state = 0x1ffffu;
        const auto output = (state & 1u) != 0u ? 1.0f : -1.0f;
        const auto feedback = ((state >> 0u) ^ (state >> 3u)) & 1u;
        state = (state >> 1u) | (feedback << 16u);
        return output;
    }

    std::array<float, 2> renderSid (double pitchClockIncrement) noexcept
    {
        auto ticks = advanceClock (sidClock, pitchClockIncrement);
        while (ticks-- > 0)
        {
            sidSampleLeft = tickSid (sidLeft);
            sidSampleRight = tickSid (sidRight);
        }
        return { sidSampleLeft, sidSampleRight };
    }

    std::array<float, 2> renderAy (double pitchClockIncrement) noexcept
    {
        auto ticks = advanceClock (ayClock, pitchClockIncrement);
        while (ticks-- > 0)
        {
            aySampleLeft = tickAy (ayLeft);
            aySampleRight = tickAy (ayRight);
        }
        return { aySampleLeft, aySampleRight };
    }
};
}
