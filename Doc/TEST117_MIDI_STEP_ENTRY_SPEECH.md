# TEST117 — MIDI Step Entry: speech and keyboard navigation

Based on LJuno-116 TEST116. No DSP or sequencer data format changes.

- Ctrl+K: always announces `MIDI Step Entry On. Step N` after the keyboard event; Ctrl+K again announces Off.
- Ctrl+M: controls only the automatic `Step N` announcements after incoming MIDI notes. Defaults Off for every entry session.
- Computer step keys Q–I / A–K and Tab / Shift+Tab always announce the chosen step, even when Ctrl+M is Off.
- Computer Space inserts Pause and announces the destination step; the last step announces completion.
- All MIDI entry still stops automatically at End Step; no Escape shortcut is introduced.
- On Windows, MIDI-driven messages are deferred slightly to avoid collision with UIA keyboard focus events.
- Preserves 64 patterns per lane, Song, Undo, Note/Velocity, and the existing preview behavior.

GitHub Actions must compile the VST3; UIA/NVDA behavior needs verification in REAPER on Windows.
