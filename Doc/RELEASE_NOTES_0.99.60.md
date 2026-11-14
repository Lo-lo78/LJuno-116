# LJuno-116 — Version 0.99.60 TEST125

**Release date:** 14 November 2026  
**Baseline:** 0.99.59 TEST124

## LFO Lace self-source and accessible navigation

1. **LFO1 Lace Source** gains **LFO 1** as a third choice. Its order is **Off / LFO 2 / LFO 1**, maintaining complete compatibility with TEST124 (`1` still selects LFO 2). **LFO2 Lace Source** gains **LFO 2**, with **Off / LFO 1 / LFO 2**. No parameter IDs or defaults change.
2. Self-Lace uses the **preceding output sample** of the selected LFO to avoid same-sample recursion, as in Biscot1. This works for mono and per-voice poly LFOs; cross-rate controls retain their original source.
3. **Ctrl+Tab** and **Ctrl+Shift+Tab** change to the next/previous parameter page; the new page's parameter grid receives focus. Nothing happens at a page boundary. Normal Tab controls retain their focus order.
4. Opening/reopening the VST3 editor brings focus to the **previously selected parameter** and page instead of the Page selector, allowing immediate parameter navigation with NVDA/OSARA. Use **Alt+D** to focus the Page selector.
5. Other Alt+Q sequencer, preset browser and modal shortcuts are left untouched.

**Testing:** Static and source-level regression tests are bundled. Windows MSVC compilation and practical NVDA/REAPER focus behavior must be verified with GitHub Actions and the VST3.
