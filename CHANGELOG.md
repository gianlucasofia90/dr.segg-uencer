# Changelog

## [0.1.3] - 2026-11-03
- **MIDI MAP**, nuova pagina con due tabelle da 16 righe.
- Input: note, CC e pitch wheel muovono qualsiasi parametro, con Min, Max, Absolute o Toggle. MIDI Learn dal tasto della riga o con clic destro su un controllo.
- Output: ogni traccia invia CC o pitch wheel, dai segnali Note, Velocity, Random, Gate, Position X e Position Y, con Min, Max e Slew. La pitch wheel torna al centro con lo stop.
- Nuovo interruttore NOTES su ogni traccia: spento, la traccia lavora solo come modulatore.
- Nuovo preset `CC Lanes`.
- ROUTING su Windows: istruzioni per loopMIDI, perché Windows non ha porte virtuali proprie. Le porte nuove compaiono da sole.
- Le porte che non si aprono ora sono segnate `UNAVAILABLE` invece di restare mute senza spiegazioni.
- Su macOS e Linux una seconda istanza crea la sua porta virtuale invece di fallire.
- Corretto: su Windows una porta virtuale salvata su Mac viene segnalata come non disponibile.

## [0.1.2] - 2026-11-03
- Finestra ridimensionabile (dal 60 al 200 %) con le proporzioni bloccate; la dimensione si salva con il progetto.
- Grafica più nitida: a ogni dimensione l'interfaccia viene ridisegnata in vettoriale, e il font pixel cade sulla sua griglia.
- Margini uguali ovunque: 16 px dai bordi e tra i pannelli; il deck in basso è un pannello come gli altri.
- SYNC e TEMPO in un'unica console nell'header; RUN accanto, in modalità FREE.
- Testo dei menu a tendina (SYNC, OUTPUT ...) sempre dentro il riquadro.
- Interruttori spenti più visibili sul deck scuro.
- Test: `--snap` accetta la scala della finestra come ultimo argomento.

## [0.1.1] - 2026-11-03
Nuova interfaccia **Egg Lab**, sorella di quella di SONIC GRAIN: stessi caratteri, piastre, manopole e spaziature, cambia solo lo sfondo.
- Fondo rosso con fascia diagonale, scacchiera e anelli giallo e cromato; header e deck con bordo a strisce di pericolo; deck antracite.
- Pannelli in vetro quasi opaco; display con cornice nera, letture negli angoli (Euclid) e LOCK in grande (Turing).
- Un colore, un significato: tutte le note in giallo, il passo che suona con un contorno rosso, i pad negati tratteggiati e scritti.
- Lista tracce: tracce attive su piastre nere, quella selezionata in giallo.
- ROOT, SCALE, DENSITY e CHAOS passano nella striscia GLOBAL sotto le tracce; l'header tiene SYNC, TEMPO e RUN.
- Controlli dei sequencer su una griglia di 4 manopole, menu e interruttori sotto.
- Script `scripts/release.ps1` / `release.bat` (Windows) e `scripts/release.command` (macOS) per pubblicare una versione.

## [0.1.0] - 2026-11-03
Prima versione.
- 16 tracce che suonano insieme, ognuna con tipo di sequencer, clock, lunghezza e destinazione propri.
- Cinque sequencer: Euclid, Step, Turing, Cartesian, Physics.
- Comuni a ogni traccia: Rate (anche terzine, quintine, settimine), Swing, Drift, Gate, Velocity, Chance, Roll, Mutate, Scale, Chord, Delay, Seed.
- Link tra tracce: Reset, Clock, Transpose, Follow, Choke.
- Uscite per traccia: Host oppure 16 porte dirette (interfaccia MIDI o porta virtuale).
- 11 preset di fabbrica e preset utente.
- Sync DAW / FREE, Density e Chaos globali, trasposizione da MIDI in ingresso, Panic.
- Formati: VST3 (macOS, Windows, Linux), AU, AAX (non firmato). VST2 solo con SDK Steinberg proprio.
