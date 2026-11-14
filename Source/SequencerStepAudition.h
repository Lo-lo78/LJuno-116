// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>

namespace ljuno
{
// A bounded and non-cyclic preview of ONE sequencer lane. Zero-based steps.
enum class StepAuditionMode { current = 0, startToCurrent = 1, currentToEnd = 2 };
struct StepAuditionRange { int first, last; };
inline StepAuditionRange makeStepAuditionRange (int startStep, int endStep,
                                                int cursor, StepAuditionMode mode) noexcept
{
    const int first = std::clamp (startStep - 1, 0, 127);
    const int last = std::clamp (endStep - 1, first, 127);
    const int current = std::clamp (cursor, first, last);
    switch (mode)
    {
        case StepAuditionMode::startToCurrent: return { first, current };
        case StepAuditionMode::currentToEnd: return { current, last };
        default: return { current, current };
    }
}
// The audio-thread command is a single atomic word: lane (2 bits), start/end
// (7 bits each) and a nonzero validity marker. No dynamic allocation or lock.
inline int packStepAudition (int lane, StepAuditionRange r) noexcept
{
    return 1 + (std::clamp (lane, 0, 2))
             + (std::clamp (r.first, 0, 127) << 2)
             + (std::clamp (r.last, 0, 127) << 9);
}
inline int unpackStepAuditionLane (int command) noexcept { return (command - 1) & 3; }
inline StepAuditionRange unpackStepAuditionRange (int command) noexcept
{
    const int bits = command - 1;
    return { (bits >> 2) & 127, (bits >> 9) & 127 };
}
}
