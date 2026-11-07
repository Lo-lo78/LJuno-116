# LJuno-116 0.99.56 TEST122 — Quattro reverberi da LSampler-24

Base: TEST121. Nessuna variazione alla struttura del sequencer, LArp, oscillatori,
LFO, routing, preset esistenti e parametri già presenti.

## Reverb Type — pagina FX

| Valore | Tipo | Motore |
|---|---|---|
| 0 | Off | Invariato |
| 1 | Reverb 1 | LJuno originale, invariato |
| 2 | Reverb 2 | LWS-7 Open Courtyard, invariato |
| 3 | Studio Room | LSampler-24, FDN otto linee |
| 4 | Concert Hall | LSampler-24, FDN otto linee |
| 5 | Plate | LSampler-24, FDN otto linee |
| 6 | Cathedral | LSampler-24, FDN otto linee |

I quattro nuovi algoritmi sono stati portati dal codice sorgente di LSampler-24
TEST178 (`FXReverb.h` + predelay/early reflections/gate di `FXEngine.h`).
La matrice Hadamard normalizzata a otto linee, la variazione della dimensione
per tipo, le sei prime riflessioni, la compensazione di tono e l'eventuale gate
sono identici nella struttura all'algoritmo del campionatore. Sono stati
adattati ai parametri già disponibili in LJuno, rispettando le loro unità.

## Controlli comuni (nessun ID nuovo)

- Reverb Predelay (slider141): millisecondi, 20–100 nella versione attuale.
- Reverb XOver / Open Sky (slider142): 50–1000 Hz.
- Reverb Bass / Warmth (slider143): 0,5–2,0.
- Reverb Decay RT60 (slider144): secondi, fino a 30 s (adattamento del limite di LSampler-24).
- Reverb Width (slider146): 0–1, corrispondenti a 0–100%.
- Reverb Early Reflections (slider148): 0–1, corrispondenti a 0–100%.
- Reverb Send L1, L2, Noise (slider371–373): indipendenti, invariati.
- Reverb Comp (slider151–156 e 161): continua a operare sul ritorno wet;
  per i nuovi tipi non modifica il dry bus.

## Sei nuovi parametri — visibili solo con Reverb Type 3–6

| ID | Nome | Intervallo | Default |
|---|---|---|---|
| slider446 | Reverb Room Size | 0–100% | 80 |
| slider447 | Reverb FDN Damping | 0–100% | 10 |
| slider448 | Reverb Tail Tone | 0–100% | 55 |
| slider449 | Reverb Gain | -24–+24 dB | 0 |
| slider450 | Reverb Gate Threshold | -80–0 dB | -80 (Off) |
| slider451 | Reverb Gate Decay | 0–5000 ms | 300 |

La vecchia Reverb Damping in Hz (slider145) rimane disponibile soltanto nei
Reverb 1 e Reverb 2: **non è la stessa cosa** del controllo FDN Damping in %.
Allo stesso modo Early Pan e Early Ratio / R2 Motion/Body (149 e 150) rimangono
esclusivi delle due modalità originali. I nuovi parametri sono disponibili
nella griglia FX tramite Alt+L e come normali parametri VST3 automatizzabili.

## Compatibilità e prestazioni

Gli ID 0–445 sono invariati, come anche i valori delle modalità Reverb Off/1/2.
Gli ID 446–451 vengono aggiunti in fondo al catalogo per non riordinare
l'automazione esistente. Nessuna modifica ai dati musicali e a preset/step.
Il suono delle modalità 1 e 2 non è stato cambiato. Con Reverb Off il nuovo DSP
non viene richiamato. La rete nuova alloca i propri buffer esclusivamente in
`prepare()`, non durante il processamento audio, e usa una firma di parametri
per evitare il ricalcolo costoso dei coefficienti nel loop di ogni campione.
Quando non è presente né ingresso né coda attiva, l'algoritmo torna inattivo.
I parametri nuovi usano default neutri e sono applicati solo ai modi 3–6.

## Controlli

- Test C++ indipendente da JUCE su tutti e quattro gli impulsi, con valori finiti,
  mono Width=0, gate, RT60 lungo e reset.
- Test Python di collegamento tra catalogo, pagine, UI, motore e Actions.
- La build completa VST3 Windows e l'ascolto in REAPER con OSARA/NVDA richiedono
  GitHub Actions e verifica sul computer reale.
