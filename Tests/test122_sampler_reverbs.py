#!/usr/bin/env python3
"""Offline source/UI consistency regression for LJuno TEST122."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
params = (root / 'Source/GeneratedParameters.h').read_text(encoding='utf-8')
pages = (root / 'Source/GeneratedPages.h').read_text(encoding='utf-8')
synth_h = (root / 'Source/SynthEngine.h').read_text(encoding='utf-8')
synth = (root / 'Source/SynthEngine.cpp').read_text(encoding='utf-8')
ui = (root / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
fdn = (root / 'Source/SamplerRoomReverb.h').read_text(encoding='utf-8')
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
workflow = (root / '.github/workflows/build-windows.yml').read_text(encoding='utf-8')

rows = re.findall(r'\{ (\d+), "(slider\d+)", "[^"]+", "([^"]+)", ([\-\d.]+)f, ([\d.]+)f,', params)
ids = [p[1] for p in rows]
assert len(ids) == len(set(ids)), 'Duplicate parameter ID'
assert len(ids) >= 451
mode = next(x for x in params.splitlines() if '{ 140, "slider140"' in x)
assert '0.0f, 6.0f' in mode
assert 'Off|Reverb 1|Reverb 2|Studio Room|Concert Hall|Plate|Cathedral' in mode

fx_ids = re.search(r'page8ParameterIds\[\] = \{(.*?)\};', pages).group(1)
fx_names = re.search(r'page8ParameterNames\[\] = \{(.*?)\};', pages).group(1)
page_ids = re.findall(r'"(slider\d+)"', fx_ids)
page_names = re.findall(r'"([^"]+)"', fx_names)
assert len(page_ids) == len(page_names)
assert all(id in ids for id in page_ids)
assert page_ids.index('slider140') < page_ids.index('slider446') < page_ids.index('slider420')
for i in range(446, 452):
    assert f'slider{i}' in page_ids
    assert f'value (s, "slider{i}")' in synth
    assert f'(number >= 446 && number <= 451)' in ui

assert 'juce::jlimit (0, 6, juce::roundToInt (value (s, "slider140")))' in synth
assert 'processReverb1 (left, right, p,' in synth
assert 'processLwsReverb (left, right, p);' in synth
assert 'processSamplerRoomReverb (left, right, p,' in synth
assert 'samplerRoomReverb.prepare (sampleRate)' in synth
assert 'samplerRoomReverb.reset();' in synth
assert 'SamplerRoomReverb samplerRoomReverb' in synth_h
assert 'p.reverbMode - 2' in synth
assert 'std::array<std::vector<float>, 8> lines' in fdn
assert 'std::array<float, 13> configured' in fdn
assert 'tailFramesRemaining' in fdn
assert 'std::clamp (double (s.decaySeconds), 0.0, 30.0)' in fdn
assert 'displayName = "Reverb Decay RT60"' in ui
assert 'LJunoSamplerReverbTest' in cmake
assert 'test122_sampler_reverbs.py' in workflow
print('TEST122 reverb modes, parameters, page, DSP and Actions: OK')
