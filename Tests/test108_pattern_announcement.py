#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""TEST108: Ctrl+Up/Down inside Alt+Q announces the same short pattern as outside.

Static source regression; the host screen reader will be verified with REAPER.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
EDITOR = (ROOT / 'Source' / 'PluginEditor.cpp').read_text(encoding='utf-8')
WORKFLOW = (ROOT / '.github/workflows/build-windows.yml').read_text(encoding='utf-8')


class Test108PatternAnnouncement(unittest.TestCase):
    def test_sequencer_editor_uses_short_pattern_announcement(self):
        start = EDITOR.index('const auto oldPattern = processor.getSequencerPattern (track);')
        end = EDITOR.index('if (keyCode == juce::KeyPress::upKey)   { changeSequencerEditorStepValue', start)
        section = EDITOR[start:end]
        self.assertIn('processor.selectSequencerPatternFromEditor (track, newPattern);', section)
        self.assertIn('sequencerEditorSelectedSteps.fill (false)', section)
        self.assertIn('clampSequencerEditorToActiveRange();', section)
        self.assertIn('refreshSequencerEditorPanel (false);', section)
        self.assertRegex(section, re.compile(
            r'announceMessageFrom \(sequencerEditorPanel,\s*'
            r'\(track == 0 \? juce::String \("Layer 1"\)\s*'
            r': track == 1 \? juce::String \("Layer 2"\) : juce::String \("Noise"\)\)\s*'
            r'\+ ", Pattern " \+ juce::String \(newPattern\)\);'
        ))
        self.assertNotIn('"Pattern " + juce::String (newPattern) + " of "', section)
        self.assertNotIn('juce::String (ljuno::SequencerState::patternsPerTrack) + ", "', section)

    def test_outside_editor_keeps_matching_announcement_and_silent_bounds(self):
        start = EDITOR.index('void LJuno116AudioProcessorEditor::changeFocusedPattern')
        end = EDITOR.index('void LJuno116AudioProcessorEditor::switchSpaceTransport', start)
        section = EDITOR[start:end]
        self.assertIn('if (next == previous) return;', section)
        self.assertIn('+ ", Pattern " + juce::String (next)', section)
        self.assertIn('juce::jlimit (1, ljuno::SequencerState::patternsPerTrack, previous + direction)', section)

    def test_actions_runs_announcement_regression(self):
        self.assertIn('python Tests/test107_64_patterns.py', WORKFLOW)
        self.assertIn('python Tests/test108_pattern_announcement.py', WORKFLOW)
        self.assertIn('LJuno-116-0.99.58-TEST123-Independent-Input-Polyphony-Windows-VST3', WORKFLOW)


if __name__ == '__main__':
    unittest.main()
