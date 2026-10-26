# 0008 – Modelul: definiții, instanțe, tag-uri și contextul de editare

**Status:** acceptat
**Data:** 2026-10-26

## Context
Faza 6 aduce grupurile, componentele și tag-urile. Până acum documentul era o singură geometrie (`geom::Mesh`).

## Decizie

### Structura (`model::Model`)
- **Definiții**: fiecare are o geometrie proprie (`Mesh`) și un nume; rădăcina modelului e și ea o definiție.
  Un **grup** e o definiție marcată `group` (o singură instanță); o **componentă** poate avea oricâte instanțe.
- **Instanțe**: o definiție plasată în altă definiție (`parent`) cu o transformare (local → părinte), nume,
  tag, ascunsă, blocată. Instanțele dintr-o definiție sunt copiii ei (Outliner).
- **Tag-uri**: nume, culoare, vizibilitate. O instanță e vizibilă dacă nu e ascunsă și tag-ul ei e vizibil.
- Definițiile componentelor rămân în model și după ce nu mai au instanțe (biblioteca „În model”);
  definiția unui grup dispare odată cu ultima lui instanță.

### Undo pe tot modelul
- `model::ChangeSet` = schimbările de structură (definiții, instanțe, tag-uri: înainte/după) plus `ChangeSet`-ul
  fiecărei geometrii atinse. Ordinea la aplicare ține cont de crearea/ștergerea definițiilor (geometria unei
  definiții noi se creează înainte de a i se aplica schimbările, iar cea a unei definiții șterse se golește prin
  primitive, deci undo o reface).
- Geometria unei definiții șterse rămâne „în cimitir”, ca la undo să-și recapete contoarele de ID-uri
  (ID-urile noi nu se suprapun cu cele din istoric).
- `CommandStack` lucrează pe un `Model` (sau, în testele vechi, pe o singură geometrie). `run` primește o funcție
  de `Mesh&` (geometria contextului activ) sau de `Model&`.

### Contextul de editare
- Documentul ține **calea** de instanțe în care s-a intrat (dublu-click pe un grup; Esc sau ✕ iese).
  Uneltele primesc geometria contextului și o cameră „în context” (`Camera::inContext`): razele, proiecțiile și
  direcțiile ei sunt în coordonatele locale ale grupului, așa că uneltele nu au trebuit rescrise.
- Viewport-ul desenează tot arborele (transformările compuse); în afara contextului totul e estompat, iar
  grupul editat are cutia punctată.
- Overlay-urile uneltelor sunt în coordonatele contextului; viewport-ul le transformă înainte de desen.

### Unelte și operații
- `geom::Selection` conține și instanțe. Select alege cea mai apropiată dintre geometrie și instanțe
  (`view::pickInstance`; dreptunghiul de selecție folosește cutiile instanțelor).
- Move/Rotate/Scale pe instanțe schimbă transformarea lor; copierea cu Option a unei **componente** face o instanță
  a aceleiași definiții (copiile rămân legate), a unui **grup** o definiție nouă (ca în SketchUp).
- Make Group / Make Component mută geometria selectată într-o definiție nouă (transformare identitate); muchiile
  folosite și de fețe rămase în context rămân și acolo. Explode lipește geometria transformată de părinte.
- Instanțele blocate nu se mută, nu se șterg și nu se editează.

## Consecințe
- Inferența vede doar geometria contextului activ (nu și pe cea din alte grupuri): de rafinat.
- Originea unei definiții noi e originea contextului (transformare identitate), nu colțul obiectului.
- Ascunderea funcționează pe instanțe; fețele și muchiile simple nu au încă „hidden”.
