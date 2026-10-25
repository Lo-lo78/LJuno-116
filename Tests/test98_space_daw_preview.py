#!/usr/bin/env python3
"""TEST98 source-level checks; does not build JUCE or run audio."""
from pathlib import Path
import re
import unittest
ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
H = (ROOT / 'Source/PluginEditor.h').read_text(encoding='utf-8')
DOC = (ROOT / 'README.md').read_text(encoding='utf-8')

class SpaceTransport98(unittest.TestCase):
    def test_instance_only_and_default_daw(self):
        self.assertIn('bool spaceControlsSequencerPreview = false;', H)
        self.assertNotIn('spaceControlsSequencerPreview', (ROOT / 'Source/PluginProcessor.cpp').read_text(encoding='utf-8'))
    def test_shortcut_early_in_routing(self):
        self.assertIn('if (altP && ! modalInput)', CPP)
        self.assertIn('switchSpaceTransport (announcer);', CPP)
        self.assertLess(CPP.index('if (altP && ! modalInput)'), CPP.index('if (presetBrowserOpen)\n    {', CPP.index('bool LJuno116AudioProcessorEditor::keyPressed')))
        self.assertLess(CPP.index('if (altP && ! modalInput)'), CPP.index('if (sequencerParameterPickerOpen)\n    {', CPP.index('bool LJuno116AudioProcessorEditor::keyPressed')))
        self.assertIn('if (! spaceControlsSequencerPreview)\n            return false;', CPP)
        self.assertIn('toggleSequencerPreview (announcer);', CPP)
    def test_one_existing_preview_audio_engine(self):
        self.assertIn('processor.requestSequencerPreview (sequence, sequencerPreviewActive);', CPP)
        self.assertIn('hasSequencerPreviewTarget()', CPP)
    def test_esc_altq_do_not_stop_preview_but_window_close_does(self):
        body = CPP.split('void LJuno116AudioProcessorEditor::closeSequencerEditor()', 1)[1].split('void LJuno116AudioProcessorEditor::selectSequencerEditorStep',1)[0]
        self.assertNotIn('processor.requestSequencerPreview', body)
        destructor = CPP.split('LJuno116AudioProcessorEditor::~LJuno116AudioProcessorEditor()',1)[1].split('void LJuno116AudioProcessorEditor::paint',1)[0]
        self.assertIn('processor.requestSequencerPreview', destructor)
        self.assertIn('if (! spaceControlsSequencerPreview && sequencerPreviewActive)', CPP)
    def test_browser_not_start_on_navigation_and_global_shortcuts(self):
        self.assertIn('presetBrowserOpen ? static_cast<juce::Component&> (presetBrowser)', CPP)
        self.assertIn('spaceControlsSequencerPreview', CPP)
        self.assertNotIn('Space toggles sequence preview', CPP)
    def test_page_navigation_alternative(self):
        self.assertIn('Alt+Shift+P', DOC)
        self.assertIn('Alt+N', DOC)
        self.assertIn('selectRelativePage (-1);', CPP)
    def test_unchanged_test97_bank(self):
        self.assertIn('patternsPerTrack = 16', (ROOT/'Source/SequencerState.h').read_text())
        self.assertIn('"slider445"', (ROOT/'Source/GeneratedPages.h').read_text())

if __name__ == '__main__':
    unittest.main()
