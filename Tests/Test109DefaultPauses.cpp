// SPDX-License-Identifier: AGPL-3.0-or-later
// Standalone SequencerState regression; compile with Tests/Shim (no VST3/JUCE build).
#include "../Source/SequencerState.h"
#include <cassert>
#include <iostream>
#include <memory>

int main()
{
    using namespace ljuno;
    auto state = std::make_unique<SequencerState>();
    assert (SequencerStep{}.note == 0);

    // All 192 newly constructed patterns, including the hidden 17..128 steps,
    // start silent on all three tracks.
    for (int slot = 0; slot < SequencerState::maximumSequences; ++slot)
        for (int step = 0; step < SequencerState::stepsPerSequence; ++step)
            assert (state->getStep (slot, step).note == 0);

    // Reset of an individual step and a whole pattern must restore Pause,
    // with a fresh Length 101 memory; other patterns must not change.
    state->setStepValue (0, 0, SequencerLayer::note, 64);
    state->resetStep (0, 0);
    assert (state->getStep (0, 0).note == 0);
    state->setStepValue (0, 127, SequencerLayer::note, 67);
    state->setStepValue (0, 127, SequencerLayer::length, 37);
    state->setStepValue (3, 0, SequencerLayer::note, 71);
    state->resetSequence (0);
    assert (state->getStep (0, 0).note == 0);
    assert (state->getStep (0, 127).note == 0);
    assert (state->getStep (0, 127).lastNormalLength == 50);
    assert (state->getStep (3, 0).note == 71);

    // Existing musical notes, including C4, must survive save/restore.
    state->setStepValue (190, 127, SequencerLayer::note, 60);
    state->setStepValue (191, 0, SequencerLayer::note, 85);
    const auto saved = state->serialiseToBase64();
    auto reloaded = std::make_unique<SequencerState>();
    assert (reloaded->restoreFromBase64 (saved));
    assert (reloaded->getStep (190, 127).note == 60);
    assert (reloaded->getStep (191, 0).note == 85);
    assert (reloaded->getStep (0, 0).note == 0);
    assert (reloaded->getStep (191, 127).note == 0);

    reloaded->copySequenceFrom (*state, 0, 9);
    assert (reloaded->getStep (9, 127).note == 0);

    reloaded->reset();
    for (int slot = 0; slot < SequencerState::maximumSequences; ++slot)
        for (int step = 0; step < SequencerState::stepsPerSequence; ++step)
            assert (reloaded->getStep (slot, step).note == 0);
    std::cout << "PASS: TEST109 all 192 patterns default/rest as Pause, resets, C4 persistence\n";
}
