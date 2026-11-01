# LJuno-116 TEST110 — Per-step Note + Length recall

In Alt+Q, Note page: Enter toggles a sounding note to Pause and then restores **the specific note last programmed in that step** (for example 72 → Pause → 72). A never-edited step begins paused and initially recalls C4 (60). Group selections act independently. Length page: Enter toggles Length 101 and the step's own previous normal length, independently from Note, including while a note is paused.

The new `lastActiveNote` value is kept in the `SequencerStep`/`AtomicStep` structures. Any nonzero note edit updates the remembered pitch; Pause does not erase it. The value follows step/pattern copy/paste, undo snapshots, and preset/project state. Reset whole step/pattern uses Pause and defaults remembered Note 60, Length 50. Backspace in the editor resets sounding fields without touching parameter locks.

LSQ2 **v7** adds one stored byte (`lastActiveNote`) per step, after `lastNormalLength`; v1-v6 are still read, preserving all 16/48/192 old physical slots. When importing earlier formats, a sounding note seeds the remembered pitch; an inactive step seeds 60. Song data/automation IDs and synth audio remain unchanged.

Tests: Python structural checks `test110_note_and_length_recall.py`; C++ JUCE-shim runtime checks `Test110NoteLengthRecall.cpp` for note toggles, length 101, independent group toggles, copy, v7 round-trip, legacy migration and reset.
