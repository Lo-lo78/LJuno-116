# LJuno-116 TEST109 — Blank sequencer patterns (Pause by default)

- Based exclusively on TEST108.
- Every new SequencerState contains 64 patterns per track for Layer 1, Layer 2 and Noise, with 128 steps each. Every untouched step is Note 0 = Pause.
- Increasing End Step 16 → 128 reveals only pauses, not C4 notes.
- New instances, full reset, single-pattern reset and single-step reset all initialise note to Pause. Backspace in Alt+Q musical step pages now resets Note to Pause, preserving Parameter Locks. A pattern's stored Length 101 normal-length memory is also restored to 50 by a full-pattern reset.
- Note-page Enter is deliberately unchanged: Pause → C4 (60), C4/other note → Pause.
- Saved presets, project notes and pattern clipboard contents are loaded exactly as recorded, including existing C4 notes. The LSQ2 v6 serialization, physical slot mapping, Song, 64-pattern navigation and VST3 parameter IDs are not changed.
- Tests: `python Tests/test109_default_pattern_pauses.py` and standalone `Tests/Test109DefaultPauses.cpp` with JUCE shim. Build VST3 on GitHub Actions.
