from pathlib import Path
import re,sys
r=Path(__file__).resolve().parents[1]
is_lb=(r/'Source/SequencerStepEditor.cpp').is_file()
s=(r/('Source/SequencerStepEditor.cpp' if is_lb else 'Source/PluginEditor.cpp')).read_text(encoding='utf-8')
h=(r/('Source/SequencerStepEditor.h' if is_lb else 'Source/PluginEditor.h')).read_text(encoding='utf-8')
assert '++midiEntryGeneration;' in s and 'midiEntryGeneration = 0;' in h
assert 'juce::Timer::callAfterDelay (90' in s
assert 'juce::Timer::callAfterDelay (75' in s
assert 'if (safe != nullptr && safe->midiEntryActive' in s
assert 'safe->midiEntryGeneration == generation' in s
assert 'midiEntrySpeak = false;' in s
assert 'midiEntrySpeak = ! midiEntrySpeak;' in s
assert 'if (pause)' in s
assert 'End of Pattern. MIDI Step Entry Off' in s
if is_lb:
 block=s.split('void SequencerStepEditor::selectStep (int inBlock)')[1].split('if (midiEntryActive)')[1].split('return; // Never toggle')[0]
 assert 'announce ("Step "' in block and 'if (midiEntrySpeak)' not in block
 tab=s.split('if (code == juce::KeyPress::tabKey && ! mods.isCtrlDown()')[1].split('return true;',1)[0]
 assert 'announce ("Step "' in tab and 'if (midiEntrySpeak)' not in tab
 assert 'if (pause)\n            announce' in s
else:
 block=s.split('void LJuno116AudioProcessorEditor::selectSequencerEditorStep (int localStep)')[1].split('if (midiEntryActive)')[1].split('return; // No double-press')[0]
 assert 'announceMessageFrom (sequencerEditorPanel, "Step "' in block and 'if (midiEntrySpeak)' not in block
 tab=s.split('if (keyCode == juce::KeyPress::tabKey')[1].split('return true;',1)[0]
 assert 'announceMessageFrom (sequencerEditorPanel,' in tab and 'if (midiEntrySpeak)' not in tab
 assert 'if (pause)\n            announceMessageFrom' in s
print('PASS: entry always announces start; computer keyboard always speaks; Ctrl+M gates MIDI-only automatic progression; stale deferred speech invalidated.')
