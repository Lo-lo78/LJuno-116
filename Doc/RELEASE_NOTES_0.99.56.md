# LJuno-116 0.99.56 — TEST120 — 4 November 2026

## Incremental parameter search (LReaParams / LSampler-24 parity)

- The **main parameter Grid (Alt+L)** and **Alt+Q > Parameters > Enter** Parameter Lock picker now support progressive multi-character type-ahead: press `F` then `I` within one second to search `FI`.
- Match the **start of the complete parameter name first**, then the start of any internal word. Ordinary substrings are not matched.
- Repeated single keys cycle forward; **Shift+letter / Shift+digit** cycles backward. Shift+1 searches for digit `1`, not the shifted punctuation character. Wrap at list boundaries.
- The current match remains eligible while completing a multi-character prefix. If the complete prefix has no match, its final character starts a new search.
- A one-second pause, arrows or other navigation starts a new search. Page changes reset the search.
- The picker searches the actual parameter name, ignoring the UI `selected` prefix used for existing Parameter Locks.
- Browser preset search, Alt+Q step shortcuts, parameter values, synthesizer audio/DSP, MIDI, parameter IDs, presets and sequencer serialization remain unchanged.
- The fixed Windows-compatible standalone search header from LSampler-24 TEST165 is reused, including the `NOMINMAX` macro guard. No LReaParams dependency is required.

**Build verification:** standalone C++ search tests and static checks are included; the VST3 build and NVDA behavior must be validated via GitHub Actions and REAPER.
