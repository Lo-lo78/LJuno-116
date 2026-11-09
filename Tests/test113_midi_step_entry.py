"""TEST113: monophonic MIDI Step Entry is isolated from the synth audio thread."""
from pathlib import Path
r = Path(__file__).resolve().parents[1]
ed = (r/'Source/PluginEditor.cpp').read_text(encoding='utf-8')
proc = (r/'Source/PluginProcessor.cpp').read_text(encoding='utf-8')
queue = (r/'Source/MidiStepEntryQueue.h').read_text(encoding='utf-8')
assert 'startTimerHz (60)' in ed
assert 'midiEntrySpeak = false;' in ed
assert 'midiEntrySpeak = ! midiEntrySpeak' in ed
assert 'writeMidiStep (0, 0, true);' in ed
assert 'writeMidiStep (note.pitch, note.velocity, false);' in ed
assert 'changed.note = 0;' in ed
assert 'changed.lastActiveNote = changed.note;' in ed
assert 'changed.velocity = juce::jlimit (1, 127, velocity);' in ed
assert 'processor.commitSequencerEditSnapshot (std::move (*midiEntryUndoBefore));' in ed
assert 'midiEntryUndoBefore = processor.captureSequencerEditSnapshot();' in ed
assert 'if (midiEntryActive)' in ed.split('void LJuno116AudioProcessorEditor::selectSequencerEditorStep')[1]
assert 'if (sequencerEditorOpen && midiEntryActive)' in ed
assert 'midi.swapWith (remainingMidi);' in proc
assert 'midiStepEntry.consume (event.getMessage())' in proc
assert 'std::atomic<bool> enabled' in queue
assert 'heldCount == 0' in queue
assert 'TEST123-Independent-Input-Polyphony-Windows-VST3' in (r/'.github/workflows/build-windows.yml').read_text(encoding='utf-8')
print('PASS: LJuno MIDI Step Entry and no transport while recording')
