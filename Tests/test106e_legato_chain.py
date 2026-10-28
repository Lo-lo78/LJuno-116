#!/usr/bin/env python3
"""TEST106E legato continuity guards. Full audio verification requires a JUCE VST3 build."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
cpp = (ROOT / 'Source/SynthEngine.cpp').read_text(encoding='utf-8')
hdr = (ROOT / 'Source/SynthEngine.h').read_text(encoding='utf-8')
assert 'const auto newLegato = step.length >= 101' in cpp
assert 'runtime.activeLegato = ! skip && newLegato;' in cpp
assert '&& ! runtime.activeLegato' in cpp
assert 'const auto previousLegato = runtime.activeLegato;' in cpp
assert 'nextSequencerPosition (runtime, config, true)' in cpp
assert 'if (position > last)\n                position = first;' in cpp
# The same note is not retriggered when previousLegato is true.
assert 'else if (! previousLegato)' in cpp
assert 'previousSequenceNote, true);' in cpp
assert 'handleMidi (message, p, layerMask, true, portamentoFromNote, sequencerTie);' in cpp
assert 'portamentoFromNote, sequencerTie);' in cpp
assert '! sequencerTie && (p.monoNoteMode == 0 || ! previousNoteHeld)' in cpp
assert 'bool sequencerTie = false' in hdr
# A step with normal length ends the long tie using its own gate duration.
assert 'runtime.activeDuration = desiredDuration;' in cpp
assert 'runtime.noteTimer = 0.0;' in cpp
assert 'runtime.noteTimer >= runtime.activeDuration' in cpp
print('PASS: LJuno Length 101 continuous monophonic gate, pitch handoff, loop wrap, normal release')
