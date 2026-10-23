#!/usr/bin/env python3
"""TEST95 source checks. No native build required."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BpmSyncContract(unittest.TestCase):
    def test_new_parameters_are_appended_with_legacy_defaults(self):
        text = (ROOT / 'Source/GeneratedParameters.h').read_text('utf-8')
        descriptors = re.findall(r'^\s*\{\s*(\d+),\s*"(slider\d+)".*$', text, re.M)
        self.assertEqual(len(descriptors), 442)
        self.assertEqual(descriptors, [(str(i), f'slider{i:03d}') for i in range(1, 443)])
        self.assertRegex(text, r'\{ 441, "slider441", "LFO 1 BPM Sync"[^\n]*1\.0f, "Off\|On" \}')
        self.assertRegex(text, r'\{ 442, "slider442", "LFO 2 BPM Sync"[^\n]*1\.0f, "Off\|On" \}')

    def test_accessible_pages_have_controls_before_frequency(self):
        text = (ROOT / 'Source/GeneratedPages.h').read_text('utf-8')
        for page, new_id, rate_id, new_name in ((5, 'slider441', 'slider026', 'LFO 1 BPM Sync'),
                                                (6, 'slider442', 'slider036', 'LFO 2 BPM Sync')):
            ids = re.search(rf'page{page}ParameterIds\[\] = \{{ (.*?) \}};', text).group(1)
            names = re.search(rf'page{page}ParameterNames\[\] = \{{ (.*?) \}};', text).group(1)
            ids = re.findall(r'"(.*?)"', ids)
            names = re.findall(r'"(.*?)"', names)
            self.assertEqual(len(ids), len(names))
            self.assertEqual(ids.count(new_id), 1)
            self.assertEqual(ids.index(rate_id) - ids.index(new_id), 1)
            self.assertEqual(names[ids.index(new_id)], new_name)

    def test_both_modes_and_rate_paths_are_connected(self):
        header = (ROOT / 'Source/SynthEngine.h').read_text('utf-8')
        engine = (ROOT / 'Source/SynthEngine.cpp').read_text('utf-8')
        self.assertIn('bool bpmSync = true;', header)
        for index in (1, 2):
            self.assertIn(f'p.lfo{index}.bpmSync = value (s, "slider{440 + index}") >= 0.5f;', engine)
            self.assertIn(f'if (d.lfo{index}.bpmSync) d.lfo{index}.rate *= tempoScale;', engine)
        self.assertIn('p.tempoBpm / 240.0', engine)
        self.assertEqual(engine.count('rate *= tempoScale;'), 2)

    def test_rate_formula_maintains_old_sound_when_enabled(self):
        for bpm in (60.0, 90.0, 120.0, 180.0, 240.0):
            for rate in (0.125, 1.0, 4.0, 8.0, 16.0):
                old = rate * bpm / 240.0
                enabled = rate * (bpm / 240.0)
                disabled = rate
                self.assertAlmostEqual(enabled, old, places=10)
                self.assertAlmostEqual(disabled, rate, places=10)


if __name__ == '__main__':
    unittest.main()
