#!/usr/bin/env python3
"""TEST120 integration check: preserve prior synth and sequencer workflows."""
from pathlib import Path
from html.parser import HTMLParser
root = Path(__file__).resolve().parents[1]
editor = (root / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
header = (root / 'Source/PluginEditor.h').read_text(encoding='utf-8')
core = (root / 'Source/IncrementalParameterSearch.h').read_text(encoding='utf-8')
key = (root / 'Source/ParameterSearchKey.h').read_text(encoding='utf-8')
workflow = (root / '.github/workflows/build-windows.yml').read_text(encoding='utf-8')
help_html = (root / 'Help/LJuno116Help.html').read_text(encoding='utf-8')
assert 'namespace ljuno {' in core
assert 'intervalMs = 1000' in core and 'sameLetterCycle' in core
assert 'CharLowerBuffW' in core and '#define NOMINMAX' in core and '#undef min' in core and '#undef max' in core
assert key.index("if (code >= '0' && code <= '9')") < key.index('const auto typed = key.getTextCharacter();')
assert key.index('F1Key') < key.index("if (code >= '0' && code <= '9')")
assert 'numberPad0' in key and 'numberPad9' in key
assert 'mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown()' in key
assert 'ljuno::IncrementalParameterSearch mainParameterSearch;' in header
assert 'ljuno::IncrementalParameterSearch sequencerParameterSearch;' in header
assert 'mainParameterSearch.next (' in editor and 'sequencerParameterSearch.next (' in editor
assert 'mainParameterSearch.reset();' in editor and 'sequencerParameterSearch.reset();' in editor
assert 'std::wstring (visibleParameterNames[' in editor
assert 'std::wstring (sequencerParameterNames[' in editor
assert 'ljunoParameterSearchCharacter (key)' in editor
assert 'selectNextParameterStartingWith (character,' in editor
assert 'selectSequencerParameterStartingWith (character,' in editor
# The search must not use the picker label that is prefixed "selected " for active locks.
assert 'sequencerParameterNames[static_cast<std::size_t> (index)]' in editor
assert '0.99.59' in editor and '14 November 2026' in editor
assert 'TEST120' in workflow and 'test120_incremental_parameter_search.py' in workflow
assert 'LJunoSearchTest' in (root / 'CMakeLists.txt').read_text()
assert 'Test120' not in (root / 'Source/PluginProcessor.cpp').read_text()
assert 'Test120' not in (root / 'Source/SynthEngine.cpp').read_text()
for lang in ('en', 'it', 'es', 'pt', 'fr', 'ru', 'zh', 'ja'):
    block = help_html.split(f'<article class="manual" id="{lang}"', 1)[1].split('</article>', 1)[0]
    assert 'TEST120' in block and '0.99.56' in block
    assert 'Parameter Locks' in block and 'Shift+' in block
print('PASS TEST120 static: search in both grids, Shift+digits, no DSP changes, eight languages, About and GitHub Actions')
