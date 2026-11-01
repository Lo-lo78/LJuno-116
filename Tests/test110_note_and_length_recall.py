#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""TEST110: per-step Note/Length recall, save migration, accessible Enter."""
import pathlib
import unittest
root = pathlib.Path(__file__).resolve().parents[1]
def read(f): return (root / f).read_text(encoding='utf-8')
class Test110(unittest.TestCase):
    def test_maintains_separate_recall_values(self):
        h = read('Source/SequencerState.h')
        s = read('Source/SequencerState.cpp')
        self.assertIn('int note = 0;', h)
        self.assertIn('int lastActiveNote = 60;', h)
        self.assertIn('std::atomic<int> lastActiveNote { 60 };', h)
        self.assertIn('int lastNormalLength = 50;', h)
        self.assertIn('std::atomic<int> lastNormalLength { 50 };', h)
        self.assertIn('if (newNote > 0)', s)
        self.assertIn('step.lastActiveNote.store (newNote', s)
        self.assertIn('if (newLength != 101)', s)
        self.assertEqual(s.count('storeRelaxed (step.lastActiveNote, 60);'), 3)
    def test_ui_toggle_tracks_individual_pitch_and_length(self):
        e = read('Source/PluginEditor.cpp')
        self.assertIn('current.note == 0 ? current.lastActiveNote : 0',e)
        self.assertIn('step.length == 101 ? step.lastNormalLength : 101',e)
        self.assertIn('for (int step : indices)',e)
        self.assertIn('processor.commitSequencerEditSnapshot (std::move (before));',e)
        self.assertNotIn('juce::roundToInt (previous) == 0 ? 60.0f : 0.0f',e)
    def test_copy_undo_preset_and_legacy_versions(self):
        s=read('Source/SequencerState.cpp')
        for txt in ('result.lastActiveNote = loadRelaxed (step.lastActiveNote)',
                    'replacement.lastActiveNote',
                    'sourceStep.lastActiveNote',
                    'stream.writeInt (7);',
                    'step.lastActiveNote)))',
                    'formatVersion < 1 || formatVersion > 7',
                    'formatVersion >= 7',
                    'savedNote > 0 ? savedNote : 60',
                    'formatVersion >= 6 ? maximumSequences',
                    'formatVersion >= 4 ? previousPatternSequenceCount'):
            self.assertIn(txt,s)
    def test_ci_and_documentation(self):
        w=read('.github/workflows/build-windows.yml')
        self.assertIn('python Tests/test110_note_and_length_recall.py',w)
        self.assertIn('TEST113-MIDI-Step-Entry',w)
        self.assertIn('TEST110',read('README.md'))
        self.assertIn('TEST110',read('Help/LJuno116Help.html'))
if __name__=='__main__': unittest.main()
