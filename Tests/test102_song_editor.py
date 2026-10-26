# SPDX-License-Identifier: AGPL-3.0-or-later
"""Structural regressions for the three-column Song Editor in TEST102."""
import pathlib
import re
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
def source(name):
    return (ROOT / 'Source' / name).read_text(encoding='utf-8')
class Test102SongEditor(unittest.TestCase):
    def test_three_independent_columns_and_immutable_snapshots(self):
        model = source('SongArrangement.cpp')
        header = source('SongArrangement.h')
        self.assertIn('static constexpr int trackCount = 3', header)
        self.assertIn('std::array<Snapshot, trackCount> columns', header)
        self.assertIn('std::atomic_load_explicit', model)
        self.assertIn('std::atomic_store_explicit', model)
        self.assertIn('stream.getNumBytesRemaining()', model)
        self.assertIn('out.writeByte (songMode()', model)
        self.assertIn('for (int i = 0; i < trackCount; ++i)', model)
    def test_song_audio_has_independent_playheads_and_slots(self):
        engine = source('SynthEngine.cpp')
        header = source('SynthEngine.h')
        for key in ('songRows', 'songCompletedSteps', 'songPlaybackSlots',
                    'currentSongColumns', 'songLoopEnabled'):
            self.assertIn(key, header)
            self.assertIn(key, engine)
        self.assertIn('state.getConfig (songPlaybackSlots[index])', engine)
        self.assertIn('songPlaybackSlots[static_cast<size_t> (sequenceIndex)]', engine)
        self.assertIn('stopSequencer (i, sampleOffset, output, p, routingMode, false)', engine)
        self.assertIn('++songRows[index]', engine)
        self.assertIn('nextConfig.launchStep - 1', engine)
        self.assertIn('nextConfig.launchOffsetMs', engine)
    def test_three_column_editor_copy_discontinuous_and_loop(self):
        cpp = source('SongEditor.cpp')
        header = source('PluginEditor.h')
        self.assertIn('std::array<int, 3> songCursor', header)
        self.assertIn('std::array<std::set<int>, 3> songSelected', header)
        for code in ('songShiftBaseline', 'juce::KeyPress::spaceKey', 'letter (\'a\')',
                     'letter (\'c\')', 'letter (\'x\')', 'letter (\'v\')',
                     'juce::KeyPress::pageDownKey', 'juce::KeyPress::deleteKey',
                     'juce::KeyPress::tabKey', 'juce::SystemClipboard'):
            self.assertIn(code, cpp)
        self.assertIn('songSelected[static_cast<size_t> (lane)].size() > 1', cpp)
    def test_global_pattern_song_shortcuts_without_windows_ctrl_command_collision(self):
        cpp = source('PluginEditor.cpp')
        self.assertIn('changePreviewMode (announcer)', cpp)
        self.assertIn('changeFocusedPattern (', cpp)
        self.assertIn('if (sequencerEditorOpen && songView)', cpp)
        self.assertIn('if (altS && sequencerEditorOpen', cpp)
        self.assertIn('key.getModifiers().isCtrlDown() && ! key.getModifiers().isAltDown()', cpp)
        self.assertIn('key.getModifiers().isCtrlDown() && ! key.getModifiers().isAltDown()', cpp)  # Win Ctrl aliases Command
    def test_song_presets_and_reaper_roundtrip(self):
        proc = source('PluginProcessor.cpp')
        self.assertIn('combinedSequencerData()', proc)
        self.assertIn('"|LJUNO-SONG-1|"', proc)
        self.assertIn('songs.restore', proc)
        self.assertIn('songs.reset()', proc)
        self.assertIn('state.setProperty ("sequencerData", combinedSequencerData()', proc)
    def test_build_includes_song_sources(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('Source/SongArrangement.cpp', cmake)
        self.assertIn('Source/SongEditor.cpp', cmake)
if __name__ == '__main__': unittest.main()
