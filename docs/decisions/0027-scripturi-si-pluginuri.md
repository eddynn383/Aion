# ADR 0027 – Scripturi și plugin-uri

- Stare: acceptat (2026-10-27)
- Context: utilizatorul vrea plugin-uri și scripturi peste model și comenzi.

## Decizii

- **JavaScript cu QJSEngine** (parte din QtQml, deja în proiect): merge la fel pe desktop și pe Web, fără un
  interpretor separat (Python ar fi cerut altă dependență și nu merge simplu în WebAssembly).
- **API mic și stabil** (`aion`, `render/ScriptEngine.h`): primitive, obiecte dinamice, plasare, materiale, linii și
  fețe, selecție, comenzi. Totul trece prin `Document`, deci prin aceleași operații (și verificări) ca interfața.
- **O rulare = un pas de undo** (`Document::beginMacro` / `endMacro`: o tranzacție deschisă, în care `runOnModel` nu mai
  deschide alta); la eroare sau la depășirea timpului (15 s, `QJSEngine::setInterrupted` dintr-un fir de supraveghere)
  tranzacția se anulează.
- **Un motor nou pe rulare**: scripturile nu-și lasă starea una alteia; plugin-urile se încarcă din `main.js` la
  fiecare comandă.
- **Plugin-uri ca dosare** cu `manifest.json` (nume, versiune, comenzi → funcții) și `main.js`, în dosarul aplicației;
  comenzile apar în palette. Documentația: `docs/scripting.md`; exemplu: `examples/plugins/gard/`.

## Limite

- Scripturile nu pot cere date interactive (dialoguri) și nu au acces la fișiere sau rețea.
- Generatoare noi de obiecte dinamice din plugin-uri: încă nu (generatoarele sunt în core).
