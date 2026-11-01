// SPDX-License-Identifier: AGPL-3.0-or-later
// TEST110 runtime sequencer test using minimal JUCE shim; not a VST3 build.
#include "../Source/SequencerState.h"
#include <cassert>
#include <iostream>
#include <memory>

using namespace ljuno;

static void toggleNote (SequencerState& state, int sequence, int index)
{
    const auto s = state.getStep (sequence, index);
    state.setStepValue (sequence, index, SequencerLayer::note,
                        static_cast<float> (s.note == 0 ? s.lastActiveNote : 0));
}
static void toggleLength (SequencerState& state, int sequence, int index)
{
    const auto s = state.getStep (sequence, index);
    state.setStepValue (sequence, index, SequencerLayer::length,
                        static_cast<float> (s.length == 101 ? s.lastNormalLength : 101));
}

// Produce a real LSQ2 v6 save (without the new lastActiveNote byte) to test
// migration from TEST109, including all 192*128 steps and variable lock lists.
static juce::String makeLegacyV6 (const SequencerState& state)
{
    juce::MemoryOutputStream out;
    out.writeInt (0x4c535132);
    out.writeInt (6);
    out.writeInt (state.getRoutingMode());
    out.writeInt (state.getSelectedSequence());
    for (int i = 0; i < 3; ++i) out.writeInt (state.getSelectedPattern (i));
    for (int seq = 0; seq < SequencerState::maximumSequences; ++seq)
    {
        const auto c = state.getConfig (seq);
        out.writeInt (c.startStep);
        out.writeInt (c.endStep);
        out.writeInt (c.playbackMode);
        out.writeFloat (c.bpmDivision);
        out.writeFloat (c.shuffle);
        out.writeFloat (c.noteSkipProbability);
        out.writeFloat (c.noteLengthRandomDepth);
        out.writeBool (c.legato);
        out.writeFloat (c.noteRandomDepth);
        out.writeFloat (c.velocityRandomDepth);
        out.writeFloat (c.repeatRandomDepth);
        out.writeInt (c.octaveShift);
        out.writeInt (c.semitoneShift);
        out.writeFloat (c.globalStepLength);
        out.writeFloat (c.globalStepVelocity);
        out.writeInt (c.globalStepRepeat);
        out.writeFloat (c.globalStepShift);
        out.writeInt (c.midiInputMode);
        out.writeInt (c.midiInputPolyphony);
        out.writeInt (c.midiChannel);
        out.writeInt (c.launchStep);
        out.writeFloat (c.launchOffsetMs);
        for (int i = 0; i < SequencerState::stepsPerSequence; ++i)
        {
            const auto s = state.getStep (seq, i);
            out.writeByte (static_cast<char> (s.note));
            out.writeByte (static_cast<char> (s.length));
            out.writeByte (static_cast<char> (s.velocity));
            out.writeByte (static_cast<char> (s.repeat));
            out.writeFloat (s.shift);
            out.writeByte (static_cast<char> (s.lastNormalLength));
            out.writeByte (static_cast<char> (s.parameterLockCount));
            for (int lock = 0; lock < s.parameterLockCount; ++lock)
            {
                out.writeInt (s.parameterLocks[static_cast<std::size_t> (lock)].sliderNumber);
                out.writeFloat (s.parameterLocks[static_cast<std::size_t> (lock)].value);
            }
        }
    }
    return juce::Base64::toBase64 (out.getData(), out.getDataSize());
}

int main()
{
    auto a = std::make_unique<SequencerState>();
    assert (a->getStep (0, 0).note == 0);
    assert (a->getStep (0, 0).lastActiveNote == 60);
    toggleNote (*a, 0, 0);
    assert (a->getStep (0, 0).note == 60);

    // Note and Length memories are independent, also with a paused note.
    a->setStepValue (0, 0, SequencerLayer::note, 72);
    a->setStepValue (0, 0, SequencerLayer::length, 37);
    toggleNote (*a, 0, 0);
    assert (a->getStep (0, 0).note == 0 && a->getStep (0, 0).lastActiveNote == 72);
    toggleLength (*a, 0, 0);
    assert (a->getStep (0, 0).length == 101 && a->getStep (0, 0).lastNormalLength == 37);
    toggleNote (*a, 0, 0);
    toggleLength (*a, 0, 0);
    assert (a->getStep (0, 0).note == 72 && a->getStep (0, 0).length == 37);

    // Different notes and lengths, toggled in a selected group and in P64.
    a->setStepValue (0, 1, SequencerLayer::note, 84);
    a->setStepValue (0, 1, SequencerLayer::length, 61);
    a->setStepValue (191, 127, SequencerLayer::note, 91);
    a->setStepValue (191, 127, SequencerLayer::length, 83);
    for (int i : {0, 1}) { toggleNote (*a, 0, i); toggleLength (*a, 0, i); }
    for (int i : {0, 1}) { toggleNote (*a, 0, i); toggleLength (*a, 0, i); }
    assert (a->getStep (0, 0).note == 72 && a->getStep (0, 0).length == 37);
    assert (a->getStep (0, 1).note == 84 && a->getStep (0, 1).length == 61);

    // Step and pattern copy preserve both memories, even when the note is paused.
    toggleNote (*a, 0, 1);
    toggleLength (*a, 0, 1);
    a->replaceStep (0, 3, a->getStep (0, 1));
    a->copySequenceFrom (*a, 0, 3);
    assert (a->getStep (3, 3).note == 0);
    assert (a->getStep (3, 3).lastActiveNote == 84);
    assert (a->getStep (3, 3).length == 101);
    assert (a->getStep (3, 3).lastNormalLength == 61);
    toggleNote (*a, 3, 3);
    toggleLength (*a, 3, 3);
    assert (a->getStep (3, 3).note == 84 && a->getStep (3, 3).length == 61);

    // New v7 saves and undo snapshots are lossless; note recall persists in Pause.
    auto saved = a->serialiseToBase64();
    auto b = std::make_unique<SequencerState>();
    assert (b->restoreFromBase64 (saved));
    assert (b->getStep (0, 1).note == 0);
    assert (b->getStep (0, 1).lastActiveNote == 84);
    assert (b->getStep (0, 1).lastNormalLength == 61);
    assert (b->getStep (191, 127).note == 91);
    toggleNote (*b, 0, 1);
    toggleLength (*b, 0, 1);
    assert (b->getStep (0, 1).note == 84 && b->getStep (0, 1).length == 61);

    // Load a v6 project, not pretending it has the memory byte.
    const auto legacy = makeLegacyV6 (*a);
    auto old = std::make_unique<SequencerState>();
    assert (old->restoreFromBase64 (legacy));
    assert (old->getStep (0, 0).note == 72 && old->getStep (0, 0).lastActiveNote == 72);
    assert (old->getStep (0, 1).note == 0 && old->getStep (0, 1).lastActiveNote == 60);
    assert (old->getStep (0, 1).length == 101 && old->getStep (0, 1).lastNormalLength == 61);
    assert (old->getStep (191, 127).note == 91 && old->getStep (191, 127).lastActiveNote == 91);
    toggleNote (*old, 0, 1);
    assert (old->getStep (0, 1).note == 60); // v6 Pause has no prior pitch to recover

    // Reset clears both memories to defaults without reactivating anything.
    b->resetStep (0, 1);
    assert (b->getStep (0, 1).note == 0);
    assert (b->getStep (0, 1).lastActiveNote == 60);
    assert (b->getStep (0, 1).lastNormalLength == 50);
    b->resetSequence (191);
    assert (b->getStep (191, 127).note == 0);
    assert (b->getStep (191, 127).lastActiveNote == 60);
    std::cout << "PASS: TEST110 note/length independent recall, copy, v7, legacy v6 and reset\n";
}
