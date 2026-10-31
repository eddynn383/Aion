# Etapa 9 – Testul de pregătire pentru implementare (runda 1)

* **Document:** `docs/plans/mobilier-pe-corp.md`, DetailedRange (liniile 570–1036)
* **Recenzori:** egm-implementation-readiness (standard implementation-readiness) – terminat · egm-codebase-details
  (același standard) – terminat · egm-style – terminat
* **Data:** 2026-10-30
* **Verdict:** **nu e gata de implementare.** După deduplicare: 🔴 6 · 🟡 ~35 · 🟢 ~25.
* **Runda e curată?** Nu.

Constatările marcate „✔” au fost verificate de agentul principal în cod. Multe blocante și majore de aici sunt
aceleași cu cele din etapele 5–8 (vezi matricea de la final); simplificările din Etapa 8 le-ar elimina pe multe.

## 🔴 Blocante

**IR-B1. Pasul 6 depinde de fișierele create la pasul 9, iar `core/` nu poate citi resurse Qt.** (825–829 vs 917–921)
`buildCabinetV2` folosește mânerele din `resources/library/hardware/`, create abia la pasul 9 și incluse cu
`qt_add_resources`. `core/` nu include Qt, iar `io::loadFile` citește doar de pe disc (`AionFile.h:43`). Pasul 6 nu se
poate compila sau testa singur. *Corectură:* feroneria construită în C++ (`createHardware(model, context, typeKey,
placement)` în `Furniture.cpp`, o cutie cu `PartSpec{Hardware}`), cu dimensiunile și poziția pe ușă / laterală; pasul 9
generează fișierele din aceeași funcție. (Aceeași cu M3 din Etapa 8.)

**IR-B2. Categoria materialelor v5 nu e completată niciodată.** (670–681, 755–760) Testul „fișier v5 → mânerele la
Alte componente (metal)” nu poate trece. În plus, dacă „Lemn de nuc / stejar / pin” primesc `wood`, ușile v5 și tot
raftul v5 ajung la „Nedefinit (lemn)”, nu la „tip nedefinit” – contradicție cu P1 și cu același test. *Corectură:*
tabelul complet pentru toate cele 30 de materiale existente; regula de completare (de exemplu în `cutList`: categorie
goală + nume din `builtinMaterials()` → categoria acestuia, fără să schimbe fișierul); decizia `board-decor` sau
`wood` pentru „Lemn de …”; test cu un dulap v5 cu uși din lemn. (Aceeași cu blocantul din etapele 6–7.)

**IR-B3. Finalizatorul nu poate primi `ChangeSet`-ul înainte de închiderea tranzacției.** (783–792) ✔ `commit()`
cheamă întâi `m_model->endRecording()` (`CommandStack.cpp:65-75`); schimbările de mesh se strâng abia acolo
(`Model.cpp:597-607`), iar `Model` expune doar `isRecording()`. Logica plăcii ar sta într-un lambda din `Document`,
deci testele D20 din `FurnitureTests` nu o văd. Lipsește și garanția „nicio schimbare dacă placa e deja Matches”.
*Corectură:* `Model::definitionsWithMeshChanges()`; finalizatorul primește `(Model&, span<const DefinitionId>)`,
apelat înainte de `endRecording()`; `model::finalizeBoards` în `Furniture.cpp`, testat acolo. (Sau eliminarea
finalizatorului prin simplificarea A din Etapa 8.)

**IR-B4. `Viewport::appendOverlay` nu există, iar desenul uneltei e în `DocumentPass`, care e păstrat în cache.**
(860–866) ✔ Nicio apariție a lui `appendOverlay` în `render/`. Overlay-ul uneltei se desenează în `appendDocument`
(`Viewport.cpp:423-472`), păstrat după `DocumentPassCache::Key`; un `hoveredBandSide` care nu atinge documentul nu ar
redesena nimic. *Corectură:* liniile de cant în `Viewport::appendModelExtras` (desenat la fiecare cadru), cu
`hoveredBandSideChanged` → `update()`.

**IR-B5. Placa înregistrată ca generator obișnuit intră în bibliotecă, în paleta de comenzi, în Inspector și strică un
test existent.** (779–781) `registry()` alimentează `libraryItems()` (`gen:board`), `Document::generators()`, grupul
generic „Parametri · …” și `ParametricTests.cpp:56-76`, care trece prin toate generatoarele și așteaptă geometrie.
*Corectură:* „board” în afara `registry()`; `entityInfo` sare grupul generic pentru plăci; `setParameter` trimite
plăcile la `setBoardValue`; `regenerate` tratează `board` înainte de `findGenerator`; testul actualizat. (Sau
simplificarea A.)

**IR-B6 (stil, ridicată). Alegeri lăsate implementatorului în instrucțiuni:** intrarea 10 (`bool* lostPartData` sau
`explodeLosesPartData`), intrarea 53 (`TransformTools.cpp` sau `StretchTool.cpp`, marcat „Creare”), intrarea 61
(`QSettings` sau `AION_LANG`, fără prioritate), intrarea 38 („din parametri sau din cutia măsurată”, fără condiție).
*Corectură:* `explodeLosesPartData`; `StretchTool.cpp` + `core/CMakeLists.txt` + înregistrare în `makeTool`;
`AION_LANG` are prioritate; parametrii pentru plăcile `board`, `measurePart` pentru celelalte.

## 🟡 Majore

### Date și format

- **Tabelele de tipuri incomplete:** chei complete pentru plăci, canturi, feronerie; formatul plăcii brute, `toCutter`,
  `grain`, compatibilitățile, valorile `Unit` (inclusiv metri liniari), grosimea implicită pe tip, departajarea în
  `nearestThickness` (17 e la egală distanță de 16 și 18). Testul „fiecare BandType e compatibil cu tipurile lui” nu
  verifică nimic. Tabel complet, marcat „provizoriu până la pasul 0”.
- **Tabelul de materiale incomplet:** toate cele 30, plus cele 7 noi cu culori și chei; `std::array<Material,30>` devine
  37; inițializare cu nume de câmp (designated initializers).
- **Modelul de date al listei:** formula `rowKey`, câmpurile `HardwareNeed`, dacă feroneria și „Alte componente” sunt
  `CutPart` cu secțiune, ordinea coloanelor CSV, conținutul celulelor L1–W2, ordinea rândurilor; maparea celor 5
  `CutSection` pe cele 4 secțiuni din fereastră.
- **`collect` e folosit și de raportul general:** parcurgerea pieselor ascunse le-ar pune și acolo. Un parametru
  `includeHidden` și un test.
- **`bands`: ordinea indicilor nedefinită;** maparea L1/L2/W1/W2 cu fibra pe Y și fără fibră (255); laturile când
  `thicknessAxis ≠ 2`; presetarea „față 2 mm, rest 0,4” (care latură e „față”); `decor` gol = „ca placa”.
- **Booleenele:** rezultatul are transformarea identitate (`Boolean.cpp:103, 116`), deci axele copiate de la A sunt
  greșite dacă A era rotit.
- **`PartSpec`:** valori implicite lipsă pentru `kind` și `nominalThickness`; `#include <array>` în `Model.h`; un
  `kind` necunoscut aruncă `part` (contrazice „valorile necunoscute se păstrează”).
- **Datele de test:** `tests/data/` nu există și nu are cale la compilare; fixture-ul v5 construit în test ca JSON
  (`io::fromJson`), fără ZIP binar în repo.

### Placa și Inspector-ul

- ✔ **Test vs regulă:** „rotită 90° în jurul lui Z, Y = 700 → lungime 700” contrazice „în axele plăcii”
  (`Placement::size` e pe axele obiectului). Unul trebuie corectat. (Și Etapele 6, 7, 8.)
- **Pivotul explicit:** formula pentru pivotul implicit și pentru unul setat de utilizator; mutarea originii trebuie să
  mute și pivotul (`setPivot`, care ajustează și cheile de animație).
- **`setBoardValue` pe o placă „Modificată”** și schimbarea tipului cu canturi incompatibile: comportament nedefinit.
- **Domeniul pentru „Z doar citire”:** toate plăcile sau doar cele `Matches`.
- **`NumberField.qml` are deja `decimals`;** valoarea 0 e fixată în `EntityInfo.qml:139` – acolo se schimbă.
- **„Fă placă”:** tipul implicit, regula fibrei când grosimea e pe X, regulile de selecție (mai multe obiecte, obiect cu
  copii, obiect care nu e cutie).

### Generatoarele și feroneria

- **Generatorul v2 nedefinit:** așezarea plăcilor, rostul ușilor, polițele, soarta parametrului `board`, conținutul
  raftului v2; alegerea versiunii trebuie făcută în `regenerate`, nu în `setParameter`; regenerarea pierde tot (Etapele
  5, 7, 8).
- **„Generator vechi” ar apărea și pe uși, ferestre, mese:** limitat la `cabinet` și `shelf`.
- **Biblioteca de feronerie:** schema de id-uri, enumerarea resurselor (`QDirIterator` pe `:/…`), încărcarea cu `QFile`
  + `io::readArchive`, gruparea variantelor, scriptul generator (nume, limbaj), mărimile glisierelor.

### Legarea, cantul, „Întinde”, traducerea

- **Legarea:** funcția nouă `io::readSourceKey`; cheia ca `sourceKey` al definiției rădăcină; salvarea unei singure
  plăci; unde se pune materialul în `extractInstance`.
- **Presetările de cant în `UserPrefs` (singleton QML):** C++ nu le poate citi; QML trimite conținutul
  (`setBands(QVariantList)`). La fel pentru limbă (`UserPrefs.language`).
- **Miniaturile „În model”:** `captureThumbnail` capturează tot viewport-ul, nu o componentă; nu e numit setter-ul și
  nici cine declanșează captura.
- **„Întinde”:** `preview…/commitOperation` e un mecanism al panourilor din `Document`, nu al uneltelor din core (acolo:
  `beginTransaction` / `abort`); sistemul de coordonate al planelor; ce înseamnă refuzul `Generator`; două plăci legate
  tăiate de plane diferite; cum mută utilizatorul planul; ce e o „zonă”.
- ✔ **Traducerea:** cheile marcate cu `qsTrId` sunt mesaje după id, care se caută cu `qtTrId`, nu cu
  `QCoreApplication::translate("aion", key)`; alias-ul corect pentru C++ este `QT_TRID_NOOP+=AION_TR`. Numele românești
  din pașii 6–11 și din fișierele v5 nu au migrare.
- **API-ul de scripturi** (`aion.board(...)`) fără semnătură; intrarea 64 trebuie împărțită pe pașii 4, 5, 7.

### Integrare

- ✔ **`furniture.*` ar fi permis în toate modurile:** `commandModes` clasifică după prefix
  (`Document.cpp:3036-3048`); trebuie adăugat `furniture.` la `modelingPrefixes`.
- **Confirmarea la Explode:** în `Main.qml` (`case "org.explode"`) și `EntityInfo.qml`, cu `Q_INVOKABLE bool
  explodeLosesPartData()` în `Document.h`; regula pentru mai multe obiecte.
- **Fișiere lipsă din plan:** `Document.cpp` pentru câmpul de categorie (pasul 16); `setInstanceMaterialByName` și
  `Copier::material` (pasul 15); `Tools.cpp` pentru unealta „board” (Rectangle e acolo, nu în `ShapeTools.cpp`);
  `app/CMakeLists.txt` pentru `BandPicker.qml`; `Main.qml` pentru acțiunea `debug.board`; `ToolDemo.qml` pentru ghidul
  animat; `UserPrefs.qml`; `Model.h/.cpp` pentru finalizator.
- **Unealta „Placă pe față”:** unde se ține tipul folosit ultima dată și ce valoare are la prima folosire (PAL 18).

### Stil (instrucțiuni ambigue)

- „copii” cu două sensuri (obiecte-copil vs instanțe ale aceleiași definiții); seria „P” cu trei sensuri; ținta lipsă
  la „scoate funcțiile P2 de animație”; criterii nemăsurabile („fără fisuri”, „captură verificată”, „traduse
  automat”); teste prea comprimate („fața de grosime la 36 → `thickness 36`”).

## 🟢 Minore (rezumat)

Căi scurte inexistente (`core/model/Furniture.h/.cpp`); datele de test în două foldere; „regula de azi” și numerele de
linie care se învechesc; „click” / „clic”, termeni englezești în proză; limitele balamalelor (≤ 900 / 901–1600 /
> 1600); „macro gol” (e macro identitate); „banda” de informare vs cantul („band”); `createBoard` trebuie să mute
vârfurile după `buildPrimitive`; testele booleene sunt în `ModelTests.cpp`; legenda de efort S/M/L; lista de nume a
limbilor „beta”; linia de istoric „6: …” în `AionFile.h`; benchmark pe modelul de stres din ADR 0038.

## Matrice de urmărire

| Problemă | Etapa 5 | Etapa 6 | Etapa 7 | Etapa 8 | Etapa 9 |
|---|---|---|---|---|---|
| Mânere v5 fără categorie | — | 1 | design-M1 | M6 | IR-B2 |
| Feroneria pasului 6 creată la pasul 9 | (M8) | — | — | M3 | IR-B1 |
| Finalizatorul / dimensiuni salvate de două ori | M1 | — | design-M3 | B1 | IR-B3 |
| Placa ca generator obișnuit | M1 | — | — | B1 | IR-B5 |
| Generatorul v2 regenerează | M2 | 6, 7 | design-M2 | M1 | majore |
| Axele câmpurilor „Dimensiuni” | — | 2 | comprehension-2 | M2 | majore |
| Evidențierea cantului și cache-ul | (M6) | 12 | structural-7 | — | IR-B4 |
| Traducerea cu chei | M7 | 9, 10 | design-M8 | M4 | majore |
| Alegeri lăsate deschise | N4 | 24 | — | — | IR-B6 |
