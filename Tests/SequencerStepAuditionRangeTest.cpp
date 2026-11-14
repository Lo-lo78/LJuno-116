// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/SequencerStepAudition.h"
#include <cstdlib>

// Keep checks alive with NDEBUG/MSVC Release, unlike assert().
static void require (bool ok) { if (!ok) std::abort(); }
int main()
{
    using namespace ljuno;
    const auto a = makeStepAuditionRange (5, 21, 12, StepAuditionMode::current);
    require (a.first == 12 && a.last == 12);
    const auto b = makeStepAuditionRange (5, 21, 12, StepAuditionMode::startToCurrent);
    require (b.first == 4 && b.last == 12);
    const auto c = makeStepAuditionRange (5, 21, 12, StepAuditionMode::currentToEnd);
    require (c.first == 12 && c.last == 20);
    require (makeStepAuditionRange (5, 21, 127, StepAuditionMode::current).first == 20);
    require (makeStepAuditionRange (5, 21, -5, StepAuditionMode::current).first == 4);
    for (int lane = 0; lane < 3; ++lane)
        for (auto range : {a,b,c})
        {
            const auto command = packStepAudition (lane, range);
            const auto roundTrip = unpackStepAuditionRange (command);
            require (unpackStepAuditionLane (command) == lane);
            require (roundTrip.first == range.first && roundTrip.last == range.last);
        }
}
