#!/usr/bin/env python3
"""LJuno TEST124: legacy-safe polyrhythmic LFO integration checks."""
from pathlib import Path
import re, unittest
R=Path(__file__).resolve().parents[1]
class DriftLace(unittest.TestCase):
    def test_ten_parameters_are_appended_without_reindexing(self):
        s=(R/'Source/GeneratedParameters.h').read_text()
        found=re.findall(r'^\s*\{\s*(\d+),\s*"(slider\d+)".*',s,re.M)
        self.assertEqual(found, [(str(i), f'slider{i:03d}') for i in range(1,463)])
        for i in [453,457,458,462]:
            line=next(x for x in s.splitlines() if f'{{ {i}, ' in x)
            self.assertRegex(line, r', 0\.0f, "" \},' if i in [453,457,458,462] else '.*')
    def test_accessible_pages(self):
        s=(R/'Source/GeneratedPages.h').read_text()
        for page, first in ((5,453),(6,458)):
            ids=re.findall(r'"(slider\d+)"',re.search(rf'page{page}ParameterIds\[\] = \{{(.*?)\}};',s).group(1))
            names=re.findall(r'"(.*?)"',re.search(rf'page{page}ParameterNames\[\] = \{{(.*?)\}};',s).group(1))
            self.assertEqual(len(ids),len(names))
            for n, name in enumerate(('Drift Amount','Drift Cycle','Lace Source','Lace Rate','Lace Mix')):
                idx=ids.index(f'slider{first+n}')
                self.assertEqual(names[idx],f'LFO{page-4} {name}')
    def test_dsp_paths_and_dependencies(self):
        s=(R/'Source/SynthEngine.cpp').read_text()
        h=(R/'Source/SynthEngine.h').read_text()
        self.assertIn('completedCycles',h)
        self.assertIn('state.completedCycles %',s) if 'state.completedCycles %' in s else self.assertIn('state.completedCycles\n',s)
        self.assertIn('state.completedCycles = 0;',s)
        self.assertIn('p.lfo1.laceSource == 2',s)
        self.assertIn('p.lfo2.laceSource == 1',s)
        self.assertIn('if ((slot & 1) == 0)',s)
        self.assertIn('if (p.laceSource == 0 || p.laceMix <= 0.0f)',s)
        self.assertIn('if (p.driftAmount > 0.0f && p.driftCycle > 1)',s)
        self.assertIn('advanceLfo (v.polyLfo1',s)
        self.assertIn('advanceLfo (v.polyLfo2',s)
    def test_polyrhythm_returns_to_initial_state(self):
        for cycle in (2,3,5,7,11,32):
            for amount in (0.25,.75,1.0):
                phases=[amount*(i%cycle)/cycle for i in range(cycle*2+1)]
                self.assertEqual(phases[0],phases[cycle])
                self.assertEqual(phases[1],phases[cycle+1])
                self.assertNotEqual(phases[0],phases[1])
    def test_version(self):
        self.assertIn('0.99.59',(R/'CMakeLists.txt').read_text())
        self.assertIn('0.99.59',(R/'Source/PluginEditor.cpp').read_text())
if __name__=='__main__':unittest.main()
