# TEST96 — Active range for Tab and Note Enter toggle

Based on the provided LJuno-116 TEST95 source archive.

- Alt+Q editor: each selected sequence (Layer 1, Layer 2, Noise) has its own Start Step / End Step (Z/X/C/V), inclusive.
- Tab / Shift+Tab traverse only 16-step blocks that intersect this active range; boundary attempts return silently.
- Entering the editor, changing the selected sequence or changing Start/End clamps the current step and block into the active range.
- Q-I/A-K only select steps inside the active range, and batch arrow edits ignore selected steps that became inactive. Data beyond the range are preserved.
- In Page 1 (Note), unmodified Enter toggles the current step between Pause (MIDI note 0) and C4 (MIDI note 60), with the usual screen-reader announcement.
- In Page 7 (Parameters), Enter retains its existing Parameter Lock picker behavior.
- No changes to the DSP engine, plugin parameters, presets or workflow.
- Compiling/running in REAPER remains to be verified using GitHub Actions.
