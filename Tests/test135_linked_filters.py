from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
def read(path):return (R/path).read_text(encoding='utf-8')
params=read('Source/GeneratedParameters.h')
assert [int(x) for x in re.findall(r'^\s*\{\s*(\d+),\s*"slider\d+"',params,re.M)]==list(range(1,473))
assert '"HP Key Follow", -4.2f, 4.2f, 0.01f, 0.08f' in params
pages=read('Source/GeneratedPages.h')
assert '"slider017", "slider466", "slider018"' in pages
assert pages.count('"slider096", "slider465"')==2
assert '"slider356", "slider467", "slider468", "slider469"' in pages
assert '"slider359", "slider470", "slider471", "slider472"' in pages
s=read('Source/SynthEngine.cpp')
assert '(note - 60) * p.hpKeyFollow / 12.0f' in s
for lfo in (1,2):
    for i,source in enumerate(('L1','L2','Noise')):
        assert f'const auto bp{lfo}{source} = value (s, "slider{467+(lfo-1)*3+i}")' in s
        for dest in ('LowPass','HighPass'):
            assert f'p.lfo{lfo}{dest}{source} += bp{lfo}{source};' in s
p=read('Source/PluginProcessor.cpp')
assert 'set ("slider015", next.lp)' in p and 'set ("slider017", next.hp)' in p
assert 'synchronisingLinkedFilter.store (true);\n            parameters.replaceState (state);' in p
assert 'descriptor.sliderNumber == 466' in read('Source/PresetManager.cpp')
assert 'descriptor.sliderNumber == 466' in read('Source/PluginEditor.cpp')
print('OK TEST135: stable IDs, accessible pages, DSP routing, physical writes and restore paths')
