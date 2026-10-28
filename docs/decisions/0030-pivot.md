# ADR 0030 – Pivot pentru obiecte și geometrie

- Stare: acceptat (2026-10-27)
- Context: rotația și scara din Inspector se făceau față de colțul minim (obiecte) sau de centru / colț (geometrie
  liberă). Utilizatorul vrea un pivot, implicit în centrul cutiei de încadrare, pentru orice primitivă (1D, 2D, 3D).

## Decizii

- **Obiectele țin pivotul în model:** `Instance::pivot` (opțional, în coordonatele definiției; gol = centrul cutiei,
  deci urmează geometria când e editată). Se salvează în `.aion` (`pivot`) doar când a fost ales.
- ~~**Poziția rămâne colțul minim.**~~ Schimbat pe 2026-10-28 (decizia utilizatorului): **Poziția e a pivotului**, vezi
  completarea de la final. Rotația, scara și dimensiunile țin pivotul pe loc (`model::setPlacement`). Un pivot nou nu
  mută obiectul. Inspector-ul trimite câte o singură valoare, deci cazurile nu se amestecă.
- **Pivotul se arată ca distanță de la colțul minim, pe axele obiectului, în mm** (cu scara aplicată): o cutie
  100 × 100 × 50 are pivotul la 50, 50, 25. Presetări: Centru, Bază (centrul feței de jos), Colț.
- **Geometria liberă** (forme, linii) are pivotul ales ținut în Document, pentru selecția curentă (ca rotația și scara
  cumulate); o selecție nouă revine la centru. Mutarea geometriei mută și pivotul.
- **Marcaj în viewport:** un punct cu trei axe scurte (RGB), desenat deasupra geometriei, ca să se vadă și în
  interiorul obiectului.
- Scripturile: `aion.rotate(id, grade)` rotește în jurul pivotului obiectului.

## Limite

- ~~Uneltele Rotate / Scale își aleg centrul ca până acum~~ — schimbat pe 2026-10-28, vezi completarea de mai jos.

## Completare (2026-10-28)

- Marcajul se trage cu Select (`Viewport::m_pivotDrag`, `Document::setSelectionPivot`): se prinde de cele 27 de
  puncte ale cutiei selecției (`pivotSnapPoints`: colțuri, mijloace, centre de fețe, centru), altfel de suprafața de
  sub cursor, altfel de un plan prin pivot cu fața spre cameră. Toată tragerea e un pas de undo (macro).

## Completare (2026-10-28): uneltele Rotate / Scale

- Bug raportat: cu pivotul în colțul unui cub, Rotate din toolbar rotea cubul în jurul punctului ales cu primul click,
  nu în jurul pivotului (cubul se mișca pe un arc). `ToolContext::pivot` = pivotul selecției (`Document::selectionPivot`).
- **Rotate**: cu o selecție, centrul e pivotul; raportorul stă pe el de la început, iar primul click alege direct
  referința (unghiul 0). **Option** la primul click = alege alt centru; colțurile, mijloacele muchiilor obiectelor și
  pivotul sunt puncte de prindere pentru centru (și pentru referință). Copierea (Option) se decide la ultimul click.
- **Scale**: punctul fix rămâne mânerul opus; cu **Option**, pivotul selecției (implicit centrul cutiei, deci
  neschimbat pentru un pivot nemutat).
- Test: „Unelte: Rotate și Scale pornesc din pivotul selecției” (`tests/core/ToolTests.cpp`).

## Completare (2026-10-28): pivotul în Animare

- Bug raportat: pivotul își schimba poziția la trecerea între moduri. Cauze: marcajul tras în Animare se raporta la
  transformarea de bază (nu la poza de la timpul curent), iar un pivot nou muta obiectele animate (cheile de poziție
  țin poziția pivotului). Acum `Document::shownTransform` dă poza (fără copia modelului, merge și în timpul tragerii),
  iar `model::setPivot` recalculează cheile de poziție ale animațiilor (ADR 0034).

## Completare (2026-10-28): Poziția = pivotul; pivotul selectabil

- Utilizatorul a văzut valori diferite în timeline (poziția pivotului) și în Inspector (colțul minim) pentru același
  obiect și a ales: **Poziția din Inspector e a pivotului** (ca în Blender), pentru obiecte și pentru geometria liberă;
  o poziție nouă mută obiectul ca pivotul să ajungă acolo. Scripturile (`place(id, { x, y, z })`) și regulile
  „Mută la” la fel; `formatVersion` 3 migrează ținta regulilor din fișierele vechi (colțul minim → pivotul).
- **Pivotul se selectează**: click pe marcaj (fără tragere) → `Document::pivotSelected`; marcajul primește un inel, iar
  Inspector-ul arată „Pivot selectat” cu coordonatele lui (ca Poziția), editabile: mută doar pivotul
  (`Document::setPivotPosition`). Esc, ✕ sau un click în altă parte îl deselectează. Grupul „Pivot” rămâne distanța de
  la colțul minim (cu presetările Centru / Bază / Colț).

## Completare (2026-10-28): fără „Pivot selectat”; tragerea se prinde ca Move

- „Pivot selectat” arăta aceleași cifre ca Poziția (același punct, aceeași referință), cu alt efect: utilizatorul l-a
  găsit confuz. A dispărut, cu tot cu starea `pivotSelected`. Pivotul se schimbă în două feluri: **trăgând marcajul**
  sau din grupul **„Pivot (mm, de la colțul minim)”** (care există și pentru geometria liberă). Poziția rămâne a
  pivotului; pentru mai multe obiecte selectate pivotul e doar centrul selecției (nu se păstrează; un pivot comun =
  un grup).
- Tragerea marcajului (cu Select) folosește o unealtă internă, `pivot` (`tools::PivotTool`, nu e în toolbar):
  `Document::beginPivotDrag` o pune în locul lui Select până la eliberare. Se prinde ca Move — inferența (capete,
  mijloace, muchii, fețe, axele din pivotul de pornire, ghidaje, grid) și punctele obiectelor de sub cursor / din jur
  — plus centrele fețelor văzute și centrul cutiei selecției și originea. Unealta nu schimbă modelul: trimite punctul
  prin `ToolRequests::pivot` (previzualizarea marcajului), iar la eliberare Document apelează `setSelectionPivot` (un
  pas de undo; în Animare, prin poza de la timpul curent). Înainte se prindea doar de cele 27 de puncte ale propriei
  cutii și de suprafețe.
