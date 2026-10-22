// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <cmath>

namespace ljuno
{
// MIDI 60 is the stationary key: at 0 every played note has C4 as its base
// frequency, at 50 there are 50 cents per key, at 100 the original frequency
// is returned *exactly* (no extra floating-point operations on old patches).
// Called on source-local fundamental pitch, before tuning, bend, LFO and FX.
inline double keyboardScaledFrequency (double inputFrequency,
                                        float scaleFraction) noexcept
{
    constexpr double midi60Hz = 261.6255653005986346778499935233;
    if (scaleFraction >= 1.0f || inputFrequency <= 0.0)
        return inputFrequency;
    if (scaleFraction <= 0.0f)
        return midi60Hz;
    return midi60Hz * std::exp2 (static_cast<double> (scaleFraction)
                                  * std::log2 (inputFrequency / midi60Hz));
}
}
