# LJuno-116

LJuno-116 is an accessible polyphonic synthesizer being migrated from JSFX to a
cross-platform JUCE VST3 plug-in.

The project is in an early migration stage. The original JSFX remains the sound
reference; the C++ plug-in must not be considered sonically complete until the
DSP comparison tests pass.

The current C++ engine includes both oscillators, oscillator interaction and
wave-mod controls, continuous oscillator morph, both five-step SuperWave
generators, the vowel formant bank, ADSR1/ADSR2 routing, pitch and filter
envelopes, resonant 12/24 dB low-pass and high-pass filters, key and velocity
tracking, two tempo-aware LFOs, noise, pan routing, portamento, drift, and pitch
bend. Mono trigger/legato operation, mono portamento modes, unison, and keyboard
split routing are also active. The dry stereo input path, JSFX chorus, tape
delay, five-band EQ, compressor, sidechain routing, glue stage and eight-line
RC reverb are active as well. The integrated LArp and final sonic parity work
still require migration.

The Osc page places Noise Type and Noise Pitch directly after Noise Level.
Classic Color is the original white/pink/brown Noise Color path and remains the
default; the classic white, pink and brown modes intentionally have no pitch.
Radio Static, Radio Tuning and Radio Crackle provide disturbed-radio textures.
The Biscot SID 6581, Amstrad CPC AY, 8-Bit Metallic and Digital Dust modes use a
per-voice clock that follows oscillator pitch and portamento. Noise Pitch sets
the base tuning, while LFO1 Noise Pitch and LFO2 Noise Pitch independently
modulate it from their respective pages. Noise Color is a centred 0..1 control:
0.5 preserves each generator's native character, with darker and brighter
settings available for every type. Noise Stereo runs from mono at its default
zero to independently generated stereo at one.

The Arp 2 page ports Biscot1's SID-style internal frequency arpeggiator. Layer 1
and Layer 2 have independent patterns, rates, octave ranges, glide, volume and
PWM movement; LP, HP and stereo movement are shared. Its glide acts directly on
the generated pitch and is independent of the synth portamento. The latched
phrase and step sequence remain stable throughout envelope release. The two
LFOs also provide independent Upper Squash and Lower Squash controls. LSH-2's
Free mode is exposed as Free Played Order, and each layer can disable Pitch
Movement while its level/PWM and shared filter/stereo movements keep running.
The final page order is Arp, Arp Modulation, Arp 2, Global.

The audio engine enters a deep-idle path after voices, stereo input and effect
tails have become silent. In that state an empty block avoids parameter, LFO,
voice and effect processing, and wakes immediately for MIDI, audio input or a
parameter change. Mono unison follows the JSFX topology: its clones receive
independent random phase offsets on every true retrigger, and Voice Pan
Alternate spreads the clones without panning the complete mono voice left.

The original REAPER RPL library is embedded in the plug-in. On first preset
use, its 67 patches are converted to readable `.Ljuno` text files under
`Documents/LJuno-116/Factory`. User presets are stored in
`Documents/LJuno-116`; folders created there become browser categories
automatically. The custom extension identifies this synth's presets; their
contents remain plain text, and the browser never speaks the extension.

## Licence

LJuno-116 is free software licensed under `AGPL-3.0-or-later`. JUCE is consumed
under its AGPLv3 option. See `LICENSE.md`.

## Configure and build on Windows

```powershell
powershell -ExecutionPolicy Bypass -File Tools/build-windows.ps1
```

The script locates Visual Studio Build Tools and opens its C++ build
environment automatically. The first configure downloads the pinned JUCE
source dependency.

The generated plug-in is written to:

`build/nmake-release/LJuno116_artefacts/Release/VST3/LJuno-116.vst3`

## Accessible keyboard controls

The visual editor is styled as a Commodore 64 power-on screen. It uses VIC-II
power-on colours 6 (dark-blue background) and 14 (light-blue border, text and
reverse-video selection), square character-cell controls, monospaced type and a
40 by 25 visual grid. A blinking character-sized block cursor can be moved with
the mouse. It is purely decorative and is excluded from the accessibility tree.

- Opening the plug-in transfers keyboard focus to the parameter-page selector,
  because REAPER otherwise provides no reliable keyboard route into the editor.
  The transfer waits until both the host peer and native accessibility provider
  are ready, then creates a real component-focus event so the initial selection
  is immediately visible to screen readers without requiring Tab then Shift+Tab.
  The unnamed editor root is ignored by accessibility clients because REAPER
  already announces the plug-in window title; this avoids repeating the title
  before the page selector is announced.
- `Alt+P` / `Alt+N`: previous or next parameter page.
  At the first or last page the corresponding shortcut is silent. When focus
  is on the page selector, the arrow keys change only its selected page;
  `Alt+L` remains the direct command for entering the parameter list.
- `Alt+Shift+letter`: open the page beginning with that letter and focus its
  parameter list. Repeating the shortcut cycles through matching pages, such as
  Env/EQ, LFO 1/LFO 2, Filter/FX, SuperWave L1/L2 and all three Arp pages.
- `Alt+D`: focus the parameter-page selector. Its accessible description also
  exposes the shortcut.
- `Alt++` / `Alt+-`: load the next or previous preset. The order is
  deterministic across every valid preset in the library and wraps at its ends.
- `Alt+B`: open the internal preset browser. Enter opens a category or loads a
  preset; Backspace returns to the parent category but never above LJuno-116.
  Up/Down moves one entry, Page Up/Page Down moves ten, and Home/End selects the
  first or last entry. Delete opens a Yes/No confirmation with No selected;
  arrows or Y/N choose and Enter confirms. Selecting a preset previews it immediately. Enter
  confirms the current preview and closes without loading the file a second
  time. `Alt+C`, Escape, the Close button, or closing the editor cancel the
  preview and restore the complete sound from before the browser was opened,
  including an unsaved patch. The selected entry and current folder are
  announced through the active screen reader; file extensions are not exposed
  in the list. After Enter confirms a valid preset, focus moves directly to the
  parameter list.
- `Alt+S`: open Save preset. When a preset is current, its filename is filled
  in and selected, so typing replaces it and leaving it unchanged requests an
  overwrite in the same category. If the name already exists, an
  accessible Yes/No confirmation opens with No selected by default. Arrow keys
  choose Yes or No and Enter confirms. No returns to the name field; Escape,
  Alt+C, or Close cancels the save.
- The preset browser remembers its last folder and selected entry. Reopening it
  returns to that position; if the entry no longer exists, it selects the
  nearest valid row.
- `Alt+L`, `Alt+V`, `Alt+E`: focus the parameter list, focus its value,
  or type a value. The value control exposes `Alt+V` in its accessible
  description.
- Parameter names shown by the VST3 and spoken in each page come from the
  user-facing label in the second quoted argument of the Lua binding. Stable
  slider IDs and original JSFX names remain available internally for preset
  compatibility.
- `Alt+R`: reset the selected parameter to its value in the Lua Init patch.
- `Alt+I`: initialize every synth and LArp parameter using the accessible Lua
  Init patch. The graphical Initialize synth button performs the same action.
- `Alt+Up` / `Alt+Down`: change the selected parameter from anywhere in
  the editor. `Alt+Page Up` / `Alt+Page Down` applies 40 steps, and
  `Alt+Home` / `Alt+End` selects its maximum or minimum.
- On the value control, the same value commands work without `Alt`.
  Home selects the maximum and End selects the minimum. Left and Right
  change the step width through 1, 5, 10, 15, and 20.
- In the parameter list, Up and Down move within an eight-item column;
  Left and Right move between columns. Page Up and Page Down move five
  parameters. Home and End select the first or last parameter in the current
  column, while `Ctrl+Home` and `Ctrl+End` select the first or last parameter
  on the current page.
- In the parameter list, typing a letter or number selects the next parameter
  on the current page whose name begins with that character. Repeating the same
  key cycles through matches and wraps to the beginning.

## Regenerating the parameter catalogue

After changing JSFX slider declarations, run:

```powershell
python Tools/generate_parameters.py
```

Parameter IDs are based on the original JSFX slider numbers and must remain
stable after release so that host automation and saved sessions keep working.
