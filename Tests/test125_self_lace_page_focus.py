#!/usr/bin/env python3
"""TEST125: self-Lace backwards compatibility and screen-reader navigation."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
PARAMS = (ROOT / 'Source/GeneratedParameters.h').read_text(encoding='utf-8')
SYNTH = (ROOT / 'Source/SynthEngine.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'Source/SynthEngine.h').read_text(encoding='utf-8')
EDITOR = (ROOT / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
GENERATE = (ROOT / 'Tools/generate_parameters.py').read_text(encoding='utf-8')
WORKFLOW = (ROOT / '.github/workflows/build-windows.yml').read_text(encoding='utf-8')

class SelfLaceAndAccessibility(unittest.TestCase):
    def test_self_lace_both_lfos_preserves_old_choices(self):
        for slider, expected in [(455, 'Off|LFO 2|LFO 1'), (460, 'Off|LFO 1|LFO 2')]:
            line = next(x for x in PARAMS.splitlines() if f'{{ {slider}, ' in x)
            self.assertIn('0.0f, 2.0f, 1.00f, 0.0f', line)
            self.assertIn('"'+expected+'"', line)
            self.assertIn('slider'+str(slider)+':0<0,2,1{'+expected.replace('|',',')+'}>', GENERATE)
        self.assertIn('lfo1LaceChoice == 1 ? 2 : lfo1LaceChoice == 2 ? 1 : 0', SYNTH)
        self.assertIn('p.lfo2.laceSource = juce::roundToInt (value (s, "slider460"))', SYNTH)

    def test_self_lace_reads_previous_sample_not_current(self):
        self.assertIn('const auto previousOwnOutput = state.coreValue;', SYNTH)
        self.assertIn('p.laceSource == ownLfoIndex', SYNTH)
        self.assertIn('? previousOwnOutput : crossSource;', SYNTH)
        self.assertIn('float crossSource, int ownLfoIndex);', HEADER)
        self.assertEqual(SYNTH.count('advanceLfo ('), 5) # definition + two mono + two poly
        for tail in ('lfoState2.coreValue, 1);','lfoState1.coreValue, 2);',
                     'v.pitchEnvelope, cross, 1);', 'v.envelope2, cross, 2);'):
            self.assertIn(tail, SYNTH)
        # Cross Rate must remain the opposite LFO, independently of Lace Source.
        self.assertIn('std::exp2 (crossSource * p.crossRate)', SYNTH)

    def test_ctrl_tab_is_page_navigation_after_modal_shortcuts(self):
        shortcut = 'if (keyCode == juce::KeyPress::tabKey\n        && key.getModifiers().isCtrlDown()'
        self.assertIn(shortcut, EDITOR)
        self.assertIn('selectRelativePage (delta);', EDITOR)
        self.assertLess(EDITOR.index('if (sequencerEditorOpen && songView)'), EDITOR.index(shortcut))
        self.assertLess(EDITOR.index('if (presetBrowserOpen)\n    {'), EDITOR.index(shortcut))
        self.assertIn('|| key.getKeyCode() == juce::KeyPress::tabKey)', EDITOR) # temporary numeric editor
        self.assertIn('requestShortcutFocus (parameterSelector);', EDITOR)
        self.assertIn('if (next == pageSelector.getSelectedId())\n        return;', EDITOR) # silent borders

    def test_open_focus_restores_parameter_not_page(self):
        body = EDITOR.split('void LJuno116AudioProcessorEditor::performInitialFocusTransfer()',1)[1].split('void LJuno116AudioProcessorEditor::moveParameterInGrid',1)[0]
        self.assertIn('auto* handler = parameterSelector.getAccessibilityHandler();', body)
        self.assertIn('parameterSelector.grabKeyboardFocus();', body)
        self.assertIn('parameterSelector.setEntryAccessibility();', body)
        self.assertNotIn('pageSelector.grabKeyboardFocus();', body)
        self.assertIn('rememberCurrentPageAndParameter();', EDITOR.split('LJuno116AudioProcessorEditor::~',1)[1].split('void LJuno116AudioProcessorEditor::paint',1)[0])
        self.assertIn('selectedParameterState (page)', EDITOR)
        self.assertIn('selectedPageState, 0', EDITOR)

    def test_version_and_workflow(self):
        self.assertIn('0.99.61', (ROOT/'CMakeLists.txt').read_text(encoding='utf8'))
        self.assertIn('ljunoVersion = "0.99.61"', EDITOR)
        self.assertIn('TEST126-Accessible-Tab-Cycle-And-Page-Shortcuts', WORKFLOW)
        self.assertIn('python Tests/test125_self_lace_page_focus.py', WORKFLOW)

if __name__=='__main__':
    unittest.main()
