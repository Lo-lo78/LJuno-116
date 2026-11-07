#!/usr/bin/env python3
"""TEST107: 64 x 3 sequencer patterns, legacy save boundaries and Song/Alt+Q wiring.
This validates source contracts; no JUCE build or audible DAW verification.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
def read(p): return (ROOT / p).read_text(encoding='utf-8')

class Patterns64(unittest.TestCase):
    def test_layout_keeps_all_original_slots(self):
        state = read('Source/SequencerState.h')
        code = read('Source/SequencerState.cpp')
        self.assertIn('patternsPerTrack = 64', state)
        self.assertIn('legacySequenceCount = 16', state)
        self.assertIn('previousPatternSequenceCount = trackCount * 16', state)
        self.assertIn('maximumSequences = trackCount * patternsPerTrack', state)
        self.assertIn('lane + trackCount * (getSelectedPattern (lane) - 1)', code)
        slot = lambda lane, pattern: lane + 3 * (pattern-1)
        self.assertEqual({slot(l,p) for l in range(3) for p in range(1,17)},set(range(48)))
        self.assertEqual({slot(l,p) for l in range(3) for p in range(1,65)},set(range(192)))

    def test_old_and_new_save_versions_have_explicit_counts(self):
        code = read('Source/SequencerState.cpp')
        self.assertIn('stream.writeInt (7);', code)
        self.assertIn('formatVersion < 1 || formatVersion > 7', code)
        self.assertIn('formatVersion >= 6 ? maximumSequences', code)
        self.assertIn('formatVersion >= 4 ? previousPatternSequenceCount', code)
        self.assertIn(': legacySequenceCount;', code)
        self.assertIn('reset();',code)
        self.assertIn('formatVersion >= 5',code)  # length-101 recall not lost
        self.assertIn('step.parameterLockCount.store (count',code)

    def test_exact_vst_id_range_in_generator_and_generated_catalogue(self):
        gen = read('Tools/generate_parameters.py')
        cpp = read('Source/GeneratedParameters.h')
        for n in (443,444,445):
            self.assertIn(f'slider{n}:1<1,64,1>',gen)
            self.assertRegex(cpp,rf'\{{ {n}, "slider{n}", [^\n]+1\.0f, 64\.0f, 1\.0f, 1\.0f')
        # TEST122 only appends new IDs; the original sequencer IDs are unchanged.
        self.assertIn('slider446:',gen)
        self.assertIn('slider451:',gen)

    def test_pattern_shortcuts_and_song_patterns_64(self):
        editor = read('Source/PluginEditor.cpp')
        proc = read('Source/PluginProcessor.cpp')
        song = read('Source/SongEditor.cpp')
        arrangement = read('Source/SongArrangement.cpp')
        self.assertIn('juce::jlimit (1, ljuno::SequencerState::patternsPerTrack, previous + direction)',editor)
        self.assertIn('juce::jlimit (1, ljuno::SequencerState::patternsPerTrack, oldPattern',editor)
        self.assertIn('juce::jlimit (1, ljuno::SequencerState::patternsPerTrack, pattern)',proc)
        self.assertIn('juce::jlimit (0, ljuno::SequencerState::patternsPerTrack - 1, old +',song)
        self.assertIn('value > ljuno::SequencerState::patternsPerTrack',song)
        self.assertIn('p >= SequencerState::patternsPerTrack',arrangement)
        self.assertIn('juce::jlimit (0, SequencerState::patternsPerTrack - 1, pattern)',arrangement)
        self.assertIn('out.writeByte (static_cast<char> (p))',arrangement)

    def test_preserves_song_independent_lanes_and_old_format(self):
        song = read('Source/SongArrangement.cpp')
        engine= read('Source/SynthEngine.cpp')
        self.assertIn('out.writeInt (1);',song) # same byte format; 0..63 fits byte
        self.assertIn('stream.readInt() != 1',song)
        self.assertIn('songPlaybackSlots[index] = lane + SequencerState::trackCount *',engine)
        self.assertIn('songPlaybackSlots[static_cast<size_t> (sequenceIndex)]',engine)
        self.assertIn('const auto pattern = sequencerState.getSelectedPattern (i);',engine)

if __name__ == '__main__': unittest.main()
