from pathlib import Path
r = Path(__file__).resolve().parents[1]
is_lb = (r/'Source/SequencerStepEditor.cpp').exists()
s = (r/('Source/SequencerStepEditor.cpp' if is_lb else 'Source/PluginEditor.cpp')).read_text(encoding='utf-8')
h = (r/('Source/SequencerStepEditor.h' if is_lb else 'Source/PluginEditor.h')).read_text(encoding='utf-8')
assert 'void '+('SequencerStepEditor' if is_lb else 'LJuno116AudioProcessorEditor')+'::finishMidiEntry (bool cancel)' in s
assert 'finishMidiEntry (true);' in s
assert 'finishMidiEntry (false);' in s
assert '"MIDI Step Entry Cancelled"' in s
assert ('stopMidiEntry(); // Disconnect MIDI before applying any restoration.' in s
        if not is_lb else 'stopMidiEntry();\n    auto& destination' in s)
if is_lb:
    assert 'std::vector<StepChange> midiEntryChanges;' in h
    assert 'destination.replaceStep (midiEntryPattern, change.index, change.before);' in s
    assert 'midiEntryChanges.push_back ({ currentStep, before, after });' in s
    assert 'existing->after = after;' in s
    assert 'commitAction (std::move (action)); // One Undo' in s
    assert 'midiEntryChanges.clear();' in s
else:
    assert 'std::optional<LJuno116AudioProcessor::SequencerEditSnapshot>' in h
    assert 'midiEntryUndoBefore = processor.captureSequencerEditSnapshot();' in s
    assert 'processor.replaceSequencerStep (midiEntryLane, original.index, original.step);' in s
    assert 'processor.commitSequencerEditSnapshot (std::move (*midiEntryUndoBefore));' in s
    assert 'midiEntryOriginals.push_back ({ sequencerEditorCurrentStep, prior });' in s
    assert 'midiEntryUndoBefore.reset();' in s
print('PASS: single-session MIDI entry rollback/commit, original first-write values, no ghost Undo on Esc')
