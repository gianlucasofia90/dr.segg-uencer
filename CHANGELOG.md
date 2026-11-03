# Changelog

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
