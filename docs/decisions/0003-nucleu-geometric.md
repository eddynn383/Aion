# 0003 – Nucleul geometric: Mesh, înregistrarea schimbărilor și formarea fețelor

**Status:** acceptat
**Data:** 2026-10-26

## Context
Faza 2 construiește topologia (vârfuri, muchii, fețe), comportamentul „lipicios” al geometriei
(PLAN.md 4.3) și undo/redo. Aici se decide dacă modelul rămâne corect după mii de operații.

## Decizie

### Stocare și ID-uri
- Topologia stă într-un `geom::Mesh`. În Faza 6, `Model` va conține mai multe `Mesh`-uri (rădăcina, grupuri,
  definiții de componente); până atunci „`Model::validate()`” din plan este `Mesh::validate()`.
- Entitățile au ID-uri tipizate (`VertexId`, `EdgeId`, `FaceId`), unice pe toată durata unui `Mesh`
  (contor monoton, nu se refolosesc). `SlotMap<Id, T>`: vector dens + index, inserare cu ID dat (pentru undo).

### Topologie (radial-edge simplificat)
- `Vertex { position, edges[] }`, `Edge { v0, v1, faces[] }` (0..n fețe: muchii libere și muchii cu > 2 fețe),
  `Face { plane, outer: Loop, inner: Loop[] }`, `Loop { uses: EdgeUse[] }`, `EdgeUse { edge, reversed }`.
- Bucla exterioară e în sens trigonometric față de normala feței, găurile în sens orar.
- Bucle stocate în față (valoare), nu ca entități separate: mai simplu de copiat și de restaurat.

### Undo/redo prin înregistrarea stărilor
- Orice modificare trece prin câteva primitive ale `Mesh` (operațiile Euler: adaugă/șterge vârf, muchie, față,
  împarte muchia, mută vârful). Când înregistrarea e activă, prima atingere a unei entități îi salvează starea
  „înainte”; la final se citește starea „după”. Rezultatul e un `ChangeSet`.
- Undo = restaurarea stărilor „înainte”, redo = a celor „după”. Nu există cod de revert scris de mână
  pentru fiecare operație, deci nu poate fi greșit pentru o operație anume.
- `CommandStack` grupează schimbările în comenzi (cu tranzacții deschise pentru uneltele interactive).

### Comportamentul lipicios (`addEdge`)
1. Capetele se unesc cu vârfurile existente aflate în toleranță; un capăt căzut pe o muchie o împarte.
2. Muchiile intersectate (inclusiv suprapuneri coliniare) se împart; segmentul nou se împarte în bucăți
   între toate punctele de pe el; bucățile care există deja nu se dublează.
3. **Fețele se recalculează pe plan:** pentru fiecare plan atins (planurile fețelor existente care conțin
   bucățile noi și planurile formate de bucata nouă cu muchiile vecine), se calculează aranjamentul plan al
   tuturor muchiilor coplanare: punțile se ignoră (rămân muchii libere pe față), ciclurile se găsesc prin
   parcurgerea „cea mai din stânga”, iar componentele disjuncte devin găuri în regiunea cea mai mică ce le conține.
   - o regiune identică cu o față existentă rămâne neschimbată (același ID);
   - o regiune aflată în „materialul” unei fețe vechi devine față (împărțirea unei fețe);
   - o regiune nouă mărginită de o muchie nouă devine față (ciclu coplanar închis);
   - fețele vechi care nu mai corespund niciunei regiuni se șterg.
- Normala unei fețe noi: moștenită de la fața din care provine; altfel, pentru fețele orizontale −Z
  (ca în SketchUp: fața de pe sol „privește” în jos, ca Push/Pull în sus să dea normale spre exterior),
  iar pentru celelalte o orientare canonică (Faza 5 o va rafina).

## Consecințe
- Recalcularea pe plan costă O(muchii coplanare) la fiecare inserare; e suficient până la Faza 10 (BVH).
- `validate()` complet (inclusiv intersecții între muchii) e O(E²) și rulează în teste și fuzz, nu în release.
