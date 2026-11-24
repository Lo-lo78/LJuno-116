#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def get(f): return (R/f).read_text(encoding='utf-8')
assert 'ljunoVersion = "0.99.67"' in get('Source/PluginEditor.cpp')
assert 'ljunoReleaseDate = "25 November 2026"' in get('Source/PluginEditor.cpp')
assert 'VERSION 0.99.67' in get('CMakeLists.txt')
assert 'name: LJuno-116-0.99.67-Linked-Filters-Windows-VST3' in get('.github/workflows/build-windows.yml')
for lang in ('en','it','es','pt','fr','ru','zh','ja'):
    part=get('Help/LJuno116Help.html').split(f'<article class="manual" id="{lang}"',1)[1].split('</article>',1)[0]
    new=part.split('aria-label="Release 0.99.67"',1)[1].split('</section>',1)[0]
    for term in ('BP Cutoff','HP Key Follow','LFO1 BP L1','LFO1 BP L2','LFO1 BP Noise','LFO2 BP L1','LFO2 BP L2','LFO2 BP Noise','+0.08','Filter Routing'):
        assert term in new,(lang,term)
    assert '0.99.67' in part.split('<h3>About</h3>',1)[1]
    assert 'Release 0.99.66' in part
for p in (R/'Doc').glob('ljuno-116-*.html'):
    assert 'id="release-09967"' in p.read_text(encoding='utf8'),p
    assert 'id="release-09966"' in p.read_text(encoding='utf8'),p
assert (R/'Doc/RELEASE_NOTES_0.99.67.md').is_file()
assert 'slider472:0<-8,8,0.01>LFO2 BP Noise' in get('Tools/generate_parameters.py')
print('OK 0.99.67 release metadata, eight languages, six manuals, linked filter documentation and history')
