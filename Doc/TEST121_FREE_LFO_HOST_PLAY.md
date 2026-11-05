# LJuno-116 0.99.56 — TEST121 — 5 novembre 2026

## Modifica

LFO 1 (`slider025`) e LFO 2 (`slider035`) con **Mode = Free (0)** ripartono in modo deterministico a ogni nuova partenza del trasporto della DAW, senza cambiare parametri o preset. Il reset riguarda fase, Phase offset applicata come sempre, Sample & Hold e sua sequenza pseudo-casuale separata, smoothing, delay e One Shot. Include entrambi gli LFO globali e gli LFO per voce in modalità Poly (`slider439`, `slider440`).

**Mode = Triggered (1)** è invariato: non viene azzerato dal trasporto. I normali trigger delle note, inclusa la semantica preesistente di One Shot, non sono stati modificati. Non vengono azzerati oscillatori, inviluppi, note, drift, rumore, sequencer, arp, FX, delay o riverbero. Il generatore casuale generale del sintetizzatore resta indipendente dal Sample & Hold degli LFO Free.

Lo stato Play viene letto da `AudioPlayHead::PositionInfo::getIsPlaying()` prima del ritorno deep-idle. Per i DAW che smettono di chiamare il plug-in in Stop, una discontinuità nella posizione in campioni riconosce anche un nuovo avvio senza buffer Stop intermedio. Non c'è sincronizzazione continua alla posizione musicale: la frequenza degli LFO resta libera.

## Verifica consigliata in REAPER

1. Assegna un LFO a una modulazione chiaramente udibile (ad esempio Pitch L1), imposta **Mode = Free** e prova prima una forma d'onda regolare.
2. Avvia Play sullo stesso punto due volte: il movimento dell'LFO deve ripartire nello stesso modo.
3. Ripeti con **Sample & Hold** e con **Polyphonic = On**.
4. Imposta **Mode = Triggered** e verifica che mantenga il comportamento originale delle note.
5. Verifica che il preview interno e il trasporto sequencer non diventino trigger del reset Free.

## Compatibilità e test

- Nessun nuovo parametro, nessuna modifica di ID, preset o formato del progetto.
- Resta la numerazione VST3 **0.99.56**.
- Test C++ `LJunoFreeLfoTransportTest` per Play/Stop, riavvii senza buffer Stop, clock continuo, Sample & Hold e seed distinti.
- Test statico `Tests/test121_free_lfo_host_play.py` verifica l'integrazione JUCE, l'isolamento dal resto del synth e la guida in 8 lingue.
- Il comportamento sonoro reale e la build Windows devono essere confermati su GitHub Actions e in REAPER.
