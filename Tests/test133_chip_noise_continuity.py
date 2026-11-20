#!/usr/bin/env python3
"""TEST133: ensure persistent SID/AY registers are never owned by reset voices.

The native unit test tests actual DAC/tap functions and high-pitch repeated
notes; this static integration test prevents routing/voice-reset regressions.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
h = (ROOT/'Source/SynthEngine.h').read_text(encoding='utf-8')
c = (ROOT/'Source/SynthEngine.cpp').read_text(encoding='utf-8')
chip = (ROOT/'Source/ChipNoiseState.h').read_text(encoding='utf-8')
workflow = (ROOT/'.github/workflows/build-windows.yml').read_text(encoding='utf-8')
cmake = (ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
voice = h.split('    struct Voice\n',1)[1].split('    struct SuperWaveParameters\n',1)[0]
assert 'ChipNoiseState' not in voice, 'Noise must not restart with a Voice reset'
assert 'sidNoiseLeft' not in voice and 'ayNoiseLeft' not in voice
assert 'std::array<ChipNoiseState, maximumVoiceCount * 2 + 1> chipNoiseStates {};' in h
assert c.count('chipNoiseStates = {};') == 1, 'Reset only on audio preparation'
assert c.count('voices = {};') >= 2, 'Guard regression expectation: voices are reset on routing'
assert 'renderNoisePair (v, chipNoiseStates[static_cast<size_t> (voiceIndex)]' in c
render = c.split('SynthEngine::renderNoisePair (',1)[1]
assert re.search(r'if \(noiseType == 7\)\s+return chip.renderSid',render)
assert re.search(r'if \(noiseType == 8\)\s+return chip.renderAy',render)
assert render.index('return chip.renderSid') < render.index('voice.chipNoiseClock +=')
assert render.index('return chip.renderAy') < render.index('voice.chipNoiseClock +=')
assert 'voice.metallicNoiseLeft' in render
assert 'voice.radioLowLeft' in render
assert 'sidLeft = 0x7fffffu' in chip and 'ayLeft = 0x1ffffu' in chip
assert '((state >> 17u) ^ (state >> 22u))' in chip
assert '((state >> 0u) ^ (state >> 3u))' in chip
assert not re.search(r'\bnew\s+[A-Z_a-z][A-Z_a-z0-9]*\s*\(', chip), 'No heap allocations'
assert not re.search(r'\b(?:rand|randomSigned)\s*\(', chip), 'No new RNG calls'
assert 'LJunoChipNoiseContinuityTest' in cmake
assert 'python Tests/test133_chip_noise_continuity.py' in workflow
assert 'LJunoChipNoiseContinuityTest.exe' in (ROOT/'Tools/build-windows.ps1').read_text(encoding='utf-8')
print('TEST133 OK: register ownership, voice reset independence, SID/AY routes and CI integration')
