// SPDX-License-Identifier: AGPL-3.0-or-later
// Offline, JUCE-independent impulse/parameter regression for TEST122.
#include "../Source/SamplerRoomReverb.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

int main()
{
    ljuno::SamplerRoomReverb r;
    r.prepare (48000.0);
    for (int mode = 1; mode <= 4; ++mode)
    {
        r.reset();
        ljuno::SamplerRoomReverb::Settings settings;
        settings.mode = mode;
        r.configure (settings);
        double energy = 0.0;
        float maximum = 0.0f;
        for (int frame = 0; frame < 96000; ++frame)
        {
            float left = 0.0f, right = 0.0f;
            r.process (frame == 0 ? 1.0f : 0.0f, 0.0f, left, right);
            assert (std::isfinite (left) && std::isfinite (right));
            energy += double (left) * left + double (right) * right;
            maximum = std::max ({ maximum, std::abs (left), std::abs (right) });
        }
        assert (energy > 0.00001 && energy < 10000.0);
        assert (maximum < 8.0f);
        std::printf ("Mode %d: energy %.6f, max %.6f\n", mode, energy, maximum);
    }
    // Changes to long RT60, size, gate and width should remain finite.
    ljuno::SamplerRoomReverb::Settings extreme;
    extreme.mode = 4;
    extreme.decaySeconds = 20.0f;
    extreme.size = 100.0f;
    extreme.damping = 100.0f;
    extreme.gateThresholdDb = -15.0f;
    extreme.gateDecayMs = 20.0f;
    extreme.width = 0.0f;
    r.configure (extreme);
    for (int frame = 0; frame < 96000; ++frame)
    {
        float left = 0.0f, right = 0.0f;
        r.process (frame % 8000 == 0 ? .3f : 0.0f, 0.0f, left, right);
        assert (std::isfinite (left) && std::isfinite (right));
        assert (std::abs (left - right) < 0.0001f); // width=0: mono
    }
    std::puts ("SamplerRoomReverb TEST122 passed");
}
