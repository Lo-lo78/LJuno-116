"""TEST111: Launch Step and Note Sources are controls, never step pages."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'Source/PluginEditor.cpp').read_text(encoding='utf-8')
header=(root/'Source/PluginEditor.h').read_text(encoding='utf-8')
source=cpp.split('void LJuno116AudioProcessorEditor::sequencerStepValueText',1)[0]
# The name of the step page can never turn into a Note Source or Launch page.
layer=cpp.split('juce::String LJuno116AudioProcessorEditor::sequencerLayerName() const')[1].split('juce::String LJuno116AudioProcessorEditor::sequencerStepValueText')[0]
assert 'sequencerEditorSourcePage' not in layer
assert 'sequencerEditorLaunchPage' not in layer
assert "if (character == '6')" in cpp and 'sequencerEditorLayer = ljuno::SequencerLayer::parameters;' in cpp
param6=cpp.split("if (character == '6')")[1].split("if (character == '7')")[0]
assert 'sequencerEditorLayer = ljuno::SequencerLayer::parameters;' in param6
param7=cpp.split("if (character == '7')")[1].split("if (character == '8')")[0]
assert 'sequencerEditorLaunchPage = true;' in param7 and 'sequencerEditorLayer =' not in param7
assert 'Page Launch Step' not in param7
for key,slot in [('8','0'),('9','1'),('0','2')]:
 assert 'selectSequencerSourcePage ('+slot+');' in cpp.split("if (character == '"+key+"')")[1]
source_focus=cpp.split('void LJuno116AudioProcessorEditor::selectSequencerSourcePage')[1].split('void LJuno116AudioProcessorEditor::changeSequencerSourceMode')[0]
assert '"Page " + sequencerSourcePageName' not in source_focus
assert 'sequencerEditorLayer =' not in source_focus
step=cpp.split('void LJuno116AudioProcessorEditor::selectSequencerEditorStep (')[1].split('void LJuno116AudioProcessorEditor::changeSequencerEditorStepValue')[0]
assert 'if (sequencerEditorSourcePage >= 0 || sequencerEditorLaunchPage)' in step
assert 'sequencerEditorSourcePage = -1;' in step and 'sequencerEditorLaunchPage = false;' in step
refresh=cpp.split('void LJuno116AudioProcessorEditor::refreshSequencerEditorPanel')[1].split('void LJuno116AudioProcessorEditor::')[0]
assert 'Focused parameter: Launch Step' in refresh and 'Focused parameter: ' in refresh
assert 'for (int row = 0; row < 2; ++row)' in refresh
assert 'bool sequencerEditorLaunchPage = false;' in header
assert 'TEST123-Independent-Input-Polyphony-Windows-VST3' in (root/'.github/workflows/build-windows.yml').read_text(encoding='utf-8')
print('PASS: LJuno TEST111 step page retained across Launch/Note Source parameter focus')
