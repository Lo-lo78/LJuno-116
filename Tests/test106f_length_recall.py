#!/usr/bin/env python3
"""Prevent regressions in the per-step Length Enter toggle in LJuno-116."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
editor = (root / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
state = (root / 'Source/SequencerState.cpp').read_text(encoding='utf-8')
header = (root / 'Source/SequencerState.h').read_text(encoding='utf-8')
manual = (root / 'Help/LJuno116Help.html').read_text(encoding='utf-8')
assert 'sequencerEditorLayer == ljuno::SequencerLayer::length)' in editor
assert 'step.length == 101 ? step.lastNormalLength : 101' in editor
assert 'const auto indices = selectedSequencerStepIndices();' in editor
assert 'processor.commitSequencerEditSnapshot (std::move (before));' in editor
assert 'int lastNormalLength = 50;' in header
assert 'std::atomic<int> lastNormalLength { 50 };' in header
assert 'if (newLength != 101)' in state
assert 'step.lastNormalLength.store (newLength' in state
assert 'stream.writeInt (5);' in state
assert 'formatVersion < 1 || formatVersion > 5' in state
assert 'formatVersion >= 5' in state
assert state.count('lastNormalLength') >= 11
for lang in ('en', 'it', 'es', 'pt', 'fr', 'ru', 'zh', 'ja'):
    assert f'<article class="manual" id="{lang}"' in manual
assert manual.count('37 → 101 → 37') >= 8
print('PASS: TEST106F Length 101 Enter toggle, step recall, persistence and documentation')
