# TEST101 — Copy, Cut, Paste shortcuts in Alt+Q

- **Ctrl+C / Ctrl+X / Ctrl+V**: selected steps, including all five musical values and up to 32 parameter locks per step. When there is no selection, operate on the focused step. Paste places copied steps consecutively from the focused step, stopping at the destination pattern End Step. Copy and paste do not change Start/End or other pattern settings.
- **Ctrl+Shift+C / Ctrl+Shift+X / Ctrl+Shift+V**: exactly one whole pattern of the current Layer 1, Layer 2 or Noise lane (128 steps, parameter locks and pattern settings). The destination is the currently selected pattern of the current lane; the 15 other patterns are not replaced. The whole-pattern clipboard uses a versioned Windows clipboard text format and works between LJuno instances. This format is separate from LB-303's clipboard.
- **Ctrl+A** continues to select only the visible page of 16 steps, intersected with the active Start/End limits.
- Both cut and paste support **Ctrl+Z / Ctrl+Shift+Z** as single undoable operations.
- Shortcuts are active only inside Alt+Q, not in the preset browser or synth parameter grid. Step and pattern clipboards are intentionally separate.
- No parameter IDs, preset formats, MIDI preview or DSP changed. Verify GitHub Actions build and REAPER/NVDA behavior.
