from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
editor = (ROOT / 'Source' / ('SequencerStepEditor.cpp' if (ROOT / 'Source' / 'SequencerStepEditor.cpp').exists() else 'PluginEditor.cpp')).read_text(encoding='utf-8')
workflow = (ROOT / '.github' / 'workflows' / 'build-windows.yml').read_text(encoding='utf-8')
assert 'juce::KeyPress::pageUpKey' in editor and 'juce::KeyPress::pageDownKey' in editor
assert 'BPM Division ' in editor
if (ROOT / 'Source' / 'SequencerStepEditor.cpp').exists():
    header = (ROOT / 'Source' / 'SequencerStepEditor.h').read_text(encoding='utf-8')
    catalog = (ROOT / 'Source' / 'ParameterCatalog.cpp').read_text(encoding='utf-8')
    assert 'void changeBpmDivision (int direction)' in header
    assert 'void SequencerStepEditor::changeBpmDivision (int direction)' in editor
    assert 'lb303::seqPerformanceIndex (' in editor
    assert 'lb303::seqPerformance::bpmDivision' in editor
    assert 'param->setValueNotifyingHost' in editor
    assert 'if (! songView && ! pickerOpen && ctrlOnly' in editor
    assert editor.index('changeBpmDivision (code == juce::KeyPress::pageUpKey ? 1 : -1)') < editor.index('if (midiEntryActive)\n    {\n        drainMidiEntry();', editor.index('changeBpmDivision (code == juce::KeyPress::pageUpKey ? 1 : -1)'))
    assert 'addSeq (seqPerformance::bpmDivision' in catalog
    assert 'python Tests/check_altq_bpm_division.py' in workflow
    assert 'TEST2.4.66' in workflow
else:
    catalog = (ROOT / 'Source' / 'GeneratedParameters.h').read_text(encoding='utf-8')
    assert 'nudgeSequencerPageParameter ("slider317", delta)' in editor
    assert 'getSelectedSequencerIndex()' in editor
    assert 'if (! songView && ! sequencerParameterPickerOpen && ctrlOnly' in editor
    assert editor.index('nudgeSequencerPageParameter ("slider317", delta)') < editor.index('if (midiEntryActive)\n    {\n        drainMidiEntry();')
    assert '"slider317", "Sequencer BPM Division"' in catalog
    assert 'python Tests/test119_altq_bpm_division.py' in workflow
    assert 'TEST119' in workflow
print('Alt+Q track BPM Division shortcut: OK')
