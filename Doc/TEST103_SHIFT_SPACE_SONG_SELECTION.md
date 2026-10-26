# LJuno-116 TEST103 — Explicit single Song row selection

In Alt+Q → Alt+S Song, **Shift+Space** toggles the focused Song row into or out of the explicit selection. **TEST104 update:** Ctrl+Space no longer toggles selection. The new command does **not** toggle the sequencer preview: unmodified Space still follows Alt+P. NVDA immediately announces `Selected Pn, x of y` or `Deselected Pn, x of y`. Normal arrows preserve selection and keep the original concise announcements: `Selected` during navigation applies even to a single explicitly selected row (TEST104).

**Important bug fix:** previously Ctrl+C, Ctrl+X and Delete consulted the set of selected rows only if it contained more than one item. This silently ignored an explicitly selected single row if the cursor had since moved. These commands now use any nonempty explicit selection, including one row. When no selection is marked they still operate on the focused row. Ctrl+A, range and discontinuous selection, compact paste, Loop, the Song arrangement and 48 source patterns are unchanged.

Tests are static regressions only; GitHub Actions compilation and REAPER/NVDA verification are still required.
