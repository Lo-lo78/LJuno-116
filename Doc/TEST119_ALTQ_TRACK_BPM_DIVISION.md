# LJuno-116 TEST119: Ctrl+Page Up/Down – Track BPM Division

Within Alt+Q Steps, Ctrl+Page Up increases and Ctrl+Page Down decreases the **BPM Division** of the currently selected sequencer track by 0.0625. The announced value comes from the existing VST3 parameter; no new parameter is introduced.

- LB-303: Bassline 1 or Bassline 2 (selected with Home/End). BPM Division is stored in the current pattern.
- LJuno-116: Layer 1, Layer 2 or Noise (selected in the sequencer editor). BPM Division is stored in the currently selected sequencer sequence.
- Announcements are in English, such as `Bassline 1, BPM Division 4.0625` or `Noise, BPM Division 3.9375`. No announcement is made if already at a boundary.
- Works in MIDI Step Entry (Ctrl+K) as well as ordinary Steps editing. Ctrl+M still affects only automatically advancing MIDI notes.
- Does not change the active Note, Length, Velocity or other step page, the selected step, Song contents, or the REAPER host tempo.
- Range: 0.03125–64.0. Works through the same existing parameter used in the main plugin window.
