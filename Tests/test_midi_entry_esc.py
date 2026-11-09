from pathlib import Path
root = Path(__file__).resolve().parents[1]
is_lb = (root / "Source/SequencerStepEditor.cpp").exists()
cpp = (root / ("Source/SequencerStepEditor.cpp" if is_lb else "Source/PluginEditor.cpp")).read_text(encoding="utf-8")
start = cpp.index("bool SequencerStepEditor::keyPressed" if is_lb else "bool LJuno116AudioProcessorEditor::handleSequencerEditorKey")
segment = cpp[start:]
active = segment.index("if (midiEntryActive)\n    {")
# ESC must terminate recording before the MIDI-mode catch-all and before the
# normal Escape that closes the sequencer editor.
mode = segment[active:]
esc = mode.index("if (code == juce::KeyPress::escapeKey" if is_lb else "if (keyCode == juce::KeyPress::escapeKey")
ctrl_m = mode.index("if (ctrlOnly && (pressedLower == 'm'" if is_lb else "if (ctrlOnly && (character == 'm'")
assert esc < ctrl_m
block = mode[esc:ctrl_m]
assert "finishMidiEntry (true);" in block
assert '"MIDI Step Entry Cancelled"' in block
assert "return true;" in block
assert "close();" not in block and "closeSequencerEditor();" not in block
assert "if (! midiEntryActive) return true;" in mode[:esc]
assert "End of Pattern. MIDI Step Entry Off" in cpp
assert "midiEntrySpeak = ! midiEntrySpeak;" in cpp
if is_lb:
    assert "if (code == juce::KeyPress::escapeKey) { close(); return true; }" in mode[ctrl_m:]
    assert "TEST2.4.64-Windows-x64" in (root / ".github/workflows/build-windows.yml").read_text(encoding="utf-8")
else:
    assert "closeSequencerEditor();" in mode[ctrl_m:]
    assert "TEST123-Independent-Input-Polyphony-Windows-VST3" in (root / ".github/workflows/build-windows.yml").read_text(encoding="utf-8")
assert "finishMidiEntry (false);" in cpp
assert "midiEntryOriginals" in cpp if not is_lb else "midiEntryChanges" in cpp
print("PASS: Escape cancels MIDI step session, Ctrl K commits, normal Escape unchanged")
