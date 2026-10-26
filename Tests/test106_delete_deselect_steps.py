# SPDX-License-Identifier: AGPL-3.0-or-later
"""Delete deselects Steps only, preserving Song/Parameter Locks shortcuts."""
from pathlib import Path
import unittest

src = (Path(__file__).resolve().parents[1] / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')

class StepDeleteDeselect(unittest.TestCase):
    def test_clear_only_selection(self):
        keys = src.split('bool LJuno116AudioProcessorEditor::handleSequencerEditorKey (', 1)[1]
        clear = keys.split('// TEST106: Delete cancels', 1)[1].split('if (keyCode == juce::KeyPress::returnKey',1)[0]
        self.assertIn('sequencerEditorSelectedSteps.fill (false);', clear)
        self.assertIn('announceMessageFrom (sequencerEditorPanel, "All steps deselected")', clear)
        self.assertIn('sequencerEditorLastStepKey = -1;', clear)
        self.assertNotIn('processor.setSequencerStepValue', clear)
        self.assertNotIn('removeSequencerEditorParameter', clear)
    def test_lock_delete_follows_deselect(self):
        keys = src.split('bool LJuno116AudioProcessorEditor::handleSequencerEditorKey (', 1)[1]
        self.assertLess(keys.index('// TEST106: Delete cancels'), keys.index('removeSequencerEditorParameter();'))
        self.assertIn('sequencerEditorLayer != ljuno::SequencerLayer::parameters', keys)
    def test_song_and_picker_dispatch_still_wins(self):
        main = src.split('bool LJuno116AudioProcessorEditor::keyPressed (const juce::KeyPress& key,',1)[1]
        self.assertLess(main.index('if (sequencerEditorOpen && songView)'),
                        main.index('if (sequencerEditorOpen)\n    {\n        auto before'))
        self.assertLess(main.index('if (sequencerParameterPickerOpen)\n    {'),
                        main.index('if (sequencerEditorOpen)\n    {\n        auto before'))

if __name__ == '__main__': unittest.main()
