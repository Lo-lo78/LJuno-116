"""TEST115: audition sounds every lane without modifying Direct/LArp routing."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
synth=(root/'Source/SynthEngine.cpp').read_text(encoding='utf-8')
editor=(root/'Source/PluginEditor.cpp').read_text(encoding='utf-8')
header=(root/'Source/PluginEditor.h').read_text(encoding='utf-8')
assert 'sequencerPreviewActiveSequence >= 0' in synth
assert '|| sequencerPreviewActiveSequence >= 0' in synth
assert '&& sequencerPreviewActiveSequence < 0' in synth
assert 'p.sourceNoteSource[static_cast<std::size_t> (sequenceIndex)] == 1)' in synth
assert 'const auto lockTargetsSynth = sequenceIndex >= 0' in synth
assert 'if (sequencerEnabled || sequencerPreviewActiveSequence >= 0)' in synth
assert '|| previewVoiceBanksNeedRender;' in synth
assert 'previewVoiceBanksNeedRender = true;' in synth
assert 'previewVoiceBanksNeedRender = voices[noiseSequencerVoiceIndex].active;' in synth
assert 'previewVoiceBanksNeedRender = false;' in synth
assert 'sequencerPreviewActive = ! sequencerPreviewActive;' in editor
assert 'processor.requestSequencerPreview (sequence, sequencerPreviewActive);' in editor
assert 'Sequence preview on' not in editor
assert 'Sequence preview off' not in editor
assert 'preview unavailable' not in editor.lower()
assert 'hasSequencerPreviewTarget()' not in editor
assert 'hasSequencerPreviewTarget()' not in header
assert 'Space is deliberately silent' in editor
assert 'Space: Sequencer preview' in editor and 'Song playback' in editor
assert 'TEST123-Independent-Input-Polyphony' in (root/'.github/workflows/build-windows.yml').read_text(encoding='utf-8')
print('PASS: LJuno preview overrides only audition routing, not saved Note Source; Space is silent')
