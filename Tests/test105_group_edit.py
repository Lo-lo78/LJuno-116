# SPDX-License-Identifier: AGPL-3.0-or-later
"""Regression: selected Steps are edited relatively; Parameter Locks stay individual."""
from pathlib import Path
import unittest
p = (Path(__file__).resolve().parents[1] / 'Source' / 'PluginEditor.cpp').read_text()

class MultiStepEditing(unittest.TestCase):
    def test_explicit_selection_does_not_force_include_cursor(self):
        section = p.split('std::vector<int> LJuno116AudioProcessorEditor::selectedSequencerStepIndices() const', 1)[1].split('void LJuno116AudioProcessorEditor::copySequencerSteps', 1)[0]
        self.assertIn('if (result.empty())', section)
        self.assertNotIn('std::find (result.begin()', section)
    def test_enter_toggles_each_selected_note_with_one_undo(self):
        section = p.split('sequencerEditorLayer == ljuno::SequencerLayer::note)\n    {',1)[1].split('if (keyCode == juce::KeyPress::deleteKey',1)[0]
        self.assertIn('for (int step : indices)', section)
        self.assertIn('juce::roundToInt (previous) == 0 ? 60.0f : 0.0f', section)
        self.assertIn('processor.commitSequencerEditSnapshot (std::move (before));', section)
    def test_relative_nudge_preserves_pitches_and_pauses(self):
        section = p.split('void LJuno116AudioProcessorEditor::changeSequencerEditorStepValue',1)[1].split('void LJuno116AudioProcessorEditor::changeSequencerEditorLayer',1)[0]
        self.assertIn('processor.addSequencerStepDelta (sequence, step, sequencerEditorLayer, delta);',section)
        self.assertIn('juce::roundToInt (old) == 0',section)
        self.assertIn('processor.commitSequencerEditSnapshot (std::move (undoBefore));',section)
        def relative_move(notes, semitones):
            # Pitch zero means Pause in LJuno and must not be transposed.
            return [min(127, max(1, n + semitones)) if n else 0 for n in notes]
        self.assertEqual(relative_move([51, 55, 56], 1), [52, 56, 57])
        self.assertEqual(relative_move([51, 0, 56], 2), [53, 0, 58])
        self.assertEqual(relative_move([51, 55, 56], -2), [49, 53, 54])
    def test_parameter_locks_not_bulk_edited(self):
        section = p.split('void LJuno116AudioProcessorEditor::changeSequencerEditorStepValue',1)[1].split('const auto indices = selectedSequencerStepIndices();',1)[0]
        self.assertIn('if (sequencerEditorLayer == ljuno::SequencerLayer::parameters)', section)

if __name__ == '__main__': unittest.main()
