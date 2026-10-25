#!/usr/bin/env python3
"""TEST99 structural regressions; runtime/audio behavior requires GitHub Actions and REAPER."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'Source/SynthEngine.cpp').read_text(encoding='utf-8')
HEADER = (ROOT / 'Source/SynthEngine.h').read_text(encoding='utf-8')

class PreviewMidiLegato99(unittest.TestCase):
    def test_virtual_poly_preview_key_is_fixed_c4(self):
        self.assertIn('constexpr auto virtualPreviewKey = static_cast<std::size_t> (60);', CPP)
        self.assertIn('runtime.held[virtualPreviewKey] = true;', CPP)
        self.assertNotIn('runtime.held[static_cast<std::size_t> (previewNote)] = true;', CPP)

    def test_input_midi_key_transposes_preview_without_running_sequencer_input(self):
        start = CPP.index('if (sequencerPreviewActiveSequence >= 0 && message.isNoteOnOrOff())')
        end = CPP.index('if (larpGeneratesNotes)\n                handleLArpInput', start)
        block = CPP[start:end]
        self.assertIn('if (message.isNoteOn())', block)
        self.assertIn('runtime.baseTranspose = sequencerPreviewLastMidiNote - 60;', block)
        self.assertIn('runtime.inputNote = sequencerPreviewLastMidiNote;', block)
        self.assertIn('else if (sequencerEnabled)', block)
        self.assertIn('handleSequencerInput (message, sample, midi, p,', block)
        self.assertNotIn('startSequencer (', block)
        self.assertNotIn('stopSequencer (', block)
        self.assertNotIn('releaseSequencerNote (', block)
        self.assertNotIn('runtime.held[', block)

    def test_poly_pitch_follows_preview_without_retrigger(self):
        poly = CPP.split('// The virtual preview C4 key stays held for the whole audition.', 1)[1]
        poly = poly.split('auto velocity = step.velocity', 1)[0]
        self.assertIn('sequencerPreviewActiveSequence >= 0', poly)
        self.assertIn('sequencerPreviewLastMidiNote - 60 : 0', poly)
        self.assertIn('+ previewTranspose', poly)
        self.assertIn('runtime.baseTranspose', CPP.split('else\n    {\n        // Historical monophonic sequencer path',1)[1])

    def test_no_second_direct_keyboard_voice_into_preview_sequencer_sources(self):
        loop = CPP.split('// Source performance controllers are always live.', 1)[1]
        loop = loop.split('// Routing matrix:', 1)[0]
        self.assertIn('handleSourceRoutedMidi (message, p);', loop)
        self.assertNotIn('handleMidi (message, p, 1 << sourceIndex', loop)

    def test_transport_and_banks_are_untouched(self):
        self.assertIn('int sequencerPreviewActiveSequence = -1;', HEADER)
        self.assertIn('patternsPerTrack = 16', (ROOT / 'Source/SequencerState.h').read_text())
        self.assertIn('Alt+P', (ROOT / 'README.md').read_text())

if __name__ == '__main__':
    unittest.main()
