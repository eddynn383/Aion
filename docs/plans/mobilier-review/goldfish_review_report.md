# Goldfish Annotated Review Report

**Status: resolved** (2026-10-30: 27 de puncte acceptate, 3 escaladări confirmate fără schimbări, 1 contestare acceptată; aplicate în `docs/plans/mobilier-pe-corp.md`, plus deciziile noi R-7 și R-8)

**Target:** `docs/plans/mobilier-pe-corp.md` (full file, 1–1036)

**Surse:** `zoom_out_report.md` (Etapa 5), `comprehension_report.md` (6), `critic_report.md` (7),
`intermediate_zoom_out_report.md` (8), `implementation_readiness_report.md` (9). Constatările identice din mai multe
etape sunt grupate într-un singur punct; id-urile originale sunt în paranteze.

**Decizii ale proprietarului luate după recenzie (2026-10-30), care stabilesc corecturile:**

| Id | Decizie |
|---|---|
| R-1 | Dimensiunea plăcii: **parametri + geometrie**, cu toate corecturile (finalizatorul rămâne) |
| R-2 | Generatorul de dulap / raft: **viu, reconciliază copiii după cheia de rol** |
| R-3 | Feroneria inclusă: **construită în cod** (`createHardware`, intrări `hardware:<cheie>`) |
| R-4 | **Un singur document**, pașii 1–12 |
| R-5 | Formatul: **mereu v6, schema înghețată la pasul 1** (toate câmpurile noi definite atunci) |
| R-6 | Unirea materialelor: **după `libraryKey`, apoi nume + categorie; la conflict, material nou** |

Decizii mecanice luate de autor (un singur răspuns apărabil, decurg din cele de mai sus): `explodeLosesPartData`
(funcție liberă); `core/src/tools/StretchTool.cpp`; `AION_LANG` are prioritate față de setări; `Instance::nameKey`
separat de `name` (cerut de P-4); pivotul explicit își păstrează poziția relativă în cutie (cerut de P-1); „Lemn de
nuc / stejar / pin” primesc categoria `board-decor`; categoria materialelor v5 se completează la încărcare.

## Prioritized Findings & AI Suggestions

### [1] Mânerele v5 nu ajung la „Alte componente” – Severity: High
(comprehension-1, design-1-M1, IR-B2, Etapa 8 M6)
*   **Affected Scope:** Obiective (57–59), P1 (106–109), §1 (218–220), §8 (335–339), intrările 7, 14–15, 25
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:** §8 și intrarea 7: la încărcarea unui fișier cu `formatVersion < 6`, `model::backfillMaterials`
    (în `Furniture.cpp`) dă fiecărui material cu `category` goală și nume egal cu un material inclus categoria și
    `libraryKey` ale acestuia. Intrarea 14: tabelul complet al celor 30 de materiale (categorie, cheie), „Lemn de …” =
    `board-decor`, plus cele 7 noi. Test nou în `IoTests`: dulap v5 cu uși „Lemn de nuc” → mânere la „Alte componente”,
    ușile „tip nedefinit”.

### [2] Lipsește secțiunea „Riscuri și măsuri” – Severity: High
(structural-1, design-1-M12)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `## Riscuri și măsuri` după „Alternative considerate”: riscurile din brief (un singur om, un
    tester amator, formatele firmelor, scăderea dublă a cantului, aplicația nesemnată, plafonul de preț), plus:
    finalizatorul (rază mare de impact), formatul v6 ireversibil (R-5) cu măsura „pasul 1 se unește în `main` doar
    împreună cu pasul 4”, reconcilierea generatorului (R-2), injecție de formule în CSV, fișiere `.aion` din bibliotecă
    malformate.

### [3] Placa „Matches” într-un corp scalat – Severity: High
(design-1-B1)
*   ***AI Suggestion:*** PROPOSE FIX (în cadrul R-1)
*   ***Proposed Edit:*** `boardState` verifică transformarea acumulată până la lume (partea liniară ortonormală, ±1
    permis); altfel `Modified/ScaledParent`, iar lista măsoară. Test: dulapul de referință scalat ×1,2.

### [4] Finalizatorul nu vede schimbările – Severity: High
(IR-B3, codebase B1)
*   ***AI Suggestion:*** PROPOSE FIX (în cadrul R-1)
*   ***Proposed Edit:*** `Model::definitionsWithMeshChanges()` (nou, `Model.h/.cpp`); `CommandStack::setFinalizer(
    std::function<void(Model&, std::span<const DefinitionId>)>)` apelat în `commit()` înainte de `endRecording()`;
    logica în `model::finalizeBoards` (`Furniture.cpp`), testată în core; garanția „nicio schimbare dacă placa e deja
    în acord”; o excepție în finalizator anulează tranzacția (`abort()`).

### [5] Mutarea originii strică pivoții și datele în coordonatele definiției – Severity: High
(design-1-M3, codebase M10, IR-M2)
*   ***AI Suggestion:*** PROPOSE FIX (în cadrul R-1)
*   ***Proposed Edit:*** la mutarea originii, fiecare instanță își ajustează transformarea și pivotul explicit prin
    `setPivot` (care mută și cheile de animație); `Scene::transforms` și ancorele căilor primesc aceeași compensare;
    pivotul explicit își păstrează poziția relativă în cutie. Test pentru pivot implicit și explicit.

### [6] Placa înregistrată ca generator obișnuit – Severity: High
(IR-B5)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** „board” în afara `registry()` (`boardGeneratorInfo()` separat); `entityInfo` sare grupul
    generic; `Document::setParameter` trimite plăcile la `setBoardValue`; `regenerate` tratează `board` primul;
    `ParametricTests.cpp:56-76` actualizat.

### [7] Pasul 6 depinde de fișierele pasului 9 – Severity: High
(IR-B1, codebase B2, Etapa 8 M3, Etapa 5 M8)
*   ***AI Suggestion:*** PROPOSE FIX (R-3)
*   ***Proposed Edit:*** `HardwareType` cu dimensiuni și o funcție de geometrie pe tip; `createHardware` la pasul 2;
    intrările 46 (fișiere, script, resurse) scoase; biblioteca are `hardware:<cheie>`.

### [8] `appendOverlay` nu există; cache-ul `DocumentPass` – Severity: High
(IR-B4, comprehension-12, structural-7)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** liniile de cant și evidențierea în `Viewport::appendModelExtras` (la fiecare cadru);
    `Document::hoveredBandSideChanged` → `Viewport::update()`; liniile persistente urmează regula ADR 0038.

### [9] Alegeri lăsate deschise – Severity: High
(IR-B6, style 9 M1–M4, zoom-out N4, comprehension-24)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `explodeLosesPartData`; `StretchTool.cpp` + `core/CMakeLists.txt` + `makeTool`; `AION_LANG`
    înaintea setărilor; segmentele din parametri pentru plăcile `board`, din `measurePart` pentru restul.

### [10] Generatorul v2 șterge datele – Severity: High
(design-1-M2, Etapa 5 M2, Etapa 8 M1, IR-M17, codebase M15)
*   ***AI Suggestion:*** PROPOSE FIX (R-2)
*   ***Proposed Edit:*** `Instance::roleKey` (în schema v6); `reconcile(model, definition, built)`: potrivește copiii
    după `roleKey`, actualizează transformarea și parametrii, păstrează `material`, `PartSpec` (cant) și varianta de
    feronerie; adaugă piesele noi, șterge doar piesele cu `roleKey` care nu mai apar (o ușă eliminată); copiii fără
    `roleKey` (adăugați de utilizator) nu se ating. Modificările de geometrie dintr-o placă se pierd (trecut la riscuri).
    Specificația v2: dispunerea plăcilor, rostul ușilor (3 mm), polițe, parametrul `board` (corp) + `back` (spate),
    balamale ≤ 900 / 901–1600 / > 1600 mm; alegerea versiunii în `regenerate`; „Generator vechi” doar pentru
    `cabinet` și `shelf`.

### [11] Schema v6 și retragerea – Severity: Medium
(design-1-M5, M6, comprehension-15, Etapa 5 M4, Etapa 8 M4)
*   ***AI Suggestion:*** PROPOSE FIX (R-5)
*   ***Proposed Edit:*** §8: schema completă la pasul 1 (`part`, `sourceKey`, `roleKey`, `nameKey`, `category`,
    `libraryKey`, `generatorVersion`); propoziția despre retragere corectată; regula de unire în `main`.

### [12] Unirea materialelor după nume – Severity: Medium
(design-1-M7, codebase M4, Etapa 5 M9)
*   ***AI Suggestion:*** PROPOSE FIX (R-6)
*   ***Proposed Edit:*** `Copier::material`: după `libraryKey`; fără cheie, nume + categorie; la conflict, material nou;
    `setInstanceMaterialByName` aliniat; risc „dubluri în fișierele vechi” trecut la riscuri.

### [13] Numele generate ca chei – Severity: Medium
(design-1-M8, comprehension-10, Etapa 5 M7, IR traducere)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `Instance::nameKey` (v6); `name` rămâne textul utilizatorului; afișarea folosește `nameKey`
    tradus dacă `name` e gol; fișierele vechi rămân cu numele în română. Traducerea în C++: alias
    `QT_TRID_NOOP+=AION_TR` și `qtTrId(key)`. Zonele înghețate primesc chei și traducere în engleză (Obiectivul 9 rămâne
    „RO și EN complete”).

### [14] Axele câmpurilor „Dimensiuni” – Severity: Medium
(comprehension-2, IR-M1, Etapa 8 M2, style 9 M12–M13)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** câmpurile sunt în axele plăcii (cum e `Placement::size`); testul corectat: „rotită 90° în
    jurul lui Z, X = 700 → lungime 700”; Z doar citire pentru toate plăcile `board`.

### [15] „Întinde” – Severity: Medium
(comprehension-7, 8, design-1-M10, IR-M14, codebase M9, Etapa 8 N1, style 9 M15)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** „Întinde” scrie parametrii de lungime fără compensarea P-1 (crește din partea tăiată); planele
    în coordonatele corpului; „zonă” = împărțirea dimensiunii în N intervale egale; un corp generat: „Întinde” schimbă
    parametrul de lățime al generatorului (nu refuză, datorită R-2); plăci Modified → refuz cu motiv; definițiile
    partajate modificate devin unice (`makeUniqueDeep`); preview cu tranzacția uneltei; tabelul §9 completat.

### [16] Ierarhia ușă / balama – Severity: Medium
(design-1-M4, Etapa 8 N3)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** ușa = grup cu placa și balamalele alături; placa rămâne frunză.

### [17] Glosar, tabel de decizii, lista pașilor, serii de id-uri – Severity: Medium
(comprehension-3, 21–23, structural-4, style 7 N16, style 9 M6–M7)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** glosar după „Prezentare”; tabel scurt de decizii (D, DS, R); lista de o linie a pașilor;
    parcursurile redenumite U1–U8, deciziile de design DS-1…DS-6; „copii” → „instanțe” / „sub-obiecte”.

### [18] Nume și consecvență – Severity: Medium
(structural-2, style 7 M1–M6, comprehension-4, 13, 14)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `PartSpec` peste tot; „deviz” definit (blocul de totaluri, fără prețuri); „tâmplarul consultat”;
    Obiectivul 1 și 8 restrânse la ce testează; „Resurse” și „Îmbunătățiri” ca H2; lista „ADR-uri afectate”
    (0017, 0025, 0038, 0039); `PLAN.md` „Ce NU facem” și ordinea platformelor în ADR 0043.

### [19] Dulapul de referință – Severity: Medium
(comprehension-5, structural-14)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** definiție completă: lățime 1000 × înălțime 2000 × adâncime 600, PAL 18, spate HDF 3, 2 uși,
    3 balamale pe ușă, 1 mâner pe ușă (în grupul ușii); N = numărul de piese (plăci + feronerie). P5: o ușă ascunsă =
    5 piese (placa, 3 balamale, mânerul).

### [20] Lipsuri în date și tabele – Severity: Medium
(IR-M6, M7, M9, codebase M13, comprehension-18–20, style 9 M9–M11)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** tabelele complete (marcate „provizoriu până la pasul 0”), `Unit` cu metri liniari, grosime
    implicită, departajare în `nearestThickness` (în sus); ordinea indicilor `bands` și maparea L1–W2 (cu fibra pe X,
    pe Y, fără fibră); `CutPart` / `HardwareNeed` / `rowKey` / ordinea CSV; maparea secțiunilor; fixture-ul v5 construit
    ca JSON în test.

### [21] Integrare lipsă – Severity: Medium
(codebase M1–M3, M5–M8, M11–M12, IR-M11–M12, M21–M22, codebase N1, N6)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `collect(includeHidden)`; `furniture.` în `modelingPrefixes`; confirmarea Explode în
    `Main.qml` / `EntityInfo.qml`; `Document.cpp` la pasul 16; `io::readSourceKey`; unealta în `Tools.cpp`; presetările
    și limba în `UserPrefs.qml`, trimise din QML; `app/CMakeLists.txt` pentru `BandPicker.qml`; `Main.qml` pentru
    `debug.board`; `ToolDemo.qml`; `EntityInfo.qml:139` în loc de `NumberField.qml`; miniatura componentei izolată
    (cameră încadrată pe definiție); ultima placă folosită în `ToolContext`, implicit PAL 18.

### [22] CSV: escaping și antet – Severity: Medium
(design-1-M12)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `csvField` pune `'` înaintea celulelor care încep cu `=`, `+`, `-`, `@`; rândul de antet
    opțional (implicit activ) până la pasul 0; separatorul rămâne cel din ADR 0017.

### [23] Amploarea (lista „În model”, cantul vizual, 8 limbi) – Severity: Medium
(Etapa 5 M5, M6, M7)
*   ***AI Suggestion:*** PROPOSE FIX (decizii păstrate de proprietar: DS-2, DS-5, D5)
*   ***Proposed Edit:*** obiective noi care le testează: „Lista În model arată componentele cu ≥ 1 instanță și plasează
    o copie legată”; „Latura de sub cursor se evidențiază în viewport”; Obiectivul 9 rămâne. Motivul păstrării în
    ADR 0043.

### [24] Pasul 4 depinde de `boardState` din pasul 5 – Severity: Low
(IR N3)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** la pasul 4 lista măsoară toate plăcile; pasul 5 adaugă ramura „Matches → parametri”.

### [25] Generatorul dulapului vs pasul 11 – Severity: Low
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** cu R-2, refuzul `Generator` din „Întinde” dispare (vezi [15]).

### [26] Minore de stil și formă
(toate etapele: „click”/„clic”, termeni englezești, căi scurte, numere de linie care se învechesc, diagrama ASCII,
blocul de cod `text`, liniile orizontale, „macro gol”, „banda”, limitele balamalelor, legenda S/M/L etc.)
*   ***AI Suggestion:*** PROPOSE FIX pentru cele care schimbă sensul (căi complete, limite, legendă, „macro identitate”,
    blocul de cod `cpp`, `#include <array>`); DECLINE pentru restul (formulări, „clic”), care nu schimbă implementarea.

### [27] Benchmark pe modelul de stres din ADR 0038 – Severity: Low
*   ***AI Suggestion:*** PROPOSE FIX — ținta de performanță pe ~1 400 de plăci, nu 500.

### Contestate / escaladate

*   **Etapa 5 M1, Etapa 8 B1 (geometria ca singură sursă):** ESCALATE → decis de proprietar (R-1 = B). Se înregistrează
    motivul în ADR 0044; corecturile [3]–[6] închid defectele găsite.
*   **Etapa 5 M2, Etapa 8 M1 (generator o singură dată):** ESCALATE → decis (R-2 = reconciliere). Corectura [10].
*   **Etapa 8 M5 (trei documente):** ESCALATE → decis (R-4 = un document).
*   **Etapa 5 M3 (`generatorVersion` → id de generator):** CONTEST — cu R-2 și R-5, `generatorVersion` e un câmp
    simplu în schema înghețată; un id nou ar dubla intrările în `registry()` și în bibliotecă.
