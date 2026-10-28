## TEST106F — Alt+Q page 2: Enter toggles Length 101 with per-step recall (0.99.55)

In Alt+Q, press **2** (Length) and then **Enter** to switch between legato Length 101 and the actual previous length of each step. For example, **37 → 101 → 37** or **80 → 101 → 80**. Each selected step keeps its own length; selected groups toggle independently. Editing a normal length updates that step's remembered value. The memory is kept by step/pattern copying, Undo/Redo and preset/REAPER-project saves. Serialized sequencer format 5 reads formats 1–4. For an old preset with an already active 101 and no saved previous length, the first return uses 50. Page 1 (Note) and page 3 (Velocity) are unchanged. The continuous Length 101 playback introduced in TEST106E is preserved.

## TEST106E — Legato 101 continuo tra step e nel loop (0.99.55)

Il sequencer Alt+Q ora mantiene l'inviluppo della voce mono fra note diverse quando lo step precedente usa Length 101, anche se il normale Mono Note Mode è impostato su Retrigger. Su note uguali, non viene emesso un nuovo Note On e il gate resta continuo. Gli step consecutivi con 101 possono proseguire senza limite e oltre il confine 16 → 1 in riproduzione ciclica; il successivo step di lunghezza normale determina il rilascio del gate. Pausa e Stop rilasciano. Il normale suono delle note non legate, Pattern/Song, browser e Help/About restano invariati. La memoria Invio per Length (pagina 2) è stata aggiunta separatamente nella TEST106F; il comportamento Invio della pagina Note rimane invece quello della TEST106E, senza importare la TEST107.

## Version 0.99.55 — 27 October 2026 (TEST106D)

The built-in eight-language Help and accessible About have been brought up to date with the TEST106C source. About (Alt+A) and the VST3 version metadata now report **0.99.55** and **27 October 2026**. The Help documents 16 patterns per track (48 total), the independent three-track Song Editor, selection and clipboard shortcuts, Delete deselection, and preset browser recall of the **last confirmed load only**. The previous 0.99.54 release notes remain available as historical documentation. This is a documentation/version update: DSP, parameters, sequencer operation and preset formats have not been changed.

In About, use **Ctrl+A** then **Ctrl+C** while the read-only text has focus to copy the release information. **Alt+H** opens the language selector for Help. This TEST106C-derived build still uses Pause ↔ C4 (60) on Enter in the Note page; it does not include TEST107's value-memory patch.

See `Doc/RELEASE_NOTES_0.99.55.md`.

## TEST106C — Preset browser: ultima selezione caricata permanente

Questa variante deriva dalla **TEST106 fornita** (non include la TEST107 su Note/Length). Alt+B riparte dalla cartella e dalla riga dell'ultimo preset **effettivamente caricato e confermato**, anche nella stessa istanza e dopo mesi di inattività. Scorrere, ascoltare un'anteprima o uscire senza Invio non cambia questa posizione; nemmeno il salvataggio di un preset la modifica. La memoria è permanente e distinta dai progetti REAPER e da LB-303. Rinomina e cancellazione aggiornano o invalidano il percorso. Se non esiste alcun preset precedentemente confermato e valido, il browser parte dall'inizio della libreria. Nessun preset viene caricato automaticamente all'avvio del VST.

Nessuna modifica al motore audio, all'editor degli step o alla disposizione dei parametri.

# TEST106 — Delete deseleziona tutti gli step
In Alt+Q, nella vista Steps, **Delete** deseleziona tutti gli step (anche selezioni non consecutive), non cancella note né valori e annuncia "All steps deselected" via NVDA. Il focus rimane sullo step corrente. Nel Song Editor Delete continua a rimuovere righe, mentre nel Parameter Lock picker rimangono le sue scorciatoie. Nella pagina Parameters, quando non esiste alcuna selezione di step, Delete conserva l'eliminazione del lock corrente.

**TEST105:** Correzione selezione step nell’editor Alt+Q: Invio sulla pagina Note commuta tutte le note esplicitamente selezionate fra pausa e C4, ogni nota in modo indipendente. Su/Giù e Pagina Su/Giù modificano i valori degli step selezionati in modo relativo, conservando le differenze originali; le pause restano pause durante la trasposizione di gruppo. Backspace reimposta gli step selezionati senza cancellare Parameter Locks; pagina 7 Parameters resta individuale. Ogni modifica collettiva è annullabile in un colpo.

**TEST104:** Nel Song Editor (Alt+Q, Alt+S) soltanto **Shift+Spazio** attiva o disattiva la selezione puntuale; Ctrl+Spazio non è più assegnato alla selezione. Quando una sola riga è esplicitamente selezionata, NVDA annuncia `Selected Pn, x of y` anche tornando su di essa con le normali frecce e Tab riferisce `1 selected`. La selezione singola resta memorizzata mentre si naviga, fino a deselezione o alle operazioni già previste. Spazio non modificato continua a gestire l'anteprima se Alt+P lo abilita. Invariati audio, Song, 48 pattern, preset e routing.

**TEST103:** Dentro Alt+Q → Alt+S (Song), **Shift+Spazio** seleziona o deseleziona esplicitamente anche un solo elemento della colonna corrente. **Ctrl+Spazio** non è più un'alternativa; copia, taglio e Delete rispettano la selezione esplicita di un solo elemento anche dopo aver spostato il cursore. Se nessuno è selezionato, il comando agisce sull'elemento focalizzato. NVDA annuncia l'azione `Selected P...` o `Deselected P...`; Spazio da solo conserva il controllo dell'anteprima secondo Alt+P. Nessuna modifica audio o preset.

**TEST102:** Accessible three-column Song Editor inside Alt+Q / Alt+S, independent Layer 1, Layer 2 and Noise playheads, Alt+M Pattern/Song selection, Ctrl+Up/Down pattern switching, discontinuous Song copy/cut/paste, independent Loop and preset persistence. See `Doc/TEST102_THREE_TRACK_SONG_EDITOR.md`.

**TEST101:** In Alt+Q Ctrl+C/X/V operates on selected steps, Ctrl+Shift+C/X/V on the current pattern only (with Windows clipboard); see `Doc/TEST101_CLIPBOARD_SHORTCUTS.md`.

# LJuno-116

## TEST96 — Sequencer keyboard navigation (after TEST95)

In the Alt+Q step editor, **Tab / Shift+Tab** move through 16-step blocks only within the selected sequence's active **Start Step / End Step** limits (Z/X and C/V). At the first/last active block the keys are silent. For example, Start 1 / End 32 exposes just the 1–16 and 17–32 blocks; Start 17 / End 32 exposes only 17–32. On a changed sequence or a reduced active range, the editor moves the cursor to the nearest valid step. Step keys outside the active range are silent and cannot modify inactive steps. Hidden step data are preserved.

In **Page 1 — Note**, **Enter** toggles the current step between **Pause (0)** and **C4 (60)** and announces the result. In **Page 7 — Parameters**, Enter still opens the Parameter Lock picker; other pages are unchanged. This patch changes no DSP or parameter IDs.


## Release 0.99.54 — 14 October 2026

This source release updates the built-in Help in eight languages and the About/version metadata.
Since the publicly announced 0.99.53 release, LJuno-116 adds:

- **Degrade**: four host-automatable FX parameters (Amount 0–100%, Bits 1–16, Hold 1–64 samples, Jitter 0–100%). At Amount 0 the module bypasses processing. The direct stereo input is excluded.
- **Sequencer step editor button**: accessible on every parameter page, placed immediately after the Value control; activate it with the keyboard or **Alt+Q**.
- **Sequencer Parameters picker**: **Alt+Enter** toggles the selected step Parameter Lock in place; Enter adds and returns to the step.

**Previously available but omitted from the 0.99.53 email**: Delay 2 (dual-tape stereo delay) and Reverb 2 (Open Courtyard eight-line FDN reverb). Both retain the original engine choices for old presets and use contextual FX parameters.

**Grid rows**: Alt+comma decreases and Alt+period increases rows per column (default eight); the setting persists between sessions.

For public GitHub release text, see `Doc/RELEASE_NOTES_0.99.54.md`.


LJuno-116 is an accessible polyphonic synthesizer being migrated from JSFX to a
cross-platform JUCE VST3 plug-in.

The project is in an early migration stage. The original JSFX remains the sound
reference; the C++ plug-in must not be considered sonically complete until the
DSP comparison tests pass.

The current C++ engine includes both oscillators, oscillator interaction and
wave-mod controls, continuous oscillator morph, both five-step SuperWave
generators, the vowel formant bank, ADSR1/ADSR2 routing, pitch and filter
envelopes, resonant 12/24 dB low-pass and high-pass filters, key and velocity
filtering, independent pre-main LP/HP filters for Layer 1, Layer 2 and Noise, including independent 12/24 dB local LP slopes,
tracking, two tempo-aware LFOs, noise, pan routing, portamento, drift, and pitch
bend. Mono trigger/legato operation, mono portamento modes, unison, and keyboard
split routing are also active. The dry stereo input path, JSFX chorus, tape
delay, six-band EQ, compressor, sidechain routing, glue stage and eight-line
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
The final routing/performance page order places Global immediately after SuperWave L2, followed by Arp, Arp Modulation, Arp 2 and Sequencer.

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
- `Alt+P`: toggle the destination of Space between **DAW** (default; REAPER
  transport) and **Sequencer preview**. NVDA announces the current choice.
  In Preview mode, Space starts/stops all LJuno sequencer lanes whose Global
  Note Source is Sequencer. This works from the parameter pages, the Alt+Q
  step editor, its parameter picker, and the preset browser. Closing Alt+Q
  does not stop the preview, so synthesis parameters can be edited while
  playing; closing the plug-in window stops the preview. Switching back to
  DAW also stops any playing preview. This preference belongs only to the
  current editor instance; it is not saved with presets or projects.
  **TEST99:** while Sequencer Preview is playing, MIDI Note On changes only
  the pitch transposition of Layer 1, Layer 2 and Noise, without restarting
  their patterns or stopping Noise. Note Off leaves the preview playing at the
  last selected pitch. The same rule applies to mono and Poly sequencer input.
  Outside preview, the existing MIDI Input Modes still operate normally.
- `Alt+Shift+P` / `Alt+N`: previous or next parameter page.
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
- `Alt+B`: open the internal preset browser. Enter opens a category or confirms
  a preset; Backspace returns to the parent category but never above LJuno-116.
  Up/Down moves one entry, Page Up/Page Down moves ten, and Home/End selects the
  first or last entry. Letters and digits jump to the next preset beginning
  with that character and cycle through further matches; the displayed row
  number is not part of the search name. F2 renames a user preset or folder.
  The Applications key or Shift+F10 opens `Preset actions`, with `New folder`
  and `Rename`. Delete opens a Yes/No confirmation for user presets. Selecting
  a preset previews it immediately, and Enter confirms the current preview.
  If the sequencer preview is playing, changing a preset lets its selected
  patterns be heard; if stopped, browsing does not auto-start playback.
  `Alt+P` and Space have the same transport meaning here as in the grids.
  `Alt+C`, Escape, the Close button, or closing the editor cancel the preview
  and restore the complete sound from before the browser was opened.
- `Factory` is read-only. Nothing in `Documents/LJuno-116/Factory` can be
  overwritten, renamed or deleted, and new folders cannot be created there.
- `Alt+S`: open Save preset. Save uses the last writable folder visited in the
  preset browser. If no writable folder is remembered, it uses the
  `Documents/LJuno-116` root. Loading a Factory preset never makes Factory the
  save destination. If the target name already exists, an accessible Yes/No
  overwrite confirmation opens with No selected by default.
- The preset browser remembers its last folder, selected entry and last writable
  user folder. Reopening it returns to that position when possible.
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


## Dual delay and reverb engines

The VST3 now exposes three-state engine selectors for the shared effects:

- **Delay Type**: Off, Delay 1 (the existing LJuno delay), Delay 2 (the LWS-7 compact dual-tape delay).
- **Reverb Type**: Off, Reverb 1 (the existing LJuno reverb), Reverb 2 (the LWS-7 Open Courtyard 8-line FDN reverb).

Values 0 and 1 keep their previous meanings, so existing presets that used Off/On continue to select Off/Delay 1 and Off/Reverb 1. On the FX page the delay controls are contextual: Off shows only Delay Type; Delay 1 shows the original Time, Sync, Feedback, Tone, Mode, Mix and LFO controls; Delay 2 hides those Delay-1-only controls and exposes its own Tape Glide, Tape 1/2 Speed, Tape 1/2 Feedback, Tape 1/2 Filter, Stereo Spread and Tape Drive controls. Delay Mix remains the shared dry/wet control and is labelled for the selected engine. Delay 2 Tape 1/2 Feedback use the same 0..2 linear law as the original delay: normal settings decay smoothly, while the upper range can reach unity feedback and sustain a loop. Reverb 2 contextually reuses the existing reverb sound controls: Predelay becomes Distance, XOver becomes Open Sky, Bass Multiplier becomes Warmth, Decay becomes RT60, Damping becomes Tail Tone, Early Level becomes Early Reflections, the magnitude of Early Pan becomes Tail Motion, and Early Ratio becomes Body Volume. Width and Wet remain Width and Mix.


## Degrade (VST3 FX page)

LJuno-116 exposes four independent, host-automatable parameters copied from
LSampler-24's Degrade algorithm: **Degrade Amount** (0–100%, default 0),
**Degrade Bits** (1–16 bits, default 8), **Degrade Hold** (1–64 samples,
default 4) and **Degrade Jitter** (0–100%, default 0). Amount blends in the
low-bit-rate, sample-held signal; Bits and Hold scale progressively with Amount,
and Jitter varies the hold duration without randomising pitch.

The module runs after the synth/FX mix as an FX insert on the four internal
stereo stems (Layer 1, Layer 2, Noise and wet FX), before the linked glue and
limiter. This preserves the existing master/aux routing. Direct stereo input is
not degraded, matching its existing finaliser bypass. Amount 0 entirely skips
the module and preserves the prior synth sound/CPU path. Existing patches omit
these new parameter IDs and therefore load Amount 0.

The user-facing sources in this archive intentionally retain their existing
VST3 generated catalogues. When the original JSFX/Lua reference sources are
available again, `Tools/generate_parameters.py` also knows about IDs 420–423
and preserves the four rows of the FX page.

### TEST90 – Global pre-Chorus FX distortion

The FX page starts with **FX Distortion Type**, **FX Distortion Amount (0–100)**
and **FX Harmonics Even Odd (-100 to +100)**. Twelve distortion characters
plus Off are available, from Warm Tube and Tape to Wave Fold and Broken Radio.
The sound is shaped **before Chorus, Delay and Reverb sends**, while the three
OSC saturators and existing aux/stem routing remain intact. Type Off or Amount
zero preserves the previous audio path. The bipolar balance says Even, Both or
Odd to screen readers. See `Doc/TEST90_FX_DISTORTION_PRE_CHORUS.md`.



## TEST91 — Independent keyboard scaling

See `Doc/TEST91_KEYBOARD_SCALE.md` for the three new OSC Keyboard Scale sliders (L1/L2/Noise), defaults 100 and C4 anchor at zero.


## TEST92 — Triangle Classic Tine

The existing `Metal L1`/`Metal L2` sliders now add a dedicated progressive wavefold and tuned overtone shimmer on Triangle Classic instead of applying the Sine Metallizer to this waveform. `Metal 0` is transparent; the Sine Metallizer and Triangle Sharktooth/`Shark Harm` remain as before. The same triangle treatment is available inside Morph and SuperWave steps. No new parameter IDs, and no change to the FX/chorus routing. Older Triangle Classic presets with nonzero Metal will intentionally have a changed character; see `Doc/TEST92_TRIANGLE_TINE.md`.

## TEST95 — LFO BPM Sync (LJuno-116)

LFO 1 and LFO 2 now each have an independent **BPM Sync** Off/On parameter next to Frequency. Defaults **On** to preserve LJuno's original always-tempo-scaled LFO behavior. Off uses an absolute frequency in Hz. See `Doc/TEST95_LFO_BPM_SYNC.md`.


### TEST97 — 16 pattern indipendenti per traccia

LJuno offre 16 pattern per ciascuna delle tre tracce Layer 1, Layer 2 e Noise:
48 pattern totali, fino a 128 step ognuno. Nella pagina **Sequencer** i tre
selettori VST3 automatizzabili sono **Pattern Layer 1**, **Pattern Layer 2**
e **Pattern Noise**. Il cambio di un pattern non cambia gli altri.

Nell'editor step (**Alt+Q**), **Home/End** cambiano traccia, mentre
**Ctrl+Freccia su/giù** cambiano avanti/indietro il pattern della traccia
selezionata (1–16, limite silenzioso). **Z/X/C/V** e **Tab** seguono
Start/End del pattern attivo. Tutti gli step, i Parameter Locks, le
configurazioni e i pattern selezionati vengono memorizzati nei preset
LJuno e nei progetti REAPER. Le tre sequenze originarie restano
rispettivamente nel Pattern 1 di Layer 1/Layer 2/Noise quando si carica
uno stato della TEST96.

**Nota:** non sono state introdotte nuove sorgenti sonore: 48 pattern sono
memoria per tre tracce che suonano in parallelo, non 48 tracce audio.


### TEST100 — Ctrl+A selects the focused 16-step sequencer page

With Alt+Q open, **Ctrl+A** selects the 16 steps mapped to Q..I/A..K on
the currently focused Tab page, not all 128 steps of the pattern. Previous
step selections are cleared. Steps outside the current pattern's active
Start/End range are never selected. NVDA announces the selected range and
count. The previous step editing, pattern clipboard, 48 patterns, MIDI
preview and Alt+P behaviour are unchanged.


### TEST106C - Browser: ricorda soltanto un caricamento confermato
- Il browser riapre sull'ultimo preset effettivamente caricato con Invio o comandi di caricamento.
- Navigazione/anteprima non confermate non spostano la memoria, neppure nella stessa istanza.
- Salva preset è distinto da carica preset; l'operazione di salvataggio non aggiorna la posizione ricordata.
- La memoria rimane persistente tra progetti, istanze e riavvii.
- I vecchi campi di navigazione non confermata sono ignorati e ripuliti.
