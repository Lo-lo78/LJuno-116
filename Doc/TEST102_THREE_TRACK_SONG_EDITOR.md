# LJuno-116 TEST102 — Three-track Song Editor

This release extends the TEST101 step editor with a Song arrangement for **Layer 1, Layer 2 and Noise**. Each independent column holds an unlimited-in-practice sequence of references to its own 16 existing patterns; steps and parameter locks remain stored in their original patterns. No new automatable VST parameters are introduced.

## Playback

Alt+Q opens the editor. Alt+S toggles Steps/Song while inside Alt+Q; outside Alt+Q, Alt+S retains the Save shortcut. Alt+M selects Pattern/Song playback from the parameter pages and Alt+Q, without itself starting preview playback. Alt+R in Song is an alias. Alt+P still switches Space between the DAW and preview. Space in preview mode starts/stops the existing three-lane sequencer. All notes played by the sequencer retain their original **Note Source = Sequencer** requirement (Direct and LArp are not changed). The Song's three playheads are independent and advance after each pattern's Start/End range, including the existing tempo-clock repeat events. Each track has its own Loop on/off and may stop independently. Navigating the editor does not jump playback.

## Song keyboard navigation

- Left/Right: change Layer 1, Layer 2 or Noise; each column retains its own cursor.
- Up/Down: preceding/following Song element, with compact NVDA output `P5, 2 of 64`.
- Page Up/Down: five elements; Home/End: first/last; Tab: full detail without moving.
- Ctrl+Up/Down: change the selected row's referenced pattern P1–P16 without changing other occurrences.
- Alt+I: insert a row after the focused one (copy its pattern, or P1 if empty).
- Delete: delete selected rows; Ctrl+Delete: confirm before clearing the column.
- Alt+L: toggle loop of the active column; Ctrl+Z and Ctrl+Shift+Z: undo/redo a whole Song operation.
- Shift+Up/Down: extend/reduce selection. Plain arrows keep previously selected rows selected. Ctrl+Space: toggle single non-contiguous row; Ctrl+Shift+Up/Down: add a new section; Ctrl+A: select the active column only.
- Ctrl+C/X/V: copy/cut/paste the selected Song rows. Unselected gaps are removed, so pasting always inserts a contiguous block after the focused row, including when copying between layers. Clipboard text has a distinct LJuno Song identifier.
- `Selected` is spoken only for selected elements if there are two or more in the selection. Tab reports the total.
- Enter: read row details, *not* a pattern picker.

The existing Alt+Q Steps editor keeps Ctrl+A limited to the focused 16-step page, Ctrl+C/X/V for steps and Ctrl+Shift+C/X/V for one pattern on one track.

## Persistence and safety

Song columns, loop toggles and Pattern/Song playback mode are stored inside the existing sequencer payload for `.Ljuno` presets and REAPER projects. Existing TEST101 and earlier sequencer base64 payloads are recognised and result in empty Song columns, both Pattern mode. The format uses a distinct `|LJUNO-SONG-1|` suffix; historic VST parameter identifiers and the 48 original pattern slots remain unchanged. Cursors, selections, keyboard target and Song Undo history remain editor-session state. Loading a preset invalidates the previous Song edit history.

**Verification:** Python structural regressions are not C++ compilation and not audio/NVDA integration testing. Build the VST3 using GitHub Actions as usual, then confirm transitions between pattern rows of unequal duration in REAPER with each track set to Note Source = Sequencer.
