# TEST112 — Correzione GitHub Actions su Windows (lettura UTF-8)

Il workflow di TEST111 falliva durante `Tests/test111_altq_parameter_focus.py`, prima di compilare il VST3, con `UnicodeDecodeError` perché `Path.read_text()` usava la codifica Windows cp1252 per leggere `Source/PluginEditor.cpp` in UTF-8.

Sono state rese esplicite le letture `encoding='utf-8'` dei due sorgenti C++ e del workflow. Il nuovo controllo `Tests/test112_windows_utf8_regression.py` impedisce di reintrodurre lo stesso errore. Il nome dell'artefatto GitHub Actions è stato aggiornato a TEST112.

**Nessuna modifica al sintetizzatore**: DSP, editor Alt+Q, tasti 6–0, 64 pattern, Note/Length, Song, ID VST3 e formati preset restano esattamente come in TEST111.

La build VST3 completa deve essere verificata su GitHub Actions; questa correzione riguarda l'errore di encoding documentato nel log della TEST111.
