#!/usr/bin/env python3
"""TEST126: main Tab loop excludes buttons; Windows Ctrl+Tab can reach page navigation."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
EDITOR=(ROOT/'Source/PluginEditor.cpp').read_text('utf-8')
WORKFLOW=(ROOT/'.github/workflows/build-windows.yml').read_text('utf-8')

class FocusNavigation(unittest.TestCase):
    def test_windows_ctrl_tab_does_not_reject_command_alias(self):
        section=EDITOR.split('// Windows JUCE reports Command as Ctrl',1)[1].split('// Exactly three stops',1)[0]
        self.assertIn('key.getModifiers().isCtrlDown()', section)
        self.assertNotIn('! key.getModifiers().isCommandDown()', section)
        self.assertIn('selectRelativePage (delta);',section)
        self.assertIn('juce::jlimit (1, totalPages, currentPage + delta) == currentPage',section)
        self.assertIn('juce::MessageManager::callAsync',section)
        self.assertIn('safeThis->parameterValue.hideTextBox (false)',section)
        self.assertLess(EDITOR.index('if (sequencerEditorOpen)\n    {',EDITOR.index('bool LJuno116AudioProcessorEditor::keyPressed')), EDITOR.index('// Windows JUCE reports Command as Ctrl'))

    def test_boundary_is_silent_and_bounded(self):
        block=EDITOR.split('void LJuno116AudioProcessorEditor::selectRelativePage',1)[1].split('void LJuno116AudioProcessorEditor::selectPageByInitial',1)[0]
        self.assertIn('juce::jlimit (1, count, pageSelector.getSelectedId() + delta)', block)
        self.assertIn('if (next == pageSelector.getSelectedId())\n        return;', block)
        self.assertLess(block.index('if (next == pageSelector.getSelectedId())'),block.index('announcePage();'))

    def test_main_tab_cycle_is_three_stop_both_directions(self):
        block=EDITOR.split('// Exactly three stops in the MAIN Tab cycle',1)[1].split("if (key.getModifiers().isAltDown()\n        && juce::CharacterFunctions::toLowerCase (key.getTextCharacter()) == 'c'",1)[0]
        self.assertIn('const bool backwards = key.getModifiers().isShiftDown()',block)
        self.assertIn('originatingComponent == &parameterSelector',block)
        self.assertIn('originatingComponent == &parameterValue',block)
        self.assertIn('parameterValue.showTextBox();',block)
        self.assertIn('requestShortcutFocus (backwards ?',block)
        self.assertIn('requestShortcutFocus (parameterSelector)',block)
        self.assertNotIn('requestShortcutFocus (sequencerButton)',block)
        self.assertNotIn('focusControl (pageSelector)',block)

    def test_numeric_editor_intercepts_shift_tab_and_ctrl_tab(self):
        block=EDITOR.split('class ShortcutValueTextEditor final',1)[1].split('class ShortcutSliderLabel',1)[0]
        self.assertIn('|| key.getKeyCode() == juce::KeyPress::tabKey)',block)
        self.assertNotIn('! key.getModifiers().isShiftDown()',block)
        self.assertIn('shortcut (key, this)',block)
        # The early numeric edit handler cannot swallow Ctrl+Tab, regular Tab or Shift+Tab.
        parent_block=EDITOR.split('if (auto* editor = dynamic_cast<juce::TextEditor*> (originatingComponent);\n        editor != nullptr && parameterValue.isParentOf (editor))',1)[1].split('const auto focusControl',1)[0]
        self.assertNotIn('requestShortcutFocus (sequencerButton)',parent_block)
        self.assertNotIn('juce::KeyPress::tabKey && ! key.getModifiers().isShiftDown()',parent_block)

    def test_main_buttons_not_in_tab_cycle_but_shortcuts_remain(self):
        for name in ('sequencerButton','resetParameter','initializeSynth','loadPreset','savePreset','help','aboutButton'):
            self.assertIn(name+'.setWantsKeyboardFocus (false);',EDITOR)
        for key in ('q','r','i','b','s','h','a','d'):
            self.assertIn("character == '"+key+"'" if key in ('r','i','b','s','d') else ("lowerCharacter == 'q'" if key=='q' else "== 'a'" if key=='a' else "== 'h'"), EDITOR)
        self.assertIn('focusControl (pageSelector);',EDITOR) # Alt+D still works
        about=EDITOR.split('if (aboutOpen)',1)[1].split('// Alt+P',1)[0]
        self.assertIn('if (keyCode == juce::KeyPress::tabKey)',about)
        self.assertIn('target = &aboutProject;',about)

    def test_version_and_workflow(self):
        self.assertIn('0.99.61',(ROOT/'CMakeLists.txt').read_text('utf8'))
        self.assertIn('ljunoVersion = "0.99.61"',EDITOR)
        self.assertIn('TEST126-Accessible-Tab-Cycle-And-Page-Shortcuts',WORKFLOW)
        self.assertIn('python Tests/test126_tab_cycle_and_page_navigation.py',WORKFLOW)
        self.assertTrue((ROOT/'Doc/RELEASE_NOTES_0.99.61.md').is_file())

if __name__=='__main__':
    unittest.main()
