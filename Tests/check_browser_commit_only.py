#!/usr/bin/env python3
"""Ensure browsing never persists a non-confirmed row or saved preset."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
manager = (root / 'Source/PresetManager.cpp').read_text(encoding='utf-8')
helper = (root / 'Source/PresetBrowserMemory.h').read_text(encoding='utf-8')


def body(src: str, symbol: str) -> str:
    p = src.index(symbol)
    start = src.index('{', p)
    depth = 0
    for pos in range(start, len(src)):
        if src[pos] == '{':
            depth += 1
        elif src[pos] == '}':
            depth -= 1
            if depth == 0:
                return src[start+1:pos]
    raise AssertionError(f'missing closing brace: {symbol}')


assert 'lastLoadedPreset' in helper
assert 'properties.saveIfNeeded()' in helper
assert 'inline void remember (' in helper
assert 'inline void renamed (' in helper
assert 'inline void remove (' in helper

if (root / 'Source/PresetBrowser.cpp').exists():
    editor = (root / 'Source/PresetBrowser.cpp').read_text(encoding='utf-8')
    browsing = body(editor, 'void PresetBrowser::browse()')
    assert 'manager.lastLoadedPresetAcrossSessions()' in browsing
    assert 'int selected = 0;' in browsing
    assert 'manager.savedBrowserRow()' not in browsing
    assert 'manager.currentFile()' not in browsing
    assert 'rememberedDirectory()' not in browsing
    closing = body(editor, 'void PresetBrowser::close (bool revertPreview)')
    assert 'manager.saveBrowserRow' not in closing
    preview = body(manager, 'juce::Result PresetManager::preview (')
    commit = body(manager, 'juce::Result PresetManager::commit (')
    load = body(manager, 'juce::Result PresetManager::load (')
    save = body(manager, 'juce::Result PresetManager::save (')
    assert 'preset_browser_memory::remember' not in preview
    assert 'preset_browser_memory::remember' not in save
    assert 'preset_browser_memory::remember' in commit
    assert 'preset_browser_memory::remember' in load
    assert 'manager.commit (entry.file)' in body(editor, 'void PresetBrowser::activateRow (')
else:
    editor = (root / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
    opening = body(editor, 'void LJuno116AudioProcessorEditor::openPresetBrowser()')
    assert 'lastLoadedPresetAcrossSessions()' in opening
    assert 'refreshPresetBrowser (0)' in opening
    assert 'presetBrowserDirectoryState' not in opening
    assert 'presetBrowserRowState' not in opening
    assert 'presetBrowserSelectionState' not in opening
    assert 'getCurrentPresetFile()' not in opening
    closing = body(editor, 'void LJuno116AudioProcessorEditor::closePresetBrowser (')
    assert 'setProperty (presetBrowserRowState' not in closing
    assert 'setProperty (presetBrowserSelectionState' not in closing
    assert 'removeProperty (presetBrowserRowState' in closing
    preview = body(manager, 'juce::Result PresetManager::previewPreset (')
    commit = body(manager, 'juce::Result PresetManager::commitPresetPreview (')
    load = body(manager, 'juce::Result PresetManager::loadPreset (')
    save = body(manager, 'juce::Result PresetManager::savePreset (')
    assert 'preset_browser_memory::remember' not in preview
    assert 'preset_browser_memory::remember' not in save
    assert 'preset_browser_memory::remember' in commit
    assert 'preset_browser_memory::remember' in load
    assert 'processor.presetManager.commitPresetPreview (entry.file)' in body(
        editor, 'void LJuno116AudioProcessorEditor::activatePresetBrowserRow (')

print('OK: only committed preset loading advances the remembered browser location')
