# ADR 0030 – Pivot pentru obiecte și geometrie

- Stare: acceptat (2026-10-27)
- Context: rotația și scara din Inspector se făceau față de colțul minim (obiecte) sau de centru / colț (geometrie
  liberă). Utilizatorul vrea un pivot, implicit în centrul cutiei de încadrare, pentru orice primitivă (1D, 2D, 3D).

## Decizii

- **Obiectele țin pivotul în model:** `Instance::pivot` (opțional, în coordonatele definiției; gol = centrul cutiei,
  deci urmează geometria când e editată). Se salvează în `.aion` (`pivot`) doar când a fost ales.
- **Poziția rămâne colțul minim.** O poziție nouă mută colțul acolo; rotația, scara și dimensiunile țin pivotul pe
  loc (`model::setPlacement`). Un pivot nou nu mută obiectul. Inspector-ul trimite câte o singură valoare, deci cele
  două cazuri nu se amestecă.
- **Pivotul se arată ca distanță de la colțul minim, pe axele obiectului, în mm** (cu scara aplicată): o cutie
  100 × 100 × 50 are pivotul la 50, 50, 25. Presetări: Centru, Bază (centrul feței de jos), Colț.
- **Geometria liberă** (forme, linii) are pivotul ales ținut în Document, pentru selecția curentă (ca rotația și scara
  cumulate); o selecție nouă revine la centru. Mutarea geometriei mută și pivotul.
- **Marcaj în viewport:** un punct cu trei axe scurte (RGB), desenat deasupra geometriei, ca să se vadă și în
  interiorul obiectului.
- Scripturile: `aion.rotate(id, grade)` rotește în jurul pivotului obiectului.

## Limite

- Pivotul nu se trage încă din viewport (doar din Inspector). Uneltele Rotate / Scale își aleg centrul ca până acum.
