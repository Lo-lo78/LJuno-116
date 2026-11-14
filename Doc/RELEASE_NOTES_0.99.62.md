# LJuno-116 — Version 0.99.62 TEST127

Release date: 14 November 2026

## Accessible sequencer step micro-audition

- Editing a step (Note, Length, Velocity, Repeat, Shift or Parameter Lock) auditions ONLY the selected lane and step. Automatically disabled while full three-lane preview or DAW transport is playing.
- Ctrl+Space auditions regardless of Alt+P. Ctrl+Alt+P cycles three independent preview spans: Current step, Start to current, Current to end.
- Preview respects the selected pattern, its active Start/End range, BPM Division, repeats, Note and Parameter Locks. Notes are released at the end; normal synth release/effects can decay naturally.
- Tab speaks Layer, Pattern and Step; Ctrl+Tab / Ctrl+Shift+Tab switch 16-step blocks. Shift+Tab remains available for previous block.
- Playback outside this local audition and serialized projects remain unchanged. The engine uses the existing sequencer audio path, not a parallel synthesizer.
- Full GitHub Actions Windows VST3 build and manual NVDA/REAPER audio validation pending.
