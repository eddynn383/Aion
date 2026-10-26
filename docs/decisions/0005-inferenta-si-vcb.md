# 0005 – Inferența (snap) și Measurements box

**Status:** acceptat
**Data:** 2026-10-26

## Context
Faza 4 trebuie să permită desenul exact fără a nimeri manual punctele: snap pe geometrie, direcții
(axe, paralel, perpendicular), aliniere cu puncte de referință, lock-uri și valori tastate (VCB).

## Decizie

### Unde stă logica
- `inference::Engine` (în `core/inference`, fără Qt) primește camera, poziția cursorului pe ecran,
  punctul de pornire opțional (`from`) și un plan de lucru opțional. Întoarce un `Inference`:
  tipul, punctul, entitatea atinsă, direcția folosită și ghidajele de desenat (`Guide`, cu o `Tint`).
- Fiecare unealtă de desen are propriul `Engine`, pentru că starea (punctele de referință, muchia de referință,
  lock-ul, histerezisul) ține de o operație. `Engine::reset()` la începutul unei operații noi,
  `resetLock()` după fiecare segment.
- Parsarea și formatarea lungimilor stau în `core/units/Length` (mm, cm, m, km, ″, ′, ft, in; virgula separă valori).

### Prioritate
1. Capăt (vârf vizibil, rază 10 px). 2. Mijloc de muchie. 3. Cu punct de pornire: intersecția unei direcții
cu o muchie, apoi intersecția unei direcții cu linia de aliniere a unui punct de referință (From Point).
4. Pe muchie, dacă e mai aproape pe ecran decât direcția. 5. Direcția (axe cu un mic avantaj, apoi paralel/perpendicular).
6. From Point pe planul punctului liber. 7. Pe față. 8. Planul de lucru (solul sau orizontala prin `from`).

- Distanțele se compară **pe ecran**: punctul de pe o direcție e cel a cărui proiecție e cea mai apropiată
  de cursor (nu cel mai apropiat de rază în 3D, care în perspectivă e vizibil decalat).
- **Histerezis:** candidatul ales anterior (același vârf, mijloc sau direcție) rămâne ales într-o rază de 1,6× mai mare.
- **Referințe:** capetele și mijloacele atinse devin puncte de referință (ultimele 4); muchia atinsă (pe muchie sau mijloc)
  devine muchia de referință pentru paralel/perpendicular. Ca în SketchUp, „le înveți” trecând cu cursorul peste ele.
- Dreptunghiul nu folosește direcții din primul colț (ar da un dreptunghi degenerat), doar puncte și aliniere,
  proiectate pe planul lui.

### Lock-uri
- Shift ținut apăsat: blochează direcția curentă (sau direcția spre punctul atins, sau planul feței). Eliberat: deblochează
  numai lock-ul pus de el.
- Săgețile: → roșie, ← verde, ↑ albastră, ↓ paralel cu muchia de referință; aceeași săgeată a doua oară deblochează.
- În timpul unui lock, un capăt atins se proiectează pe direcția blocată (cu ghidaj punctat).

### Afișare
- Ghidajele și marcajul se desenează fără test de adâncime (`ViewportFrame::topLines/topPoints`), după restul scenei.
- Etichetele (tipul inferenței, cotele live) și Measurements box sunt în QML; Viewport publică pozițiile pe ecran
  (`annotations`, `pointerPosition`). Culorile: capăt verde, mijloc cyan, pe muchie roșu, pe față albastru,
  axe în culoarea lor, paralel/perpendicular magenta.
- Measurements box: tastarea ajunge în viewport (focus la click); cât timp există text, scurtăturile dintr-o literă
  sunt dezactivate (literele sunt unități), Backspace șterge caractere, Enter aplică, Esc golește.

## Consecințe
- Inferența e testată pe scene sintetice (`tests/core/InferenceTests.cpp`), inclusiv testul „gata când” din Faza 4.
- Etichetele sunt în engleză („Endpoint”, „On Red Axis”), ca în PLAN.md; restul interfeței rămâne în română.
- Încă nu există: inferența „Center” (vine cu cercurile, Faza 5), animația marcajului, intersecția a două linii de aliniere.
