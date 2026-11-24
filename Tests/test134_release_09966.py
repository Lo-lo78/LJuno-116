#!/usr/bin/env python3
"""0.99.66 release integration: metadata, current Help/Guides, history and DSP parity."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
def get(f):return (R/f).read_text(encoding='utf-8')
assert 'ljunoVersion = "0.99.67"' in get('Source/PluginEditor.cpp')
assert 'ljunoReleaseDate = "25 November 2026"' in get('Source/PluginEditor.cpp')
assert 'project(LJuno116 VERSION 0.99.67 ' in get('CMakeLists.txt')
assert 'LJuno-116-0.99.66-SID-AY-Noise-Continuity-Windows-VST3' in get('.github/workflows/build-windows.yml')
help=get('Help/LJuno116Help.html')
for lang in ('en','it','es','pt','fr','ru','zh','ja'):
    part=help.split(f'<article class="manual" id="{lang}"',1)[1].split('</article>',1)[0]
    assert 'Release 0.99.66' in part and 'Biscot SID 6581' in part,lang
    assert 'Amstrad CPC AY' in part and 'Noise Pitch' in part,lang
    assert '0.99.67' in part.split('<h3>About</h3>',1)[1], lang
    assert '0.99.65' in part and '0.99.58' in part,lang # history preserved
for p in ('Doc/ljuno-116-introduction.html','Doc/ljuno-116-whatsnew.html',
          'Doc/ljuno-116-sound-engine.html','Doc/ljuno-116-accessible-ui.html',
          'Doc/ljuno-116-modulation-guide.html','Doc/ljuno-116-patch-building-guide.html',
          'README.md'):
    assert '0.99.66' in get(p),p
for v in ('0.99.65','0.99.64','0.99.58'):
    assert (R/f'Doc/RELEASE_NOTES_{v}.md').is_file(),v
notes=get('Doc/RELEASE_NOTES_0.99.66.md')
assert 'TEST133' in notes and '23-bit' in notes and '17-bit' in notes
assert (R/'Source/ChipNoiseState.h').is_file()
assert 'python Tests/test133_chip_noise_continuity.py' in get('.github/workflows/build-windows.yml')
print('OK: 0.99.67 About with 0.99.66 history, 8-language Help, manuals, metadata, old releases and SID/AY notes')
