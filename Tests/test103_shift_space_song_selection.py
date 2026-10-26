# SPDX-License-Identifier: AGPL-3.0-or-later
"""TEST103: Song explicit singleton selection and transport separation."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SONG = (ROOT / "Source" / "SongEditor.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "Source" / "PluginEditor.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github" / "workflows" / "build-windows.yml").read_text(encoding="utf-8")

class Test103SongSelection(unittest.TestCase):
    def test_shift_space_is_the_only_song_toggle(self):
        self.assertIn('const auto toggleSongRow = [&]()', SONG)
        self.assertIn('code == juce::KeyPress::spaceKey && mods.isShiftDown()', SONG)
        self.assertNotIn('code == juce::KeyPress::spaceKey && ! mods.isAltDown() && ! mods.isShiftDown()', SONG)
        self.assertEqual(SONG.count('toggleSongRow(); return true;'), 1)
        self.assertLess(SONG.index('code == juce::KeyPress::spaceKey && mods.isShiftDown()'),
                        SONG.index('if (mods.isCtrlDown() || mods.isCommandDown())'))
    def test_clipboard_and_delete_honor_singleton(self):
        self.assertGreaterEqual(SONG.count('if (! selected.empty())'), 3)
        self.assertNotIn('if (selected.size() > 1)', SONG)
        self.assertIn('Selected P', SONG)
        self.assertIn('Deselected P', SONG)
        self.assertIn('if (selected.size() == data.size() && selected.size() > 1)', SONG)
    def test_plain_space_is_still_preview(self):
        self.assertIn('! key.getModifiers().isAnyModifierKeyDown()', MAIN)
        self.assertIn('LJuno-116-TEST104-Windows-VST3', WORKFLOW)

if __name__ == '__main__': unittest.main()
