# LJuno-116 0.99.64 TEST131 — AHDSR Hold for both envelopes

Date: 16 November 2026

## Functionality

- Insert **Hold** between Attack and Decay in **both** actual DSP envelopes: ADSR1 (shared amp/filter/pitch/pan destinations) and ADSR2 (second envelope routed to enabled destinations, including Layer 1, Layer 2 and Noise).
- New stable VST3 parameters: `slider463` **Hold** (ADSR1) and `slider464` **ADSR2 Hold**. Both range **0.000–5.000 seconds** in increments of **0.001 seconds** (1 ms). Both default to **0.000 s**.
- The two controls appear immediately after their Attack controls on **Env** and **Filter** pages, and in the REAPER automation and sequencer Parameter Locks catalogue.
- Envelopes hold their exact maximum level (1.0) before entering Decay. Per-voice sample counters give independent timing in polyphonic playback. The independent pitch envelope remains at its maximum during the ADSR1 Hold stage.
- Note Off, pedal release and voice stealing continue to interrupt Hold immediately with the existing Release/steal behaviour. A live Hold value change updates the remaining hold on the next render-constant update.
- With Hold=0 the old Attack-to-Decay transition is preserved, so old projects/presets and all existing parameter IDs remain unchanged. No edits to compressor Attack/Release, LFO sample-and-hold, LArp Chord Hold or Reverb Degrade Hold; none of these are ADSR Hold stages.
- This is a **TEST** build. Public version remains **0.99.64** until separately published.

## Suggested REAPER test

1. Load an existing patch: Hold and ADSR2 Hold should both be 0, with unchanged sound.
2. In **Alt+L > Env**, set Attack low, Sustain ~0.2, Hold ~0.5 s, press and hold a MIDI note: hear the level stay full for approximately 0.5 s before Decay.
3. Release the MIDI note *during* Hold: sound must enter Release immediately (and respond to sustain pedal as usual).
4. Enable ADSR2 destination(s), set ADSR2 Hold ~0.2 s and ADSR1 Hold ~0.8 s: both envelopes should run independently.
5. Test polychords, mono legato/ties, sequencer with Parameter Locks, and changing Hold while a note is sounding.
6. Check that Ctrl+Shift+A still activates the selected Hold's REAPER envelope and adds the ", Auto" announcement without deleting its points.

Source regressions: `python Tests/test131_ahdsr_hold.py`; optional native C++ envelope simulation runs if a compatible C++ compiler is available. Compilation of the complete Windows VST3 is performed by GitHub Actions.
