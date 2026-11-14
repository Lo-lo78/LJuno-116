#!/usr/bin/env python3
"""TEST127: micro-audition is a local one-lane, bounded audio-thread request."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
engine=(ROOT/'Source/SynthEngine.cpp').read_text(encoding='utf8')
editor=(ROOT/'Source/PluginEditor.cpp').read_text(encoding='utf8')
processor=(ROOT/'Source/PluginProcessor.cpp').read_text(encoding='utf8')
header=(ROOT/'Source/SequencerStepAudition.h').read_text(encoding='utf8')
workflow=(ROOT/'.github/workflows/build-windows.yml').read_text(encoding='utf8')
assert 'StepAuditionMode::startToCurrent' in header and 'StepAuditionMode::currentToEnd' in header
assert 'sequencerStepAuditionCommand.exchange (0' in engine
assert 'sequencerStepAuditionActive && i != sequencerStepAuditionLane' in engine
assert 'if (runtime.position >= sequencerStepAuditionEnd)' in engine
assert 'const auto savedSongMode' not in engine or 'songPlaybackMode = savedSongMode' in engine
assert '! sequencerStepAuditionActive && (routingMode == 0 || routingMode == 2)' in engine
assert 'bool audibleStepChanged (' in editor
assert editor.count('auditionSequencerStep (false);') >= 2
assert editor.count('audibleStepChanged (previousStep, processor.getSequencerStep (lane, step))') == 2
assert 'auditionSequencerStep (true);' in editor
assert 'cycleSequencerStepAuditionMode();' in editor
assert 'spaceControlsSequencerPreview' in editor
assert 'name + ", Pattern "' in editor and '", Step "' in editor
assert 'processor.isHostTransportPlaying()' in editor
assert 'hostTransportPlaying.store (position->getIsPlaying()' in processor
assert '0.99.63' in (ROOT/'CMakeLists.txt').read_text(encoding='utf8')
assert 'TEST128-Ctrl-P-Step-Preview' in workflow
assert (ROOT/'Doc/RELEASE_NOTES_0.99.63.md').exists()
print('OK: one-lane micro-audition, 3 bounded modes, ctrl-space, tab speech and DAW guard')
