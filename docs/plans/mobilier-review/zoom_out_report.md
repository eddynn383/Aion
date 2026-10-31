# Etapa 5 – Zoom Out inițial

* **Document:** `docs/plans/mobilier-pe-corp.md` (liniile 1–1036)
* **Recenzor:** egm-zoom-out (context nou, cu standardul zoom-out)
* **Data:** 2026-10-30

## Verdict

- **Context suficient:** da. Scopul și motivația (cele 5 defecte concrete, concurentul) sunt clare pentru un cititor
  nou.
- **Rezolvă cauza, nu simptomul:** da. Designul adaugă date explicite de piesă pe `Definition`, în loc să ghicească
  mai bine din geometrie.
- **Blocante:** niciunul. **Majore:** 9. **Minore:** 5. Aproape toate sunt despre amploare și simplitate.

**Notă a agentului principal:** M2, M5, M6 și M7 contrazic decizii luate explicit de autor (D16/D23, P-2, P-5, D5).
Recenzorul nu a văzut discuția; le-a judecat doar după obiectivele din document. Acceptarea lor înseamnă redeschiderea
acelor decizii.

## Majore

### M1. Lungimea și lățimea plăcii sunt salvate de două ori, iar asta cere un finalizator în tot nucleul

- **Unde:** §3 Placa parametrică (244–268); alternativele D, H, I; cerințele de performanță; pasul 5 (intrările 26–28).
- **Problema:** starea plăcii se calculează deja din geometrie, dar designul mai salvează și `parameters {length,
  width}`, care repetă ce măsoară `measurePart`. Doar sincronizarea lor după un Push/Pull cere `adoptBoardGeometry`,
  mutarea originii cu compensarea instanțelor, înregistrarea `generator = "board"` și un finalizator nou în
  `CommandStack` care rulează la fiecare commit. Designul recunoaște riscul („o eroare în finalizator afectează toate
  comenzile”). Codul are deja modelul mai simplu: `setPrimitiveSegments` / `isPristine` (`Modeling.cpp:190-269`) nu
  salvează mărimea, ci o citesc din mesh.
- **Propunerea:** geometria e singura sursă pentru dimensiuni. Inspector-ul afișează `measurePart`; scrierea în L/W
  reface cutia la grosimea nominală din `PartSpec`. „Readu la parametri” reface din dreptunghiul măsurat plus grosimea
  nominală și pune scara la 1. Push/Pull pe capăt (D20) merge fără niciun cârlig. Dispar: `parameters` pentru plăci,
  `generator = "board"`, `adoptBoardGeometry`, finalizatorul (intrarea 27), `detachBoard` și starea `Detached`.
  Alternativa D trebuie rescrisă.

### M2. Generatorul v2 și „Întinde” sunt două căi concurente de a lăți un corp

- **Unde:** §4 (273–282), §9, obiectivul 7, pașii 6 și 11, refuzul `Generator`, P8.
- **Problema:** un corp v2 păstrează un parametru de lățime care îi reface plăcile, deci se pierd decorul, cantul și
  modificările din plăci. „Întinde” le păstrează, dar trebuie să refuze corpurile generate. Utilizatorul are două
  controale de lățime cu pierderi de date diferite, iar asta costă `generatorVersion`, „Actualizează”, „Transformă în
  corp obișnuit” și alegerea între builder-ul v1 și v2.
- **Propunerea:** generatorul v2 construiește o singură dată un corp obișnuit din plăci parametrice și feronerie, fără
  regenerare (starea finală din D16, aplicată de la început). Lățimea se schimbă doar cu „Întinde”. Dulapurile v1
  rămân cum sunt; „Actualizează” le înlocuiește cu o construcție v2. Dacă D16/D23 rămân cum sunt scrise, ADR 0043 să
  spună de ce sunt justificate două mecanisme.

### M3. `generatorVersion` e un câmp nou acolo unde ajunge un id de generator

- **Unde:** liniile 214, 280–282, 614–617, 623–625, 827–828.
- **Problema:** v1 și v2 diferă doar prin builder. Un id separat („cabinet” pentru fișierele vechi, „cabinet.v2”
  pentru cele noi) poartă aceeași informație; `findGenerator` / `builderFor` aleg deja după id.
- **Propunerea:** fără `generatorVersion`; id-uri noi de generator. M2 poate face câmpul inutil oricum.

### M4. Trecerea la v6, care nu se poate anula, vine la pasul 1, înainte de orice beneficiu

- **Unde:** §8, cerințele nefuncționale (compatibilitate, retragere), pasul 1.
- **Problema:** build-ul web se publică automat din `main`. De la unirea pasului 1, orice fișier salvat pe web devine
  v6 și ilizibil pentru build-urile vechi, deși nimic din el nu folosește câmpurile noi până la pașii 2–5.
- **Propunerea:** se scrie `formatVersion 6` doar când modelul conține date v6 (`part`, `sourceKey`, `category` sau
  `libraryKey` nevide); altfel v5. Alternativ: pasul 1 nu intră în `main` până la pasul 4/5.

### M5. Lista „În model” cu miniaturi în memorie, deși Outliner-ul listează componentele

- **Unde:** P-2, cerințele nefuncționale, pasul 8 (intrările 43–44).
- **Problema:** adaugă `modelComponents`, `placeModelComponent`, o memorie de miniaturi, cârlige de captură la Make
  Component și la ieșirea din editare și o secțiune nouă în bibliotecă. Niciun obiectiv și niciun parcurs nu au nevoie
  de ea; obiectivul 5 / P6 se îndeplinesc prin drop din bibliotecă.
- **Propunerea:** scoasă din pasul 8 și mutată la „Îmbunătățiri ulterioare”. Legarea (`sourceKey`) rămâne.

### M6. Toate extraopțiunile de cant intră în pasul 7 (P-5)

- **Unde:** P-5, `BandSide::decor`, `bandAllowanceByType`, pasul 7 (intrările 35–40).
- **Problema:** obiectivul 3 / P4 cer tipul de cant pe latură, dimensiunea de tăiere și metri pe tip cu adaos global.
  P-5 adaugă evidențierea la hover, linii colorate, decor de cant pe latură (încă o dimensiune de grupare), presetări
  în `UserPrefs`, adaos pe tip și o pagină nouă în Preferințe. Pasul 7 e singurul L din drumul principal, mai ales din
  cauza lor.
- **Propunerea:** pasul 7 păstrează tipul de cant pe latură, `cutSize`, cele 3 presetări incluse, `BandNeed` pe tip și
  adaosul global. Restul se amână. `BandSide::decor` iese din schema v6 (câmpurile necunoscute se păstrează deja,
  deci se poate adăuga fără altă versiune).

### M7. Traducerea în 8 limbi e speculativă, iar numele pieselor ca chei amestecă `Instance::name`

- **Unde:** obiectivul 9, §10, P-4, pasul 12.
- **Problema:** designul amână XLSX și licențierea „după ce cineva arată că plătește”, dar se angajează la 6 limbi
  traduse automat, `CFBundleLocalizations`, o listă de 8 limbi și o verificare în CI. Separat, numele generate salvate
  ca chei fac din `Instance::name` uneori o cheie, alteori text de utilizator; nimic nu spune cum le deosebește
  afișarea (un utilizator poate scrie chiar „part.side.left”).
- **Propunerea:** pasul 12 aduce infrastructura plus RO și EN; celelalte 6 limbi trec la „Îmbunătățiri ulterioare”.
  Numele generate rămân text, în limba activă la creare (ca în SketchUp). Dacă rămân cheile, un câmp separat
  `Instance::nameKey`.

### M8. Feroneria are două surse și un drum de resurse de care nu are nevoie

- **Unde:** `HardwareType` (§2), §6, pasul 9 (intrările 46–47), alternativa M.
- **Problema:** feroneria e definită de două ori: în tabelul C++ și în fișiere `.aion` generate de un script. Geometria
  e oricum procedurală, deci scriptul, `qt_add_resources` și încărcarea fișierelor sunt în plus. Plăcile folosesc deja
  drumul mai simplu (`board:<tip>` construit în cod).
- **Propunerea:** intrări `hardware:<cheie>` construite în core din tabelul `HardwareType`, cu o funcție de geometrie pe
  tip. Fără fișiere și fără script.

### M9. Categoria materialului atinge mai mult decât cazul vechi pe care îl servește

- **Unde:** §1, clasificarea din listă, pasul 2 (intrările 14–16).
- **Problema:** categoria contează doar pentru obiectele fără `PartSpec`, care au deja două căi de actualizare („Fă
  placă”, „Actualizează”). Totuși designul schimbă regula generală de unire a materialelor la inserare (nume +
  categorie), adaugă un câmp în panoul general de materiale și o secțiune `UndefinedWood`.
- **Propunerea:** categoria rămâne pe materialele incluse (ca mânerele să ajungă la „Alte componente”); regula de
  unire nu se schimbă; fără câmpul din panoul de materiale deocamdată; o singură secțiune „Nedefinit”, cu motivul.

## Minore

- **N1. Optimizare prematură la drop:** citirea „doar a cheii” din fișier cere un cititor parțial fără un cost măsurat.
  Propunere: fișierul se încarcă întreg, ca azi, apoi se verifică `sourceKey`.
- **N2. `isSimpleBox` repetă un model existent:** `isPristine` / `primitiveSize` (`Modeling.cpp:190-216`). Propunere:
  aceeași funcție, cu aceeași toleranță (`tol::kLength`), nu o regulă separată de 0,01 mm.
- **N3. „Întinde” pe geometrie liberă:** e o funcție generală, de care obiectivele nu au nevoie. Propunere: prima
  versiune refuză (`FreeGeometry`). Pașii 10 și 11 marcați opționali după al doilea punct de control.
- **N4. Duplicare și altitudine:** structura `PartSpec` apare și în §1, și în intrarea 6; intrarea 10 lasă deschisă o
  alegere de implementare. Propunere: structura doar în §1; funcția liberă `explodeLosesPartData`.
- **N5. Nealinierea cu documentele de bază:** `PLAN.md` (linia 25) are încă „Modelare parametrică” la „Ce NU facem”;
  linia 21 a designului pune web înaintea Windows, invers față de `CLAUDE.md`. Propunere: pasul 0 corectează
  `PLAN.md`, iar ADR 0043 notează schimbarea ordinii platformelor, cu motivul.

## Cele mai mari simplificări posibile

- **M1:** geometria ca singură sursă pentru dimensiunea plăcii → fără finalizator, fără `adoptBoardGeometry`, fără
  starea `Detached`.
- **M2/M3:** generator care construiește o dată + „Întinde” ca singura cale de lățire → fără `generatorVersion`,
  `makePlainBody` și refuzul `Generator`.
- **M4:** v6 scris doar când există date v6 → retragerea rămâne posibilă.
- **M5/M6/M7:** amânarea listei „În model”, a extraopțiunilor de cant și a 6 din 8 limbi → pașii 7, 8 și 12 se reduc la
  ce testează obiectivele.
