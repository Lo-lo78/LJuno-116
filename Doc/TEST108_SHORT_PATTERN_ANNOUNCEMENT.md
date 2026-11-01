# LJuno-116 TEST108 — Short Alt+Q pattern announcement

- Base: TEST107 (64 patterns for Layer 1, Layer 2 and Noise).
- In the step editor Alt+Q, Ctrl+Up/Down now announces `Layer 1, Pattern N`, `Layer 2, Pattern N` or `Noise, Pattern N` (the same concise order as the main parameter grid).
- Only the screen reader message changed. There is no change to pattern selection, editor focus, step contents, sequencer, Song, preset serialization, VST3 parameter IDs or audio DSP.
- Existing P1/P64 silent bounds remain unchanged.
- Validation: `python Tests/test108_pattern_announcement.py`.
