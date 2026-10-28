# ADR 0032 – Index pentru picking și inferență pe modele mari

- Stare: acceptat (2026-10-28)
- Context: fiecare mișcare a mouse-ului face mai multe căutări pe geometrie (pick, apoi inferența: capete, mijloace,
  intersecții), fiecare o buclă peste toate fețele, muchiile și vârfurile. Pe un model cu 100 000 de fețe, un pick
  plus inferența durau ~1,5 s (build de debug).

## Decizii

- **`Mesh::revision()`**: un număr nou (unic în program) la orice schimbare; o copie neschimbată îl păstrează.
  Indexurile derivate din geometrie se refac doar când el se schimbă, fără observatori sau invalidări explicite.
- **BVH pe fețe și pe muchii** (`core/src/view/PickIndex.cpp`): ordonare Morton cu sortare radix, arbore prin
  înjumătățirea șirului, frunze de 8. Construcția e liniară (după sortare) și rapidă și în debug (~0,9 s pentru
  100 000 de fețe și 400 000 de muchii; înainte, cu nth_element, ~6,6 s). Vârfurile se găsesc prin muchiile lor
  (plus lista vârfurilor izolate), fără un al treilea arbore.
- **Căutări „superset”**: `forEachFaceAlong` (cutiile atinse de rază), `forEachVertexNear` / `forEachEdgeNear`
  (cutiile din jurul razei, cu o margine care acoperă raza în pixeli la adâncimea cutiei). Filtrul exact (proiecția,
  vizibilitatea) rămâne la apelant, deci rezultatele sunt aceleași ca la buclele simple (verificat de un test).
- **Prag:** indexul se folosește de la 4000 de fețe + muchii; sub prag, buclele simple sunt la fel de rapide și nu
  ocupă memorie. Cache-ul ține ultimele 128 de indexuri (modelul și definițiile mari).

## Rezultat

- 100 000 de fețe: pick + inferență ~1 ms în loc de ~1450 ms (debug). Testul `[.perf]` (ascuns) măsoară.

## Limite

- Prima mișcare după o editare a unui model foarte mare plătește reconstrucția indexului. Desenarea (triangularea și
  buffer-ele GPU) are optimizările ei separate.
