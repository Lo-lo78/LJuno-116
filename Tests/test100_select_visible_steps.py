#!/usr/bin/env python3
"""TEST100 focused 16-step Ctrl+A selection structural regressions."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'Source/PluginEditor.h').read_text(encoding='utf-8')

class SelectVisibleSteps100(unittest.TestCase):
    def test_ctrl_a_is_handled_within_editor_only(self):
        ctrl = CPP.split('if (sequencerEditorOpen\n        && (key.getModifiers().isCtrlDown()',1)[1]
        ctrl = ctrl.split('if (sequencerParameterPickerOpen)',1)[0]
        self.assertIn("if (shortcut == 'a' && ! key.getModifiers().isShiftDown()", ctrl)
        self.assertIn('! sequencerParameterPickerOpen', ctrl)
        self.assertIn('selectVisibleSequencerSteps();', ctrl)
        self.assertIn('void selectVisibleSequencerSteps();', HEADER)

    def test_page_limited_and_active_bounds(self):
        section = CPP.split('void LJuno116AudioProcessorEditor::selectVisibleSequencerSteps()',1)[1]
        section = section.split('void LJuno116AudioProcessorEditor::selectSequencerEditorStep',1)[0]
        self.assertIn('clampSequencerEditorToActiveRange();', section)
        self.assertIn('config.startStep - 1', section)
        self.assertIn('config.endStep - 1', section)
        self.assertIn('sequencerEditorBlock * 16', section)
        self.assertIn('sequencerEditorBlock * 16 + 15', section)
        self.assertIn('sequencerEditorSelectedSteps.fill (false);', section)
        self.assertIn('sequencerEditorSelectedSteps[static_cast<std::size_t> (step)] = true;', section)
        self.assertIn('steps selected', section)

    def test_does_not_change_pattern_or_transport(self):
        self.assertIn('Ctrl+A Select visible 16 steps', CPP)
        self.assertIn('int sequencerEditorBlock = 0;', HEADER)
        self.assertIn('bool sequencerPreviewActive = false;', HEADER)
        self.assertIn('patternsPerTrack = 16', (ROOT / 'Source/SequencerState.h').read_text(encoding="utf-8"))

if __name__ == '__main__':
    unittest.main()
