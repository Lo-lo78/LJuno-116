// SPDX-License-Identifier: AGPL-3.0-or-later
// Standalone SequencerState test (compile against Tests/Shim, not JUCE VST3).
#include "../Source/SequencerState.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace ljuno;
    SequencerState state;
    // Different normal values, same toggle destination, independent recall.
    state.setStepValue (0, 0, SequencerLayer::length, 37);
    state.setStepValue (0, 1, SequencerLayer::length, 86);
    auto toggle = [&] (int index)
    {
        auto s = state.getStep (0, index);
        state.setStepValue (0, index, SequencerLayer::length,
                            static_cast<float> (s.length == 101 ? s.lastNormalLength : 101));
    };
    toggle (0); toggle (1);
    assert (state.getStep (0, 0).length == 101);
    assert (state.getStep (0, 1).length == 101);
    assert (state.getStep (0, 0).lastNormalLength == 37);
    assert (state.getStep (0, 1).lastNormalLength == 86);
    toggle (0); toggle (1);
    assert (state.getStep (0, 0).length == 37);
    assert (state.getStep (0, 1).length == 86);

    // New manual value becomes the next remembered value.
    state.setStepValue (0, 0, SequencerLayer::length, 21);
    toggle (0);
    assert (state.getStep (0, 0).length == 101);
    assert (state.getStep (0, 0).lastNormalLength == 21);

    // Pattern copy, step copy, save/restore and repeat toggle.
    state.replaceStep (0, 2, state.getStep (0, 0));
    assert (state.getStep (0, 2).lastNormalLength == 21);
    state.copySequenceFrom (state, 0, 3);
    assert (state.getStep (3, 2).lastNormalLength == 21);
    const auto bytes = state.serialiseToBase64();
    SequencerState restored;
    assert (restored.restoreFromBase64 (bytes));
    assert (restored.getStep (0, 0).length == 101);
    assert (restored.getStep (0, 0).lastNormalLength == 21);
    restored.setStepValue (0, 0, SequencerLayer::length,
                           static_cast<float> (restored.getStep (0, 0).lastNormalLength));
    assert (restored.getStep (0, 0).length == 21);
    assert (restored.getStep (3, 2).lastNormalLength == 21);

    // Reset all recall values, and note/velocity remain untouched.
    state.resetStep (0, 0);
    assert (state.getStep (0, 0).lastNormalLength == 50);
    assert (state.getStep (0, 0).length == 50);
    assert (state.getStep (0, 0).note == 60);
    assert (state.getStep (0, 0).velocity == 100);
    std::cout << "PASS: 106F per-step Length Enter recall, copying, saving and reset\n";
}
