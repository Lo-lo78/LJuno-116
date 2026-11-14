#!/usr/bin/env python3
"""TEST121: DAW Play restarts only Free LFOs; preserves old synth contracts."""
from pathlib import Path
from html.parser import HTMLParser
import re

root = Path(__file__).resolve().parents[1]
engine = (root / 'Source/SynthEngine.cpp').read_text(encoding='utf-8')
engine_h = (root / 'Source/SynthEngine.h').read_text(encoding='utf-8')
processor = (root / 'Source/PluginProcessor.cpp').read_text(encoding='utf-8')
processor_h = (root / 'Source/PluginProcessor.h').read_text(encoding='utf-8')
transport = (root / 'Source/HostPlayLfoReset.h').read_text(encoding='utf-8')
random = (root / 'Source/FreeLfoRandom.h').read_text(encoding='utf-8')
workflow = (root / '.github/workflows/build-windows.yml').read_text(encoding='utf-8')
help_html = (root / 'Help/LJuno116Help.html').read_text(encoding='utf-8')

# The host Play reset must be detected even when audio processing is skipped.
start = processor.index('// TEST121: detect DAW Play')
idle = processor.index('if (synthEngine.isDeepIdle()', start)
assert start < idle
assert 'position->getIsPlaying()' in processor[start:idle]
assert 'position->getTimeInSamples()' in processor[start:idle]
assert 'hostPlayLfoReset.update' in processor[start:idle]
assert 'synthEngine.restartFreeLfosOnHostPlay (free1, free2);' in processor[start:idle]
assert 'getPlainParameterValue ("slider025") < 0.5f;' in processor[start:idle]
assert 'getPlainParameterValue ("slider035") < 0.5f;' in processor[start:idle]
assert 'hostPlayLfoReset.clear();' in processor
assert 'ljuno::HostPlayLfoReset hostPlayLfoReset;' in processor_h
assert 'if (*samplePosition != expected)' in transport  # suspended while stopped

reset_start = engine.index('void SynthEngine::restartFreeLfosOnHostPlay')
reset_end = engine.index('void SynthEngine::retriggerLfos', reset_start)
reset = engine[reset_start:reset_end]
for fragment in ('if (freeLfo1)', 'if (freeLfo2)', 'reset (lfoState1,',
                 'reset (lfoState2,', 'reset (voice.polyLfo1,',
                 'reset (voice.polyLfo2,', 'state.previousPhase = 1.0f;',
                 'state.freeSampleHoldRandom = seed;'):
    assert fragment in reset, fragment
for forbidden in ('randomState =', 'voices =', 'sequencerRuntime =', 'larpState =',
                  'chorusPhase =', 'delayWritePosition ='):
    assert forbidden not in reset, forbidden
assert 'void restartFreeLfosOnHostPlay (bool freeLfo1, bool freeLfo2) noexcept;' in engine_h

# Dedicated S&H stream must never reseed general synthesis randomness.
assert 'if (p.mode == 0) // Free has a separate deterministic RNG.' in engine
assert 'nextFreeLfoSampleHold (state.freeSampleHoldRandom)' in engine
assert 'state.sampleHold = randomSigned(); // Triggered unchanged.' in engine
assert 'x ^= x << 13' in random and 'x ^= x >> 17' in random
assert '0x116f0101u' in reset and '0x116f0202u' in reset
assert 'freeLfoVoiceSeed (0x116f0101u, voice.note)' in reset
assert 'freeLfoVoiceSeed (0x116f0202u, voice.note)' in reset
assert 'freeLfoVoiceSeed (freeSeed, note)' in engine
assert 'if (parameters.mode == 1 || oneShot)' in engine  # note behavior preserved
assert 'if (p.lfo1.polyphonic) initialise' in engine
assert 'if (p.lfo2.polyphonic) initialise' in engine

# Automated Windows compilation includes an executable C++ behavioral test.
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
build = (root / 'Tools/build-windows.ps1').read_text(encoding='utf-8')
assert 'LJunoFreeLfoTransportTest' in cmake and 'LJunoFreeLfoTransportTest' in build
assert 'test121_free_lfo_host_play.py' in workflow
assert 'TEST123-Independent-Input-Polyphony-Windows-VST3' in workflow
assert 'TEST120' in workflow  # previous search regression stays active

class Manuals(HTMLParser):
    languages = []
    def handle_starttag(self, tag, attrs):
        d = dict(attrs)
        if tag == 'article' and 'manual' in d.get('class', '').split():
            self.languages.append(d.get('id'))

parser = Manuals()
parser.feed(help_html)
assert parser.languages == ['en', 'it', 'es', 'pt', 'fr', 'ru', 'zh', 'ja']
for lang in parser.languages:
    block = help_html.split(f'<article class="manual" id="{lang}"', 1)[1].split('</article>', 1)[0]
    assert 'TEST121' in block and 'TEST120' in block
    assert 'Free' in block and 'Triggered' in block and '0.99.56' in block
assert '14 November 2026' in (root / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
assert (root / 'Doc/TEST121_FREE_LFO_HOST_PLAY.md').exists()
print('PASS TEST121: DAW transport Free-only global/poly LFO, deterministic S&H, no engine reset, eight languages and build')
