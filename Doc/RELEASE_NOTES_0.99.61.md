# LJuno-116 — Version 0.99.61 TEST126

**Release date:** 14 November 2026  
**Baseline:** 0.99.60 TEST125

## Fixed Ctrl+Tab and reduced Tab focus cycle

- **Ctrl+Tab** moves to the next page and **Ctrl+Shift+Tab** to the previous. On Windows, JUCE's Command flag aliases Ctrl, so TEST125's `!isCommandDown()` prevented both commands from running. The guard is removed. The first/last page boundaries remain silent and do not wrap.
- Main **Tab** cycle: **Grid → Value → numeric Editor → Grid**; **Shift+Tab** reverses it. The numeric editor forwards both Tab directions and Ctrl+Tab to the owning editor. Committed numeric edits are preserved when leaving the field or switching page.
- The Page selector is shortcut-only (**Alt+D**). Sequencer (**Alt+Q**), Reset (**Alt+R**), Initialize (**Alt+I**), Browser (**Alt+B**), Save (**Alt+S**), Help (**Alt+H**), and About (**Alt+A**) are outside the main Tab cycle. They remain clickable and their shortcuts remain active. The About dialog's own Tab navigation is unchanged.
- Restarting the editor still focuses the remembered page/parameter. LFO Self-Lace and previous saved values remain unchanged.

**Validation:** Python source and workflow regression tests included. Full Windows VST3 build and NVDA/OSARA use still require confirmation in GitHub Actions and REAPER.
