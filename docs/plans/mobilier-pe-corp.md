# Aion: proiectarea mobilierului pe corp

*   **Autor:** Eduard Boboc
*   **Stare:** Propus (revizuit după recenzia EGM din 2026-10-30)
*   **Data:** 2026-10-30
*   **Sursa:** [`concepts_brief.md`](../../concepts_brief.md) (brainstorm cu recenzori EGM – Elephant-Goldfish Model,
    patru runde). Rapoartele recenziei: [`goldfish_review_report.md`](../../goldfish_review_report.md).

--------------------------------------------------------------------------------

## Prezentare

Aion e un modelator 3D nativ (C++20, Qt 6 Quick, Qt RHI), inspirat de SketchUp, făcut de un singur om. Documentul
descrie cum devine Aion o unealtă pentru mobilier pe corp (dulapuri, corpuri de bucătărie, rafturi). Proiectantul are
o bibliotecă deja plină: piese de bază (plăci, balamale, glisiere, mânere) și corpuri gata făcute (suspendat, la sol,
soldat, corp pentru electrocasnice, dulap cu uși, raft). Le trage în scenă și le modifică: dimensiuni, decoruri, polițe, separatoare,
cant. La final primește o listă de debitare corectă, cu cant pe fiecare latură, un deviz pe bucăți și un fișier pe
care firma de debitare îl poate folosi.

Platforme: macOS întâi, apoi web (WebAssembly), apoi Windows, din același cod. Ordinea web → Windows schimbă decizia
din `CLAUDE.md` (macOS → Windows → Web); motivul (concurenții sunt pe Windows, nișa e macOS și web) se scrie în ADR
0043. `core/` rămâne fără Qt.

**Pașii planului** (detaliați în „Planul detaliat de implementare”):

| Pas | Ce aduce | Efort |
|---|---|---|
| 0 | ADR-uri, `PLAN.md`, `CLAUDE.md` | S |
| 1 | Formatul v6 cu schema completă, datele de piesă pe definiții | M |
| 2 | Tabelele de tipuri, categoria materialelor, feroneria construită în cod | M |
| 3 | Măsurarea pieselor, comanda „Fă placă” | S–M |
| 4 | Lista de debitare v1.5 (cu piesele ascunse și bife) | M |
| 5 | Placa parametrică | L |
| 6 | Corpurile din bibliotecă (generatoare cu reconciliere, separatoare) | L |
| 7 | Cantul (date, Inspector, viewport, listă) | L |
| 8 | Legarea componentelor salvate, lista „În model” | M–L |
| 9 | Feroneria în bibliotecă, variante, deviz | M |
| 10 | Unealta „Placă pe față” | M |
| 11 | „Întinde” | L |
| 12 | Traducerea | M, apoi L |

**Efort:** S = câteva zile, M = 1–2 săptămâni, L = 3 săptămâni sau mai mult, pentru un singur om.

**Publicare:** pașii 1–4 se unesc în `main` în același timp (regula și motivul: §8).

## Glosar

*   **Placă:** o piesă dreptunghiulară dintr-un material în foi (PAL, MDF, HDF, placaj) sau lemn masiv. În Aion: un
    obiect cu `PartSpec` de tip placă.
*   **PAL / MDF / HDF:** PAL = plăci din așchii; MDF = plăci din fibre de densitate medie; HDF = plăci subțiri din
    fibre de densitate mare (spate, funduri de sertar).
*   **Placă brută (format):** foaia de la furnizor din care se taie piesele (de exemplu 2800 × 2070 mm).
*   **Fibra:** direcția modelului lemnului pe decor; piesa trebuie tăiată cu fibra pe lungime.
*   **Cant:** banda lipită pe marginea unei plăci (ABS, melamină), cu grosimea ei (0,4 / 1 / 2 mm).
*   **Adaos:** milimetri de cant adăugați pe fiecare muchie la calculul metrilor de cant (pierderea la aplicare).
*   **Dimensiune finită / de tăiere:** finită = piesa cu cant; de tăiere = finită minus grosimea cantului marcat „se
    scade”.
*   **Decor:** materialul de aspect al plăcii (alb, nuc, stejar), ținut pe instanță.
*   **Latură locală:** cele 4 margini ale plăcii, în axele ei: L1, L2 (paralele cu fibra; fără fibră, cele lungi), W1, W2
    (celelalte două). L1 e latura din față. Regula exactă e în §5.
*   **Fața mare / capăt:** fața mare = una dintre cele două fețe pe grosime; capăt = oricare dintre cele 4 fețe înguste.
*   **Feronerie:** balamale, glisiere, mânere; se numără pe bucată.
*   **Corp:** un ansamblu de plăci și feronerie (dulap, corp de bucătărie). **Corp din bibliotecă:** unul gata făcut,
    cu câmpuri în Inspector (lățime, uși, polițe, separatoare).
*   **Separator:** o placă verticală în interiorul corpului, care împarte spațiul.
*   **Instanță:** o plasare a unei definiții într-un părinte. **Instanțe legate:** au aceeași definiție; o schimbare la
    una se vede la toate. **Apariție:** o instanță pe un drum anume din arbore; o placă dintr-un corp plasat de 3 ori are
    3 apariții. **Sub-piesă:** o piesă din interiorul altui obiect.
*   **Rol:** eticheta unei piese create de un corp din bibliotecă („lateral stânga”, „ușa 1”), după care corpul își
    recunoaște piesele la refacere.
*   **Componentă salvată:** un corp salvat cu „Salvează în bibliotecă”, cu o cheie stabilă (`sourceKey`).
*   **Lista „În model”:** componentele din documentul deschis, arătate în bibliotecă.
*   **Stările plăcii parametrice:** „În acord cu parametrii” (în cod `Matches`); „Modificată manual” (`Modified`): forma
    nu mai corespunde parametrilor; „Desprinsă” (`Detached`): placa nu mai are parametri, dimensiunile se măsoară.
    Starea se calculează, nu se salvează.
*   **Finalizator:** o funcție rulată la sfârșitul fiecărei comenzi, care aduce parametrii plăcilor la zi (§3).
*   **Deviz:** blocul de totaluri de la sfârșitul listei de debitare: plăci brute, metri de cant, bucăți de feronerie.
    **Fără prețuri** în acest design.
*   **Validarea:** autorul testează el însuși fiecare pas (punctele de control); abia apoi aplicația ajunge la alți
    utilizatori, de exemplu un tâmplar profesionist (decizia R-10).

## Problema

**Pentru cine:** amatori avansați și ateliere mici care proiectează mobilier pe corp, în România și apoi în alte țări.
Concurentul real e SketchUp + OpenCutList: OpenCutList e gratuit, dar cere SketchUp desktop, cu abonament anual.
PRO100, Polyboard și Mozaik rulează doar pe Windows și costă de la sute la mii de dolari.

**Ce face Aion azi și de ce nu ajunge:**

1. **Lista de debitare dă rezultate greșite** (`core/src/model/Quantities.cpp`):
    *   orice grup sub 100 mm grosime e socotit placă, deci mânerele din oțel ale dulapului generat cer o placă de
        oțel și cant (`core/src/model/Parametric.cpp`, `buildCabinet`; `Quantities.cpp`, `cutList`);
    *   piesele identice cu nume diferite („Lateral stânga” / „Lateral dreapta”) apar pe rânduri separate (cheia de
        grupare include numele);
    *   latura mai lungă devine mereu „lungimea”, deci direcția fibrei se pierde;
    *   cantul e doar estimat (perimetrul tuturor pieselor);
    *   spatele de 5 mm e din materialul corpului, nu din HDF;
    *   piesele ascunse dispar din listă și din deviz, împreună cu tot ce e în ele (`collect`).
2. **Nu există un model de date pentru mobilier.** Aion nu știe că un obiect e o placă de PAL de 18 mm. `Material`
    are doar proprietăți de aspect (culoare, rugozitate, metal, textură). Materialele și piesele sunt identificate
    după numele în română.
3. **Generatorul de dulap e o demonstrație:** o singură grosime pentru tot corpul, limitată la 10–50 mm, fără polițe
    sau sertare. Orice schimbare de parametru reface corpul de la zero și șterge modificările (`setParameter`,
    `regenerate`).
4. **Biblioteca nu leagă componentele.** Fiecare plasare creează o definiție nouă (`insertModel`), iar „Salvează în
    bibliotecă” pierde tot în afară de câmpurile generatorului (`extractInstance`).
5. **Interfața e doar în română**, fără nicio infrastructură de traducere.

**Soluția, pe scurt:** fiecare piesă de mobilier are date de piesă proprii (`PartSpec`: tip, grosime, fibră, cant).
Lista de debitare citește aceste date în loc să ghicească din geometrie. Biblioteca vine plină de plăci, feronerie
și corpuri gata făcute; corpurile se redimensionează din câmpuri și își păstrează personalizarea. Componentele salvate
se leagă ca în SketchUp. Interfața se poate traduce.

## Obiective

Fiecare obiectiv are un test care trebuie să treacă.

1. **Lista de debitare clasifică piesele după datele lor, nu după geometrie.** Pe dulapul de referință (§4): lateralele
    pe un rând ×2, ușile ×2, spatele HDF 3, cele 8 balamale și 2 mânere la feronerie. Un fișier v5 cu dulapul de azi:
    mânerele la „Alte componente”, plăcile marcate „tip nedefinit”.
2. **Plăci parametrice din bibliotecă.** O placă PAL 18 trasă în model se redimensionează din Inspector (lungime,
    lățime, tip, grosime din lista tipului). Decorul pus cu găleata rămâne după orice schimbare de parametru.
    O modificare manuală (Push/Pull pe fața mare, Bevel, Scale) marchează placa „Modificată manual”, cu „Readu la
    parametri” și „Desprinde de parametri”, fiecare un singur pas de undo.
3. **Corpuri din bibliotecă care își păstrează personalizarea.** Un „Corp la sol 600” cu ușa vopsită nuc, cant de 2 mm
    și un separator pus cu clic, dus la 800 din Inspector: decorul și cantul rămân, separatorul rămâne la aceeași
    poziție relativă.
4. **Cant pe fiecare latură**, cu dimensiunea finită, dimensiunea de tăiere și metrii de cant pe tip. Pentru 2 uși de
    498,5 × 2000 cu ABS 2 mm („se scade”) pe toate laturile și 50 mm adaos pe muchie: 10,39 m de ABS 2 mm și
    dimensiunea de tăiere 494,5 × 1996.
5. **Piesele ascunse rămân controlabile.** Lista de debitare arată toate piesele; cele ascunse sunt debifate, pe rânduri
    separate, cu mesajul „N piese ascunse nu sunt incluse — Include toate”. Totalurile și CSV-ul urmează bifele.
    Raportul general de cantități rămâne „doar ce e vizibil”.
6. **Componentele salvate se leagă.** 4 plasări ale aceluiași corp salvat → o singură definiție. Plăcile și corpurile
    din bibliotecă nu se leagă niciodată prin plasare. Lista „În model” arată componentele cu ≥ 1 instanță și plasează
    o instanță legată.
7. **Feroneria se numără pe toate aparițiile.** 4 instanțe legate ale unui corp cu 2 uși × 2 balamale → 16 balamale în
    deviz.
8. **Un corp făcut din plăci se lățește fără goluri sau suprapuneri între plăci** cu „Întinde”: 600 → 800 mm, plăcile tăiate de plan își schimbă
    lungimea, celelalte se mută, nicio grosime nu se schimbă.
9. **Datele de piesă se păstrează la:** salvare, inserare, „Salvează în bibliotecă”, ⌘V și Make Unique. La operațiile
    booleene, rezultatul păstrează `PartSpec` de la primul operand și devine desprins de parametri. Explode avertizează
    înainte de pierdere. Un build v5 refuză un fișier v6 cu mesajul „fișier dintr-o versiune mai nouă”.
10. **Interfața se poate traduce.** Română și engleză complete (inclusiv zonele înghețate); germană, poloneză, italiană,
    franceză, spaniolă, portugheză traduse automat și marcate „beta”. CI-ul oprește o cheie fără traducere în RO sau EN.
11. **Cantul se vede.** Latura de sub cursor în Inspector se evidențiază în viewport; laturile cu cant au linii
    colorate; o presetare salvată de utilizator rămâne după repornire.

## Non-obiective

*   **Ieșiri de producție:** CNC, G-code, nesting pe forma reală, găurire automată.
*   **Cataloage de mărci** (Blum, Hettich, Häfele) incluse în aplicație. Feroneria e generică, cu forme simple;
    utilizatorul își poate salva propriile modele.
*   **Dialog de ajustare a grosimii.** Nepotrivirile sunt doar marcate în listă.
*   **Optimizarea tăierii (nesting).** Firmele de debitare au optimizatoarele lor.
*   **Prețuri, XLSX, planșe 2D, licențiere și plată.** Se decid după validarea comercială (Alt-T).
*   **Funcții noi pentru zonele înghețate:** animație, timeline, reguli, Modul Interacțiune, DOF, video, asistentul AI,
    colaborare, importuri (`.skp`, COLLADA, IFC, DWG). Rămân funcționale și se repară; primesc doar traducerea textelor.
*   **Schimbarea limbii fără repornire** și limbi scrise de la dreapta la stânga.
*   **Schimbarea comportamentului ⌘V:** ADR 0039 rămâne. ⌘V și Move cu ⌥ fac copii unice, spre deosebire de SketchUp.

## Decizii

| Id | Decizie | Secțiune |
|---|---|---|
| D10 | Obiectele fără `PartSpec`: metal / sticlă / plastic / piatră → „Alte componente”; lemn → „Nedefinit (lemn)”; fără categorie sau decor de placă → regula veche, marcat „tip nedefinit” | §5 |
| D11 | Componentele salvate se leagă la plasare; plăcile și corpurile din bibliotecă nu | §6 |
| D12 | Placa are parametri în Inspector; modificările manuale o marchează „Modificată manual” | §3 |
| D13 | Formatul `.aion` v6 | §8 |
| D14 | Categoria materialului intră acum | §1, §8 |
| D17 | Lista de debitare arată și piesele ascunse, debifate; bifele se resetează la deschiderea ferestrei | §5 |
| D19 | Cantul pe fiecare latură e parte din placă | §1, §5 |
| D20 | Push/Pull pe un capăt al unei plăci simple actualizează parametrii | §3 |
| D21 | Exportul implicit: dimensiunea finită plus coloanele de cant; tipul de cant spune dacă „se scade” | §5 |
| D22 | Mesajul „N piese ascunse nu sunt incluse — Include toate” | §5 |
| D23 | Un dulap vechi se reface cu plăci noi doar la „Actualizează” | §4 |
| D24 | Scale pe o placă funcționează și o marchează „Modificată manual” | §3 |
| DS-1 | Pivotul rămâne pe loc în lume la schimbarea dimensiunilor; un pivot explicit își păstrează poziția relativă | §3 |
| DS-2 | Lista „În model” completă, cu miniaturi | §6 |
| DS-3 | „Întinde” pe un corp legat schimbă toate instanțele; piesele legate în afara corpului devin unice | §9 |
| DS-4 | Traducerea cu chei (`qsTrId`, `AION_TR`) | §10 |
| DS-5 | Cantul vizual complet (evidențiere, linii, decor propriu, presetări, adaos pe tip) | §5, pasul 7 |
| DS-6 | Traducerea vine ultima (pasul 12) | §10 |
| R-1 | Dimensiunea plăcii: parametri + geometrie, sincronizate de un finalizator | §3 |
| R-2 | Corpurile din bibliotecă se refac din câmpuri și reconciliază piesele după rol | §4 |
| R-3 | Feroneria inclusă e construită în cod | §2 |
| R-4 | Un singur document, pașii 1–12 | Prezentare |
| R-5 | Mereu v6, schema completă la pasul 1 | §8 |
| R-6 | Materialele se unesc după cheie, apoi nume + categorie | §6 |
| R-7 | Biblioteca are piese de bază și corpuri gata făcute, cu câmpuri proprii | §4 |
| R-8 | Separatoarele: din câmpuri și cu clic; cele puse cu clic se țin proporțional | §4 |
| R-10 | Fără sesiunea cu tâmplarul înainte de implementare: autorul testează întâi, apoi alți utilizatori | Prezentare, Verificare |
| R-9 | Corpurile din bibliotecă au id-uri noi; `cabinet` / `shelf` rămân doar pentru fișierele vechi; fără câmp de versiune | §4 |

D1–D9, D15, D16, D18 sunt în `concepts_brief.md`. Alternativele au prefixul „Alt-” (Alt-A…Alt-W), ca să nu se confunde cu
deciziile D, R și cu parcursurile U.

**Decizii vechi schimbate de acest design:** ADR 0017 („doar ce e vizibil” rămâne pentru raportul general, nu și pentru
lista de debitare; gruparea nu mai include numele; pragul de 100 mm rămâne doar pentru piesele fără date); ADR 0025
(plăcile își țin geometria în propria definiție, cu axele X = lungime, Y = lățime, Z = grosime; generatoarele
reconciliază în loc să șteargă); ADR 0038 (liniile de cant stau în `DocumentPass`; doar evidențierea laturii de sub cursor se desenează la fiecare cadru); ADR
0039 (rămâne; ⌘V și Make Unique golesc doar `sourceKey`); `PLAN.md` („Modelare parametrică” iese din „Ce NU facem”);
`CLAUDE.md` (ordinea platformelor).

## Întrebări deschise

Nu blochează implementarea: designul folosește valori implicite alese de autor (§2, §4, §5), verificate de autor la
punctele de control și, mai târziu, de alți utilizatori (R-10). Fiecare întrebare spune ce poate schimba.

*   **Formatul firmei de debitare:** coloane, separator, dimensiunea finită sau de tăiere, rândul de antet → §5 (CSV),
    pasul 4.
*   **Numele laturilor și care e „fața”** → §5 (L1–W2), pasul 7.
*   **Tipurile, grosimile, decorurile de plăci folosite** → tabelele din §2.
*   **Balamalele și glisierele folosite; câte balamale pe ușă** → §2, §4 (regula balamalelor).
*   **Feroneria din piesele ascunse intră în deviz?** → D17, D22 (implicit: nu, până la „Include toate”).
*   **Cum lățește azi un corp** → prioritatea pasului 11.
*   **Corpurile identice dintr-o bucătărie: legate sau independente?** → Obiectivul 6. Dacă „independente”, pasul 8 se
    reduce la lista „În model” fără legare automată.

## Parcursuri principale ale utilizatorului

### U1: Lista de debitare pentru un dulap existent

*   **Dat fiind:** un fișier `.aion` v5 cu dulapul generat de azi (2 uși „Lemn de nuc”, mânere „Oțel periat”).
*   **Când:** utilizatorul deschide Fișier → Raport de cantități → Listă de debitare.
*   **Atunci:** plăcile apar grupate indiferent de nume (rândul arată „folosită în: Lateral stânga, Lateral dreapta”),
    marcate „tip nedefinit”; mânerele apar la „Alte componente”; CSV-ul are aceleași rânduri.

### U2: Un corp din bibliotecă, personalizat

*   **Dat fiind:** un document gol.
*   **Când:** utilizatorul trage „Corp la sol” din bibliotecă, vopsește ușa „Nuc (decor)”, pune cant „toate 2 mm” pe
    ușă, adaugă cu clic un separator la o treime din lățime, apoi scrie 800 la lățime în Inspector.
*   **Atunci:** corpul are 800 mm; ușa e tot nuc, cu cantul de 2 mm; separatorul e tot la o treime; lista de debitare
    arată plăcile cu dimensiunile noi.

### U3: Modificare manuală și revenire

*   **Dat fiind:** o placă PAL 18 de 600 × 400, decor nuc.
*   **Când:** utilizatorul trage cu Push/Pull capătul la 800, apoi fața mare la 20 mm, apoi apasă „Readu la parametri”.
*   **Atunci:** după primul Push/Pull lungimea din parametri e 800, fără marcaj; după al doilea placa e „Modificată
    manual” și lista arată „grosime 20, nominală 18”; după „Readu” placa e 800 × 400 × 18, decorul nuc e păstrat, iar
    ⌘Z readuce grosimea de 20.

### U4: Cant și export

*   **Dat fiind:** dulapul de referință.
*   **Când:** utilizatorul alege presetarea „toate 2 mm” pe uși și exportă lista.
*   **Atunci:** fiecare ușă are ABS 2 mm pe L1, L2, W1, W2; lista arată 10,39 m de ABS 2 mm; CSV-ul începe cu rândul
    „Dimensiuni: finite”, iar coloanele de tăiere arată 494,5 × 1996.

### U5: Piese ascunse

*   **Dat fiind:** dulapul de referință, cu o ușă ascunsă pentru o prezentare.
*   **Când:** utilizatorul deschide lista de debitare.
*   **Atunci:** placa ușii, cele 4 balamale și mânerul ei apar pe rânduri separate, debifate; mesajul spune „6 piese
    ascunse nu sunt incluse — Include toate”; după „Include toate”, totalurile sunt aceleași ca atunci când nimic nu e
    ascuns; la redeschiderea ferestrei bifele revin după vizibilitate. (Comportament provizoriu până la pasul 0.)

### U6: O bucătărie din corpuri legate

*   **Dat fiind:** un corp „Jos 600” salvat în bibliotecă de utilizator.
*   **Când:** utilizatorul îl plasează de 4 ori și schimbă lungimea unei polițe într-unul dintre ele.
*   **Atunci:** toate cele 4 corpuri se schimbă; Inspector-ul arată „Afectează 4 instanțe”; Make Unique pe unul rupe
    legătura doar pentru el.

### U7: Lățirea unui corp făcut din plăci

*   **Dat fiind:** un corp de 600 mm cu 2 uși, construit de utilizator din plăci parametrice.
*   **Când:** utilizatorul folosește „Întinde în 2 locuri egale” cu +200 mm pe lățime.
*   **Atunci:** corpul are 800 mm; baza, capacul și spatele s-au lungit cu 200 mm; fiecare ușă cu 100 mm; laterala din
    partea + s-a mutat cu 200 mm; nicio placă nu e „Modificată manual”.

### U8: Un dulap vechi actualizat

*   **Dat fiind:** un fișier v5 cu dulapul generat de azi.
*   **Când:** utilizatorul îl selectează și apasă „Actualizează” în Inspector.
*   **Atunci:** dulapul e refăcut ca „Dulap cu uși” din bibliotecă (spate HDF 3, uși cu balamale și mâner), într-un
    singur pas de undo; până la apăsare rămâne neschimbat.

## Planul tehnic (arhitectura)

### 0. Straturile atinse

Arhitectura rămâne cea din `PLAN.md`: `core/` (C++ fără Qt, cu teste) → `render/` (`Document`, punte spre QML) →
`app/` (QML). Toată logica de mobilier stă în `core/`, ca să meargă și în WebAssembly și să fie testată fără UI.

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│ app/qml   EntityInfo (Placă, Cant, Corp, Feronerie) · QuantitiesDialog (bife)    │
│           LibraryPanel (Plăci, Feronerie, Corpuri, În model) · PreferencesPage    │
├──────────────────────────────────────────────────────────────────────────────────┤
│ render/   Document: boardInfo, setBoardValue, restoreBoard, detachBoard,         │
│           makeBoard, cutList(excluded), placeLibraryItem, addDivider, stretch,   │
│           înregistrarea finalizatorului, traducerea cheilor din core (qtTrId)    │
│           Viewport: liniile de cant în appendModelExtras                         │
├──────────────────────────────────────────────────────────────────────────────────┤
│ core/     model/Furniture   PartSpec (în Model.h), BoardType, BandType,           │
│                             HardwareType, createBoard, createHardware,           │
│                             measurePart, boardState, finalizeBoards              │
│           model/Parametric  corpurile din bibliotecă, reconcilierea după rol      │
│           model/Quantities  lista v1.5: clasificare, grupare, bife, cant, CSV    │
│           model/Stretch     „Întinde”                                            │
│           model/Transfer, Copying, Operations: păstrarea datelor                 │
│           commands/CommandStack  finalizatorul                                   │
│           io/AionFile       formatul v6                                          │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1. Datele noi din model (`core/include/aion/core/model/Model.h`)

Câmpurile noi se adaugă la sfârșitul structurilor (încărcarea construiește `Definition` și `Material` pozițional).

```cpp
struct BandSide {
    std::string bandKey;   // "" = fără cant; altfel cheia din tabelul de canturi ("abs.2")
    std::string decor;     // "" = ca placa (decorul instanței); altfel libraryKey-ul unui material
    bool operator==(const BandSide&) const = default;
};

struct PartSpec {
    enum class Kind { Board, Hardware };
    Kind kind = Kind::Board;
    std::string typeKey;                // Board: "pal", "mdf", "hdf", "plywood", "solid"; Hardware: "hinge.overlay.110"...
    double nominalThickness = 0.0;      // mm (doar Board)
    std::uint8_t thicknessAxis = 2;     // axa locală a grosimii (implicit Z)
    std::uint8_t grainAxis = 0;         // axa locală a fibrei (implicit X); 255 = fără fibră
    std::array<BandSide, 4> bands {};   // ordinea în §5
    std::string rawKind;                // un `kind` necunoscut din fișier, păstrat și scris înapoi; "" altfel
    bool operator==(const PartSpec&) const = default;
};

struct Definition {
    // ... câmpurile de azi
    std::optional<PartSpec> part;       // nul = obiect obișnuit
    std::string sourceKey;              // UUID-ul unei componente salvate; "" = fără
};

struct Instance {
    // ... câmpurile de azi
    std::string roleKey;                // rolul în corpul generat ("side.left", "door.1.hinge.2"); "" = pusă de utilizator
    std::string nameKey;                // cheia numelui dat de program; afișată tradusă dacă `name` e gol
    std::optional<double> relativePosition; // separator pus cu clic: fracția din lățimea interioară a corpului
};

struct Material {
    // ... câmpurile de azi
    std::string category;   // "wood" | "metal" | "glass" | "plastic" | "stone" | "board-decor" | "" (necunoscut)
    std::string libraryKey; // cheia materialului inclus; "" = creat de utilizator
};
```

**Decorul** stă în `Instance::material`, deci instanțele legate pot avea decoruri diferite. `Definition::parameters`
rămâne `map<string,double>` și păstrează doar numere (la placă: `length`, `width`; la corp: lățime, înălțime, adâncime,
uși, polițe, separatoare).

### 2. Tabelele de tipuri și feroneria (`core/include/aion/core/model/Furniture.h`)

Tabele C++ constante, marcate **provizoriu până la pasul 0**:

| BoardType | Grosimi (mm) | Placă brută (mm) | Fibră | La firmă | Unitate | Implicit |
|---|---|---|---|---|---|---|
| `pal` | 16, 18, 25 | 2800 × 2070 | da | da | m² | 18 |
| `mdf` | 16, 18, 19, 22 | 2800 × 2070 | nu | da | m² | 18 |
| `hdf` | 3, 4 | 2800 × 2070 | nu | da | m² | 3 |
| `plywood` | 12, 15, 18 | 2500 × 1250 | da | da | m² | 18 |
| `solid` | 18, 20, 26, 40 | — | da | nu | m liniari | 20 |

| BandType | Grosime (mm) | Se scade | Compatibil cu |
|---|---|---|---|
| `abs.0.4` | 0,4 | nu | pal, mdf, plywood |
| `abs.1` | 1 | da | pal, mdf, plywood |
| `abs.2` | 2 | da | pal, mdf, plywood |
| `melamine.0.4` | 0,4 | nu | pal |

`HardwareType`: cheie, nume (cheie de traducere), unitate (bucată / pereche), dimensiunile formei simple și o funcție de
geometrie (cutii și cilindri). Valori: balamale aplicate / semi-aplicate / încastrate × 110° / 165°
(`hinge.<tip>.<unghi>`), glisiere 300, 350, 400, 450, 500, 550, 600 mm (`slide.<lungime>`, pereche), mâner 128 mm
(`handle.128`). `createHardware(model, context, typeKey, placement)` construiește o definiție cu `PartSpec{Hardware}` și
forma simplă. **Nu există fișiere de feronerie** (R-3).

`nearestThickness(type, value)`: grosimea permisă cea mai apropiată; la egalitate, cea mai mare (17 → 18).

### 3. Placa parametrică

**Geometria stă în mesh-ul definiției**, nu într-un subgrup. `createBoard` construiește cutia cu `geom::buildPrimitive`,
apoi mută vârfurile ca originea să fie în colțul minim (X = lungime, Y = lățime, Z = grosime). Fețele nu au material
propriu, deci decorul din instanță se vede pe toată placa.

**Parametri:** `generator = "board"`, `parameters = {length, width}`; tipul, grosimea și cantul stau în `PartSpec`.
Generatorul `board` **nu e în lista generală de generatoare** (`registry()`), ca să nu apară în bibliotecă ca intrare
separată și să nu aibă un al doilea grup de câmpuri în Inspector. `regenerate` tratează `board` înaintea celorlalte:
golește mesh-ul și îl reface.

**Starea plăcii se calculează, nu se salvează:**

```text
boardState(model, occurrence) →
    Detached        generator ≠ "board" (dar part = Board)
    Matches         mesh = 6 fețe, 8 vârfuri, fără sub-piese; vârfurile = cutia L × W × T din parametri (tol::kLength);
                    partea liniară a transformării acumulate până la lume e ortonormală (±1 permis pentru oglindire)
    Modified        altfel, cu un motiv: Geometry | Thickness | Scaled | ScaledParent | Children
```

**Finalizatorul (D20, R-1).** `CommandStack::setFinalizer` primește o funcție apelată în `commit()` **înainte** de
`endRecording()`, numai pentru stivele pe model (nu la `push()`, undo sau redo). Lista definițiilor atinse vine dintr-o
funcție nouă, `Model::definitionsWithMeshChanges()`. `Document` înregistrează `model::finalizeBoards`: pentru fiecare
definiție `board` atinsă, fără sub-piese: dacă mesh-ul e încă o cutie aliniată la axe, dimensiunea pe `thicknessAxis`
e grosimea nominală și fiecare față își păstrează axa normalei, lungimea și lățimea măsurate intră în parametri și
originea se mută în colțul minim. Dacă dimensiunile sunt egale cu parametrii **și** colțul minim e în 0, nu face nimic.
Plăcile cu sub-piese nu sunt atinse (rămân „Modificate manual”, motiv `Children`). O excepție în finalizator anulează
tranzacția (`abort()`). Un script rulează ca o singură comandă, deci vede parametrii noi abia după ce se termină.

**Compensarea la mutarea originii.** Vârfurile se mută cu −d (d în coordonatele plăcii). În aceeași comandă, pentru
fiecare instanță a definiției, cu L = partea liniară a transformării ei (rotație, scară, oglindire):
*   translația transformării primește `+ L·d`;
*   un pivot explicit devine `pivot − d`, scris direct (**nu** prin `setPivot`, care ar rescrie cheile de animație);
*   fiecare intrare din `Scene::transforms` și fiecare `PathAnchor::before` a instanței primesc `+ L_salvat·d`, cu L-ul
    transformării salvate acolo;
*   cheile de animație și ancorele relative (`along`, `lateral`, `up`) rămân neschimbate;
*   ghidajele, adnotările și curbele Pen cu contextul în definiția plăcii se mută cu −d.

Placa nu se mișcă în lume, iar animațiile redau aceleași poziții.

**Schimbarea dimensiunilor (DS-1).** `setBoardValue(length | width)` reface placa astfel încât pivotul fiecărei instanțe
să rămână pe loc în lume. Pivotul implicit e centrul cutiei; un pivot explicit își păstrează poziția relativă (fracția
pe fiecare axă). Schimbarea tipului alege grosimea cu `nearestThickness` și golește canturile incompatibile cu noul tip.
Pe o placă „Modificată manual”, `setBoardValue` e dezactivat; Inspector-ul cere întâi „Readu la parametri” sau
„Desprinde”.

**„Readu la parametri”:** reface mesh-ul din parametri și aduce |scara| instanței la 1, cu semnul păstrat (o oglindire
rămâne), cu pivotul pe loc. Pentru
`ScaledParent`, mesajul trimite la corpul scalat. **„Desprinde de parametri”:** golește `generator` și `parameters`;
`PartSpec` rămâne, iar lista măsoară geometria. Ambele sunt o comandă (un pas de undo) și lucrează pe definiție
(„Afectează N apariții”).

**Câmpurile din Inspector.** Pentru o placă `board`, grupul „Parametri · Placă” are Tip (listă), Grosime (listă din
`BoardType`), Lungime, Lățime (cu zecimale). Câmpurile „Dimensiuni” X și Y sunt în axele plăcii (ca `Placement::size`
pentru orice obiect): X = lungime, Y = lățime, oricum ar fi rotită placa; scrierea în X sau Y schimbă parametrii (nu
scalează); Z e doar citire; pe o placă „Modificată manual” câmpurile sunt doar citire. Scale funcționează (D24) și
marchează placa `Scaled`.

### 4. Corpurile din bibliotecă (R-2, R-7, R-8)

**Tipurile incluse** (generatoare cu id-uri noi, separate de `cabinet` / `shelf` de azi): „Corp la sol”, „Corp suspendat”, „Soldat” (corp înalt),
„Corp pentru electrocasnice” (cuptor sau frigider, cu nișă), „Dulap cu uși”, „Raft”. Fiecare are câmpuri în Inspector: lățime, înălțime,
adâncime, număr de uși, polițe, separatoare verticale, grosimea corpului (`board`), grosimea spatelui (`back`).

**Construcția fiecărui corp** (aceeași regulă pentru toate tipurile, provizorie până la pasul 0):
*   laterale pe toată înălțimea; bază și capac între laterale; spate HDF încastrat între laterale, bază și capac;
*   uși aplicate pe toată înălțimea și lățimea corpului, cu rost de 3 mm doar între uși; ușa e un grup cu placa,
    balamalele și mânerul, puse alături;
*   balamale pe ușă: înălțimea ușii ≤ 900 mm → 2; 901–1600 → 3; > 1600 → 4;
*   separatoarele (din câmp și puse cu clic) împart interiorul în spații; polițele se împart egal în fiecare spațiu.

**Dulapul de referință** (folosit în obiective, parcursuri și teste): ieșirea lui „Dulap cu uși” cu lățime 1000,
înălțime 2000, adâncime 600, PAL 18, spate HDF 3, 2 uși, fără polițe → 2 laterale 2000 × 600, bază și capac 964 × 600,
spate 964 × 1964, 2 uși 498,5 × 2000, fiecare cu 4 balamale `hinge.overlay.110` și un mâner `handle.128` în grupul
ușii.

**Specificația corpurilor** (valori alese de autor, ajustate după testare). Axele corpului: X = lățime (spre dreapta),
Y = adâncime (fața corpului e la Y = 0, spatele la Y = `depth`), Z = înălțime; originea în colțul stânga-față-jos.
`depth` e adâncimea corpului **fără uși**; ușile stau în fața lui (Y de la −grosimea ușii la 0).

| Corp (id) | `width` | `height` | `depth` | `doors` | `shelves` (pe spațiu) | `dividers` |
|---|---|---|---|---|---|---|
| Corp la sol (`base`) | 600 (150–1200) | 720 (300–1000) | 560 (300–700) | 1 (0–4) | 1 (0–6) | 0 (0–4) |
| Corp suspendat (`wall`) | 600 (150–1200) | 720 (300–1000) | 320 (200–450) | 1 (0–4) | 1 (0–6) | 0 (0–4) |
| Soldat (`tall`) | 600 (300–900) | 2100 (1200–2600) | 560 (300–700) | 1 (0–2) | 4 (0–10) | 0 (0–2) |
| Corp pentru electrocasnice (`appliance`) | 600 (450–900) | 2100 (1200–2600) | 560 (450–700) | – | 1 (0–4) | 0 |
| Dulap cu uși (`wardrobe`) | 1000 (400–3000) | 2000 (500–2700) | 600 (300–700) | 2 (0–6) | 0 (0–8) | 0 (0–5) |
| Raft (`rack`) | 800 (300–2000) | 1800 (300–2700) | 300 (150–600) | 0 (0–4) | 4 (0–12) | 0 (0–5) |

Toate au și `board` (grosimea corpului, din grosimile PAL, implicit 18) și `back` (grosimea spatelui, din grosimile HDF,
implicit 3); o valoare scrisă se aduce la cea permisă cea mai apropiată. Pas: 1 mm pentru dimensiuni, 1 pentru numere.
`appliance` mai are `nicheBottom` (implicit 850, 0–1500) și `nicheHeight` (implicit 600, 300–900): o nișă deschisă
între două polițe fixe (`niche.bottom`, `niche.top`); sub ea o ușă (`door.lower`), deasupra o ușă (`door.upper`).
Decorul implicit al corpului și al ușilor: „Alb (decor)”.

**Orientarea pieselor** (axele locale ale plăcii: X = lungime / fibră, Y = lățime, Z = grosime):

| Rol | Grosimea pe axa corpului | Lungimea (fibra) pe | Dimensiuni |
|---|---|---|---|
| `side.left`, `side.right`, `divider.N` | X | Z | `height` × `depth` (separatorul: înălțimea interioară × `depth` − `back`) |
| `bottom`, `top`, `space.S.shelf.M`, `niche.*` | Z | X | lățimea spațiului × (`depth` − `back`) |
| `back` | Y | Z | (`height` − 2·`board`) × (`width` − 2·`board`) |
| `door.N`, `door.lower`, `door.upper` | Y | Z | înălțimea ușii × lățimea ușii |

**Spații și polițe.** Separatoarele din câmp se pun la distanțe egale pe lățimea interioară; cele puse cu clic se
adaugă la pozițiile lor; spațiile sunt intervalele dintre toate separatoarele. `shelves` = numărul de polițe **în
fiecare spațiu**, la distanțe egale pe înălțimea interioară.

**Uși, balamale, mâner.** Ușile au lățimi egale, cu rost de 3 mm între ele. Ușa i (de la stânga, i = 1…n) are
balamalele pe stânga dacă i ≤ ⌈n / 2⌉, altfel pe dreapta (o singură ușă: stânga). Balamalele: prima și ultima la
100 mm de marginile ușii, restul la distanțe egale între ele (numărul după §4). Mânerul: vertical, pe marginea liberă,
la 40 mm de ea, centrat pe înălțimea ușii. Grupul ușii primește `Behavior` Rotate, ancora pe latura balamalelor,
105°.

**Reconcilierea.** Fiecare piesă creată de generator are un `roleKey` („side.left”, „door.1”, „door.1.hinge.2”,
„space.2.shelf.1”, „divider.1”). La o schimbare de câmp, generatorul construiește lista de piese dorite și o
potrivește cu piesele existente după `roleKey`. Rolurile se caută printre sub-piesele directe ale corpului și în
grupurile cu rol (ușa); la mai multe piese cu același rol, câștigă id-ul cel mai mic.
*   piesa există → i se **suprascriu** transformarea și parametrii (pentru uși: poza închisă, cu `Behavior::open =
    false`; cheile de animație ale piesei se rebazează cu diferența transformării de bază); își păstrează decorul,
    cantul (`PartSpec.bands`) și varianta de feronerie;
*   piesa e nouă → se adaugă;
*   piesa nu mai e cerută (o poliță în minus, o ușă în minus) → se șterge;
*   piesele fără `roleKey` (puse de utilizator) nu se ating, cu excepția separatoarelor puse cu clic (mai jos).

**Ce se pierde la refacere:** modificările de geometrie dintr-o placă cu rol (un decupaj) și mutările de mână ale
pieselor cu rol. Înainte de orice schimbare de câmp, dacă vreo piesă cu rol diferă de ce ar
produce generatorul (formă, dimensiuni sau poziție), apare un dialog „Modificările de mână din N piese se pierd —
Continuă / Anulează”; „Transformă în corp obișnuit” oprește regenerarea.

**Editări de structură într-un corp din bibliotecă:**
*   copia unui corp întreg își păstrează rolurile;
*   copia unei singure piese (⌘V, Move cu ⌥, ⌥D, Array) sau o piesă mutată în alt părinte pierde `roleKey` și
    `relativePosition`; dacă totuși două piese au același rol, cea cu id-ul mai mare își pierde rolul și devine piesă a
    utilizatorului;
*   tipul plăcii (`typeKey`) schimbat de utilizator pe o piesă cu rol se păstrează; grosimea vine din câmpul `board` /
    `back`;
*   Explode, Make Group și mutarea unei piese cu rol în alt părinte cer întâi confirmarea „Corpul devine obișnuit (nu se
    mai reface)”, apoi rulează „Transformă în corp obișnuit”.

**Separatoarele puse cu clic (R-8).** „Adaugă separator”: clic într-un spațiu al corpului → o placă cu grosimea câmpului
`board`, pe toată înălțimea și adâncimea interioară, la mijlocul spațiului (cu ⌥: la poziția clicului). Nu se poate pune
la mai puțin de 50 mm de alt separator; altfel, refuz cu mesaj în bara de stare. Placa primește `relativePosition` =
poziția centrului ei, ca fracție din lățimea interioară, măsurată de la fața interioară a lateralei stângi. Mutarea de
mână a unui separator nu reface corpul imediat; polițele se aliniază la următoarea schimbare de câmp. Înainte de fiecare refacere, fracția se recalculează din poziția curentă (o mutare de mână se păstrează);
după refacere, separatorul are aceeași fracție, toată înălțimea interioară nouă și grosimea `board`, și împarte polițele
ca un separator din câmp.

**Dulapurile vechi (D23, decizia R-9).** Generatoarele `cabinet` și `shelf` de azi rămân, nemodificate, doar pentru
fișierele vechi; nu mai apar în bibliotecă sau în paleta de comenzi. Inspector-ul arată „Generator vechi — Actualizează”
pentru un obiect cu `generator = cabinet` sau `shelf`. „Actualizează” îl înlocuiește cu „Dulap cu uși” (`wardrobe`),
respectiv „Raft” (`rack`), cu aceeași lățime și înălțime și `depth` = adâncimea veche − grosimea ușii (la `cabinet`,
adâncimea veche includea ușile), într-un pas de undo. Nu există un câmp de versiune: id-ul
generatorului alege construcția.

### 5. Lista de debitare v1.5 (`Quantities`)

```text
cutList(model, options) →
  1. Parcurge toate instanțele, și cele ascunse (`collect(includeHidden = true)`); fiecare apariție știe dacă e ascunsă
     (ea, tag-ul ei sau un părinte). quantityReport folosește `includeHidden = false`, ca azi.
  2. Clasifică fiecare apariție:
       part.kind = Board     → secțiunea Plăci; feroneria din interiorul ei se numără, cu motivul „în placă”
       part.kind = Hardware  → secțiunea Feronerie, o bucată; nu coboară în sub-piese
       part cu typeKey necunoscut → Nedefinite, motiv „tip necunoscut”
       fără part, cu fețe    → după categoria materialului dominant (D10):
                                  metal / glass / plastic / stone   → Alte componente (bucăți)
                                  wood                              → Nedefinite, motiv „lemn fără tip”
                                  "" sau board-decor                → regula veche (≤ 100 mm = placă) în Plăci,
                                                                      marcat „tip nedefinit”
  3. Dimensiunile: placă Matches → parametrii (de la pasul 5); altfel → measurePart pe transformarea acumulată.
  4. Grupează după: secțiune, tip, grosime, L, W, decor, cele 4 canturi, decorurile cantului, ascuns.
  5. Rândurile cu rowKey în options.excluded nu intră în totaluri (deviz).
```

**Materialul dominant** se alege după arie, folosind materialul efectiv pe drumul apariției (materialul feței, altfel al
instanței, altfel al părinților – ușa e vopsită ca grup); la egalitate, după `libraryKey`, apoi după nume
(determinist).

**`rowKey`** = textul cheii de grupare de la pasul 4, unit cu „|”. Rămâne același după butonul „Recalculează” din
fereastră și după schimbarea opțiunilor.

**Laturile de cant.** Fie U și V cele două axe locale diferite de `thicknessAxis`, în ordine crescătoare. `bands[0]` =
−V, `bands[1]` = +V, `bands[2]` = −U, `bands[3]` = +U. Cu fibra pe U: L1 = −V (latura din față), L2 = +V, W1 = −U, W2 = +U. Cu
fibra pe V: L1 = −U, L2 = +U, W1 = −V, W2 = +V. Fără fibră: L e axa mai lungă.

**Dimensiunea de tăiere:** lungime − cant(W1) − cant(W2) și lățime − cant(L1) − cant(L2), doar pentru tipurile de cant
cu „se scade”.

**Deviz:** plăci brute pe tip și grosime (aria pieselor incluse × (1 + pierderi, implicit 15%) / aria plăcii brute din
`BoardType`, rotunjit în sus; lemnul masiv în metri liniari, fără plăci brute), metri de cant pe tip și decor (laturile
cu cant, pe dimensiunea finită, plus adaosul pe tip sau global), feronerie pe bucăți (glisierele pe perechi).

**Fereastra:** secțiunile Plăci, Feronerie, Alte componente, Nedefinite; coloana de bife (vizibile bifate, ascunse
debifate; se resetează la fiecare deschidere a ferestrei, se păstrează la „Recalculează” și la schimbarea opțiunilor);
coloana de motive (grosime nepotrivită, formă neregulată, tip nedefinit, ascuns); mesajul D22; alegerea „finite / de
tăiere” la export.

**CSV:** un rând de antet „Dimensiuni: finite” sau „de tăiere” (se poate opri din setări până la pasul 0); apoi
coloanele: Secțiune; Nume (folosită în); Tip; Grosime; Lungime; Lățime; Buc.; Decor; L1; L2; W1; W2; Lungime de tăiere;
Lățime de tăiere; Motiv. Doar rândurile incluse. Separatorul `;` și virgula zecimală rămân cele din ADR 0017. Celulele
care încep cu `=`, `+`, `-`, `@`, TAB sau CR primesc un `'` în față.

### 6. Biblioteca și legarea componentelor (D11, DS-2, R-6)

*   **Categoriile bibliotecii:** Plăci (`board:<tip>`), Feronerie (`hardware:<cheie>`), Corpuri (`gen:<tip>`), Ale mele
    (`file:<cale>`), În model.
*   **Plăcile, feroneria și corpurile incluse** se construiesc în cod; fiecare plasare face o definiție nouă.
*   **Componentele salvate:** „Salvează în bibliotecă” scrie un `sourceKey` nou (UUID) ca `sourceKey` al definiției
    rădăcină din fișier și pe definiția din model, într-un pas de undo (un grup devine componentă, ca în `copyLinked`).
    O singură placă salvată primește și ea cheie (e o componentă salvată, nu o placă din bibliotecă). La resalvarea
    aceleiași definiții, cheia se păstrează.
*   **Plasarea unei componente salvate:** se încarcă fișierul ca azi; dacă modelul are o definiție cu același
    `sourceKey`, cu cel puțin o instanță în arborele modelului (cele ascunse se numără; cele din definiții orfane nu),
    care nu e strămoș al locului de plasare, se plasează o instanță a ei. Altfel se inserează fișierul. La mai multe
    potriviri, id-ul cel mai mic. Definiția din model câștigă față de fișier. Un fișier invalid e refuzat la încărcare,
    iar plasarea afișează eroarea, fără să schimbe modelul.
*   **⌘V, Move cu ⌥ și Make Unique** golesc `sourceKey` pe definițiile noi.
*   **Lista „În model”:** componentele făcute de utilizator (nu grupuri, nu colecții, nu piese cu `part`, nu corpuri din
    bibliotecă) cu ≥ 1 instanță, cu numele, numărul de
    apariții și miniatura. Miniatura se face sincron prin izolare temporară
    (restul modelului ascuns, camera încadrată pe componentă, `renderImage`, apoi starea refăcută), la Make Component și la ieșirea
    din editarea ei, și se ține în memorie. Plasarea din listă face o instanță legată.
*   **Unirea materialelor la inserare (R-6):** un material cu `libraryKey` se unește doar cu unul cu aceeași cheie; fără
    cheie, după nume + categorie; altfel se adaugă un material nou. Aceeași regulă pentru scripturile care pun
    materiale după nume.

### 7. Păstrarea datelor pe toate drumurile

| Drum | Azi | După |
|---|---|---|
| Salvare / deschidere | — | toate câmpurile noi în v6 |
| `insertModel` | copiază definiția întreagă | neschimbat (păstrează și `sourceKey`) |
| `extractInstance` („Salvează în bibliotecă”) | copiază doar `generator`, `parameters`, `shape` | copiază definiția întreagă (fără nume și `group`); materialul instanței devine materialul rădăcinii; o placă scalată e readusă la scara 1 în copia salvată (modelul nu se schimbă) |
| Copie a unei singure piese dintr-un corp din bibliotecă (⌘V, Move cu ⌥) sau mutare în alt părinte | — | pierde `roleKey` și `relativePosition` |
| Explode, Make Group, mutare în alt părinte pe piese cu rol | — | confirmare, apoi „Transformă în corp obișnuit” |
| `copyDeep` (⌘V), `makeUniqueDeep` | copiază definiția | la fel, dar golesc `sourceKey` |
| Booleene | grup nou, fără date | rezultatul are transformarea primului operand și mesh-ul construit în coordonatele lui; primește `PartSpec` de la el, desprins de parametri |
| Explode | definiția dispare | `explodeLosesPartData` e întrebat înainte; interfața cere confirmare |

### 8. Formatul `.aion` v6 (D13, R-5)

`kFormatVersion` devine 6 la pasul 1, cu **schema completă**: `part`, `sourceKey`, `roleKey`,
`nameKey`, `relativePosition`, `category`, `libraryKey`. Orice câmp adăugat după pasul 1 cere v7. Fiecare salvare scrie
v6. Câmpurile goale nu se scriu; la citire primesc valori implicite. Un `kind` sau un `typeKey` necunoscut se păstrează
ca text și se scrie înapoi neschimbat.

**Completarea materialelor la încărcare.** La deschiderea unui fișier cu `formatVersion < 6`, `model::backfillMaterials`
dă fiecărui material cu `category` goală și cu numele egal cu al unui material inclus categoria și `libraryKey` ale
acestuia. „Lemn de nuc / stejar / pin” au categoria `board-decor`.

**Retragerea:** după ce pasul 1 e publicat (pe web, automat din `main`), fișierele salvate sunt v6 și nu se mai deschid
în build-urile v5. De aceea pașii 1–4 se unesc împreună.

### 9. „Întinde” (`core/include/aion/core/model/Stretch.h`)

Intrare: corpul, axa, una sau mai multe poziții de plan (în coordonatele corpului), distanța. „În N locuri egale”:
lățimea interioară pe axă se împarte în N zone egale; planele sunt la mijlocul fiecărei zone; fiecare primește distanța
/ N.

| Piesa | Ce se întâmplă |
|---|---|
| Corp din bibliotecă (generator) | Se schimbă câmpul de lățime / înălțime / adâncime; reconcilierea face restul |
| În întregime de partea + a planului | Se mută cu distanța |
| În întregime de partea − | Rămâne |
| Placă `board` „Matches”, tăiată pe lungime sau lățime | Parametrul crește cu distanța din partea tăiată (fără regula pivotului); dacă originea e pe partea +, instanța se mută |
| Placă „Modificată” sau desprinsă, tăiată | Refuz cu motiv, modelul neschimbat |
| Placă tăiată prin grosime | Refuz, modelul neschimbat |
| Feronerie | Nu se întinde; se mută dacă pivotul ei e pe partea + |
| Grup fără date | Se coboară în el, cu planul transformat |
| Geometrie liberă tăiată | Refuz (`FreeGeometry`) în prima versiune |

Totul într-o comandă, pe definiția corpului, deci pe toate instanțele legate (DS-3). Înainte, orice definiție partajată
din corp care se schimbă și are instanțe în afara corpului devine unică (`makeUniqueDeep`). Suprapunerile dintre corpuri
așezate în șir nu sunt semnalate.

### 10. Traducerea (DS-4, DS-6)

*   **`core/`:** macro-ul identitate `AION_TR(cheie)` marchează cheile pentru `lupdate` (`-tr-function-alias
    QT_TRID_NOOP+=AION_TR`). `render/` le traduce cu `qtTrId(cheie)`.
*   **QML:** `qsTrId` peste tot, inclusiv în zonele înghețate (doar textele, fără schimbări de comportament);
    `qt_add_translations` adaugă fișierele `.ts` / `.qm` (și în build-ul web).
*   **Alegerea limbii:** Preferințe → Limbă (8 limbi; DE, PL, IT, FR, ES, PT marcate „beta”), salvată în
    `UserPrefs.language` (`ui/language`), aplicată după repornire. Variabila `AION_LANG`, dacă există, are prioritate.
    O cheie fără traducere în limba aleasă cade pe engleză, apoi pe română.
*   **Numele pieselor:** numele dat de program e în `Instance::nameKey`; `Instance::name` rămâne textul scris de
    utilizator. Afișarea: `name` dacă nu e gol, altfel `nameKey` tradus. Fișierele vechi își păstrează numele în română.
    Textele compuse în core („N piese ascunse…”) întorc cheie + argumente; pluralele folosesc `%n`.
*   Pașii 1–11 scriu textele noi în română; pasul 12 le transformă, cu cele vechi, în chei.

## Alternative considerate

### Alt-A. Concurență directă cu PRO100 / Polyboard / Mozaik pe funcții de producție

*   **Pentru:** atelierele mari plătesc pentru CNC, găurire, nesting.
*   **Contra:** CNC, găurire și nesting cer biblioteci de mașini și formate de producție; concurenții sunt pe Windows,
    unde Aion nu are avantaj.
*   **De ce nu:** un singur om nu poate ajunge din urmă un produs ca Mozaik. Nișa lui Aion e macOS și web.

### Alt-B. Funcțiile de mobilier ca plugin JavaScript

*   **Pentru:** nucleul rămâne general.
*   **Contra:** plugin-urile nu pot crea generatoare, citi lista de debitare sau scrie fișiere.
*   **De ce nu:** funcțiile de mobilier ar depinde de aceste limite.

### Alt-C. Aion pentru arhitecți, designeri de interior și tâmplari deodată

*   **Contra:** trei grupuri cu nevoi diferite (planșe și DWG; randări; debitare).
*   **De ce nu:** un singur om ar face trei produse pe jumătate (brief, D1).

### Alt-D. Tipul și grosimea pe material, cu dialog la nepotrivire (runda 2 a brainstormului)

*   **Contra:** „Ajustează” ar fi refăcut dulapul și ar fi șters materialul abia pus; o singură grosime pentru tot corpul.
*   **De ce nu:** grosimea face parte din placă.

### Alt-E. Plăci ca simple cutii etichetate, dimensiunea doar din geometrie

*   **Pentru:** fără finalizator, fără mutarea originii; recomandată de recenzia de simplitate.
*   **Contra:** placa nu își mai amintește dimensiunea de proiect; „Readu” reface din forma curentă.
*   **De ce nu:** decizia R-1; placa păstrează parametrii. Cutiile etichetate rămân starea „Desprinsă”.

### Alt-F. Corpuri construite o singură dată, lățite doar cu „Întinde”

*   **Pentru:** fără reconciliere.
*   **Contra:** câmpurile din Inspector dispar după plasare; „Întinde” vine abia la pasul 11; numărul de uși nu se mai
    schimbă.
*   **De ce nu:** decizia R-2 / R-7.

### Alt-G. Plăcile ca generatoare cu subgrup (`Parts::box`)

*   **Contra:** container gol la dublu-clic, alt nume în listă, decorul ascuns de materialul subgrupului.
*   **De ce nu:** geometria plăcii stă în mesh-ul definiției ei.

### Alt-H. Tipul, grosimea și cantul în `Definition::parameters`

*   **Contra:** `parameters` acceptă doar numere; un text acolo face build-urile vechi să refuze fișierul.
*   **De ce nu:** aceste date stau în `PartSpec`, cu chei text.

### Alt-I. Câmpuri noi adăugate treptat în formatul v6

*   **Contra:** un build v6 mai vechi pierde fără avertisment câmpurile adăugate de unul mai nou.
*   **De ce nu:** schema completă la pasul 1 (R-5).

### Alt-J. „Modificată manual” salvată sau calculată din revizia mesh-ului

*   **Contra:** o stare salvată se desincronizează la undo; revizia nu se salvează și se schimbă la undo.
*   **De ce nu:** se calculează din geometrie (8 vârfuri).

### Alt-K. Regula D20 în unealta Push/Pull

*   **Contra:** Move pe vârfuri, scripturile și uneltele viitoare ar ocoli-o.
*   **De ce nu:** finalizatorul în `CommandStack` (riscul lui e în „Riscuri și măsuri”).

### Alt-L. Un câmp `generatorVersion` care alege între generatorul vechi și cel nou

*   **Pentru:** același id (`cabinet`) pentru dulapul vechi și cel nou.
*   **Contra:** corpurile noi au oricum id-uri noi („Corp la sol”, „Soldat”); câmpul ar fi o a doua cale de a alege
    construcția și s-ar pune din greșeală și pe uși, ferestre, scări.
*   **De ce nu:** decizia R-9: id-urile noi aleg construcția; `cabinet` și `shelf` rămân doar pentru fișierele vechi.

### Alt-M. Numele L1 / L2 / W1 / W2 salvate în loc de laturile locale

*   **Contra:** schimbarea fibrei ar muta cantul pe alte muchii.
*   **De ce nu:** se salvează laturile locale; numele se calculează.

### Alt-N. Oglindirea schimbă laturile de cant

*   **Contra:** reflexia stă pe instanță, deci cantul rămâne deja pe muchiile corecte.
*   **De ce nu:** nu e nevoie de cod.

### Alt-O. Legarea tuturor plasărilor din bibliotecă

*   **Contra:** toate plăcile PAL 18 ar deveni aceeași definiție.
*   **De ce nu:** se leagă doar componentele salvate.

### Alt-P. O intrare în bibliotecă pentru fiecare variantă de placă

*   **Contra:** 5 grosimi × 20 de decoruri = 100 de intrări.
*   **De ce nu:** o intrare pe tip; grosimea și decorul se aleg după plasare.

### Alt-Q. Feroneria ca fișiere `.aion` incluse în aplicație

*   **Pentru:** forme detaliate.
*   **Contra:** `core/` nu poate citi resurse Qt; două surse de date; pasul 6 ar depinde de pasul 9.
*   **De ce nu:** feroneria construită în cod (R-3).

### Alt-R. Decorul în `PartSpec`

*   **Contra:** instanțele legate n-ar putea avea decoruri diferite.
*   **De ce nu:** decorul stă în `Instance::material`.

### Alt-S. Redimensionarea plăcii prin scalarea instanței

*   **Contra:** întinde decorul, deformează plăcile rotite oblic.
*   **De ce nu:** calea principală sunt parametrii. Scale rămâne permis (D24), dar marchează placa.

### Alt-T. Abonament lunar

*   **Contra:** concurează direct cu abonamentul SketchUp, fără export în formatul firmelor și fără prețuri.
*   **De ce nu:** modelul de plată se decide după validarea comercială.

### Alt-U. Unirea materialelor doar după nume, cu decoruri cu nume distincte

*   **Pentru:** regula generală nu se schimbă.
*   **Contra:** un nume ales de utilizator se poate ciocni tot.
*   **De ce nu:** decizia R-6.

### Alt-V. Toate cele 8 limbi complete deodată

*   **Contra:** fiecare limbă cere întreținere la fiecare text nou; textele în germană și poloneză sunt mai lungi și
    cer verificarea fiecărui control.
*   **De ce nu:** română și engleză complete, restul „beta”.

### Alt-W. Trei documente de design separate (plăci; legare și „Întinde”; traducere)

*   **Pentru:** documente mai mici; pașii 8–11 scriși după pasul 0.
*   **De ce nu:** decizia R-4, un singur document.

## Riscuri și măsuri

| Risc | Gravitate | Măsură | Unde |
|---|---|---|---|
| Un singur om, prea multe fronturi | Blocant | Lista înghețată în `PLAN.md` și ADR 0043; punctele de control | Pasul 0 |
| Un singur tester, amator (înainte de a cere bani) | Blocant | Autorul testează la fiecare punct de control; aplicația merge la utilizatori externi (de exemplu un tâmplar) înainte de a cere bani | Verificare |
| Formatele firmelor de debitare sunt necunoscute (exportul) | Blocant | O comandă reală devine test de referință; rândul de antet se poate opri | Pașii 0, 4 |
| Firma scade cantul a doua oară | Major | Exportul implicit cu dimensiunea finită și rând de antet | §5 |
| Formatul v6 nu se mai poate retrage după publicare | Major | Pașii 1–4 se unesc în `main` în același timp | §8 |
| O eroare în finalizator afectează toate comenzile | Major | Atinge doar definițiile `board` schimbate; nicio schimbare dacă e deja în acord; o excepție anulează tranzacția; teste proprii | §3, pasul 5 |
| Compensarea la mutarea originii uită un câmp (pivot, scenă, cale, ghidaje) | Major | Lista câmpurilor în §3 și câte un test pentru fiecare | Pasul 5 |
| Reconcilierea suprascrie modificările de mână ale pieselor cu rol (decupaje, mutări) | Major | Avertisment în Inspector înainte de schimbare; „Transformă în corp obișnuit” | §4 |
| Editări de structură într-un corp generat (copiere, Explode, grupare) dublează piese | Major | Rolurile se pierd la copierea unei piese; Explode și gruparea transformă întâi corpul în corp obișnuit | §4, §7 |
| Unirea după cheie dublează materialele în fișierele vechi | Minor | Completarea categoriei la încărcare | §8 |
| Injecție de formule în CSV (securitate) | Major | `'` înaintea celulelor care încep cu `=`, `+`, `-`, `@` | §5 |
| Fișier `.aion` malformat în bibliotecă (securitate) | Minor | Încărcarea refuză fișierele invalide; plasarea afișează eroarea | §6 |
| Aplicația nesemnată, fără actualizări și raportare de crash-uri (înainte de a cere bani) | Major | În afara acestui design | Îmbunătățiri ulterioare |
| Prețul e plafonat de SketchUp + OpenCutList | Major | Se plătește pentru bibliotecă, lista de debitare (Obiectivele 1, 4, 5), export și deviz | Prezentare |

## Cerințe nefuncționale

*   **Compatibilitate:** vezi riscul „Formatul v6” și §8.
*   **Performanță:** lista de debitare pe modelul de stres din ADR 0038 (~1 400 de plăci, 200 de corpuri) se calculează
    în < 200 ms în build-ul release (prag provizoriu, revizuit după prima măsurătoare; test `[.perf]`). Finalizatorul
    rulează doar pe definițiile `board` schimbate. Liniile de cant stau în `DocumentPass` (se refac doar când se schimbă
    modelul; setarea „arată cantul” intră în `DocumentPassCache::Key`); doar evidențierea laturii de sub cursor se
    desenează în `Viewport::appendModelExtras`, la fiecare cadru. Fps-ul în orbită pe modelul de stres rămâne cel de azi
    (± 5%). Miniaturile din „În model” se fac doar la Make Component și la ieșirea din editare.
*   **Corectitudinea datelor pentru producție:** exportul scrie ce dimensiune trimite; testele de referință folosesc o
    comandă reală de la pasul 0.

## Resurse

*   [`concepts_brief.md`](../../concepts_brief.md), [`brainstorm_context.md`](../../brainstorm_context.md)
*   ADR-uri noi: 0043 (domeniul mobilier), 0044 (placa parametrică și formatul v6), create la pasul 0
*   ADR-uri existente: 0017 (raport de cantități), 0023 (oglindire), 0025 (obiecte dinamice), 0026 (bibliotecă), 0027
    (scripturi), 0029 (bevel), 0030 (pivot), 0038 (performanța viewport-ului), 0039 (copii), 0041 (segmente)
*   `PLAN.md`, `CLAUDE.md`

## Îmbunătățiri ulterioare

*   Exportul în formatul fiecărei firme (profiluri de coloane, separator, zecimală).
*   Nesting ghilotină cu fibră și lățimea pânzei, cu desene ale plăcilor brute.
*   Prețuri pe tip de placă, cant și feronerie.
*   Găurire pe sistemul de 32 mm ca date.
*   Planșe 2D cu cote, vederi explodate.
*   Avertisment pentru corpuri care se suprapun după „Întinde”.
*   Semnarea aplicației, actualizări automate, raportarea crash-urilor.
*   „Întinde” pe geometrie liberă.

--------------------------------------------------------------------------------

## Planul detaliat de implementare

Fiecare pas e o ramură (`feature/mobilier-<n>`), cu commit-uri mici, teste pentru tot ce e în `core/` și o rulare
`AION_STARTUP` + `AION_SCREENSHOT` pentru ce e în interfață (`CLAUDE.md`), care reproduce parcursul corespunzător.
Comenzile noi au prefixul `furniture.`, adăugat la `modelingPrefixes` în `Document::commandModes` (doar Modelare);
operațiile noi de model încep cu `require(Modeling)`. Pașii 1–11 scriu textele noi în română (DS-6). Datele de test
se construiesc în test, ca JSON (`io::fromJson`), fără fișiere binare în repo. Numerele de linie din cod sunt cele
din 2026-10-30.

### Pasul 0 (fără cod)

#### 1. `docs/decisions/0043-domeniul-mobilier.md` (Creare)

*   **Conținut:** nișa și concurentul; ordinea platformelor macOS → web → Windows și motivul; lista înghețată (ADR 0018,
    0020, 0021, 0033, 0034, 0035; zonele primesc doar traducerea); ADR 0039 rămâne valabil; biblioteca de piese și
    corpuri (R-3, R-7, R-8); reconcilierea (R-2); „Întinde” (DS-3); traducerea (DS-4, DS-6); motivele pentru DS-2, DS-5
    și cele 8 limbi. Modifică ADR 0017 și 0025 (lista „Decizii vechi schimbate”).

#### 2. `docs/decisions/0044-placa-parametrica-si-formatul-v6.md` (Creare)

*   **Conținut:** `PartSpec` și câmpurile noi; laturile de cant; categoria și completarea la încărcare (D10, D14);
    `boardState`, finalizatorul și compensarea (D12, D20, D24, R-1, DS-1); cantul (D19, D21, DS-5); lista v1.5 (D17,
    D22); legarea și unirea materialelor (D11, DS-2, R-6); formatul v6 (D13, R-5); tabelul drumurilor de păstrare.

#### 3. `PLAN.md` (Modificare)

*   **Conținut:** „Etapa 8 – Mobilier”, cu pașii 1–12 ca sarcini bifabile și criteriile de „gata” = testele din acest
    document; lista înghețată; „Modelare parametrică” scoasă din „Ce NU facem”.

#### 4. `CLAUDE.md` (Modificare)

*   **Conținut:** „Următorii pași” trimite la Etapa 8; punctul 0 despre funcțiile rămase din planul de animație se
    mută în `PLAN.md`, Etapa 6, ca amânat; ordinea platformelor actualizată.

#### 5. `docs/plans/mobilier-de-verificat.md` (Creare)

*   **Conținut:** întrebările din „Întrebări deschise”, cu valoarea implicită aleasă și locul unde se schimbă; autorul
    notează aici ce schimbă după testare.

### Pasul 1: formatul v6 și datele noi (M)

#### 6. `core/include/aion/core/model/Model.h`, `core/src/model/Model.cpp` (Modificare)

*   **Schimbări:** structurile din §1 (`BandSide`, `PartSpec`, câmpurile noi din `Definition`, `Instance`, `Material`),
    la sfârșit, cu `#include <array>`. `std::vector<DefinitionId> Model::definitionsWithMeshChanges() const`:
    definițiile încă existente al căror mesh are schimbări înregistrate, plus cheile din `m_recording->definitions` și
    definițiile create în tranzacție, sortate. Cere `bool geom::Mesh::hasRecordedChanges() const`
    (`core/include/aion/core/geom/Mesh.h`, Modificare).

#### 7. `core/src/io/AionFile.cpp`, `core/include/aion/core/io/AionFile.h` (Modificare)

*   **Schimbări:** `kFormatVersion = 6`, cu linia „6: date de piesă, chei, categorii” în istoricul din comentariu.
    Scriere: `definitions[i].part` (`kind` ca text „board” / „hardware”, `type`, `thickness`, `thicknessAxis`,
    `grainAxis`, `bands`: 4 obiecte `{band, decor}`), `sourceKey`; `instances[i].roleKey`,
    `nameKey`, `relativePosition` (număr); `materials[i].category`, `libraryKey` – toate doar când nu sunt goale. Citire cu
    valori implicite; un `kind` necunoscut se păstrează ca text într-un câmp `PartSpec::rawKind` și se scrie înapoi.
    `io::readSourceKey(std::span<const std::uint8_t>)` citește doar `sourceKey`-ul rădăcinii dintr-o arhivă.

#### 8. `core/src/model/Transfer.cpp`, `core/src/model/Copying.cpp` (Modificare)

*   **Schimbări:** `extractInstance` după tabelul din §7. `copyDeep` și `makeUniqueDeep` golesc `sourceKey` pe
    definițiile noi.

#### 9. `core/src/model/Boolean.cpp`, `core/include/aion/core/model/Operations.h`, `core/src/model/Operations.cpp` (Modificare)

*   **Schimbări:** rezultatul booleean primește transformarea operandului A și mesh-ul în coordonatele lui, plus
    `PartSpec` de la A, cu `generator` gol. Funcție nouă `bool explodeLosesPartData(const Model&, InstanceId)`:
    adevărat dacă definiția are `part`, `sourceKey` nevid sau `generator == "board"`.

#### 10. `render/include/aion/render/Document.h`, `render/src/Document.cpp`, `app/qml/Main.qml`, `app/qml/shell/EntityInfo.qml` (Modificare)

*   **Schimbări:** `Q_INVOKABLE bool explodeLosesPartData()` pe selecție (adevărat dacă oricare obiect pierde date).
    `Main.qml` (`case "org.explode"`) și `EntityInfo.qml` (butonul Explode) cer confirmare „Datele de piesă se pierd
    pentru N obiecte” înainte de `explodeSelection`.

#### 11. `tests/core/IoTests.cpp`, `tests/core/CopyingTests.cpp`, `tests/core/ModelTests.cpp` (Modificare)

*   **Teste:** salvare / încărcare cu toate câmpurile noi (`sameAs`); un JSON v5 se deschide cu `part` gol; `formatVersion` 7 e refuzat; un `kind` și un `typeKey` necunoscute se scriu înapoi
    neschimbate; `PartSpec` supraviețuiește `insertModel`, `extractInstance`, `copyDeep`, `makeUniqueDeep` și scăderii
    booleene (cu A rotit 30°: axele corecte); `sourceKey` gol pe copie, păstrat pe original;
    `definitionsWithMeshChanges` după un Push/Pull întoarce definiția atinsă.

### Pasul 2: tabelele, categoria, feroneria (M)

#### 12. `core/include/aion/core/model/Furniture.h`, `core/src/model/Furniture.cpp` (Creare); `core/CMakeLists.txt` (Modificare)

*   **Schimbări:** `BoardType`, `BandType`, `HardwareType` cu valorile din §2; `enum class Unit { SquareMeters,
    LinearMeters, Piece, Pair }`; `boardTypes()`, `bandTypes()`, `hardwareTypes()`, `findBoardType(key)`,
    `findBandType(key)`, `findHardwareType(key)` (întorc pointer sau `nullptr`); `nearestThickness(type, value)`;
    `createHardware(model, context, typeKey, placement)`; `backfillMaterials(Model&)`.

#### 13. `core/src/model/MaterialLibrary.cpp`, `core/include/aion/core/model/MaterialLibrary.h` (Modificare)

*   **Schimbări:** toate cele 30 de materiale primesc `category` și `libraryKey` (inițializare cu nume de câmp):
    „Oțel periat”, „Aluminiu”, „Cupru”, „Alamă” → `metal`; „Sticlă” → `glass`; „Lemn de nuc / stejar / pin” → `board-decor`; celelalte → categorie
    goală, cu cheie (`white.matte`, `concrete`…). Se adaugă la sfârșit 7 materiale (`std::array` crește la 37): „Alb
    (decor)”, „Gri (decor)”, „Nuc (decor)”, „Stejar (decor)”, „Wenge (decor)” (`board-decor`) și „Nuc masiv”, „Stejar
    masiv” (`wood`), cu culorile și texturile procedurale existente. `std::optional<std::size_t>
    libraryMaterialByKey(std::string_view)`.

#### 14. `core/src/io/AionFile.cpp` (Modificare)

*   **Schimbări:** la încărcarea unui fișier cu `formatVersion < 6`, cheamă `model::backfillMaterials`.

#### 15. `core/src/model/Transfer.cpp`, `core/src/model/Parametric.cpp`, `render/src/Document.cpp` (Modificare)

*   **Schimbări:** `Copier::material` după regula R-6 (§6). `libraryMaterial` (Parametric) și `libraryMaterialIn`,
    `selectLibraryMaterial`, `setInstanceMaterialByName` (Document) caută după `libraryKey`, cu numele ca rezervă.

#### 16. `app/qml/shell/MaterialsPanel.qml`, `render/src/Document.cpp` (Modificare)

*   **Schimbări:** câmpul „Categorie” în editorul de material (fără → "", lemn → `wood`, metal → `metal`, sticlă →
    `glass`, plastic → `plastic`, piatră → `stone`, decor de placă → `board-decor`); `createMaterial` / `updateMaterial` /
    `materialEntry` citesc și scriu `category`.

#### 17. `render/src/Document.cpp`, `app/qml/shell/LibraryPanel.qml` (Modificare)

*   **Schimbări:** intrările `hardware:<cheie>` în `libraryItems()` (categoria „Feronerie”), plasate prin
    `createHardware`; `libraryItemSize` pentru ele.

#### 18. `tests/core/FurnitureTests.cpp` (Creare), `tests/CMakeLists.txt` (Modificare)

*   **Teste:** PAL {16, 18, 25}, HDF {3, 4}; fiecare `BandType` are cel puțin un tip compatibil și toate cheile lui
    există în `boardTypes()`; `nearestThickness(pal, 17) = 18`; „Oțel periat” are `metal`; `createHardware("hinge.
    overlay.110")` are `PartSpec{Hardware}` și 1 bucată; un JSON v5 cu „Oțel periat” fără categorie → după
    `backfillMaterials`, `metal`; unirea: „Nuc (decor)” inserat lângă un „Nuc (decor)” al utilizatorului cu `wood` → două
    materiale.

### Pasul 3: măsurarea și „Fă placă” (S–M)

#### 19. `core/include/aion/core/model/Furniture.h`, `core/src/model/Furniture.cpp` (Modificare)

*   **Schimbări:** `PartMeasure { double length, width, thickness; bool box; bool extraShells; std::string reason; }`;
    `measurePart(model, definition, worldTransform)` măsoară doar mesh-ul propriu în axele definiției, înmulțit cu
    lungimea coloanelor transformării: grosimea pe `thicknessAxis`, lungimea pe `grainAxis`. Fără `part`: regula din
    ADR 0017 (fața cea mai mare), extrasă din `Quantities.cpp`. `isSimpleBox(mesh, size)`, lângă `isPristine` din
    `Modeling.cpp`, cu `tol::kLength`. `makeBoard(model, instance, typeKey)`: pune `part` pe definiție; axa grosimii =
    dimensiunea cea mai mică; fibra = axa cea mai lungă dintre celelalte două; grosimea = `nearestThickness`; tipul
    implicit `pal`. Refuză obiectele cu sub-piese și selecțiile de mai multe obiecte.

#### 20. `render/src/Document.cpp`, `render/include/aion/render/Document.h`, `app/qml/Main.qml` (Modificare)

*   **Schimbări:** comanda `furniture.makeBoard` („Fă placă”), în categoria „Mobilier” a paletei de comenzi.

#### 21. `tests/core/FurnitureTests.cpp` (Modificare)

*   **Teste:** cutia 600×400×18 → `measurePart` = 600, 400, 18, `box`; Push/Pull pe fața de grosime până la 36 →
    `thickness = 36`; Bevel pe o muchie → `box = false`, aceleași L, W, T; rotire 37° în jurul lui Z → 600×400×18;
    fibra pe Y → lungimea = 400; o linie desenată în interior → `extraShells`; „Fă placă” pe o cutie 600×18×400 → axa
    grosimii Y, PAL 18.

### Pasul 4: lista de debitare v1.5 (M)

#### 22. `core/include/aion/core/model/Quantities.h`, `core/src/model/Quantities.cpp` (Modificare)

*   **Schimbări:**
    *   `collect(..., bool includeHidden)`: cu `true` parcurge și instanțele ascunse; fiecare `Occurrence` are `hidden`
        (ea, tag-ul ei sau un părinte). `quantityReport` folosește `false`.
    *   `enum class CutSection { Board, Hardware, Other, Undefined }`.
    *   `CutPart` primește: `section`, `typeKey`, `nominalThickness`, `finishedLength`, `finishedWidth`, `cutLength`,
        `cutWidth`, `std::array<std::string,4> bands`, `std::array<std::string,4> bandDecors`, `hidden`, `rowKey`,
        `std::vector<std::string> usedIn`, `std::vector<std::string> flags`, `reason`. Feroneria și „Alte componente”
        sunt tot `CutPart`, cu secțiunea lor și `rowKey`.
    *   `CutListOptions` primește `std::set<std::string> excluded`, `bool cutDimensions`, `bool headerRow = true`,
        `double bandAllowance = 50.0`, `std::map<std::string,double> bandAllowanceByType`.
    *   `CutList` primește `std::vector<BandNeed> bands` (`bandKey`, `decor`, `meters`, `edges`) și
        `std::vector<HardwareNeed> hardware` (`typeKey`, `count`, `usedIn`); `edgeBanding` rămâne pentru piesele fără
        `part`.
    *   Clasificarea, gruparea, `rowKey`, materialul dominant determinist și totalurile din §5. La acest pas, toate
        plăcile se măsoară (ramura „Matches → parametri” vine la pasul 5).
    *   `toCsv(list, options, format)`: rândul de antet, coloanele în ordinea din §5, doar rândurile incluse;
        `csvField` pune `'` înaintea celulelor care încep cu `=`, `+`, `-`, `@`.
    *   Ordinea rândurilor: secțiune, tip, grosime, lungime descrescătoare.

#### 23. `render/src/Document.cpp`, `render/include/aion/render/Document.h` (Modificare)

*   **Schimbări:** `cutList(options)` citește cheile `excluded` (listă de `rowKey`), `cutDimensions`, `headerRow`,
    `bandAllowance`, `bandAllowanceByType` și întoarce `rowKey`, `hidden`, secțiunea, motivele, numărul de piese
    ascunse; `exportReport` trece aceleași chei mai departe.

#### 24. `app/qml/components/DataTable.qml` (Modificare)

*   **Schimbări:** proprietăți opționale `checkColumn: bool`, `rowKeys`, `checkedKeys`; semnal `rowToggled(key,
    checked)`; coloana de bife la început, cu componentele existente.

#### 25. `app/qml/overlays/QuantitiesDialog.qml` (Modificare)

*   **Schimbări:** `excludedKeys` inițializat din rândurile ascunse doar în `onAboutToShow`; păstrat la
    `onCutOptionsChanged` și la „Actualizează”; trimis la `cutList` și la export. Mesajul D22 și butonul „Include
    toate”; secțiunile Plăci / Feronerie / Alte componente / Nedefinite; coloana de motive; alegerea „finite / de
    tăiere” la export (implicit finite); avertisment la export dacă există piese ascunse debifate.

#### 26. `tests/core/FurnitureFixtures.h` (Creare), `tests/core/QuantitiesTests.cpp` (Modificare)

*   **Fixture-uri (JSON în cod):** dulapul de referință din §4, scris ca plăci etichetate și feronerie (la pasul 4
    generatorul nu există încă; la pasul 6, testul de la intrarea 38 verifică că generatorul produce exact acest
    fixture); dulapul v5 de azi
    (uși „Lemn de nuc”, mânere „Oțel periat”); comanda reală de la pasul 0.
*   **Teste:**
    *   dulapul de referință: lateralele un rând ×2, ușile ×2, spatele HDF 3, 8 balamale și 2 mânere la Feronerie;
    *   o ușă ascunsă → 6 piese pe rânduri separate cu `hidden` (placa, 4 balamale, mânerul);
    *   cu cheile lor în `excluded` → totalurile fără ele, CSV fără rândurile lor; fără excluderi → totalurile = cele de
        la modelul fără nimic ascuns;
    *   dulapul v5 → mânerele la „Alte componente”, plăcile `Board` cu motivul „tip nedefinit”;
    *   o cutie „Nuc masiv” fără `part` → „Nedefinite”, motiv „lemn fără tip”; o cutie de 18 mm „Nuc (decor)” fără `part`
        → Plăci, „tip nedefinit”;
    *   dulapul de referință scalat ×1,2 → dimensiuni măsurate (1200…), nu nominale;
    *   un nume de piesă „=1+1” → în CSV `'=1+1`;
    *   `quantityReport` nu numără piesele ascunse;
    *   modelul de stres ADR 0038 (`[.perf]`): lista < 200 ms în release (prag provizoriu).

### Pasul 5: placa parametrică (L)

#### 27. `core/include/aion/core/model/Furniture.h`, `core/src/model/Furniture.cpp` (Modificare)

*   **Schimbări:**
    *   `createBoard(model, context, typeKey, thickness, length, width, placement)`: definiție `generator = "board"`,
        `parameters {length, width}`, `part`, mesh-ul cutiei cu originea în colțul minim; `nameKey = "part.board"`.
    *   `rebuildBoard(model, definition)`: golește mesh-ul și îl reface din parametri.
    *   `BoardState { Matches, Modified, Detached }`, `ModifiedReason { Geometry, Thickness, Scaled, ScaledParent,
        Children }`; `boardState(model, occurrencePath)` după §3.
    *   `setBoardValue(model, definition, key, value)`: `length`, `width`, `type`, `thickness`, după §3 (pivoți,
        `nearestThickness`, canturi incompatibile golite, refuz pe „Modificată”).
    *   `adoptBoardGeometry(model, definition)` și `finalizeBoards(Model&, std::span<const DefinitionId>)`, după §3.
    *   `shiftBoardOrigin(model, definition, d)`: compensarea din §3: translația fiecărei instanțe `+ L·d`, pivotul
        explicit `− d` (scris direct, fără `setPivot`), `Scene::transforms` și `PathAnchor::before` `+ L_salvat·d`,
        ghidajele, adnotările și curbele Pen ale definiției `− d`; cheile de animație neschimbate.
    *   `restoreBoard(model, instance)`, `detachBoard(model, definition)`.

#### 28. `core/src/model/Parametric.cpp`, `core/include/aion/core/model/Parametric.h` (Modificare)

*   **Schimbări:** `boardGeneratorInfo()` separat de `registry()` (specificații `length` 10–5000, `width` 10–3000, pas
    0,1, zecimale 1); `regenerate` cheamă `rebuildBoard` pentru `board` înaintea lui `findGenerator`; `ParameterSpec`
    primește `int decimals = 0`.

#### 29. `core/include/aion/core/commands/CommandStack.h`, `core/src/commands/CommandStack.cpp` (Modificare)

*   **Schimbări:** `setFinalizer(std::function<void(model::Model&, std::span<const model::DefinitionId>)>)`, apelat
    **primul** în `commit()` (înainte să se golească numele tranzacției și înainte de `endRecording()`), numai pentru
    stivele pe model, cu lista din `definitionsWithMeshChanges()`. La excepție: `abort()`, excepția **nu** merge mai
    departe, `commit()` întoarce `false`, iar mesajul se păstrează în `std::string CommandStack::lastError()`;
    `Document` îl arată cu `setError`. Test: un finalizator care aruncă lasă modelul `sameAs` cel dinainte, fără pas de
    undo.

#### 30. `core/src/model/Quantities.cpp` (Modificare)

*   **Schimbări:** ramura „placă `Matches` → dimensiunile din parametri”.

#### 31. `render/src/Document.cpp`, `render/include/aion/render/Document.h` (Modificare)

*   **Schimbări:** înregistrează `model::finalizeBoards` ca finalizator; `entityInfo` adaugă `board` (tip, grosime,
    grosimi permise, lungime, lățime, starea și motivul, numărul de instanțe) și sare grupul generic „Parametri” pentru
    plăci; `Q_INVOKABLE setBoardValue(QString key, QVariant value)`, `restoreBoard()`, `detachBoard()`;
    `Document::setParameter` trimite plăcile la `setBoardValue`; `setPlacement` pe o placă `Matches`: X → `length`,
    Y → `width` (axele plăcii), Z ignorat; în modul Animare, dimensiunile plăcii sunt doar citire; intrările
    `board:<tip>` în `libraryItems()` (categoria „Plăci”), `placeLibraryItem`, `libraryItemSize`.

#### 32. `app/qml/Main.qml` (Modificare)

*   **Schimbări:** acțiunea de script `debug.board` (generator, parametri, `part`, starea și motivul, instanțele,
    `sourceKey`), lângă `debug.cutlist`.

#### 33. `app/qml/shell/EntityInfo.qml` (Modificare)

*   **Schimbări:** grupul „Parametri · Placă” (`PropertyGroup`): Tip, Grosime (liste), Lungime, Lățime (zecimalele din
    specificație, în delegatul de la `EntityInfo.qml:139`); starea „Modificată manual — <motiv>” cu „Readu la
    parametri” și „Desprinde de parametri”; „Afectează N instanțe”; Z din „Dimensiuni” doar citire pentru plăci.

#### 34. `app/qml/shell/LibraryPanel.qml` (Modificare)

*   **Schimbări:** categoria „Plăci” (PAL, MDF, HDF, placaj, lemn masiv).

#### 35. `tests/core/FurnitureTests.cpp`, `tests/core/CommandStackTests.cpp`, `tests/core/ModelTests.cpp`, `tests/core/ParametricTests.cpp` (Modificare)

*   **Teste:**
    *   decorul instanței rămâne după `setBoardValue(length, 800)`;
    *   finalizatorul pe o stivă de test: Push/Pull pe capătul +X până la 800 → lungime 800, `Matches`, placa pe loc în
        lume; pe capătul −X → la fel, cu originea mutată;
    *   Push/Pull pe lățime → `width` actualizat;
    *   Push/Pull pe fața de grosime până la 20 → `Modified/Thickness`;
    *   Bevel → `Modified/Geometry`; `restoreBoard` → cutie; undo → bevel;
    *   Scale ×1,5 → `Modified/Scaled`; `restoreBoard` → scara 1;
    *   oglindire pe X → `Matches`; părinte scalat ×1,2 → `Modified/ScaledParent`;
    *   rotită 90° în jurul lui Z, `setPlacement` cu X = 700 → lungime 700;
    *   pivot implicit, 600 → 800 → crește 100 de fiecare parte; pivot explicit la capăt (x = 600) → capătul rămâne
        pe loc în lume;
    *   placă rotită 90° în jurul lui Y, cu pivot explicit și chei de poziție: după Push/Pull pe capătul −X, placa nu se
        mișcă în lume și animația redă aceleași poziții; o scenă salvată cu placa oglindită o readuce în același loc;
    *   o placă mutată de mână în contextul ei (aceleași dimensiuni, colț în afara lui 0) → finalizatorul o readuce în
        acord; o rotire cu 90° în context → „Modificată manual”, cantul pe aceleași muchii;
    *   o placă cu o sub-piesă → finalizatorul nu o atinge; un ghidaj din contextul plăcii rămâne pe loc în lume;
    *   definiție cu 4 instanțe → toate compensate, un pas de undo;
    *   tipul PAL 18 → HDF → grosimea 4, canturile ABS golite;
    *   un Push/Pull pe un cub obișnuit → același `ChangeSet` ca înainte de finalizator; `rebuildBoard` nu declanșează
        a doua oară schimbări;
    *   `ParametricTests`: generatoarele din `registry()` nu includ `board`.

### Pasul 6: corpurile din bibliotecă (L)

#### 36. `core/src/model/Parametric.cpp`, `core/include/aion/core/model/Parametric.h` (Modificare)

*   **Schimbări:**
    *   Generatoarele noi: `base` („Corp la sol”), `wall` („Corp suspendat”), `tall` („Soldat”), `appliance` („Corp pentru
        electrocasnice”, cu nișă de înălțime dată), `wardrobe` („Dulap cu uși”), `rack` („Raft”). Câmpuri:
        `width`, `height`, `depth`, `doors`, `shelves`, `dividers`, `board`, `back` (și `nicheHeight` la `appliance`).
        Construcția din §4, cu `createBoard` și `createHardware`; fiecare piesă primește `roleKey` (polițele cu spațiul:
        `space.N.shelf.M`) și `nameKey`.
    *   `reconcile(model, definition, desired)`: potrivirea după `roleKey` din §4 (căutare în sub-piesele directe și în
        grupurile cu rol; id-ul cel mai mic la egalitate); ușile primesc poza închisă și `Behavior::open = false`;
        cheile de animație ale pieselor cu rol se rebazează cu diferența transformării de bază; înainte de refacere,
        `relativePosition` a separatoarelor puse cu clic se recalculează din poziția lor curentă.
    *   `rolePiecesDiffer(model, definition)`: adevărat dacă vreo piesă cu rol diferă de ce ar produce generatorul.
    *   `cabinet` și `shelf` rămân în `registry()` cu un indicator `legacy` (nu apar în bibliotecă și în paleta de
        comenzi), ca fișierele vechi să se refacă la fel ca azi.
    *   `upgradeGenerator(model, definition)`: `cabinet` → `wardrobe`, `shelf` → `rack`, cu aceleași dimensiuni.
    *   `makePlainBody(model, definition)`: golește generatorul corpului („Transformă în corp obișnuit”).
    *   `addDivider(model, body, pointInBody, atClick)`: separatorul pus cu clic, după §4 (refuz sub 50 mm de alt
        separator).
    *   `copyDeep` și mutarea în alt părinte golesc `roleKey` și `relativePosition` pe piesa copiată / mutată, dacă nu e
        copiat corpul întreg (`core/src/model/Copying.cpp`, `core/src/model/Operations.cpp`).

#### 37. `render/src/Document.cpp`, `render/include/aion/render/Document.h`, `app/qml/shell/EntityInfo.qml`, `app/qml/shell/LibraryPanel.qml` (Modificare)

*   **Schimbări:** categoria „Corpuri” în bibliotecă; `entityInfo` raportează dacă generatorul e vechi (`legacy`) și dacă un corp conține
    plăci „Modificate”; Inspector-ul arată „Generator vechi — Actualizează” doar pentru `cabinet` și `shelf`, butonul
    „Transformă în corp obișnuit” pentru corpurile generate și avertismentul `rolePiecesDiffer` înaintea unei schimbări de
    câmp; Explode, Make Group și mutarea în alt părinte pe piese cu rol cer confirmarea „Corpul devine obișnuit” (în
    `Main.qml` și `Outliner.qml`), apoi `makePlainBody`;
    `Q_INVOKABLE upgradeGenerator()`, `makePlainBody()`; comanda `furniture.addDivider` (mod de unealtă: următorul clic
    în corp pune separatorul).

#### 38. `tests/core/ParametricTests.cpp` (Modificare)

*   **Teste:** un dulap v5 rămâne neschimbat la deschidere; `setParameter` pe el folosește builder-ul vechi;
    `upgradeGenerator` → „Dulap cu uși” cu spate HDF 3 și uși cu feronerie, un pas de undo; „Corp la sol” 600 cu decor
    nuc pe `door.1`, cant `abs.2` pe ușă și un separator la 1/3 → lățimea 800: decorul, cantul și separatorul la 1/3
    rămân; `shelves` 3 → 2 → `space.1.shelf.3` șters, celelalte păstrate; un separator pus cu clic la 1/3 împarte polițele
    în două spații; un separator mutat de mână la 1/2 rămâne la 1/2 după refacere; o ușă deschisă (`open = true`) →
    după refacere închisă, `open = false`; ⌘V pe o poliță din corp → copia fără `roleKey`, refacerea nu dublează
    polițele; o piesă adăugată de utilizator fără `roleKey`
    rămâne; o ușă de 2000 mm are 4 balamale; după `makePlainBody`, o schimbare pe o placă nu reface corpul; lista de
    debitare a lui „Dulap cu uși” 1000×2000×600 = rândurile din testul de la pasul 4.

### Pasul 7: cantul (L)

#### 39. `core/include/aion/core/model/Furniture.h`, `core/src/model/Furniture.cpp` (Modificare)

*   **Schimbări:** `setBand(model, definition, sideIndex, bandKey, decor)`; `bandSideName(part, sideIndex)` → „L1”…
    „W2” după §5; `cutSize(part, measure)`; `BandPreset { std::string name; std::array<BandSide,4> sides; }`;
    `builtinBandPresets()`: „Fără”, „Toate 2 mm”, „L1 2 mm, restul 0,4 mm”.

#### 40. `core/src/model/Quantities.cpp` (Modificare)

*   **Schimbări:** coloanele de cant, dimensiunea de tăiere din `cutSize`, `BandNeed` pe (tip, decor) cu adaosul pe tip
    sau global; decorul „ca placa” rezolvat pe fiecare instanță.

#### 41. `render/src/Document.cpp`, `render/include/aion/render/Document.h` (Modificare)

*   **Schimbări:** `entityInfo.board.bands` (cele 4 laturi cu nume, tip, decor); `setBand(side, bandKey, decor)`;
    `setBands(QVariantList)` (presetarea trimisă din QML); `Q_PROPERTY int hoveredBandSide` (−1 = niciuna) cu semnal,
    fără să atingă documentul.

#### 42. `render/src/Viewport.cpp`, `render/include/aion/render/Viewport.h` (Modificare)

*   **Schimbări:** liniile colorate ale laturilor cu cant (culoarea decorului cantului) se desenează în `appendDocument`
    (`DocumentPass`), cu setarea „arată cantul” în `DocumentPassCache::Key`; doar latura `hoveredBandSide` a plăcii
    selectate se desenează, mai groasă, în `appendModelExtras` (la fiecare cadru). Segmentele: din parametri
    pentru plăcile `board` „Matches”, din `measurePart` pentru celelalte; transformate cu transformarea instanței,
    inclusiv reflexia. `hoveredBandSideChanged` → `update()`.

#### 43. `app/qml/components/BandPicker.qml` (Creare); `app/CMakeLists.txt`, `app/qml/shell/EntityInfo.qml`, `app/qml/theme/UserPrefs.qml`, `app/qml/overlays/PreferencesPage.qml`, `app/qml/overlays/QuantitiesDialog.qml` (Modificare)

*   **Schimbări:** `BandPicker.qml` (înregistrat în `qt_add_qml_module`): un dreptunghi cu 4 laturi pe care se dă clic;
    meniul fiecărei laturi (fără / tipurile compatibile / decor); trecerea cursorului → `doc.hoveredBandSide`;
    presetările incluse și ale utilizatorului, „Salvează ca presetare”. Grupul „Cant” în Inspector, sub „Parametri ·
    Placă”. `UserPrefs.bandPresets` (JSON) și `bandAllowance`, `bandAllowanceByType`. Preferințe → Mobilier: adaosul
    global și pe tip. Lista: coloanele L1–W2 și metrii pe tip.

#### 44. `tests/core/FurnitureTests.cpp`, `tests/core/QuantitiesTests.cpp` (Modificare)

*   **Teste:** 2 uși de 498,5 × 2000, `abs.2` (se scade) pe toate laturile → tăiere 494,5 × 1996, metri
    4 × 2498,5 + 8 × 50 = 10 394 mm; `abs.0.4` (nu se scade) → 498,5 × 2000; oglindire pe X → cantul pe aceleași muchii
    în lume; rotire 180° în jurul lui Z → cantul de pe L1 ajunge în spate; fibra pe Y → L1/L2 devin −U/+U, cantul
    rămâne pe aceleași muchii; decor „ca placa” pe două instanțe legate cu decoruri diferite → două `BandNeed`.

### Pasul 8: legarea și lista „În model” (M–L)

#### 45. `core/include/aion/core/model/Transfer.h`, `core/src/model/Transfer.cpp` (Modificare)

*   **Schimbări:** `findLinkedDefinition(model, sourceKey, context)` după §6; `placeLinked(model, definition, context,
    transform)`.

#### 46. `render/src/Document.cpp`, `render/include/aion/render/Document.h`, `render/src/Viewport.cpp`, `render/include/aion/render/Viewport.h` (Modificare)

*   **Schimbări:** `saveSelectionToLibrary` scrie `sourceKey` (nou sau păstrat la resalvarea aceleiași definiții) în
    fișier și pe definiția din model, într-un pas de undo. `placeLibraryItem("file:…")`: încarcă fișierul, citește
    `sourceKey`-ul rădăcinii și, dacă `findLinkedDefinition` găsește o definiție, plasează o instanță a ei.
    `modelComponents()` (nume, formă, număr de instanțe, miniatură), `placeModelComponent(id, at)`;
    `setComponentThumbnail(id, QByteArray)`. `Viewport::captureDefinitionThumbnail(id)`: randare separată, cu camera
    încadrată pe definiție. Jurnal: „definiție refolosită X” / „definiție nouă (motiv: niciuna accesibilă | ciclu)”.

#### 47. `app/qml/shell/LibraryPanel.qml`, `app/qml/Main.qml` (Modificare)

*   **Schimbări:** secțiunea „În model” (dale cu miniatura sau iconița formei, numele, „×N”), plasare prin tragere.
    `Main.qml` cere miniatura la Make Component și la ieșirea din editarea unei componente.

#### 48. `tests/core/ParametricTests.cpp` (Modificare; testele de inserare / extragere stau aici)

*   **Teste:** 4 plasări cu aceeași cheie → o definiție; toate instanțele șterse, apoi plasare → definiție nouă (orfana
    nu se refolosește); instanță doar într-o definiție orfană → definiție nouă; plasare în interiorul definiției legate
    → definiție nouă, `validate()` curat; ⌘V (`copyDeep`), apoi plasare → se leagă de original; plăcile și corpurile din
    bibliotecă nu au `sourceKey`.

### Pasul 9: feroneria în bibliotecă și deviz (M)

#### 49. `core/include/aion/core/model/Furniture.h`, `core/src/model/Furniture.cpp`, `render/src/Document.cpp`, `app/qml/shell/EntityInfo.qml` (Modificare)

*   **Schimbări:** `replaceHardwareVariant(model, definition, newTypeKey)`: înlocuiește forma și `typeKey` pe definiție
    (deci pe toate instanțele legate), un pas de undo. Inspector-ul feroneriei: varianta ca listă (grupată după tip:
    balama / glisieră / mâner) și „Afectează N instanțe”. Devizul din listă arată feroneria pe tip.

#### 50. `tests/core/FurnitureTests.cpp`, `tests/core/QuantitiesTests.cpp` (Modificare)

*   **Teste:** corp cu 2 uși × 2 balamale, plus 3 instanțe legate (⌥D) → 16 balamale, fără sub-piese în Plăci;
    `replaceHardwareVariant` pe o balama legată → toate instanțele schimbate, un pas de undo.

### Pasul 10: unealta „Placă pe față” (M)

#### 51. `core/src/tools/Tools.cpp`, `core/include/aion/core/tools/Tool.h` (Modificare)

*   **Schimbări:** unealta `board`, înregistrată în `makeTool`: ca Rectangle, pe o față sau pe planul inferat; la al
    doilea clic creează o placă cu `createBoard`, cu axa grosimii pe normala feței, spre exterior. Tipul și grosimea
    vin din `ToolContext::boardDefaults` (pointer la o structură ținută în `Document`, implicit `pal`, 18, pus la fiecare
    construire a contextului; fișierele `render/src/Document.cpp` / `Document.h` intră în acest pas); Inspector-ul arată
    „Placă nouă: tip / grosime” cât unealta e activă.
    VCB: lungime, lățime.

#### 52. `app/qml/shell/Toolbar.qml`, `app/qml/shell/ToolGuides.js`, `app/qml/components/ToolDemo.qml` (Modificare); `resources/icons/board.svg` (Creare)

*   **Schimbări:** unealta în grupul „Mobilier” al toolbar-ului (doar Modelare), textul ghidului și scena animată.

#### 53. `tests/core/ToolTests.cpp` (Modificare)

*   **Teste:** un dreptunghi pe o față de 600 × 720 → placă PAL 18 cu axa grosimii pe normala feței, `Matches`.

### Pasul 11: „Întinde” (L)

#### 54. `core/include/aion/core/model/Stretch.h`, `core/src/model/Stretch.cpp` (Creare); `core/CMakeLists.txt` (Modificare)

*   **Schimbări:** `StretchResult stretch(model, bodyInstance, axis, std::vector<double> planes, double distance)` după
    tabelul din §9; refuz cu motiv (`ThroughThickness`, `ModifiedBoard`, `FreeGeometry`) fără nicio schimbare;
    `equalPlanes(model, bodyInstance, axis, n)`.

#### 55. `core/src/tools/StretchTool.cpp` (Creare); `core/CMakeLists.txt`, `core/src/tools/Tools.cpp` (Modificare)

*   **Schimbări:** unealta `stretch`, înregistrată în `makeTool`: clic pe o față a corpului alege axa (normala); planul
    se vede și se mută cu cursorul; VCB: distanța și „N” (`200` sau `200/2`); previzualizare cu tranzacția uneltei
    (`beginTransaction` / `abort`), confirmare la Enter.

#### 56. `render/src/Document.cpp`, `app/qml/shell/Toolbar.qml`, `app/qml/shell/ToolGuides.js`, `app/qml/components/ToolDemo.qml` (Modificare)

*   **Schimbări:** unealta în grupul „Mobilier”; motivele de refuz în bara de stare; ghidul animat.

#### 57. `tests/core/StretchTests.cpp` (Creare), `tests/CMakeLists.txt` (Modificare)

*   **Teste:** corpul din U7 (600 mm, PAL 18, 2 uși, construcția din §4, din plăci `board`) → 800: `validate()` fără erori, plăcile
    `Matches`, baza 564 → 764; două uși, două
    plane, +200 → fiecare ușă +100; plan prin grosimea unei plăci → refuz, modelul `sameAs` înainte; corp cu 4 instanțe
    legate → toate la 800; o poliță legată cu alt corp → devine unică, celălalt corp neschimbat; feroneria doar mutată;
    corp din bibliotecă → câmpul `width` = 800; undo → `sameAs`.

### Pasul 12: traducerea (M, apoi L)

#### 58. `core/include/aion/core/text/Translate.h` (Creare)

*   **Schimbări:** `#define AION_TR(key) key` (macro identitate, marchează cheia pentru `lupdate`); convenția cheilor
    `modul.zona.text` (de exemplu `cutlist.column.length`).

#### 59. Fișierele din `core/src/**` cu texte afișate (Modificare)

*   **Schimbări:** textele devin `AION_TR("…")`: generatoarele și câmpurile lor, `nameKey` ale pieselor, antetele CSV și
    numele implicite din `Quantities.cpp`, mesajele de stare ale uneltelor, numele pașilor de undo, erorile de
    încărcare. Textele compuse întorc cheie + argumente. Textele doar pentru dezvoltare (`MeshValidate.cpp`) rămân
    netraduse.

#### 60. Fișierele din `render/` care trimit texte din core (Modificare)

*   **Schimbări:** cheile trec prin `qtTrId(key)` înainte de QML; numele pieselor: `name` sau `qtTrId(nameKey)`.

#### 61. Toate fișierele `app/qml/**/*.qml` și `.js` (Modificare)

*   **Schimbări:** fiecare text vizibil devine `qsTrId("…")`, inclusiv în zonele înghețate (doar textele); pluralele cu
    `%n`. Un script din `tools/` face înlocuirea și scrie traducerea română a fiecărei chei.

#### 62. `translations/aion_ro.ts`, `translations/aion_en.ts`, `translations/aion_de.ts`, `translations/aion_pl.ts`, `translations/aion_it.ts`, `translations/aion_fr.ts`, `translations/aion_es.ts`, `translations/aion_pt.ts` (Creare)

*   **Schimbări:** RO completată de scriptul de la intrarea 61; EN completă, verificată de autor; celelalte 6 traduse cu
    unealta aleasă de autor, în afara build-ului, și marcate `type="unfinished"` până la verificarea de către un vorbitor
    nativ. Scriptul de la intrarea 61 se numește `tools/extract_strings.py`: cheia = `<fișier fără extensie>.<text
    românesc fără diacritice, cu „_”>`, scurtată la 60 de caractere, cu sufix numeric la ciocnire.

#### 63. `app/CMakeLists.txt`, `app/src/main.cpp` (Modificare)

*   **Schimbări:** `CMakeLists.txt` rădăcină: `LinguistTools` în `find_package(Qt6 …)`. `qt_add_translations(aion_app
    … SOURCE_TARGETS aion_core aion_render aion_app)` (și în build-ul web), cu `lupdate -tr-function-alias
    QT_TRID_NOOP+=AION_TR`. `main.cpp` alege limba: `AION_LANG`, apoi `ui/language`, apoi limba sistemului dacă e una
    dintre cele 8, altfel engleza; încarcă și `aion_en.qm` și `aion_ro.qm` ca traducători de rezervă (pentru cheile
    lipsă, §10), plus `qtbase_xx.qm`, înainte de `loadFromModule`. CI instalează `lupdate` / `lrelease` (Qt de pe
    gazdă pentru web).

#### 64. `app/qml/overlays/PreferencesPage.qml`, `app/qml/theme/UserPrefs.qml`, `app/macos/Info.plist.in` (Modificare)

*   **Schimbări:** Preferințe → General → Limbă (8 limbi; toate în afară de RO și EN marcate „beta”), mesajul „se aplică
    după repornire”; `UserPrefs.language`; `CFBundleLocalizations` cu cele 8 limbi.

#### 65. `.github/workflows/ci.yml` (Modificare); `tools/check_translations.py` (Creare)

*   **Schimbări:** CI rulează `lupdate` și eșuează dacă o cheie folosită lipsește din `aion_ro.ts` sau `aion_en.ts`.

#### 66. `docs/scripting.md`, `render/src/ScriptEngine.cpp` (Modificare, în pașii 4, 5 și 7)

*   **Schimbări:** pasul 4: `aion.cutList(options)` → obiectul listei (citire). Pasul 5: `aion.board(type, thickness,
    length, width, [x, y, z])` → id-ul instanței sau `null` la tip necunoscut; `aion.setBoardValue(id, key, value)` →
    `true` / `false`. Pasul 7: `aion.setBand(id, side, bandKey, decor)`. Fiecare funcție e documentată în același pas; `docs/scripting.md`
    spune că un script e o singură comandă, deci parametrii plăcilor se actualizează abia la sfârșitul lui.

### Completări după reverificarea din runda 2 (se aplică pașilor indicați)

*   **Pasul 1:** `geom::Mesh::hasRecordedChanges()` în `Mesh.h`; `sourceKey` golit în `cloneShallow`
    (`Copying.cpp`) și în `copyLinked` (împreună cu `roleKey`, `relativePosition` pe copia de sus).
*   **Pasul 2:** cheile materialelor incluse = numele fără diacritice, cu „-” (`alb-mat`, `otel-periat`); categoriile:
    „Oțel periat”, „Aluminiu”, „Cupru”, „Alamă” → `metal`; „Sticlă”, „Sticlă fumurie” → `glass`; „Piatră”, „Beton”,
    „Beton închis”, „Beton turnat”, „Gresie 30 × 30” → `stone`; „Lemn de stejar / nuc / pin”, „Bambus”, „Lemn (fibră)”
    → `board-decor`; restul → categorie goală. Cele 7 materiale noi: „Alb (decor)” (242, 241, 236), „Gri (decor)” (150,
    152, 156), „Nuc (decor)”, „Stejar (decor)”, „Wenge (decor)” (cu textura procedurală „lemn”, 800 mm, nuanțată), „Nuc
    masiv”, „Stejar masiv” (`wood`, aceeași textură). `builtinTexture` devine un tabel pe index. Generatoarele din core
    pun materialele fără textură; `Document` completează textura după fiecare comandă care adaugă un material inclus
    (`Document::fillLibraryTextures`).
*   **Pasul 4:** CSV-ul neutralizează și TAB și CR (test cu TAB); `occurrences()` primește și ea `includeHidden`;
    opțiunile noi se citesc în `cutListOptions(options)` (`Document.cpp`); bifele folosesc
    `app/qml/components/CheckBox.qml` (Creare, înregistrat în `app/CMakeLists.txt`); adaosul = `bandAllowanceByType[tip]`
    dacă există, altfel `bandAllowance`; modelul de performanță se generează în test: 200 de `wardrobe` (~1 400 de plăci).
*   **Pasul 5:** numele pieselor până la pasul 12: `render/src/PartNames.cpp` (Creare) – tabel cheie → text românesc,
    folosit de Outliner, Inspector și listă; înlocuit de `qtTrId` la pasul 12. `setBoardValue` are două variante:
    `(key, double)` pentru dimensiuni și grosime, `(key, std::string_view)` pentru tip. O placă din bibliotecă se pune
    culcată, 600 × 400. Testele din `ParametricTests` folosesc `findGenerator("board") == nullptr` și `generators()`.
*   **Pasul 6:** `GeneratorInfo::legacy` (ultimul câmp) filtrat în `Document::generators()` și `libraryItems()`;
    „Adaugă separator” e o unealtă internă `divider` (ca `pivot`, prin `ToolRequests`): corpul = cel mai apropiat strămoș
    cu generator de corp; ⌥ citit din modificatorii evenimentului; Esc anulează. Confirmarea „Corpul devine obișnuit” la
    Explode vine după confirmarea „Datele de piesă se pierd”, într-un singur dialog cu ambele mesaje.
*   **Pasul 7:** `Viewport::showBands` (`Q_PROPERTY`, implicit `true`, salvat în `UserPrefs`), buton în
    `app/qml/shell/ViewportControls.qml`; schimbarea emite `appearanceChanged` (golește `DocumentPass`); liniile de cant
    se desenează ca `topLines`, cu un decalaj de 0,3 mm spre exteriorul plăcii; culoarea pentru „ca placa” = culoarea
    decorului instanței, mai închisă cu 20%. „Toate 2 mm” pe o placă HDF lasă laturile fără cant (tipul nu e
    compatibil). Decorul cantului acceptă doar materiale incluse (`libraryKey`).
*   **Pasul 8:** `placeLibraryItem` folosește `sourceKey`-ul din fișierul deja încărcat (fără `io::readSourceKey`).
*   **Pasul 11:** `makeStretchTool()` declarat în `core/src/tools/ToolHelpers.h`; id-ul adăugat în comentariul din
    `Tool.h`; iconița `resources/icons/stretch.svg` (Creare); corpul = instanța selectată, altfel cea mai de sus din
    contextul activ sub cursor; refuzul `ModifiedBoard` acoperă și plăcile desprinse.
*   **Pasul 12:** intrarea 66 e făcută la pașii 4, 5, 7 (fiecare dintre acești pași o include); `ScriptEngine.h`
    (Modificare) pentru noile funcții `Q_INVOKABLE`.

### Planul de verificare

*   **Compilare:** `make build` (debug, UBSan) și `make build CONFIG=release`; CI pe macOS, Windows (MSVC) și web.
*   **Teste unitare:** `make test` (Catch2, toate testele de mai sus); `aion_tests "[.perf]"` pentru lista pe modelul de
    stres.
*   **Performanța viewport-ului:** `navbench:orbit` pe modelul de stres, cu liniile de cant active, înainte și după
    pasul 7: fps-ul în orbită în ± 5%.
*   **Analiză statică:** `clang-format` pe fișierele modificate; `tools/check_translations.py` de la pasul 12.
*   **Interfață:** pentru fiecare pas cu interfață, o rulare `AION_STARTUP=… AION_SCREENSHOT=…` care reproduce
    parcursul corespunzător (U1 și U5 la pasul 4, U3 la pasul 5, U2 fără cant și U8 la pasul 6, U2 complet și U4 la
    pasul 7, U6 la pasul 8, U7 la pasul 11); autorul compară captura cu rezultatul „Atunci” al parcursului.
*   **Puncte de control (R-10):** după pasul 4 și după pasul 7, autorul modelează un dulap real, exportă CSV-ul și îl
    verifică față de lista lui. Trimiterea la o firmă de debitare și testarea de către alți utilizatori vin după
    pasul 7.

--------------------------------------------------------------------------------

*Revizuit cu EGM*
