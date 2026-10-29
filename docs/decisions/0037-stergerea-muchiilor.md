# ADR 0037 – Ștergerea muchiilor: guma și ⌫ păstrează fețele, Delete le ia

- Stare: acceptat (2026-10-29)
- Context: cerința utilizatorului: o față cu linii desenate în ea (libere sau care o taie, dar nu fac parte din
  conturul ei) trebuie să rămână când liniile se șterg cu guma sau cu ⌫, ca în SketchUp; cu Delete, liniile pleacă
  împreună cu fața pe care stau.

## Decizii

- **Două operații în core** (`geom/Operations.h`):
  - `eraseEdgesKeepingFaces`: fețele de o parte și de alta a unei muchii șterse se unesc într-una (`mergeFacesAcross`),
    apoi vârfurile rămase doar între două muchii coliniare dispar (`dissolveVertex`); o linie liberă din interiorul unei
    fețe pleacă fără să atingă fața.
  - `eraseEdgesWithFaces`: muchiile pleacă împreună cu fețele lipite de ele; o linie liberă ia și fața care o conține
    (după mijlocul ei).
- **Uneltele și tastele**: guma folosește prima operație; `tools::deleteSelection(ctx, keepFaces)` le alege pe amândouă;
  în aplicație ⌫ = `Document::deleteSelection(true)`, Delete (`edit.delete`) = `deleteSelection(false)`.
  ⌫ pe o curbă în editare șterge punctul selectat; pe o cale selectată (Animare) șterge calea.
- **Picking-ul** (`math::intersectRayTriangle`) are o margine relativă de 1e-9 pe coordonatele baricentrice: o rază
  exact pe diagonala comună a două triunghiuri (de exemplu un click la mijlocul unei fețe pătrate) nu mai trece printre
  ele, spre o față din spate.

## Consecințe

- Fețele unite păstrează materialul și tag-ul uneia dintre ele (prima de pe muchie). O muchie între fețe care nu se pot
  uni (în alte planuri, ca muchia unui cub, sau orientate invers) se șterge ca înainte, cu fețele ei.
- Delete pe o față singură se comportă ca înainte (fața pleacă, muchiile rămân).
