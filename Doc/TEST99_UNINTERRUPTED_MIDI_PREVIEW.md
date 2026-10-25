# LJuno-116 TEST99 — MIDI keys transpose without interrupting sequencer preview

Based on TEST98 (Alt+P DAW/Preview). No new automatable parameters, no change to 48 pattern banks or accessibility shortcuts.

## What changed

When Alt+P has selected **Sequencer preview** and Space has started the preview:

- The preview holds a dedicated virtual middle-C input note for Poly sequencer lanes, separate from real keyboard key-down/key-up messages.
- MIDI Note On transposes all three running sequencer lanes to the key pressed, at the incoming MIDI sample position. It does **not** restart the pattern, alter Start/End, reset the step position, or release Noise.
- MIDI Note Off does not stop a lane or remove the preview's virtual input note; the last transposition stays in effect until another key is played.
- Poly sequencer lanes transpose their permanent virtual C4 key instead of adding/removing physical notes to the chord table. Mono lanes update their transpose without retriggering.
- Physical Note On/Off are no longer directly injected a second time into sequencer synth sources: the generated notes keep ownership of their voices. Direct and LArp paths continue through their existing note-source routing.
- MIDI output suppression/original pass-through follows the existing sequencer channel and routing mode policy.
- When preview is **stopped**, ordinary keyboard Note On/Off and configured MIDI Input Modes continue to work as in TEST98.

All preview transport rules from TEST98 remain: Alt+P DAW/Preview, Space Play/Stop in grids/Alt+Q/browser, continuing outside Alt+Q, stopping on complete plugin-window close.

## Verification

Build via GitHub Actions. In REAPER/NVDA, enable `Note Source = Sequencer` for Layer 1, Layer 2 and Noise. Start preview with Alt+P and Space; play and release multiple MIDI keys, including C4, while watching/listening for uninterrupted step timing and persistent Noise. Repeat with Layer 1/2 MIDI Input Poly enabled and with All Trigger selected. Then stop preview and confirm normal Note Off stops ordinary physical-key sequencer playback.
