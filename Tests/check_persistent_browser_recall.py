#!/usr/bin/env python3
"""TEST browser memory: last COMMITTED preset must outlive the project.

Source wiring contract (full JUCE/VST3 build runs on GitHub Actions).
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
manager = (root / 'Source/PresetManager.cpp').read_text(encoding='utf-8')
helper = (root / 'Source/PresetBrowserMemory.h').read_text(encoding='utf-8')
editor = (root / 'Source/PluginEditor.cpp').read_text(encoding='utf-8')
header = (root / 'Source/PresetManager.h').read_text(encoding='utf-8')

assert '.github/workflows/build-windows.yml' not in editor
assert 'juce::PropertiesFile properties (options (application))' in helper
assert 'o.osxLibrarySubFolder = "Application Support";' in helper
assert 'properties.saveIfNeeded();' in helper
assert 'file.getRelativePathFrom (library)' in helper
assert 'file.isAChildOf (library)' in helper
assert 'inline void renamed' in helper and 'inline void remove' in helper
assert 'juce::File lastLoadedPresetAcrossSessions() const;' in header
assert 'preset_browser_memory::recalled' in manager
assert 'preset_browser_memory::remember' in manager
assert 'preset_browser_memory::renamed' in manager
assert 'preset_browser_memory::remove' in manager
assert 'lastLoadedPresetAcrossSessions()' in editor
assert 'lastLoaded.getParentDirectory()' in editor if 'LJuno116AudioProcessorEditor::openPresetBrowser' in editor else 'target.getParentDirectory()' in editor

# Preview must not advance the persistent pointer to another preset.
preview = manager.split('juce::Result PresetManager::previewPreset', 1)[1].split('\n}', 1)[0]
assert 'preset_browser_memory::remember' not in preview

# All loading paths (including next/previous) lead to the persistence hook.
assert 'preset_browser_memory::remember' in manager.split('juce::Result PresetManager::loadPreset', 1)[1].split('\n}', 1)[0]

print('Persistent browser recall wiring OK')
