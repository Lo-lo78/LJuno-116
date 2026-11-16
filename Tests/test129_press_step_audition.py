#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
e=(r/'Source/PluginEditor.cpp').read_text(encoding='utf-8')
h=(r/'Source/PluginEditor.h').read_text(encoding='utf-8')
a=(r/'Source/SequencerStepAudition.h').read_text(encoding='utf-8')
w=(r/'.github/workflows/build-windows.yml').read_text(encoding='utf-8')
select=e[e.index('void LJuno116AudioProcessorEditor::selectSequencerEditorStep'):e.index('void LJuno116AudioProcessorEditor::changeSequencerEditorStepValue')]
assert 'sequencerEditorCurrentStep = step;\n    // Physical step keys' in select
assert 'auditionSequencerStep (false);' in select
assert select.index('auditionSequencerStep (false);') < select.index('if (midiEntryActive)')
assert 'if (doublePress)' in select and 'selected = ! selected' in select
assert 'sequencerStepAuditionMode + 1' in e
assert 'sequencerStepAuditionMode + 1) % 2' in e
assert 'const char* names[] = { "Start to current step", "Current step to end" }' in e
assert 'int sequencerStepAuditionMode = 0' in h
assert 'StepAuditionMode::current' in a
assert 'sequencerPreviewActive' in e[e.index('void LJuno116AudioProcessorEditor::auditionSequencerStep'):e.index('void LJuno116AudioProcessorEditor::cycleSequencerStepAuditionMode')]
assert 'processor.isHostTransportPlaying()' in e
assert 'LJuno-116-0.99.64-TEST130-Host-Automation-Windows-VST3' in w
assert 'python Tests/test129_press_step_audition.py' in w
assert (r/'Doc/RELEASE_NOTES_0.99.64.md').is_file()
print('PASS TEST129: step press immediately auditions, two Ctrl+P modes and transport guards')
