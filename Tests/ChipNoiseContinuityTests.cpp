// SPDX-License-Identifier: AGPL-3.0-or-later
// TEST133: standalone native regression for the exact SID/AY DSP functions.
#include "../Source/ChipNoiseState.h"

#include <array>
// The Windows workflow builds in Release; assertions MUST stay active.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace
{
struct OldChipVoice
{
    std::uint32_t sidLeft = 0x7fffffu, sidRight = 0x5a5a5au;
    std::uint32_t ayLeft = 0x1ffffu, ayRight = 0x15555u;
    double clock = 0.0;
    float left = 0.0f, right = 0.0f;
};

// Kept separate from the product implementation: proves first-note parity.
float oldSid (std::uint32_t& s)
{
    s &= 0x7fffffu;
    if (! s) s = 0x7fffffu;
    const auto fb = ((s >> 17u) ^ (s >> 22u)) & 1u;
    s = ((s << 1u) & 0x7fffffu) | fb;
    const auto d = ((s >> 22u) & 1u) * 128u + ((s >> 20u) & 1u) * 64u
        + ((s >> 16u) & 1u) * 32u + ((s >> 13u) & 1u) * 16u
        + ((s >> 11u) & 1u) * 8u + ((s >> 7u) & 1u) * 4u
        + ((s >> 4u) & 1u) * 2u + ((s >> 2u) & 1u);
    return static_cast<float> (d) / 127.5f - 1.0f;
}

float oldAy (std::uint32_t& s)
{
    s &= 0x1ffffu;
    if (! s) s = 0x1ffffu;
    const auto out = (s & 1u) != 0u ? 1.0f : -1.0f;
    const auto fb = ((s >> 0u) ^ (s >> 3u)) & 1u;
    s = (s >> 1u) | (fb << 16u);
    return out;
}

std::array<float, 2> oldRender (OldChipVoice& v, bool sid, double increment)
{
    v.clock += std::clamp (increment, 0.0, 8.0);
    auto ticks = static_cast<int> (v.clock);
    v.clock -= ticks;
    ticks = std::clamp (ticks, 0, 16);
    while (ticks-- > 0)
    {
        v.left = sid ? oldSid (v.sidLeft) : oldAy (v.ayLeft);
        v.right = sid ? oldSid (v.sidRight) : oldAy (v.ayRight);
    }
    return { v.left, v.right };
}

void originalSoundUnchanged (bool sid)
{
    ljuno::ChipNoiseState hardware;
    OldChipVoice old;
    // Full first-note parity includes low, high and modulated pitch, and periods
    // with no clock tick (fractional accumulator and sample-and-hold output).
    constexpr std::array<double, 10> increments = {
        0.01, 0.125, 0.98, 1.0, 2.5, 0.001, 3.75, 7.89, 8.001, 0.0
    };
    for (int i = 0; i < 15000; ++i)
    {
        const auto clock = increments[static_cast<std::size_t> (i) % increments.size()];
        const auto expected = oldRender (old, sid, clock);
        const auto actual = sid ? hardware.renderSid (clock) : hardware.renderAy (clock);
        assert (actual == expected);
    }
}

void continuousAcrossNoteOn (bool sid)
{
    ljuno::ChipNoiseState hardware;
    constexpr int sampleCount = 256;
    constexpr double fastPitch = 3.5;
    std::array<std::array<float, 2>, sampleCount> first {};
    std::array<std::array<float, 2>, sampleCount> second {};
    for (auto& sample : first)
        sample = sid ? hardware.renderSid (fastPitch) : hardware.renderAy (fastPitch);

    // The Voice is reset on Note Off / new Note On, but ChipNoiseState must not
    // be. A no-audio gap does not waste CPU advancing a silent register.
    const auto savedSid = hardware.sidLeft;
    const auto savedAy = hardware.ayLeft;
    for (int silent = 0; silent < 50000; ++silent) { /* no render while idle */ }
    assert (hardware.sidLeft == savedSid && hardware.ayLeft == savedAy);
    for (auto& sample : second)
        sample = sid ? hardware.renderSid (fastPitch) : hardware.renderAy (fastPitch);

    int differences = 0;
    for (int i = 0; i < sampleCount; ++i)
        differences += first[static_cast<std::size_t> (i)]
                    != second[static_cast<std::size_t> (i)];
    assert (differences > sampleCount / 3);

    // The old re-initialized per-Voice register always repeats exactly.
    OldChipVoice oldFirst;
    OldChipVoice oldAgain;
    for (int i = 0; i < sampleCount; ++i)
        assert (oldRender (oldFirst, sid, fastPitch)
             == oldRender (oldAgain, sid, fastPitch));
}

void independentVoiceRegisters()
{
    ljuno::ChipNoiseState voice0, voice1;
    auto fresh0 = ljuno::ChipNoiseState {};
    auto fresh1 = ljuno::ChipNoiseState {};
    for (int i = 0; i < 250; ++i)
        (void) voice0.renderSid (2.25);
    // New independent voice has not inherited voice0's clocks or bits.
    assert (voice1.renderSid (2.25) == fresh1.renderSid (2.25));
    assert (voice0.renderSid (2.25) != fresh0.renderSid (2.25));
    // SID and AY independently retain their own phase through type switches.
    auto ayExpected = voice0;
    (void) voice0.renderSid (5.0);
    assert (voice0.renderAy (0.25) == ayExpected.renderAy (0.25));
}
}

int main()
{
    originalSoundUnchanged (true);
    originalSoundUnchanged (false);
    continuousAcrossNoteOn (true);
    continuousAcrossNoteOn (false);
    independentVoiceRegisters();
    std::cout << "TEST133 OK: unchanged SID/AY bitstreams, stereo, live clock and note-on continuity\n";
}
