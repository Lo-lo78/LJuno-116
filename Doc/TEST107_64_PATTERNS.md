# LJuno-116 TEST107 — 64 Patterns per track

- Base: LJuno-116 0.99.55 TEST106F, no DSP or synth changes.
- Layer 1, Layer 2 and Noise: 64 patterns each = 192 independent pattern slots, 128 steps per pattern.
- Stable physical slot `lane + 3 * (pattern - 1)`. The original 48 slots are byte-for-byte addressed by the same indexes.
- VST3 pattern selectors: existing slider443, slider444, slider445, range 1..64; stable IDs (but review old automated normalized values).
- Alt+Q: Ctrl+Up/Down on current lane changes P1..P64, silently at either edge. Editor announces Pn/64.
- Song Editor: three independent columns now accept/serialize references P1..P64. Ctrl+Up/Down changes a row. Ctrl+C/X/V handles row clipboard; Alt+I inserts.
- LSQ2 v6 stores 192 sequences. Older LSQ2 v1–v3 have 16 stored sequences and v4–v5 have 48; loader explicitly reads the old count and resets additional slots to their defaults. Length 101 state in v5+ retained.
- Song LJS1 v1 format is unchanged (one byte per pattern reference, 0..63 fits).
- The per-step clipboard, pattern copy/cut/paste, Undo/Redo and 32 Parameter Locks are untouched.
- 8-language embedded Help updated. Automated source checks in `Tests/test107_64_patterns.py`.
- Build only on GitHub Actions; no local VST3 compile or audio validation.
