# LJuno-116 TEST98 — Alt+P sequencer preview transport

Based on TEST97, without changing any VST parameter IDs or the 48 sequencer patterns.

- Alt+P chooses whether **Space** is routed to the DAW (default) or the LJuno sequencer preview.
- Space in preview mode starts/stops all three sequencer runtimes together, but **only** Note Sources assigned to **Sequencer** sound. Direct and LArp are not converted to sequencer sources.
- Commands work on parameter pages, Alt+Q editor, its parameter picker, and preset browser. Browser preset changes may be heard while preview runs; browsing while stopped does not auto-start it.
- Closing Alt+Q returns to the previous page **without stopping** the preview; closing the plug-in editor window stops it.
- Alt+P returning to DAW stops any active preview. DAW mode leaves Space unhandled by the LJuno shortcut, subject to REAPER's keyboard routing.
- Old Alt+P previous-page shortcut moves to Alt+Shift+P. Alt+N next page remains.
- No second playback engine; existing `requestSequencerPreview` handles transport.
- Target is editor-only, default DAW each time the plug-in editor opens, not saved in `.preset` or REAPER states.

Build on GitHub Actions and verify Space/Alt+P with NVDA and REAPER.
