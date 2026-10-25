#!/usr/bin/env python3
"""TEST97 structure and routing regression tests; no native JUCE compiler."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

def source(name):
    return (ROOT / 'Source' / name).read_text(encoding='utf-8')

def physical_slot(lane, pattern):
    return lane + 3 * (pattern - 1)

class PatternTest97(unittest.TestCase):
    def test_48_distinct_slots_with_first_three_legacy(self):
        slots = {physical_slot(lane, pattern) for lane in range(3) for pattern in range(1, 17)}
        self.assertEqual(len(slots), 48)
        self.assertEqual(slots, set(range(48)))
        self.assertEqual([physical_slot(lane, 1) for lane in range(3)], [0, 1, 2])
        h = source('SequencerState.h')
        for expected in ('trackCount = 3', 'patternsPerTrack = 16',
                         'legacySequenceCount = 16', 'maximumSequences = trackCount * patternsPerTrack'):
            self.assertIn(expected, h)
        self.assertIn('return lane + trackCount * (getSelectedPattern (lane) - 1);', source('SequencerState.cpp'))

    def test_preset_format4_and_legacy_decoder(self):
        cpp = source('SequencerState.cpp')
        self.assertIn('stream.writeInt (4);', cpp)
        self.assertIn('stream.writeInt (getSelectedPattern (lane));', cpp)
        self.assertIn('const auto storedCount = formatVersion >= 4 ? maximumSequences : legacySequenceCount;', cpp)
        self.assertIn('selectedPatterns[static_cast<std::size_t> (lane)].store', cpp)
        self.assertIn('reset();', cpp)

    def test_three_automatable_vst_parameters_on_sequencer_page(self):
        param = source('GeneratedParameters.h')
        pages = source('GeneratedPages.h')
        ids = re.search(r'page15ParameterIds\[\] = \{(.*?)\};', pages).group(1)
        names = re.search(r'page15ParameterNames\[\] = \{(.*?)\};', pages).group(1)
        for n, label in ((443, 'Pattern Layer 1'), (444, 'Pattern Layer 2'), (445, 'Pattern Noise')):
            self.assertEqual(param.count(f'{{ {n}, "slider{n}"'), 1)
            self.assertIn(f'"slider{n}"', ids)
            self.assertIn(f'"{label}"', names)
        self.assertEqual(len(re.findall(r'"slider\d+"', ids)), len(re.findall(r'"[^"]+"', names)))

    def test_selected_lane_has_only_its_own_pattern(self):
        proc = source('PluginProcessor.cpp')
        for call in ('getConfig', 'getStepValue', 'addStepDelta', 'getStepParameterCount',
                     'addStepParameterLock', 'resetStep', 'getConfigValue', 'setConfigValue'):
            self.assertRegex(proc, rf'sequencerState\.{call} \(getActiveSequenceSlot \(')
        self.assertIn('const auto lane = id == "slider443" ? 0 : id == "slider444" ? 1 : 2;', proc)
        self.assertIn('syncSequencerBankToParameters();', proc)

    def test_audio_has_three_running_tracks_and_loads_selected_pattern(self):
        engine = source('SynthEngine.cpp')
        header = source('SynthEngine.h')
        self.assertIn('std::array<SequencerRuntime, SequencerState::trackCount>', header)
        self.assertIn('getConfig (sequencerState.getActiveSequenceIndex (i))', engine)
        self.assertIn('state.getStep (state.getActiveSequenceIndex (sequenceIndex), stepIndex)', engine)
        self.assertIn('stopSequencer (i, 0, midi, p, routingMode, false);', engine)
        self.assertIn('startSequencer (i, note, velocity, sequenceConfigs[', engine)
        self.assertNotIn('for (int i = 0; i < SequencerState::maximumSequences; ++i)', engine)

    def test_keys_and_live_editor_preview(self):
        editor = source('PluginEditor.cpp')
        self.assertIn('newPattern = juce::jlimit (1, 16, oldPattern', editor)
        self.assertIn('processor.selectSequencerPatternFromEditor (track, newPattern);', editor)
        self.assertIn('key.getModifiers().isCtrlDown()', editor)
        self.assertIn('changeSequencerEditorSequence (-1);', editor)
        self.assertIn('changeSequencerEditorSequence (1);', editor)
        self.assertIn('PATTERN " << processor.getSequencerPattern (sequence)', editor)

    def test_clipboard_uses_heap_not_multi_mb_stack(self):
        proc = source('PluginProcessor.cpp')
        self.assertIn('std::make_unique<ljuno::SequencerState>()', proc)
        self.assertNotIn('ljuno::SequencerState clipboardState;', proc)
        self.assertIn('getActiveSequenceSlot (destination)', proc)

if __name__ == '__main__':
    unittest.main()
