#!/usr/bin/env python3
"""Preserve public 0.99.65 Help/manual archives and fixed parameter IDs."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
def t(path): return (R/path).read_text(encoding='utf-8')
assert 'ljunoVersion = "0.99.67"' in t('Source/PluginEditor.cpp')
assert 'ljunoReleaseDate = "25 November 2026"' in t('Source/PluginEditor.cpp')
assert 'project(LJuno116 VERSION 0.99.67 ' in t('CMakeLists.txt')
assert 'LJuno-116-0.99.66-SID-AY-Noise-Continuity-Windows-VST3' in t('.github/workflows/build-windows.yml')
assert (R/'Doc/RELEASE_NOTES_0.99.65.md').exists()
assert (R/'Doc/RELEASE_NOTES_0.99.64.md').exists()
help=t('Help/LJuno116Help.html')
for lang in ('en','it','es','pt','fr','ru','zh','ja'):
  part=help.split(f'<article class="manual" id="{lang}"',1)[1].split('</article>',1)[0]
  assert '0.99.65' in part and 'ADSR2 Hold' in part and 'Ctrl+Shift+A' in part,lang
for path in ('Doc/ljuno-116-whatsnew.html','Doc/ljuno-116-accessible-ui.html',
             'Doc/ljuno-116-sound-engine.html','Doc/ljuno-116-modulation-guide.html',
             'Doc/ljuno-116-patch-building-guide.html','README.md'):
  assert '0.99.65' in t(path),path
assert 'slider463:0<0,5,0.001>Hold' in t('Tools/generate_parameters.py')
assert 'slider464:0<0,5,0.001>ADSR2 Hold' in t('Tools/generate_parameters.py')
assert 'Ctrl+Shift+A' in t('Doc/RELEASE_NOTES_0.99.65.md')
print('OK: 0.99.65 history, 0.99.67 current metadata, manuals and release notes')
