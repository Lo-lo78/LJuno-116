#!/usr/bin/env python3
"""TEST130: REAPER active-envelope automation, native keyboard routing and NVDA."""
from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
cpp=(root/'Source/PluginEditor.cpp').read_text(encoding='utf-8')
h=(root/'Source/PluginEditor.h').read_text(encoding='utf-8')
proc=(root/'Source/PluginProcessor.cpp').read_text(encoding='utf-8')
proch=(root/'Source/PluginProcessor.h').read_text(encoding='utf-8')
workflow=(root/'.github/workflows/build-windows.yml').read_text(encoding='utf-8')

class HostAutomation(unittest.TestCase):
    def test_real_active_state_and_points(self):
        for token in ('GetFXEnvelope','GetSetEnvelopeInfo_String','"ACTIVE"',
                      '"VISIBLE"','"ARM"','GetTouchedOrFocusedFX',
                      'TrackFX_GetParamName','getParameters().indexOf(param)',
                      'GetMasterTrack','GetTrack'):
            self.assertIn(token,proc,token)
        self.assertIn('getVST3ClientExtensions() override',proch)
        self.assertIn('reaperHostExtension = std::make_unique',proc)
        self.assertIn('getReaperEnvelopeActive',proc)
        self.assertIn('toggleReaperEnvelopeActive',proc)
    def test_param_uses_current_grid_index_and_host_parameter(self):
        method=cpp.split('LJuno116AudioProcessorEditor::selectedAutomationParameter() const',1)[1].split('LJuno116AudioProcessorEditor::getHostAutomationEnvelopeState',1)[0]
        self.assertIn('parameterSelector.getSelectedItemIndex()',method)
        self.assertIn('visibleParameterIndices[',method)
        self.assertIn('processor.parameters.getParameter (',method)
        self.assertIn('generated::parameters',method)
        self.assertNotIn('getRawParameterValue',method)
    def test_ctrl_shift_a_windows_and_all_value_contexts(self):
        method=cpp.split('LJuno116AudioProcessorEditor::handleAutomationShortcut (',1)[1].split('void LJuno116AudioProcessorEditor::selectParameter',1)[0]
        self.assertIn('mods.isCtrlDown() && mods.isShiftDown()',method)
        self.assertIn('! mods.isAltDown()',method)
        self.assertNotIn('! mods.isCommandDown()',method)
        self.assertIn('toggleHostAutomationEnvelope (param, source)',method)
        self.assertIn('param->setValueNotifyingHost (param->getValue())',method)
        self.assertIn('parameterSelector.setAutomationKeyHandler',cpp)
        self.assertIn('parameterValue.setAutomationKeyHandler',cpp)
        self.assertEqual(h.count('bool keyPressed (const juce::KeyPress& key) override'),3)  # About, Grid, Value
        numeric=cpp.split('class ShortcutValueTextEditor final',1)[1].split('class ShortcutSliderLabel',1)[0]
        self.assertIn('key.getModifiers().isCtrlDown()',numeric)
        self.assertIn('key.getModifiers().isShiftDown()',numeric)
        self.assertIn('&& shortcut)',numeric)
    def test_nvda_active_indicator_and_modal_protection(self):
        self.assertIn('observedAutomationState == 1 ? ", Auto" : ""',cpp)
        self.assertIn('parameterValue.setParameterAccessibilityName (',cpp)
        self.assertIn('announceMessageFrom (focused, parameterSelector.getText())',cpp)
        self.assertIn('if (aboutOpen || sequencerEditorOpen || presetBrowserOpen',cpp)
        self.assertIn('automationStatusTimer.startTimer (500)',cpp)
        self.assertIn('automationStatusTimer.stopTimer()',cpp)
        self.assertIn('syncAutomationIndicator (true)',cpp)
        self.assertIn('getHostAutomationEnvelopeState',h)
    def test_legacy_navigation_and_ci(self):
        self.assertIn('selectRelativePage (delta);',cpp)
        self.assertIn('key.getModifiers().isShiftDown() ? -1 : 1;',cpp)
        self.assertIn('python Tests/test130_host_automation.py',workflow)
        self.assertIn('TEST130-Host-Automation-Windows-VST3',workflow)
        self.assertIn('0.99.65',(root/'CMakeLists.txt').read_text(encoding='utf-8'))
        manual=(root/'Help/LJuno116Help.html').read_text('utf-8')
        self.assertGreaterEqual(manual.count('Ctrl+Shift+A'),8)
        self.assertTrue((root/'Doc/TEST130_HOST_AUTOMATION.md').exists())

if __name__=='__main__': unittest.main()
