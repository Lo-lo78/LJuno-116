# SPDX-License-Identifier: AGPL-3.0-or-later
"""Song single selections must announce Selected, and only Shift+Space may toggle."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SONG = (ROOT / 'Source' / 'SongEditor.cpp').read_text(encoding='utf-8')
MAIN = (ROOT / 'Source' / 'PluginEditor.cpp').read_text(encoding='utf-8')

class Test104SongSelectionAnnouncement(unittest.TestCase):
    def test_single_element_has_selected_in_navigation_and_grid(self):
        self.assertIn('const bool marked = songSelected[track].count (position) != 0;', SONG)
        self.assertIn('const bool selected = songSelected[static_cast<size_t> (lane)].count (index) != 0;', SONG)
        self.assertIn('if (selected > 0) message << ", "', SONG)
        self.assertNotIn('selected > 1 && songSelected[track].count', SONG)
    def test_only_shift_space_toggles_selection(self):
        self.assertEqual(SONG.count('toggleSongRow(); return true;'), 1)
        self.assertIn('code == juce::KeyPress::spaceKey && mods.isShiftDown()', SONG)
        self.assertNotIn('code == juce::KeyPress::spaceKey && ! mods.isAltDown() && ! mods.isShiftDown()', SONG)
        self.assertIn('Shift Space toggles selection', SONG)
    def test_audio_and_preview_path_remains_untouched(self):
        self.assertIn('! key.getModifiers().isAnyModifierKeyDown()', MAIN)

if __name__ == '__main__':
    unittest.main()
