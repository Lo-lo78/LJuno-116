# LJuno-116 0.99.64 TEST130 — REAPER automation parity

Date: 16 November 2026

- Ctrl+Shift+A in main Grid (Alt+L), Value and numeric Value edit toggles the REAPER envelope's ACTIVE state for the currently selected physical VST3 parameter.
- In REAPER, VST3 client extension queries `GetFXEnvelope` and `GetSetEnvelopeInfo_String` for `ACTIVE`, without deleting envelope points. On enable the lane is made VISIBLE/ARM; on disable it becomes hidden/unarmed while retaining data.
- The REAPER host connection verifies the same track, focused FX, parameter index and host parameter name. A failed query never silently toggles another FX.
- The native grid Value and parameter list expose the `, Auto` suffix while ACTIVE. An independent 500 ms message-thread timer detects envelope edits made outside the plugin without changing focus.
- If the REAPER API is unavailable, only unambiguous host context-menu Enable/Disable envelope actions are allowed. Show/Hide is never considered an automation toggle.
- Modal Alt+Q Steps/Songs and Parameter Locks, browsers, About and save dialogs keep their pre-existing keyboard ownership.
- Ctrl+Tab and Ctrl+Shift+Tab already work, and are preserved. No DSP, parameter IDs, preset or pattern format modifications.

Windows JUCE aliases Command with Ctrl. The shortcut deliberately does **not** check `!isCommandDown()`.

Test via `python Tests/test130_host_automation.py` and Windows GitHub Actions. An in-host NVDA/REAPER test is still required.
