#!/usr/bin/env python3
"""TEST101: step vs current-pattern clipboard wiring, regression checks (not a C++ build)."""
from pathlib import Path
import unittest
R = Path(__file__).resolve().parents[1]
UI = (R/'Source/PluginEditor.cpp').read_text(encoding='utf-8')
UH = (R/'Source/PluginEditor.h').read_text(encoding='utf-8')
P = (R/'Source/PluginProcessor.cpp').read_text(encoding='utf-8')
PH = (R/'Source/PluginProcessor.h').read_text(encoding='utf-8')
S = (R/'Source/SequencerState.cpp').read_text(encoding='utf-8')
SH = (R/'Source/SequencerState.h').read_text(encoding='utf-8')

class Clipboard101(unittest.TestCase):
    def test_shortcuts_separated_and_editor_only(self):
        code = UI.split('if (sequencerEditorOpen\n        && (key.getModifiers().isCtrlDown()',1)[1].split("if (shortcut == 'z')",1)[0]
        self.assertIn('if (shortcut == \'c\' && ! sequencerParameterPickerOpen)',code)
        for k,step,pattern in [('c','copySequencerSteps (false)','copyCurrentSequencer()'),
                               ('x','copySequencerSteps (true)','cutCurrentSequencer()'),
                               ('v','pasteSequencerSteps()','pasteCurrentSequencer()')]:
            b=code.split("if (shortcut == '"+k+"'",1)[1]
            nextblocks=[b.find("if (shortcut == '"+z+"'") for z in 'cxvz' if b.find("if (shortcut == '"+z+"'")>0]
            b=b[:min(nextblocks)] if nextblocks else b
            self.assertIn('key.getModifiers().isShiftDown()',b)
            self.assertIn(step,b)
            self.assertIn('processor.'+pattern,b)
        self.assertIn('Ctrl+A Select visible 16 steps',UI)
        self.assertIn('Ctrl+Shift+C/X/V Current Pattern',UI)
    def test_step_clipboard_preserves_locks(self):
        for a in ['std::vector<ljuno::SequencerStep> sequencerStepClipboard',
                  'std::vector<int> selectedSequencerStepIndices() const',
                  'void copySequencerSteps (bool cut)', 'void pasteSequencerSteps()']:
            self.assertIn(a,UH)
        self.assertIn('processor.getSequencerStep (lane, index)',UI)
        self.assertIn('processor.replaceSequencerStep (lane, insertion + i,',UI)
        self.assertIn('processor.resetSequencerStep (lane, index)',UI)
        self.assertIn('processor.captureSequencerEditSnapshot()',UI)
        self.assertIn('processor.commitSequencerEditSnapshot (std::move (before))',UI)
        self.assertIn('replaceStep (int sequence, int step, const SequencerStep&)',SH)
        body = S.split('void SequencerState::replaceStep',1)[1].split('void SequencerState::resetSequence',1)[0]
        for field in ['note','length','velocity','repeat','shift','parameterLockCount','parameterLocks']:
            self.assertIn(field,body)
        self.assertIn('storeRelaxed (step.parameterLockCount, count);',body)
        self.assertIn('last - insertion + 1',UI)
    def test_pattern_clipboard_is_one_pattern_and_cross_instance(self):
        self.assertIn('LJUNO116-PATTERN-V1:',P)
        self.assertIn('juce::SystemClipboard::copyTextToClipboard',P)
        self.assertIn('juce::SystemClipboard::getTextFromClipboard()',P)
        self.assertIn('staged->copySequenceFrom (sequencerState,',P)
        self.assertIn('getActiveSequenceSlot (getSelectedSequencerIndex()), 0',P)
        self.assertIn('sequencerState.copySequenceFrom (*staged, 0, getActiveSequenceSlot (destination))',P)
        self.assertNotIn('sequencerClipboardValid',PH)
        self.assertIn('if (! staged->restoreFromBase64',P)
        self.assertIn('sequencerState.resetSequence (getActiveSequenceSlot (sequence))',P)
    def test_selection_and_polyphony_do_not_leak(self):
        self.assertIn('sequencerEditorSelectedSteps.fill (false); // Do not edit steps selected in the previous pattern.',UI)
        self.assertIn('sequencerEditorSelectedSteps.fill (false); // Selection never leaks to another lane.',UI)
        copy = S.split('void SequencerState::copySequenceFrom',1)[1].split('juce::String SequencerState::serialiseToBase64',1)[0]
        self.assertNotIn('storeRelaxed (sequences[0].midiInputPolyphony',copy)
        self.assertIn('sharedPolyphony',P)
    def test_no_parameter_id_changes(self):
        self.assertIn('Pattern " << processor.getSequencerPattern',UI)
        self.assertIn('maximumSequences = trackCount * patternsPerTrack',SH)
        self.assertIn('bool LJuno116AudioProcessor::pasteCurrentSequencer()',P)

if __name__ == '__main__':
    unittest.main()
