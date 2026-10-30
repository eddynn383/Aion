# ADR 0042 – Inferența pe obiecte

- Stare: acceptat (2026-10-30)
- Design: [`docs/plans/inferenta-pe-obiecte.md`](../plans/inferenta-pe-obiecte.md). Leagă: ADR 0005 (inferența și VCB),
  0030 (pivot), 0032 (indexul de picking), 0038 (performanța viewport-ului), 0026 (Biblioteca).
- Context: motorul de inferență vedea doar `ToolContext::mesh` (geometria liberă a contextului editat). Grupurile,
  componentele, primitivele și importurile, adică aproape tot ce face utilizatorul, erau invizibile: nu puteai opri
  înălțimea unui stâlp în dreptul vârfului altuia și nu puteai pune un obiect pe colțul altuia.

## Decizii

- **`SnapScene`** (`core/inference/SnapScene.h`): o listă de `SnapSource { mesh*, toContext, instance, lo, hi }`.
  Sursele nu copiază geometria: indică mesh-urile definițiilor și poartă transformarea local → coordonatele contextului
  și cutia mesh-ului în coordonatele contextului (calculată o dată, la construirea scenei, o dată pe definiție). Motorul
  o primește prin `Engine::withScene`; la fiecare hover păstrează doar sursele a căror cutie e lângă cursor
  (`view::NearCursor`: raza cursorului cu o margine de `max(pointRadiusPx × hysteresis, edgeRadiusPx)` pixeli la
  adâncimea cutiei), deci costul nu crește cu numărul de obiecte. Sursele rămase se interoghează prin indexul de picking
  (BVH, ADR 0032) sau, sub pragul lui, prin bucle pe un singur obiect mic. `model::snapScene` (`model/SnapSources.h`) o construiește: obiectele
  vizibile la orice adâncime, plus geometria liberă a contextelor părinte; fără mesh-ul contextului (e `ctx.mesh`),
  instanțele editate și obiectele pe care unealta le transformă acum (`exclude` = `Tool::snapExclude()`: Move / Rotate /
  Scale, abia după ce baza sau centrul e ales, cu sau fără selecție; lipirea nu exclude nimic, copiile nu sunt încă în
  model).
- **Origine, nu ID-uri.** `Inference::origin` (`Context` | `Object`), `sourceInstance` și `normal`. ID-urile de vârf /
  muchie / față sunt mereu din `ctx.mesh`; o inferență pe un obiect le are goale. `Inference::pick` e ce se vede din
  `ctx.mesh` sub cursor, sau gol: o față a contextului ascunsă de un obiect nu apare (Text, Pen, normala uneltelor). Uneltele iau doar punctul din ea,
  așa că nu pot împărți o muchie sau porni Push/Pull pe geometria altui obiect (anti-obiectiv din design).
- **Referința ca segment** (`ReferenceEdge`): Parallel / Perpendicular lucrează pe un segment în coordonatele contextului,
  nu pe un `EdgeId`, ca muchiile obiectelor să poată fi referință.
- **`Kind::Center`**: centrul cercurilor, arcelor și poligoanelor (muchiile cu același `Edge::curve`), calculat din trei
  puncte ale curbei și acceptat doar dacă toate punctele sunt la aceeași distanță în același plan. Apare după ce
  cursorul a atins curba, ca în SketchUp; centrele găsite se păstrează într-o listă scurtă a celor recente (nu un index
  spațial). Aliniere dublă: punct în dreptul a două referințe pe două axe, cu două linii punctate.
- **Unelte**: `ToolContext::snap`; `tools::inferAt` / `inferenceNormal` (`ToolHelpers.h`) sunt singurul drum al
  uneltelor spre motor (Line, Arc, Rectangle, Circle, Polygon, primitive, Pen, Push/Pull, Move, Rotate, Scale, Offset,
  măsurători, pivot, lipire). `Document::snapScene(exclude)` o ține în cache după `modelRevision()`.
- **Move**: punctele cutiei (`instanceGrips`) rămân ca ținte și puncte de apucare, dar un colț real bate unul al cutiei
  cu 0,5 px; când sunt în același loc, punctul cutiei rămâne aprins, iar marcajul inferenței se desenează peste el.
- **Primitive**: Cub / Cilindru / Con au trei clicuri (centrul bazei, lățimea, înălțimea, aceasta din urmă prinsă ca la
  Push/Pull, cu ghidaj punctat, doar deasupra bazei: sub ea înălțimea rămâne 0 și click-ul nu face nimic); sfera rămâne
  cu doi (și ghidul ei animat are doi pași). VCB: la pasul 2 „1000” = lățimea, „1000,500” = lățimea și
  înălțimea; la pasul 3 „500” = înălțimea. Previzualizarea desenează corpul (rețea la sferă, generatoare de contur din
  cameră la cilindru / con).
- **Drop din Bibliotecă**: urmează inferența (`Viewport::dropPointAt`, `Document::inferDrop`), iar punctul care ajunge pe
  țintă e **centrul bazei** obiectului. `createParametricAtCorner` păstrează `aion.object` pe colț (API-ul de scripturi
  nu se schimbă). Acțiune de script: `dropat:id,x,y`.

## Performanța

Prima măsurătoare (cu unealta Select) nu rula deloc inferența: Select nu cheamă `inferAt`. Cifrele de mai jos sunt
cu o unealtă de desen activă și cu motorul măsurat direct.

**Motorul** (program de test legat de `build/mac-release/core/libaion_core.a`, nepus în repo): N sfere de 24 de segmente,
fiecare cu definiția ei, într-o grilă de 20 de coloane, `model::snapScene` + `Engine::infer` în 200 de poziții ale
cursorului, după încălzire. „Fără tăiere” = toate sursele interogate la fiecare hover (codul de dinaintea cutiilor).
Rezultatele sunt identice (aceeași sumă de control a punctelor, tipurilor și instanțelor).

| obiecte | fără tăiere | cu tăiere | `snapScene` (o dată pe revizie) |
|---|---|---|---|
| 200 | 13,5 ms / hover | 0,054 ms / hover | 0,28 ms |
| 1 000 | 70,4 ms / hover | 0,145 ms / hover | 1,6 ms |
| 3 000 | 259 ms / hover | 0,33 ms / hover | 4,2 ms |

`aion_tests "[.perf][inference]"` (1 000 de sfere, în build-ul de debug cu UBSan): 9,4 ms / hover cu tăiere (pragul
testului: un cadru, 16 ms); fără tăiere, cele 220 de hover-uri nu s-au terminat în ~15 minute.

**Aplicația** (release, 1 000 de sfere, Line activ, `navbench:hover,8`, firul principal eșantionat cu `sample <pid> 4`):

```
view.top;wait:1000;scriptjs:Array.from({length:1000},(_, i)=>aion.sphere((i%20)*600,Math.floor(i/20)*600,0,400));wait:1500;view.extents;wait:2000;tool.line;moves:600,400;navbench:hover,8
```

| | eșantioane pe firul principal | din ele în `Engine::infer` |
|---|---|---|
| fără tăiere | 2 983 | 2 950 (99%) |
| cu tăiere | 2 974 | ≤ 35 (≈ 1%) |

`navbench fps` nu e o măsură bună aici: variază între rulări de la 5 la 60 fps (fereastra, randarea) independent de
varianta motorului, așa că nu-l folosim pentru comparație.

## Consecințe

- Orice unealtă nouă care alege un punct trebuie să treacă prin `tools::inferAt`, altfel nu vede obiectele.
- O unealtă nu folosește niciodată ID-uri dintr-o inferență cu `origin == Object` pe `ctx.mesh`.
- Rămân în afara designului: lipirea geometriei de alt obiect, rotirea obiectelor din Bibliotecă după fața pe care cad.
