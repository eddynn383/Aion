# ADR 0023 – Oglindire, array și soften pe selecție

- Stare: acceptat (2026-10-27)
- Context: din `docs/feature-ideas.md`: Mirror / Flip pe axe, array liniar și polar cu panou, Soften / Smooth cu
  slider de unghi.

## Decizii

- **Un modul în core** (`model/Modeling.h`), pe selecția mixtă (instanțe + geometrie liberă din context), cu teste.
- **Oglindirea** e o reflexie (scară −1 pe o axă) față de centrul cutiei selecției. Instanțele primesc reflexia în
  transformare (determinant negativ; `placementOf` o arată ca scară negativă pe X). Pentru geometria liberă, bucla
  oglindită ar întoarce normala, așa că fiecare față atinsă e reorientată după normala veche oglindită.
- **Array**: copia k primește pasul aplicat de k ori. Liniar = translație; polar = rotație în jurul unei axe (cu
  rotirea copiilor) sau doar deplasarea centrului pe cerc. Copia unui grup e independentă, a unei componente legată
  (comportamentul existent al `copyInstance`).
- **Soften**: un singur flag `soft` pe muchie (ascunde muchia și netezește umbrirea), după unghiul dintre normalele
  celor două fețe; muchiile cu altă număr de fețe nu se schimbă.
- **Previzualizare prin undo**: panourile Array și Soften aplică operația ca pas normal; la fiecare schimbare,
  `Document` anulează pasul anterior și îl refac cu parametrii noi. „Gata” îl păstrează, „Anulează” / Esc îl scoate.
  Simplu și corect (aceeași cale ca operația finală), fără o geometrie „fantomă” separată.

## Limite

- Centrul array-ului polar e la o rază dată de selecție (pe X, sau pe Y pentru axa X), nu ales cu click în viewport.
- Oglindirea e mereu față de centrul selecției (nu față de un plan ales).
