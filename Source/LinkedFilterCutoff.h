// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace ljuno::linkedFilter
{
// LP and HP have different logarithmic ranges. Equal increments of their
// *raw* values would therefore change the frequency ratio. Work in log Hz.
struct Cutoffs { float lp, hp; };
struct Scale
{
    double lpLog, hpLog;
    explicit Scale (double sampleRate = 48000.0)
        : lpLog (std::log2 (std::max (41.0, std::min (18000.0, sampleRate * 0.45)) / 40.0)),
          hpLog (std::log2 (std::max (21.0, std::min (6000.0, sampleRate * 0.45)) / 20.0)) {}
    double span() const { return 2.0 * lpLog + hpLog; }
    float centre (Cutoffs c) const
    {
        return static_cast<float> (std::clamp (((c.lp + 1.0) * lpLog + c.hp * hpLog) / span(), 0.0, 1.0));
    }
    Cutoffs move (Cutoffs c, float target) const
    {
        // Both stop together when either raw cutoff reaches its limit.
        const auto delta = std::clamp ((static_cast<double> (target) - centre (c)) * span() * 0.5,
            std::max ((-1.0 - c.lp) * lpLog, -c.hp * hpLog),
            std::min ((1.0 - c.lp) * lpLog, (1.0 - c.hp) * hpLog));
        return { static_cast<float> (std::clamp (c.lp + delta / lpLog, -1.0, 1.0)),
                 static_cast<float> (std::clamp (c.hp + delta / hpLog, 0.0, 1.0)) };
    }
};
}
