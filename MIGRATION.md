# LJuno-116 migration status

## TEST31 independent local LP slopes

- Filter Routing exposes independent LP Cutoff, LP Resonance, LP Slope (12/24 dB), HP Cutoff and HP Resonance for Layer 1, Layer 2 and Noise.
- The local filters run before the historical main synth filter and use the same musical cutoff mapping and resonance/Q curves as the main filter. Their LP slopes are now independent from the Main LP Slope and from one another.
- Neutral local settings are true bypass (LP Cutoff 1, LP Resonance 0, HP Cutoff 0, HP Resonance 0), so Init and older presets retain the historical sound and avoid extra filter CPU until a local filter is used.
- The old Filter Layer Routing parameters remain in the parameter catalogue and DSP for preset/project compatibility, but the accessible Filter Routing page now presents the new local filter controls instead.
- The new local filter parameters are available to Sequencer Parameter Locks.

## 0.99.51 accessibility and preset-browser safety

The Alt+Q editor now announces the current absolute step against the end of the current 16-step block, for example `Sequence 1 of 3, Layer 1, steps 5 of 16` or, after moving to the second block, `steps 18 of 32`. The page name and value-edit increment are no longer included in the initial position announcement.

The preset browser now supports alphanumeric cycling by preset name, F2 rename, and an Applications-key/Shift+F10 menu with New folder and Rename. `Factory` is enforced as read-only by the preset manager as well as by the editor: factory presets and folders cannot be overwritten, renamed or deleted, and folders cannot be created inside Factory. The browser remembers the last writable user folder and Save preset uses that location; if none is available it falls back to the `Documents/LJuno-116` root.

## 0.99.5 release preparation

The About panel, CMake project version and embedded eight-language Help now report
0.99.5 with the 21 September 2026 release date. Global is positioned immediately
after SuperWave L2 so the three-source MIDI/Note Source routing is encountered
before LArp and Sequencer. LArp and Sequencer retain Omni as their immediate-use
MIDI-channel default.

Version 0.11.20 opens help using the selected language's real local .html file
through the Windows file association, mirroring the original Lua's dependable
CF_ShellExecute behaviour. It no longer relies on a file URL with a # fragment.

Version 0.12.2 embeds the concise HTML guide in English, Italian, Spanish,
Portuguese, French, Russian, Simplified Chinese and Japanese. The Help button and Alt+H open
an accessible language menu, then launch the selected section in the default
browser; no separately installed manual is required.

Version 0.11.18 maps Backspace to reset the selected parameter from the parameter
grid and Value slider, while retaining character deletion inside the numeric
editor. Enter in that editor now commits and returns to the matching grid item
through the fresh accessible-focus path.

Version 0.11.17 defers Alt+L/D/V focus changes until the numeric editor's key
event has completed, closes that temporary editor, clears JUCE's stale logical
accessibility focus, and raises a fresh native UIA focus event on the destination.

Version 0.11.16 adds focused screen-reader feedback for every digit and decimal
point typed in the Value editor. Punctuation is sent verbatim, so its spoken name
follows the active screen reader's language and punctuation settings; Backspace
uses the same language-neutral character feedback.

Version 0.11.15 routes announcements made inside the numeric editor through its
focused UI Automation provider after JUCE's text-change event has settled. This
also makes Alt+L/D/V leave the editor through a deferred native focus event and
restores Alt+arrow, Alt+Page and Alt+Home/End value editing with spoken feedback.

Version 0.11.14 removes Alt+arrow, Alt+Page and Alt+Home/End value changes,
restricts the numeric Value editor to digits and the decimal point, and announces
the last character removed with Backspace. Alt+letter global commands remain
available from inside the editor.

Version 0.11.13 speaks the same value-only message used by the parameter grid
when Alt navigation edits a value inside the numeric editor. Alt+B now toggles
the preset browser: its second press cancels and closes exactly like Alt+C,
Escape or Close.

Version 0.11.12 replaces the listener attached after Alt+E with a dedicated
numeric TextEditor that handles Alt shortcuts before JUCE's normal text editing.
Alt+L and the Alt navigation value commands are therefore deterministic.

Version 0.11.11 routes Alt shortcuts through the temporary numeric text editor
opened by Alt+E. Assigned Alt commands work while editing, and unassigned Alt
letters are consumed instead of being inserted into the numeric value.

Version 0.11.10 gives the parameter grid the concise accessible hint
`Alt+navigation keys, or Enter for Value`, covering both direct editing and the
Value focus route without enumerating every navigation key.

Version 0.11.9 groups each oscillator Wave with its Morph and keeps all five
Noise controls contiguous on the Osc page. Enter now moves from the parameter
grid to Value, and Enter on Value returns to the selected grid parameter.

Version 0.11.8 restores stereo noise in factory presets 54 Stefano percussione
reverb, 55 Stefano cassa con reverb, 56 Stefano sparo and 57 Stefano Claps.
Existing Factory files receive this correction once; user presets are untouched.

Version 0.11.7 adds Noise Stereo, with mono as the default and continuously
variable width up to independent stereo. Noise Color is now the PWM-like 0..1
range with a neutral 0.5 and affects every noise type. Classic Color maps its
new midpoint to the exact former zero setting. Legacy text and RPL preset values
are migrated from -1..1 when loaded; newly saved presets carry a range marker.

Version 0.11.6 corrects the noise architecture. Classic Color, white, pink and
brown remain unpitched, while the disturbed-radio and vintage-computer modes
run from a per-voice clock tied to oscillator pitch and portamento. Biscot SID
uses its original 23-bit register and oscillator-increment clocking. Noise Pitch
is on Osc; independent LFO1 Noise Pitch and LFO2 Noise Pitch destinations are on
the corresponding LFO pages. Version 0.11.5 first introduced Noise Type.

Version 0.11.4 stores factory and user presets with the `.Ljuno` extension.
Their format remains readable plain text, while the browser accepts only files
belonging to this synth and continues to hide the extension. Existing `.txt`
files are left untouched rather than deleted or overwritten.

Version 0.11.3 combines the Biscot1-style internal frequency arpeggiator with
LSH-2's independently switchable pitch destination. Layer 1 and Layer 2 can
keep level, PWM, filter and stereo movement running while Pitch Movement is off.
LSH-2's Free traversal is exposed as Free Played Order. Each layer has its own
SID Sequence, Ascending, Descending, Up and Down, Free Played Order and Random
patterns, tempo rate, octave range and pitch glide independent of the normal
portamento. The phrase latch is preserved while notes are released, so the
sequence continues unchanged through envelope release. Per-layer level/PWM and
shared LP/HP/stereo movement are active. LFO1 and LFO2 also have independent
Upper Squash and Lower Squash controls. Arp and Arp Modulation are followed by
Arp 2, with Global last.

## Reference sources

- `LJuno-116.jsfx`: authoritative sound and MIDI behaviour.
- `LJuno-116.lua`: authoritative keyboard and accessibility UX.
- `Doc/`: functional documentation and terminology.

## Completed foundation

- JUCE VST3 project pinned to JUCE 8.0.15.
- AGPL-3.0-or-later project licensing.
- 277 JSFX slider declarations parsed into stable `sliderNNN` VST parameter IDs.
- Choice labels, ranges, increments and defaults preserved in generated C++.
- Stereo output, optional stereo input and optional stereo sidechain buses.
- MIDI input and MIDI output capability.
- APVTS state save and restore.
- High-contrast editor with an accessible 13-page selector, full 277-parameter
  selector, editable value control and parameter reset.
- Explicit focus transfer from the plug-in editor into the first JUCE control.
- Parameter navigation uses a logical grid with eight rows per column: Up/Down
  changes row and Left/Right changes column. Alt+P and Alt+N change page.
- While the parameter grid has focus, letters and numbers select the next
  parameter by its user-facing Lua label beginning with that character, with
  wraparound. VST3 parameter names also use those public labels while stable IDs
  and internal JSFX names preserve preset compatibility.
- Each page exposes only the parameters in its Lua binding table. Alt+L focuses
  the parameter list, Alt+V focuses its value, and Alt+E opens numeric editing.
- Alt+Shift plus a page initial opens that page; repeated initials cycle through
  every match. Alt+D returns directly to the page selector and is exposed in
  its accessible description.
- Spoken feedback uses the active screen reader through native accessibility
  events; direct JUCE announcements are intentionally avoided on Windows.
- Successful Windows x64 VST3 build.
- First audible engine milestone: sample-accurate MIDI dispatch, sustain pedal,
  16-voice allocation, exponential main ADSR, and the JSFX basic oscillator
  formulas for Sine, Triangle Classic, Triangle Sharktooth, Saw and Pulse.
- Triangle Sharktooth now uses the JSFX leaky integrated PolyBLEP state per
  voice and per layer, rather than a scaled classic triangle.
- Initial pitch-bend and velocity-to-volume paths are active.
- JSFX drift, per-sample portamento and white/pink/brown noise-colour paths are active.
- Dry stereo audio input is preserved and summed after the synth output chain,
  matching the JSFX routing.
- The delay-line Juno chorus, tempo/free tape delay with stereo/mono feedback,
  LFO time modulation, 5 Hz DC blocker and five-band EQ are active.
- The selectable pre/post RMS compressor, channels 3/4 sidechain routing,
  glue stage, soft limiter, eight-line RC reverb and wet-path compressor are active.
- The original 67-preset RPL bank is decoded and embedded as `.Ljuno` plain-text
  files. A deterministic, screen-reader-accessible preset browser supports recursive user categories,
  Previous/Next, Browser and Save under `Documents/LJuno-116`.
- Deep idle bypasses the complete parameter/voice/effect path after voices,
  input and tails are silent, and wakes for MIDI, stereo input or automation.
- Mono unison uses the JSFX random clone-phase offsets on true retrigger;
  Voice Pan Alternate affects its clones but no longer pans the full mono voice.
- Factory preset 20, Bass Industrial, now follows the JSFX hidden-modulator
  topology: the out-of-mix Layer-2 SuperWave uses its complete Wave Mod and
  character signal to phase-modulate only the central Layer-1 Pulse. Mono-unison
  clones retain phase, detune and pan but do not receive duplicate cross-modulation.
- Editor creation transfers focus to the page selector so screen-reader users
  can enter the plug-in reliably. That transfer is delayed until the host peer
  and UI Automation provider are ready, then uses a real JUCE component focus
  transition so NVDA does not require a Tab/Shift+Tab round trip. The preset browser explicitly announces its
  folder and selected row when opened. Loading a preset with Enter closes the
  browser directly onto the parameter grid and announces page, parameter and
  value rather than the preset filename.

## DSP migration rule

Do not replace JSFX algorithms with approximate JUCE stock processors merely to
produce sound. Port one subsystem at a time and compare deterministic renders
against the JSFX reference before marking it complete.

## Planned DSP order

1. MIDI parsing, note state and voice allocation.
2. Main ADSR and ADSR2 state machines.
3. Oscillator Layer 1 and Layer 2 waveforms.
4. SuperWave and oscillator interaction.
5. LFOs and modulation routing.
6. Filters and formant filter.
7. Pan, noise and performance controls.
8. LArp timing, patterns and MIDI output.
9. Chorus, EQ, delay, reverb and compressors (active).
10. Final output, limiting and sidechain paths.

## Accessibility acceptance criteria

- Every interactive control is keyboard-focusable in a predictable order.
- Every control exposes a unique name, role, textual value and legal range.
- Enumerated parameters announce labels rather than numeric indexes.
- Page changes, resets, preset actions and compatibility warnings are announced.
- Core operation never depends on colour, pointer input or visual position.
- Test with NVDA, JAWS and Narrator on Windows, and VoiceOver on macOS.

## 0.99.3 source FX sends

Chorus, Delay and Reverb now expose independent Layer 1, Layer 2 and Noise sends. The historical global wet parameters remain in the stable VST3 parameter catalogue for project/automation compatibility but are hidden from the current FX page; the DSP runs the wet engines at unity and uses sliders 365-373 as their source-send levels.

When an older project or `.Ljuno` preset has no source-send parameters, LJuno migrates the former global wet value to all three sends. Delay 1 uses the historical Delay Mix value; Delay 2 uses Delay 2 Mix; Reverb uses Reverb Wet; Chorus uses Chorus Level. The compressor routing and processing are unchanged.

## 0.99.4 per-source expression and voice modes

Direct MIDI performance is now source-aware for Layer 1, Layer 2 and Noise. Mod Wheel (CC1), Channel Pressure and Poly Aftertouch follow the three source MIDI-channel assignments and keep independent expression state per source. LFO1 Mod Wheel Amount and LFO2 Aftertouch Amount remain shared amount controls; only the incoming controller state is source-specific.

The former global Pitch Bend Range parameter remains in the stable parameter catalogue for automation compatibility but is hidden from the current pages. Sliders 387-389 expose Pitch Bend Range L1, L2 and Noise. When loading an older project, the historical range is copied to all three.

Sliders 390-391 add Layer 1 Voice Mode and Layer 2 Voice Mode with Follow Global, Poly and Mono. Follow Global preserves the historical Voices behaviour exactly. Explicit Poly uses the global Voices value as that layer's maximum polyphony. Explicit Mono gives the layer its own held-note stack and voice bank while continuing to use the existing Mono Note Mode, Mono Portamento Mode and Mono Unison controls. Old projects default to Follow Global.

## 0.99.4 Note Source routing

Sliders 392-394 add `Layer 1 Note Source`, `Layer 2 Note Source` and `Noise Note Source` with `Direct`, `Sequencer` and `LArp`. Direct notes continue to follow each source MIDI Channel, while Pitch Bend, Mod Wheel, Aftertouch and Sustain remain source-channel performance controls regardless of the selected note generator.

The Sequencer and LArp can now run concurrently. Sequence 1 owns Layer 1, Sequence 2 owns Layer 2 and Sequence 3 owns Noise only when that source selects Sequencer. The common LArp drives only the sources that select LArp. Extra L2/Noise voice banks remain enabled whenever source note routing is active. LArp Pitch, PWM, Volume and Pan modulation is also restricted to LArp-routed sources so a sequenced or direct part is not modulated accidentally.

Older presets and project states that do not contain sliders 392-394 are migrated to the generator that previously had synth priority: an internal Sequencer mode is preferred when active, otherwise an internal LArp mode is selected, otherwise Direct is used.

## 0.99.5 Sequencer Parameter Locks

The Alt+Q step editor replaces the historical per-step CC Number / CC Value pair with a Parameter page. Each step may store multiple fixed-size synth Parameter Locks without allocating memory in the audio thread. The picker exposes parameters from the synth pages while excluding Global, Sequencer, Arp and Arp Modulation; Arp 2 remains available.

Sequencer state serialization is now format version 3. Version 1/2 states still load normally for note, timing and sequence configuration, but their legacy CC Number / CC Value bytes are intentionally discarded rather than being reinterpreted as synth parameter IDs. This prevents an old MIDI CC number from accidentally becoming a lock on an unrelated synth parameter.

A Parameter Lock is an internal step override and does not write into the VST parameter state, automation lane or preset. The lock is active for the current sequencer step and the underlying synth parameter returns to its normal value when the next active step does not lock it. If more than one sequencer lane currently locks the same synth parameter, the most recently triggered lane wins until that lock is released.

## TEST33 - three-source envelope routing

- Added ADSR 3 Attack, Decay, Sustain and Release as VST3 parameters 411-414.
- The Env page now presents ADSR 1, ADSR 2 and ADSR 3 as three matching four-control blocks.
- L1, L2 and Noise amplitude envelope blends now scan 0..3: ADSR1->ADSR2, ADSR2->ADSR3, ADSR3->ADSR1.
- The historical 0..1 segment remains DSP-identical to the previous ADSR1/ADSR2 blend behaviour.
- New Pitch Env Blend (416) and Pan Env Blend (417) use the same circular 0..3 scan: ADSR1->ADSR2, ADSR2->ADSR3, ADSR3->ADSR1. Legacy parameters 22 and 159 keep their historical 0..1 normalization and forward old host automation into the matching 0..1 segment.
- The old Filter ADSR2 Off/On parameter remains in the stable VST3 catalogue for compatibility but is removed from the accessible pages. New slider 415, Filter Envelope Source, selects ADSR 1, ADSR 2 or ADSR 3. Old Off/On states and automation map to ADSR 1/ADSR 2 respectively.
- Filter Envelope Source also controls the formant-envelope source, matching the historical Filter ADSR2 behaviour.
- ADSR3 pitch release follows Pitch Release Direction with its own release state, so circular Pitch Env blending remains continuous through note-off.
