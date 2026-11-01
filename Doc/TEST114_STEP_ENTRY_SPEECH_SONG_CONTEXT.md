# TEST114 — Step Entry speech and preview context

When MIDI Step Entry is enabled with Ctrl+K, an English entry announcement is
always sent to NVDA/UIA. Ctrl+M is Off by default, silencing per-step progress.
When switched On it announces the next step after each MIDI Note On or Pause and
after changing the insertion step; the Windows UIA announcement uses ImportantAll
instead of MostRecent to avoid merging consecutive steps. The last recorded step
always announces the end and exits automatically. Ctrl+K exits manually and
announces the exit.

Alt+S always selects the preview mode for the newly displayed editor (Steps =
Pattern; Song = Song); Alt+M can be used to override until the next context
switch. The same happens when first opening Alt+Q Steps.
