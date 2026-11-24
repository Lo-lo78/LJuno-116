#!/usr/bin/env python3
"""TEST131: both stable AHDSR controls, audio state transitions and preset safety.

Static assertions always run. A native test extracts both actual C++ DSP functions
and compiles them in an isolated harness when g++ or clang++ is installed.
The Windows VST3 CI build remains the authoritative full integration check.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
ENGINE=(ROOT/'Source/SynthEngine.cpp').read_text(encoding='utf-8')
HEADER=(ROOT/'Source/SynthEngine.h').read_text(encoding='utf-8')
PARAMS=(ROOT/'Source/GeneratedParameters.h').read_text(encoding='utf-8')
PAGES=(ROOT/'Source/GeneratedPages.h').read_text(encoding='utf-8')
GENERATOR=(ROOT/'Tools/generate_parameters.py').read_text(encoding='utf-8')


def source_method(start,stop):
    return ENGINE.split(start,1)[1].split(stop,1)[0]


class AHDSRRegression(unittest.TestCase):
    def test_new_host_parameters_appended_only(self):
        ids=[int(n) for n in re.findall(r'^\s*\{\s*(\d+),\s*"slider\d+"',PARAMS,re.M)]
        self.assertEqual(len(ids),len(set(ids)))
        self.assertEqual(ids[462:464],[463,464])
        self.assertIn('{ 463, "slider463", "Hold", "Hold", 0.0f, 5.0f, 0.001f, 0.0f, "" }',PARAMS)
        self.assertIn('{ 464, "slider464", "ADSR2 Hold", "ADSR2 Hold", 0.0f, 5.0f, 0.001f, 0.0f, "" }',PARAMS)
        self.assertIn('slider463:0<0,5,0.001>Hold',GENERATOR)
        self.assertIn('slider464:0<0,5,0.001>ADSR2 Hold',GENERATOR)

    def test_envelope_pages_in_correct_order(self):
        for page in (1,3):
            for suff,expected in (
                ('Ids',['slider005','slider463','slider006','slider007','slider008',
                         'slider087','slider464','slider088','slider089','slider090']),
                ('Names',['Attack','Hold','Decay','Sustain','Release',
                          'ADSR2 Attack','ADSR2 Hold','ADSR2 Decay','ADSR2 Sustain','ADSR2 Release']),
            ):
                pat=rf'page{page}Parameter{suff}\[\] = \{{(.*?)\}};'
                entries=re.findall(r'"([^"]+)"',re.search(pat,PAGES).group(1))
                for old,new in ((expected[0],expected[1]),(expected[5],expected[6])):
                    self.assertEqual(entries[entries.index(old)+1],new)
                self.assertEqual(entries.count(expected[1]),1)
                self.assertEqual(entries.count(expected[6]),1)

    def test_dsp_has_separate_sample_counters_and_persistent_release(self):
        for name in ('holdElapsed1','holdElapsed2','holdSamples1','holdSamples2','Stage::hold'):
            self.assertIn(name,ENGINE)
        self.assertIn('enum class Stage { idle, attack, hold, decay',HEADER)
        self.assertIn('p.hold = value (s, "slider463")',ENGINE)
        self.assertIn('p.hold2 = value (s, "slider464")',ENGINE)
        self.assertIn('v.pitchEnvelope = 1.0f;',ENGINE)
        for method in ('advanceEnvelope (','advanceEnvelope2 ('):
            self.assertIn(method,ENGINE)
        self.assertIn('v.stage = holdSamples > 0 ? Stage::hold : Stage::decay;',ENGINE)
        self.assertIn('v.stage2 = holdSamples > 0 ? Stage::hold : Stage::decay;',ENGINE)
        self.assertIn('v.stage = Stage::release;',ENGINE)
        self.assertIn('v.stage2 = Stage::release;',ENGINE)
        self.assertTrue((ROOT/'Doc/TEST131_AHDSR_HOLD.md').exists())

    def test_compiled_real_cpp_envelope_transitions_if_compiler_present(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('Standalone C++ compiler missing: Windows CI builds the full VST3')
        # Compile the *actual* two functions, not a Python reimplementation.
        body=ENGINE.split('float SynthEngine::advanceEnvelope (Voice&',1)[1].split('// TEST121: A DAW Play',1)[0]
        body='float SynthEngine::advanceEnvelope (Voice&'+body
        harness=r'''
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>
namespace juce { template<class T> T jlimit(T low,T high,T v) {return std::min(high,std::max(low,v));} }
class SynthEngine {
public:
 enum class Stage { idle, attack, hold, decay, sustain, release, steal };
 struct Params { float sustain=.2f,sustain2=.3f,pwm=.5f; };
 struct Voice {
  float envelope=0,envelope2=0;
  int holdElapsed1=0,holdElapsed2=0;
  bool active=false,held=false,pending=false;
  int note=-1,pendingNote=-1,layerMask=7,pendingLayerMask=7,age=0;
  float pendingVelocity=0,pwm1=.5f,pwm2=.5f,drift=0,ping=0;
  double frequency=440,targetFrequency=440;
  Stage stage=Stage::idle,stage2=Stage::idle;
 };
 struct { bool microMotionAny=false; } cachedRenderConstants;
 int ageCounter=0;
 static constexpr float epsilon=.000001f;
 void setVoicePerformanceTargets(Voice&,int,float,const Params&,bool){}
 void initializePolyLfos(Voice&,const Params&){}
 void seedVoiceMicroMotion(Voice&){}
 bool usesAdsr2(const Params&)const{return true;}
 float portamentoForMask(const Params&,int)const{return 0.0f;}
 float advanceEnvelope(Voice&,const Params&,float,int,float,float,float);
 float advanceEnvelope2(Voice&,const Params&,bool,float,int,float,float)const;
};
'''
        main=r'''
int main() {
 SynthEngine e;
 SynthEngine::Params p;
 using Stage=SynthEngine::Stage;
 SynthEngine::Voice a,b;
 a.active=a.held=true; a.stage=Stage::attack;
 b.active=b.held=true; b.stage2=Stage::attack;
 // Immediate Attack; independent Hold lengths: 5 and 9 samples.
 assert(e.advanceEnvelope(a,p,1.0f,5,0.25f,0.5f,0.5f)==1.0f);
 assert(a.stage==Stage::hold);
 assert(e.advanceEnvelope2(b,p,true,1.0f,9,0.25f,0.5f)==1.0f);
 assert(b.stage2==Stage::hold);
 for(int i=0;i<5;i++) {
   assert(e.advanceEnvelope(a,p,1.0f,5,0.25f,0.5f,0.5f)==1.0f);
 }
 assert(a.stage==Stage::decay);
 assert(b.stage2==Stage::hold); // no crosstalk
 assert(e.advanceEnvelope(a,p,1.0f,5,0.25f,0.5f,0.5f)<1.0f);
 for(int i=0;i<9;i++) e.advanceEnvelope2(b,p,true,1.0f,9,0.25f,0.5f);
 assert(b.stage2==Stage::decay);
 assert(e.advanceEnvelope2(b,p,true,1.0f,9,0.25f,0.5f)<1.0f);
 // Hold 0 retains the historical Attack -> Decay transition.
 SynthEngine::Voice z; z.active=z.held=true; z.stage=Stage::attack; z.stage2=Stage::attack;
 e.advanceEnvelope(z,p,1.0f,0,0.2f,0.5f,0.5f);
 e.advanceEnvelope2(z,p,true,1.0f,0,0.2f,0.5f);
 assert(z.stage==Stage::decay && z.stage2==Stage::decay);
 // Note Off during Hold immediately starts Release on both stages.
 SynthEngine::Voice r; r.active=r.held=true; r.stage=Stage::attack; r.stage2=Stage::attack;
 e.advanceEnvelope(r,p,1.0f,500,0.1f,0.5f,0.5f);
 e.advanceEnvelope2(r,p,true,1.0f,500,0.1f,0.5f);
 assert(r.stage==Stage::hold && r.stage2==Stage::hold);
 r.held=false; r.stage=Stage::release; r.stage2=Stage::release;
 assert(e.advanceEnvelope(r,p,1.0f,500,0.1f,0.5f,0.5f)<1.0f);
 assert(e.advanceEnvelope2(r,p,true,1.0f,500,0.1f,0.5f)<1.0f);
 // Live automation: shortening Hold to zero finishes it on the next sample.
 SynthEngine::Voice c; c.active=c.held=true; c.stage=Stage::attack;
 e.advanceEnvelope(c,p,1.0f,300,0.1f,0.5f,0.5f);
 e.advanceEnvelope(c,p,1.0f,300,0.1f,0.5f,0.5f);
 e.advanceEnvelope(c,p,1.0f,0,0.1f,0.5f,0.5f);
 assert(c.stage==Stage::decay);
 // Disabling ADSR2 resets its timer and level.
 e.advanceEnvelope2(b,p,false,1.0f,9,0.1f,0.5f);
 assert(b.stage2==Stage::idle && b.envelope2==0 && b.holdElapsed2==0);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as td:
            src=Path(td)/'ahdsr.cpp'; exe=Path(td)/'ahdsr.exe'
            src.write_text(harness+body+main,encoding='utf-8')
            subprocess.run([compiler,'-std=c++17','-O0','-Wall','-Wextra','-o',str(exe),str(src)],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            subprocess.run([str(exe)],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)

if __name__=='__main__': unittest.main()
