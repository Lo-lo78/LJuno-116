# LJuno-116 0.99.65 TEST133 — SID/AY noise continuity

Date: 20 November 2026

## Problem

The SID 6581 (Noise Type 7) and Amstrad CPC AY (Noise Type 8) were kept inside
`SynthEngine::Voice`. Every `v = {}` on a retrigger or voice cleanup restored
the same LFSR seed and fractional clock. Consequently, fast repeated notes
could reproduce the exact same noise attack, making high-pitched chip noise
sound like a repeated tonal waveform.

## Fix

- Retain SID's exact 23-bit taps/DAC map and AY's exact 17-bit taps/output.
- Put independent per-voice-register SID/AY hardware states in `SynthEngine`,
  **outside** the voice/envelope structure, as Biscot1 already retains its SID
  seed across Note On events.
- Reuse the already computed pitch-clock increment; fractional clock phase and
  last sample survive Note On, Note Off, release, voice stealing, zero-voice
  cleanup, mono retrigger, poly/Poly-2 reuse, sequencer and MIDI routing.
- A full `prepare()` resets the noise hardware as before (new processor/audio
  initialization). Silent voices do not run the chip; a subsequent note
  resumes the stored bitstream instead of restarting it.
- SID and AY use independent clocks, so switching between them doesn't reset
  either sequence. Radio, Metallic, Digital Dust and white/pink/brown noises
  retain their previous per-voice behaviour and DSP.
- No new VST parameters or preset fields, no phase randomization, no dynamic
  allocation or silent-sample processing. All established oscillator pitch
  modulation, Keyboard Scale, pan, saturation and FX paths remain intact.

## Validation

Run `python Tests/test133_chip_noise_continuity.py` and the standalone C++
`LJunoChipNoiseContinuityTest` (automatically built/run by the Windows script).
The C++ test compares 15,000 samples against the historical SID and AY bit
algorithms with varying clocks, then verifies that sequential high-pitch note
attacks are not identical after voice resets. The Windows CI still builds the
complete VST3, which cannot be fully auditioned in a Linux source-only run.

## Listening check in REAPER

1. Noise Type **Biscot SID 6581**, high Noise Pitch, Noise Level clearly audible;
   repeat the same short MIDI note with pauses. New attacks should no longer
   repeat the same digital sequence from the beginning.
2. Repeat with **Amstrad CPC AY**. The AY chip naturally has a periodic LFSR;
   this fix removes artificial **note-on** repeatability, not its authentic
   digital clock character or any high-pitch aliasing.
3. Test mono/poly, Layer 1 + 2 + Noise and the Noise sequencer lane, then
   verify an old preset with white/pink/brown noise remains unchanged.
