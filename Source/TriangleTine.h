// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

namespace ljuno
{
// A triangle-specific resonant wavefolder. Unlike the sine Metallizer, this
// retains the triangular core and introduces a gently asymmetric (even-
// harmonic) tine component plus a pitched fifth harmonic. The extra partials
// taper out toward Nyquist. No allocation, oscillator state or latency.
// metal uses the existing Metal L1/L2 control, 0..10; at zero the caller's
// original Triangle Classic waveform is returned bit-for-bit.
inline float triangleTine (float triangle, double phase, double increment,
                           float metal) noexcept
{
    if (metal <= 0.0f)
        return triangle;

    const auto amount = std::clamp (metal * 0.1f, 0.0f, 1.0f);
    const auto harmonicGuard = [increment] (double harmonic) noexcept
    {
        return static_cast<float> (std::clamp ((0.46 - increment * harmonic) / 0.14,
                                              0.0, 1.0));
    };

    // Folding gives the triangle two/four extra slopes without turning it into
    // a saw. A frequency-sensitive fold depth avoids extreme high-note aliasing.
    const auto foldDepth = amount * harmonicGuard (6.0);
    const auto secondGain = 0.32f * amount * harmonicGuard (2.0);
    const auto fifthGain = 0.25f * amount * amount * harmonicGuard (5.0);
    if (foldDepth == 0.0f && secondGain == 0.0f && fifthGain == 0.0f)
        return triangle;

    constexpr auto pi = 3.14159265358979323846264338327950288;
    auto output = triangle;
    if (foldDepth > 0.0f)
    {
        const auto folded = static_cast<float> (
            std::sin (0.5 * pi * (1.0 + 2.6 * foldDepth) * triangle));
        const auto foldMix = 0.52f * foldDepth;
        output = triangle * (1.0f - foldMix) + folded * foldMix;
    }

    // Second partial adds the vibrating-metal / electric-tine character.
    // A fifth partial supplies glassy brightness without detuning the pitch.
    const auto angle = 2.0 * pi * phase;
    if (secondGain > 0.0f)
        output += secondGain * static_cast<float> (std::sin (2.0 * angle));
    if (fifthGain > 0.0f)
        output += fifthGain * static_cast<float> (std::sin (5.0 * angle));

    // Lightweight static headroom compensation, scaled by the audible
    // shaping amount. Fully band-limited-out effects must not attenuate the
    // high notes: those should resolve to the original triangle.
    const auto audibleAmount = std::max (foldDepth, secondGain / 0.32f);
    return output / (1.0f + 0.12f * audibleAmount);
}
} // namespace ljuno
