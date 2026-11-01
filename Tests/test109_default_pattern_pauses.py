#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""TEST109: new/reset LJuno patterns start on Pause for all 128 steps.

Source-level checks for GitHub Actions; see Test109DefaultPauses.cpp for
runtime SequencerState tests against the JUCE shim.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
def source(name):
    return (ROOT / name).read_text(encoding='utf-8')

class DefaultPauses(unittest.TestCase):
    def test_note_zero_is_default_for_both_step_structs(self):
        header = source('Source/SequencerState.h')
        self.assertRegex(header, r'int note = 0;')
        self.assertRegex(header, r'std::atomic<int> note \{ 0 \};')
        self.assertNotRegex(header, r'\bnote\s*(?:=|\{)\s*60\b')
        self.assertIn('patternsPerTrack = 64', header)
        self.assertIn('stepsPerSequence = 128', header)

    def test_reset_reset_step_and_reset_sequence_all_use_pauses(self):
        text = source('Source/SequencerState.cpp')
        parts = (
            text.split('void SequencerState::reset()')[1].split('void SequencerState::resetStep')[0],
            text.split('void SequencerState::resetStep')[1].split('void SequencerState::replaceStep')[0],
            text.split('void SequencerState::resetSequence')[1].split('int SequencerState::getRoutingMode')[0],
        )
        for part in parts:
            self.assertIn('storeRelaxed (step.note, 0);', part)
            self.assertNotIn('storeRelaxed (step.note, 60);', part)
            self.assertIn('storeRelaxed (step.lastNormalLength, 50);', part)
        self.assertEqual(text.count('storeRelaxed (step.note, 0);'), 3)

    def test_backspace_resets_to_pause_and_enter_recalls_pitch(self):
        editor = source('Source/PluginEditor.cpp')
        self.assertIn('SequencerLayer::note, 0.0f); // Backspace restores Pause.', editor)
        self.assertIn('current.note == 0 ? current.lastActiveNote : 0', editor)
        self.assertIn('return "Pause";', editor)
        self.assertIn('processor.commitSequencerEditSnapshot (std::move (before));', editor)

    def test_old_stored_notes_are_not_migrated_or_overwritten(self):
        code = source('Source/SequencerState.cpp')
        self.assertIn('stream.writeInt (7);', code)
        self.assertIn('formatVersion < 1 || formatVersion > 7', code)
        self.assertIn('formatVersion >= 6 ? maximumSequences', code)
        self.assertIn('step.note.store (juce::jlimit (0, 128,', code)
        self.assertIn('reset();', code)  # untouched old-format slots become pauses
        self.assertIn('storeRelaxed (destinationStep.note, loadRelaxed (sourceStep.note))', code)

    def test_workflow_checks_new_regression(self):
        workflow = source('.github/workflows/build-windows.yml')
        self.assertIn('python Tests/test109_default_pattern_pauses.py', workflow)
        self.assertIn('python Tests/test109_default_pattern_pauses.py', workflow)

if __name__ == '__main__':
    unittest.main()
