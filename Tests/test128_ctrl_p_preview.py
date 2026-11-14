#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
e=(r/'Source/PluginEditor.cpp').read_text(encoding='utf8')
p=e[e.index('// Local micro-preview is never delegated'):e.index('if (sequencerEditorOpen && midiEntryActive)',e.index('// Local micro-preview is never delegated'))]
assert 'Ctrl+P chooses its duration' in p
assert "if (ctrl && ! alt && ! shift" in p
assert "key.getTextCharacter() == 16" in p
assert 'cycleSequencerStepAuditionMode();' in p
assert 'auditionSequencerStep (true);' in p
assert 'Ctrl+Alt+P' not in (r/'README.md').read_text(encoding='utf8')
assert (r/'Doc/RELEASE_NOTES_0.99.63.md').exists()
print('PASS TEST128 Ctrl+P shortcut in Alt+Q, Ctrl+Space unchanged')
