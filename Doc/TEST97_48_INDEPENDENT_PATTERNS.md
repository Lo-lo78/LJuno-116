# LJuno-116 TEST97 — 48 pattern indipendenti

Tre tracce di sequencer (Layer 1, Layer 2, Noise) con 16 pattern per traccia.

- Parametri VST3 nuovi: slider443 (Pattern Layer 1), slider444 (Pattern Layer 2), slider445 (Pattern Noise), range 1–16. Le automazioni pregresse restano inalterate.
- Nell'editor Alt+Q: Home/End cambiano traccia; Ctrl+Su/Giù cambia il pattern della traccia corrente.
- Z/X/C/V e Tab rispettano lo Start Step/End Step del pattern selezionato.
- Il motore conserva tre runtime di sequenza; ciascuno legge dal relativo pattern attivo. La selezione può essere automatizzata.
- Formato dati sequencer LSQ2 versione 4, include selettori 3×16 e 48 blocchi completi con Parameter Locks. Le versioni 1–3 precedenti (16 blocchi) vengono caricate nei primi 16 slot: i tre pattern 1 sono in posizioni 0,1,2.
- Undo, copia/incolla, preview, salvataggio preset/progetto comprendono le nuove banche.
- TEST97 richiede ancora build Windows in GitHub Actions e verifica audio/NVDA in REAPER.
