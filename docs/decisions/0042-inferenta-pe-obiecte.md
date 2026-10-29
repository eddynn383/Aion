# ADR 0042 – Inferența pe obiecte

- Stare: acceptat (2026-10-30)
- Design: [`docs/plans/inferenta-pe-obiecte.md`](../plans/inferenta-pe-obiecte.md). Leagă: ADR 0005 (inferența și VCB),
  0030 (pivot), 0032 (indexul de picking), 0038 (performanța viewport-ului), 0026 (Biblioteca).
- Context: motorul de inferență vedea doar `ToolContext::mesh` (geometria liberă a contextului editat). Grupurile,
  componentele, primitivele și importurile, adică aproape tot ce face utilizatorul, erau invizibile: nu puteai opri
  înălțimea unui stâlp în dreptul vârfului altuia și nu puteai pune un obiect pe colțul altuia.

## Decizii

- **`SnapScene`** (`core/inference/SnapScene.h`): o listă de `SnapSource { mesh*, toContext, instance }`. Sursele nu
  copiază geometria: indică mesh-urile definițiilor și poartă transformarea local → coordonatele contextului. Motorul o
  primește prin `Engine::withScene` și interoghează fiecare sursă prin indexul de picking (BVH, ADR 0032), deci
  hover-ul nu face bucle pe toate vârfurile. `model::snapScene` (`model/SnapSources.h`) o construiește: obiectele
  vizibile la orice adâncime, plus geometria liberă a contextelor părinte; fără mesh-ul contextului (e `ctx.mesh`),
  instanțele editate și selecția care se transformă (`exclude`).
- **Origine, nu ID-uri.** `Inference::origin` (`Context` | `Object`), `sourceInstance` și `normal`. ID-urile de vârf /
  muchie / față sunt mereu din `ctx.mesh`; o inferență pe un obiect le are goale. Uneltele iau doar punctul din ea,
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
  cu 0,5 px.
- **Primitive**: Cub / Cilindru / Con au trei clicuri (centrul bazei, lățimea, înălțimea, aceasta din urmă prinsă ca la
  Push/Pull, cu ghidaj punctat); sfera rămâne cu doi. VCB: la pasul 2 „1000” = lățimea, „1000,500” = lățimea și
  înălțimea; la pasul 3 „500” = înălțimea. Previzualizarea desenează corpul (rețea la sferă, generatoare de contur din
  cameră la cilindru / con).
- **Drop din Bibliotecă**: urmează inferența (`Viewport::dropPointAt`, `Document::inferDrop`), iar punctul care ajunge pe
  țintă e **centrul bazei** obiectului. `createParametricAtCorner` păstrează `aion.object` pe colț (API-ul de scripturi
  nu se schimbă). Acțiune de script: `dropat:id,x,y`.

## Performanța

Model de test: 200 de sfere create printr-un singur `scriptjs:` (24 de segmente, valoarea implicită; scriptul nu poate
seta segmentele): 200 × 24 × 12 = 57 600 de fețe, aranjate într-o grilă 20 × 10. Build release (`mac-release`),
HEAD față de commit-ul de dinaintea feature-ului (`aa8c5a9`, construit într-un worktree separat), 3 rulări pe variantă:

```
view.top;wait:1000;scriptjs:Array.from({length:200},(_, i)=>aion.sphere((i%20)*600,Math.floor(i/20)*600,0,400));wait:1500;view.extents;wait:2000;moves:600,400;fpslog:5;navbench:hover,5
```

| | media pe secundele 1–5 (`navbench fps`) | ultima valoare (media rulării) |
|---|---|---|
| înainte (`aa8c5a9`) | 33,7 fps | 27,7 fps |
| după (HEAD) | 34,8 fps | 34,7 fps |

Hover-ul nu scade (103% față de înainte pe medie; zgomotul dintre rulări, 17–47 fps, e mai mare decât diferența).
Nu a fost nevoie de cull pe sursă (`rayHitsBox`) și nici de cache pentru `curveCenter`. `aion_tests "[.perf]"`
trece (pick + inferență pe 2 100 de instanțe: 0,16 ms; cu index 0,05 ms față de 31,6 ms fără).

## Consecințe

- Orice unealtă nouă care alege un punct trebuie să treacă prin `tools::inferAt`, altfel nu vede obiectele.
- O unealtă nu folosește niciodată ID-uri dintr-o inferență cu `origin == Object` pe `ctx.mesh`.
- Rămân în afara designului: lipirea geometriei de alt obiect, rotirea obiectelor din Bibliotecă după fața pe care cad.
