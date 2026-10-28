# Pen-ul și căile de parcurgere — plan de implementare

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** un Pen ca în Figma (curbe Bézier plane, editabile) în Modelare și în Animare, iar în Animare căi de
parcurgere pe care obiectele ancorate merg cu fața înainte, cu durată, easing, loc în timeline și decalaj față de cale.

**Architecture:** matematica Bézier e în `core/geom/Bezier` (fără Qt, cu teste). În Modelare, o curbă Pen = muchii
obișnuite (`geom::drawCurve`, același `curve` id) + o entitate `model::PenCurve` care păstrează punctele și mânerele;
editarea redesenează muchiile. În Animare, o curbă Pen = o entitate `model::MotionPath` (obiect de scenă, ca o cameră),
cu ancorele ei; `model::applyPaths` pune obiectele ancorate pe cale la un timp, iar `Document::pose()` îl folosește.
Uneltele noi (`pen`, `penedit`) sunt în `core/tools`; ce nu pot face singure (o cale, o editare de cale) trimit prin
`ToolRequests`, ca `PivotTool`.

**Tech Stack:** C++20, Qt 6 Quick/QML, Qt RHI, Catch2, glm, nlohmann-json.

**Spec:** [`docs/plans/pen-si-cai.md`](pen-si-cai.md) (aprobat de utilizator pe 2026-10-28). Citește-l întâi; acest
plan argumentează din el.

## Global Constraints

- `core/` nu include Qt; orice cod nou din `core/` vine cu teste Catch2 în `tests/core/` (fișierele noi se adaugă în
  `tests/CMakeLists.txt` și `core/CMakeLists.txt`, liste explicite).
- Unitatea internă: mm. Comparațiile geometrice numai prin `core/include/aion/core/math/Tolerance.h` (`tol::kLength`…).
- Undo / redo pentru orice schimbare (desen, fiecare editare, ancorare, setări, tragerea barei): prin
  `commands::CommandStack` / `Document::runOnModel`; undo trece prin `Document::stepHistory` (nu ocoli).
- Formatul `.aion`: `kFormatVersion = 4` (`core/include/aion/core/io/AionFile.h`); fișierele v1–v3 se deschid
  neschimbate (cheile noi lipsesc → nimic de încărcat).
- UI: culori doar din `Theme.qml`; componente existente (`PropertyGroup`, `NumberField`, `RowAction`, `IconButton`,
  `EasingEditor`); textele în română. QML: nu folosi ca nume de variabile `long`, `int`, `short`, `byte`, `char`,
  `float`, `double` (CI le respinge); un Popover nu poate avea proprietăți `opacity` / `spacing`.
- Comenzile / uneltele noi se clasifică în `Document::commandModes`; operațiile noi de model încep cu `require(...)`.
- Redarea și exportul arată doar modelul (`Viewport::cleanView()`): căile nu se văd acolo.
- Commit-uri mici, în română, terminate cu `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; ramuri
  `feature/...`, unite în `main` cu `--no-ff`, apoi CI urmărit (`gh`) după fiecare merge.
- Verificare vizuală: `AION_STARTUP` / `AION_SCREENSHOT` (rulări cu profilul „Aion Test”); limite de timp cu
  `/usr/bin/perl -e 'alarm N; exec @ARGV' ...`. După `view.*` / `view.extents` așteaptă ≥ 3000 ms înainte de
  acțiuni în coordonate de lume (camera încă se mișcă).

## Review Focus

1. **Decalaj lateral mai mare decât raza unei curbe strânse** → obiectul nu trebuie să sară sau să se întoarcă brusc:
   orientarea vine din tangenta căii, poziția rămâne finită. Test în Task 6.
2. **Curbe degenerate** (două puncte identice, mânere de lungime 0, o cale de 2 puncte dreaptă) → tangentă fără NaN
   (cade pe direcția coardei). Test în Task 1.
3. **Ancorează, șterge calea, undo** → obiectul revine pe cale, cu ancora intactă; redo îl duce înapoi unde era. Test
   în Task 6.
4. **Calea editată cu obiecte ancorate** → obiectele urmează forma nouă; scoase după editări, revin exact la `before`.
   Test în Task 6.
5. **Click lângă primul punct al Pen-ului, când inferența prinde altceva în apropiere** → închiderea câștigă. Test în
   Task 3.

---

### Task 1: Matematica Bézier (`core/geom/Bezier`)

**Files:**
- Create: `core/include/aion/core/geom/Bezier.h`, `core/src/geom/Bezier.cpp`
- Test: `tests/core/BezierTests.cpp`
- Modify: `core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `aion::geom`):
  ```cpp
  struct BezierPoint {
      Vec3d point { 0.0 };
      Vec3d in { 0.0 };   // mânerul de intrare (absolut); == point → fără mâner
      Vec3d out { 0.0 };  // mânerul de ieșire
      bool smooth = false; // mânerele legate (o tragere a unuia îl oglindește pe celălalt)
      bool operator==(const BezierPoint&) const = default;
  };
  struct BezierPath {
      std::vector<BezierPoint> points;
      bool closed = false;
      [[nodiscard]] std::size_t segmentCount() const; // closed ? n : n - 1 (0 sub 2 puncte)
      bool operator==(const BezierPath&) const = default;
  };
  [[nodiscard]] Vec3d bezierPoint(const BezierPath& path, std::size_t segment, double t);
  [[nodiscard]] Vec3d bezierTangent(const BezierPath& path, std::size_t segment, double t); // unitară
  // Eșantionarea pentru muchii: abaterea coardei ≤ tolerance (mm); 2..64 subdiviziuni pe segment; fără puncte
  // duplicate; pentru o cale închisă, primul punct nu se repetă la final.
  [[nodiscard]] std::vector<Vec3d> sampleBezier(const BezierPath& path, double tolerance);
  [[nodiscard]] double defaultTolerance(const BezierPath& path); // clamp(diagonala cutiei · 0.002, 0.5, 10.0)
  // Parametrizarea după lungime (tabel dens: 64 de subdiviziuni pe segment).
  struct PathFrame { Vec3d point { 0.0 }; Vec3d tangent { 1.0, 0.0, 0.0 }; };
  class ArcLength {
  public:
      explicit ArcLength(const BezierPath& path);
      [[nodiscard]] double length() const;
      // distance < 0 sau > length: pe o cale deschisă, prelungirea dreaptă pe tangenta capătului;
      // pe una închisă, distanța se ia modulo lungimea.
      [[nodiscard]] PathFrame at(double distance) const;
  };
  // Editare (forma rămâne aceeași la inserare: de Casteljau).
  [[nodiscard]] BezierPath insertPoint(const BezierPath& path, std::size_t segment, double t);
  [[nodiscard]] BezierPath removePoint(const BezierPath& path, std::size_t index);
  [[nodiscard]] BezierPath transformed(const BezierPath& path, const math::Transform& t);
  ```

- [ ] **Step 1: Write the failing tests** in `tests/core/BezierTests.cpp`, tag `[core][bezier]`:
  - `"Bézier: un segment drept fără mânere"`: puncte `(0,0,0)` și `(1000,0,0)`, mânere = punctele → `sampleBezier(p, 1)`
    are exact 2 puncte; `ArcLength(p).length() ≈ 1000`; `at(500).point ≈ (500,0,0)`, `at(500).tangent ≈ (1,0,0)`.
  - `"Bézier: un sfert de cerc aproximat are lungimea corectă"`: punctele `(1000,0,0)` → `(0,1000,0)` cu mânerele la
    `k = 552.28` (`out = (1000,k,0)`, `in = (k,1000,0)`) → `length()` în ±0.5% din `π·1000/2`; fiecare punct din
    `sampleBezier(p, 1)` la `|r − 1000| ≤ 1.2`.
  - `"Bézier: viteza după lungime e uniformă"`: pe același sfert, distanțele dintre `at(i·L/10)` consecutive diferă cu
    < 1%.
  - `"Bézier: prelungirea capetelor și calea închisă"`: pe calea dreaptă, `at(-200).point ≈ (-200,0,0)`,
    `at(1300).point ≈ (1300,0,0)`; o cale închisă (pătrat cu colțuri) dă `at(L + 100) ≈ at(100)`.
  - `"Bézier: inserarea nu schimbă forma"`: `insertPoint(p, 0, 0.3)` are 3 puncte și `bezierPoint` pe calea nouă
    reproduce 20 de puncte ale celei vechi (± `tol::kLength`); `removePoint` pe punctul inserat revine la 2 puncte.
  - `"Bézier: tangentă finită pe curbe degenerate"` (Review Focus 2): două puncte identice; mânere de lungime 0; un
    punct neted cu mânerul pe punct → `bezierTangent` și `ArcLength::at(...).tangent` sunt finite și unitare (cad pe
    coardă sau pe `(1,0,0)` când coarda e 0).

- [ ] **Step 2: Run** `cmake --build build/mac-debug --target aion_tests && build/mac-debug/tests/aion_tests "[bezier]"`
  — Expected: FAIL la compilare (`Bezier.h` lipsește).
- [ ] **Step 3: Implement** `Bezier.h` / `Bezier.cpp` cu interfața de mai sus (cubic Bézier `point, out, in(next),
  next`; eșantionare adaptivă prin subdivizare până la abaterea cerută; tabelul de lungime cu `(segment, t)` la fiecare
  intrare — tangenta din derivata Bézier la `t` interpolat, nu din coarda tabelului).
- [ ] **Step 4: Run** aceeași comandă — Expected: PASS.
- [ ] **Step 5: Commit** — `git commit -m "Bézier: eșantionare, lungime, cadrul la o distanță, inserare / ștergere de puncte"`

### Task 2: Curbele Pen în model (`model::PenCurve`), geometria lor și fișierul v4

**Files:**
- Modify: `core/include/aion/core/model/Ids.h` (`PenCurveTag`, `using PenCurveId`), `core/include/aion/core/model/Model.h`
  (entitatea, SlotMap, `ChangeSet::penCurves`, add / remove / set), `core/src/model/Model.cpp` (apply / record),
  `core/include/aion/core/geom/Operations.h` + `core/src/geom/Operations.cpp` (`drawCurve` cu id dat),
  `core/include/aion/core/io/AionFile.h` (`kFormatVersion = 4`), `core/src/io/AionFile.cpp`
- Create: `core/include/aion/core/model/PenCurves.h`, `core/src/model/PenCurves.cpp`
- Test: `tests/core/PenCurveTests.cpp`, `tests/core/IoTests.cpp` (un caz nou)

**Interfaces:**
- Consumes: Task 1 (`BezierPath`, `sampleBezier`, `defaultTolerance`).
- Produces:
  ```cpp
  // Model.h
  struct PenCurve {
      DefinitionId definition;   // definiția în care s-a desenat (editabilă doar acolo)
      std::uint32_t curve = 0;   // id-ul de pe muchiile ei (Edge::curve)
      geom::BezierPath path;     // în coordonatele definiției
      math::Vec3d normal { 0.0, 0.0, 1.0 }; // normala planului
      bool operator==(const PenCurve&) const = default;
  };
  PenCurveId addPenCurve(PenCurve c); void removePenCurve(PenCurveId); void setPenCurve(PenCurveId, PenCurve);
  [[nodiscard]] const geom::SlotMap<PenCurveId, PenCurve>& penCurves() const;
  // geom/Operations.h
  DrawLineResult drawCurve(Mesh& mesh, const std::vector<Vec3d>& points, bool closed, bool smooth, std::uint32_t curve);
  // model/PenCurves.h (namespace aion::model)
  PenCurveId addPenCurveGeometry(Model& model, DefinitionId definition, const geom::BezierPath& path, const math::Vec3d& normal);
  bool replacePenCurve(Model& model, PenCurveId id, const geom::BezierPath& path); // false dacă nu e editabilă
  [[nodiscard]] bool penCurveEditable(const Model& model, PenCurveId id);
  [[nodiscard]] PenCurveId penCurveForEdge(const Model& model, DefinitionId definition, geom::EdgeId edge); // gol = niciuna
  ```
  Reguli (spec 3.1): editabilă = muchiile ei există încă exact pe eșantionarea curbei (fiecare pereche consecutivă de
  puncte e o muchie cu `curve` id-ul ei; alte muchii cu același id, de exemplu copii, nu contează) **și** toate fețele
  lipite de ele sunt coplanare cu planul curbei (după Push/Pull apare o față laterală → nu mai e editabilă).
  `replacePenCurve` șterge muchiile vechi (`eraseEdge`, care reface fețele pe plan) și desenează cu `drawCurve(…, curve)`
  — împărțirea feței-gazdă se reface singură. `smooth = true` la `drawCurve`.

- [ ] **Step 1: Write the failing tests** (`tests/core/PenCurveTests.cpp`, `[core][pen]`), cu un `Model` și un
  `CommandStack` (ca în `ModelTests.cpp`):
  - `"Pen: o curbă deschisă pe sol dă muchii cu același curve id și rămâne editabilă"`: un S din 3 puncte (al doilea
    neted) → `penCurves().size() == 1`; toate muchiile noi au `curve` = cel al entității; `penCurveEditable == true`;
    `penCurveForEdge(model, root, oricareMuchie)` = id-ul ei.
  - `"Pen: o curbă închisă devine o față"`: 4 puncte netede, `closed = true` → apare exact o față nouă, cu normala
    `(0,0,1)`.
  - `"Pen: o curbă închisă pe o față-gazdă o împarte și rămâne editabilă"`: un dreptunghi 4000×3000 desenat, apoi o
    curbă închisă în interior → 2 fețe; `penCurveEditable == true`; `replacePenCurve` cu un punct mutat → tot 2 fețe,
    iar muchiile vechi au dispărut (nu există muchie între pozițiile vechi).
  - `"Pen: după Push/Pull nu mai e editabilă"`: curba închisă, `geom::pushPull` pe fața ei → `penCurveEditable ==
    false`, `replacePenCurve` întoarce `false` și nu schimbă nimic.
  - `"Pen: un vârf mutat o face needitabilă, o copie nu"`: `mesh.setPosition` pe un vârf al curbei → `false`;
    separat, o „copie” = aceleași muchii desenate în altă parte cu același id (`drawCurve(mesh, puncteMutate, false,
    true, acelașiCurveId)`) → originalul rămâne `true`.
  - `"Pen: undo / redo"`: `stack.run("Pen", [&](model::Model& m) { addPenCurveGeometry(...); })` apoi `undo` → nicio
    entitate și nicio muchie; `redo` → înapoi, editabilă.
  - În `IoTests.cpp`: `"Fișier: curbele Pen se salvează și se încarcă (v4)"` — `toJson` / `fromJson` păstrează
    `PenCurve` identic; un JSON v3 fără cheia `penCurves` se încarcă fără erori.
- [ ] **Step 2: Run** `build/mac-debug/tests/aion_tests "[pen]"` — Expected: FAIL (tipuri lipsă).
- [ ] **Step 3: Implement** entitatea (urmând exact modelul `CameraObject`: SlotMap, `ChangeSet` map, `addEntity` /
  `removeEntity` / `setEntity`), `drawCurve` cu id (cel vechi devine `drawCurve(mesh, points, closed, smooth,
  mesh.newCurveId())`), `PenCurves.cpp`, serializarea (`"penCurves": [{ id, definition, curve, normal:[3], closed,
  points:[[px,py,pz, ix,iy,iz, ox,oy,oz, smooth]] }]`), `kFormatVersion = 4` (comentariul din `AionFile.h` explică v4).
  Make Group / Explode / Make Unique / copierea între definiții: curbele rămân legate de definiția veche → acolo nu mai
  sunt intacte → needitabile (nu e nevoie de cod în plus; un test cu `makeGroup` arată `false`).
- [ ] **Step 4: Run** `build/mac-debug/tests/aion_tests` (toată suita) — Expected: PASS.
- [ ] **Step 5: Commit** — `"Curbele Pen în model: geometria lor, editabilitatea, fișierul v4"`

### Task 3: Unealta Pen (desenul, în core)

**Files:**
- Create: `core/src/tools/PenTool.cpp`
- Modify: `core/src/tools/ToolHelpers.h` (`makePenTool`), `core/src/tools/Tools.cpp` (`if (id == "pen")`),
  `core/include/aion/core/tools/Tool.h` (`ToolContext::drawsPaths`, `ToolRequests::penPath`, `penNormal`)
- Test: `tests/core/PenToolTests.cpp`

**Interfaces:**
- Consumes: Task 1, Task 2 (`addPenCurveGeometry`).
- Produces:
  ```cpp
  // ToolContext
  bool drawsPaths = false; // Animare: curba devine o cale (cerere), nu geometrie
  // ToolRequests
  std::optional<geom::BezierPath> penPath; // Animare: curba terminată, în coordonatele contextului
  math::Vec3d penNormal { 0.0, 0.0, 1.0 };
  ```
  Comportament (spec 3.1): planul = fața de sub primul click (normala ei, prin `pickWithInstances` / inferența
  `OnFace`) sau solul `z = 0`; fiecare punct = inferența (ca Line: `withGuides`, `withGridSnap`, `from` = punctul
  anterior) proiectată pe plan. pointerDown fixează punctul; tragerea > `kDragThresholdPx` până la pointerUp = mânerul
  `out` (cursorul proiectat pe plan), `in` = simetricul, `smooth = true`; fără tragere = colț (`in = out = point`).
  pointerDown la ≤ 10 px pe ecran de primul punct (cu ≥ 2 puncte) = închide (câștigă în fața inferenței — Review Focus
  5). Enter, `clickCount >= 2` sau Esc cu ≥ 2 puncte = termină deschisă; Esc cu < 2 puncte = anulează. Commit: Modelare
  → `ctx.stack.run("Pen", [&](model::Model& m) { addPenCurveGeometry(m, ctx.context, path, normal); })`; Animare
  (`ctx.drawsPaths`) → `ctx.requests->penPath = path; penNormal = normal`. Overlay: liniile curbei eșantionate, segmentul
  previzualizat până la cursor, mânerele punctului curent (linii punctate + puncte), `appendInference`. `status()`:
  „Click: punct cu colț · trage: punct neted · click pe primul punct: închide · Enter: termină · Esc”.

- [ ] **Step 1: Write the failing tests** (`[core][tools][pen]`), cu o cameră `view::standardViewPose(Top, ...)` ca
  în `ToolTests.cpp`:
  - `"Pen: click, click cu tragere, Enter → o curbă cu un punct neted"`: 3 puncte, al doilea tras → `penCurves()` are
    o curbă; `path.points[1].smooth`, `in` și `out` simetrice față de punct; `points[0]` colț.
  - `"Pen: click pe primul punct închide curba și face o față"`.
  - `"Pen: închiderea câștigă lângă alt punct prins"` (Review Focus 5): un capăt de muchie existent la 6 px de primul
    punct; click la 3 px de primul punct → `closed == true`, primul punct neschimbat.
  - `"Pen: pe fața unui obiect, curba stă pe planul feței"`: o față verticală (x = 1000); toate punctele curbei au
    `x ≈ 1000`, `normal ≈ (±1,0,0)`.
  - `"Pen: Esc cu un singur punct anulează; cu două termină"`.
  - `"Pen: în Animare cere o cale în loc de geometrie"`: `ctx.drawsPaths = true` → nicio muchie nouă, `requests.penPath`
    are punctele, `requests.penNormal ≈ (0,0,1)`.
- [ ] **Step 2: Run** `build/mac-debug/tests/aion_tests "[pen]"` — Expected: FAIL.
- [ ] **Step 3: Implement** `PenTool` (urmează structura `ArcTool` din `ShapeTools.cpp` pentru inferență, plan și
  overlay; helper-ele din `ShapeTools.cpp` folosite de ambele se mută în `ToolHelpers.h` dacă e nevoie).
- [ ] **Step 4: Run** toată suita — Expected: PASS.
- [ ] **Step 5: Commit** — `"Unealta Pen: puncte cu colț și netede, închidere, plan pe sol sau pe față"`

### Task 4: Editarea curbei (`penedit`, în core)

**Files:**
- Create: `core/src/tools/PenEditTool.cpp`
- Modify: `core/include/aion/core/tools/Tool.h` (`Key::Delete`; `ToolContext::editCurve`; `ToolRequests::curveEdit`,
  `curveEditDone`), `ToolHelpers.h` (`makePenEditTool`), `Tools.cpp` (`"penedit"`)
- Test: `tests/core/PenToolTests.cpp`

**Interfaces:**
- Consumes: Task 1.
- Produces:
  ```cpp
  enum class Key { Escape, Enter, Left, Right, Up, Down, Delete, Other };
  // ToolContext: curba editată, în coordonatele contextului (Document o pune înainte de fiecare apel)
  std::optional<geom::BezierPath> editCurve; math::Vec3d editNormal { 0.0, 0.0, 1.0 };
  // ToolRequests
  std::optional<geom::BezierPath> curveEdit; // forma nouă (după o tragere / inserare / ștergere), un pas de undo
  bool curveEditDone = false;                 // ieșirea din editare (Esc, click în afară)
  ```
  Comportament (spec 3.1): mânerele vizibile = ale punctului selectat și ale vecinilor; tragerea unui punct îl mută cu
  mânerele lui; a unui mâner (punct `smooth`) îl oglindește pe celălalt, cu Option (`alt`) rupe perechea (`smooth =
  false`); Option + click pe un punct comută neted ↔ colț (colț = mânerele pe punct; neted = mânere pe direcția
  vecinilor, ⅓ din distanță); click pe curbă (≤ 6 px) = `insertPoint` acolo; `Key::Delete` = `removePoint` pe cel
  selectat (curba rămâne cu ≥ 2 puncte); Esc sau click la > 12 px de curbă și de puncte = `curveEditDone`. Mișcările
  sunt pe planul curbei, cu inferență. Overlay: curba, punctele (grips `Endpoint` pentru colțuri, `Midpoint` pentru
  netede), mânerele (linii + puncte), selectatul „hot”.

- [ ] **Step 1: Write the failing tests** (`[core][tools][pen]`): pentru fiecare gest de mai sus câte un caz —
  `"Pen edit: tragerea unui punct"`, `"… unui mâner neted oglindește"`, `"… Option rupe perechea"`,
  `"… Option + click comută colț / neted"`, `"… click pe curbă inserează fără să schimbe forma"`,
  `"… Delete șterge punctul selectat"`, `"… Esc și click în afară termină"`; aserțiunile pe `requests.curveEdit`
  (punctele așteptate, ± `tol::kLength`) și `requests.curveEditDone`.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** `PenEditTool` (nu scrie în model: doar cereri).
- [ ] **Step 4: Run** toată suita — Expected: PASS.
- [ ] **Step 5: Commit** — `"Editarea curbelor Pen: puncte, mânere, Option, inserare, ștergere"`

### Task 5: Pen-ul în aplicație (Modelare)

**Files:**
- Modify: `render/src/Document.cpp`, `render/include/aion/render/Document.h`, `core/src/tools/Tools.cpp`
  (`SelectTool`: dublu click pe o muchie de curbă Pen editabilă → `ToolRequests::editPenCurve`),
  `core/include/aion/core/tools/Tool.h` (`ToolRequests::editPenCurve` = `model::PenCurveId`),
  `app/qml/shell/Toolbar.qml`, `app/qml/Main.qml`, `app/src/AppInfo.*` (dacă trebuie dublu click simulat),
  `app/qml/shell/EntityInfo.qml`
- Test: `tests/core/ToolTests.cpp` (dublu click → cerere), restul verificare în aplicație

**Interfaces:**
- Consumes: Task 2, 3, 4.
- Produces (Document): `void beginCurveEdit(model::PenCurveId)`, `void beginPathEdit(model::MotionPathId)` (Task 7),
  `bool curveEditing() const`, interne: unealta `penedit` pusă în locul celei curente cu același mecanism ca
  `beginPivotDrag` (`m_suspendedTool`), `ToolContext::editCurve` completat în `withTool` din curba editată;
  `requests.curveEdit` → `runOnModel("Editează curba", replacePenCurve…)`; `curveEditDone` → revine unealta.
- Toolbar: un grup nou `{ id: "pen", modes: [0, 1], tools: [{ id: "pen", icon: "pen-line", name: "Pen", key: "Shift+P",
  phase: 34 }] }` (dacă `Shift+P` e deja folosit, fără scurtătură); `commandModes`: `tool.pen` → `{ Modeling, Animation }`.
- `ToolContext::drawsPaths = m_workMode == Animation` în `withTool`.
- `Document::deleteSelection()`: cât e editarea activă, trimite `Key::Delete` uneltei în loc să șteargă selecția.
- Inspector: o selecție care e o curbă Pen nu mai e raportată ca „Arc” (`entityInfo` sare peste `recognizeShape`
  când toate muchiile au `curve` = al unei `PenCurve`); titlul „Curbă”, cu lungimea în detalii.
- Acțiuni de script noi (`Main.qml`): `dragw:x1,y1,z1,x2,y2,z2` (tragere în coordonate de lume, ca `clickw`),
  `dblclickw:x,y,z`.

- [ ] **Step 1: Write the failing test** în `ToolTests.cpp`: `"Select: dublu click pe o curbă Pen editabilă cere
  editarea ei"` (`requests.editPenCurve` = id-ul; pe o curbă după Push/Pull → nu cere, comportamentul vechi rămâne).
- [ ] **Step 2: Run** — FAIL. **Step 3: Implement** testul + integrarea de mai sus. **Step 4: Run** suita — PASS;
  `make build` — fără erori.
- [ ] **Step 5: Verificare în aplicație** (capturi arătate utilizatorului): desenezi un S cu `tool.pen;clickw;dragw;
  key:enter`; o formă închisă + `tool.pushpull`; dublu click pe S, tragi un punct (`dragw`), `edit.undo` revine.
- [ ] **Step 6: Commit** — `"Pen în Modelare: toolbar, editare cu dublu click, Inspector"`; merge în `main`, push, CI.

### Task 6: Căile în model (`model::MotionPath`), evaluarea și fișierul

**Files:**
- Modify: `Ids.h` (`MotionPathId`), `Model.h` / `Model.cpp` (entitate + `ChangeSet::motionPaths`;
  `MotionPathId addMotionPath(MotionPath)`, `void removeMotionPath(MotionPathId)`,
  `void setMotionPath(MotionPathId, MotionPath)`, `motionPaths()`), `AionFile.cpp`
- Create: `core/include/aion/core/model/MotionPaths.h`, `core/src/model/MotionPaths.cpp`
- Test: `tests/core/MotionPathTests.cpp`, `tests/core/IoTests.cpp`

**Interfaces:**
- Consumes: Task 1 (`ArcLength`, `PathFrame`), `model::ease`, `model::evaluateValues`, `model::bounds`,
  `model::decompose`.
- Produces:
  ```cpp
  struct PathAnchor {
      InstanceId instance;
      double along = 0.0;   // mm, de-a lungul căii (negativ = în urmă)
      double lateral = 0.0; // mm, spre stânga (normala × tangenta)
      double up = 0.0;      // mm, pe normala planului
      double yaw = 0.0;     // grade, în jurul normalei
      int front = 0;        // sferturi de tură: axa locală Rz(front·90°)·X a obiectului e fața
      math::Transform before; // transformarea de dinainte de ancorare
      bool operator==(const PathAnchor&) const = default;
  };
  struct MotionPath {
      std::string name;
      math::Transform transform;      // poziția / rotația / scara căii
      geom::BezierPath curve;         // în coordonatele căii
      math::Vec3d normal { 0.0, 0.0, 1.0 }; // a planului, în coordonatele căii
      AnimationId animation;
      double start = 0.0, duration = 5.0; // secunde
      Easing easing {};
      std::vector<PathAnchor> anchors; // ordinea = ordinea pe cale
      bool visible = true;
      bool operator==(const MotionPath&) const = default;
  };
  // MotionPaths.h (namespace aion::model)
  [[nodiscard]] math::Transform anchorTransform(const Model& model, const MotionPath& path, const PathAnchor& anchor,
      double time, const math::Vec3d& scale);
  void applyPaths(Model& posed, const Model& source, AnimationId animation, double time); // pune ancorele pe cale
  bool attachToPath(Model& model, MotionPathId path, InstanceId instance);   // false dacă e deja pe o cale a animației
  void detachFromPath(Model& model, MotionPathId path, InstanceId instance);  // pune `before` la loc
  void removePath(Model& model, MotionPathId path);                           // scoate toate ancorele, apoi calea
  void removeAnimationWithPaths(Model& model, AnimationId animation);
  void absorbAnchorEdit(Model& model, MotionPathId path, InstanceId instance, const math::Transform& edited, double time);
  void rotateAnchorFront(Model& model, MotionPathId path, InstanceId instance); // +90°
  [[nodiscard]] std::optional<MotionPathId> pathOf(const Model& model, AnimationId animation, InstanceId instance);
  ```
  Formula (spec 3.2): `u = clamp((time − start) / duration, 0, 1)`, `s = ease(easing, u) · L` (`L` = lungimea; cale
  închisă = un tur), distanța obiectului `d = s + along`; `F = ArcLength(curve).at(d)` dus în lume cu `transform`;
  `t` = tangenta, `n` = normala (în lume), `l = n × t`; punctul de bază al obiectului = centrul feței de jos a cutiei
  definiției (`((lo.x+hi.x)/2, (lo.y+hi.y)/2, lo.z)`, local); transformarea = `T(F.point + l·lateral + n·up) · [t l n]
  · Rz(yaw) · Rz(−front·90°) · S(scale) · T(−bazaLocală)`. Orientarea vine **numai** din `t` (Review Focus 1).
  `attachToPath`: `before` = transformarea curentă; `front` = sfertul care aduce axa orizontală locală cea mai
  apropiată de tangenta de start pe `t`; `along` = 0 pentru primul, altfel `ultimul.along − (lungUltim/2 + lungNou/2 +
  max(200, 0.2·max(lungUltim, lungNou)))` (lungimile pe axa feței, cu scara); apoi `Instance::transform =
  anchorTransform(…, start, scale)`. `absorbAnchorEdit`: diferența dintre baza editată și cea așteptată la `time` se
  descompune pe `t / l / n` → `along / lateral / up`; rotirea în jurul lui `n` → `yaw`; `before` nu se schimbă.
  `applyPaths`: scara vine din pista obiectului (dacă are canale de scară), altfel din `before`; poziția și rotirea
  pistei se ignoră.

- [ ] **Step 1: Write the failing tests** (`[core][paths]`), cu `boxObject`-ul din `ToolTests.cpp` mutat într-un
  helper comun sau copiat:
  - `"Căi: obiectul merge pe cale, cu fața înainte"`: o cale dreaptă pe X de 10 m, start 0, durată 4 s, easing liniar
    → la 2 s baza e la 5 m; tangenta pe X → axa feței obiectului pe X; baza obiectului pe sol (`z ≈ 0`).
  - `"Căi: pe o curbă obiectul se întoarce lin"`: sfertul de cerc din Task 1 → rotirea în jurul lui Z crește monoton
    de la 90° la 180° pe 10 pași.
  - `"Căi: easing-ul se aplică pe lungime"`: `easeInOut` → la `u = 0.5` obiectul e la jumătatea lungimii.
  - `"Căi: ancorarea și scoaterea"` (Review Focus 4): obiect la (7000, 3000, 0) rotit 30°; `attachToPath` → e la
    start; `detachFromPath` → transformarea identică cu cea inițială (± `tol::kLength`), inclusiv după o editare a
    curbei (`setMotionPath` cu un punct mutat) între ele.
  - `"Căi: coloana nu se suprapune"`: două cutii de 600 mm → `along` al doilea ≤ −(600 + 200); la final, cele două
    cutii nu se intersectează; la start, a doua e pe prelungire (x < 0).
  - `"Căi: decalajul din Move"`: `absorbAnchorEdit` cu baza mutată 500 mm la stânga → `lateral ≈ 500`, `along` neschimbat.
  - `"Căi: decalaj lateral mare pe o curbă strânsă"` (Review Focus 1): sfert de cerc cu raza 500, `lateral = 2000` →
    poziții finite, rotirea egală cu cea a tangentei.
  - `"Căi: calea închisă face un tur"`: la `u = 1` obiectul e înapoi la start.
  - `"Căi: ștergerea căii și undo"` (Review Focus 3): `removePath` într-un `stack.run` → obiectul la `before`; `undo`
    → calea și ancora înapoi, obiectul pe cale; `redo` → iar la `before`.
  - `"Căi: ștergerea animației le ia și căile"`.
  - `IoTests`: `"Fișier: căile se salvează și se încarcă (v4)"` — `motionPaths` cu ancore identice după `toJson` /
    `fromJson`; un fișier v3 se încarcă fără căi.
- [ ] **Step 2: Run** `build/mac-debug/tests/aion_tests "[paths]"` — FAIL.
- [ ] **Step 3: Implement** entitatea (ca `CameraObject`), `MotionPaths.cpp`, serializarea
  (`"motionPaths": [{ id, name, transform:[16], normal:[3], closed, points, animation, start, duration, easing, visible,
  anchors:[{ instance, along, lateral, up, yaw, front, before:[16] }] }]`).
- [ ] **Step 4: Run** toată suita — PASS.
- [ ] **Step 5: Commit** — `"Căi de parcurgere în model: ancore, coloană, decalaj, evaluare, fișier"`

### Task 7: Căile în Document (desen, poză, selecție, ancore, timp)

**Files:**
- Modify: `render/src/Document.cpp`, `render/include/aion/render/Document.h`
- Test: verificare în aplicație (logica e în core, testată în Task 6)

**Interfaces:**
- Consumes: Task 3 (`requests.penPath`), Task 4/5 (`beginCurveEdit` mecanism), Task 6.
- Produces (QML API, `Q_INVOKABLE` / proprietăți):
  - `createMotionPath(path, normal)` (intern, din `requests.penPath`): `require(Animation)`; fără animație → `createAnimation()`;
    `runOnModel("Cale nouă")`: `transform = T(primul punct)`, curba relativă la el, `start = 0`, `duration` = a
    animației, `easing` = `defaultEasing`; nume „Calea N”; selectează calea.
  - Selecția: `selectObject("path", id)`, `selectedObject` pentru o cale = `{ kind: "path", id, name, position[3]
    (centrul cutiei, lume), rotation[3] (Euler XYZ, grade), size[3], start, duration, easing, closed, animation,
    animationName, active (animația ei e cea activă), anchors: [{ instance, name, along, lateral, up, yaw }] }`;
    `setObjectProperties` pentru o cale: `name`, `position`, `rotation`, `size`, `duration`, `start`, `easing`.
  - `paths` (`Q_PROPERTY QVariantList`, `NOTIFY outlineChanged`): `{ id, name, kind: "path", visible, active }`.
  - Ancore: `anchorToPath(double pathId, double instanceId)`, `detachFromPath(double pathId, double instanceId)`,
    `rotateAnchor(double pathId, double instanceId)`, `anchorInfo` (în `entityInfo` pentru o instanță ancorată în
    animația activă: `{ path, pathName, along, lateral, up, yaw }`), `setAnchorOffsets(const QVariantMap&)`.
  - `setPathTiming(double pathId, double start, double duration)` — lungește animația dacă `start + duration` > durata
    ei (un singur `runOnModel`).
  - `timeline()`: cheia nouă `paths: [{ id, name, start, duration, easing, anchors: [names] }]` (doar ale animației
    active).
  - `pose()` și evaluarea din Modul interacțiune (unde se aplică `evaluateInstance` pentru o animație, `Document.cpp`
    ~3122): `model::applyPaths(posed, m_model, animație, timp)` după pistele obișnuite.
  - `withTool`: în Animare, instanțele mutate care sunt ancorate → `absorbAnchorEdit` (un `runOnModel("Față de
    cale")`) în loc de `keyTransforms`; în Modelare, comanda uneltei și `absorbAnchorEdit` într-o singură tranzacție
    (`m_stack.beginTransaction` / `commit`, ca `beginMacro`).
  - `removeAnimation` → `removeAnimationWithPaths`; `deleteSelectedObject` pentru o cale → `removePath`.
  - `beginPathEdit(id)`: editarea cu `penedit` (Task 4) pe curba căii dusă în coordonatele contextului;
    `requests.curveEdit` → `setMotionPath` (curba înapoi în coordonatele căii).
  - `commandModes`: acțiunile pe căi doar în Animare (`require(Animation)`).

- [ ] **Step 1: Implement** cele de mai sus; `make build` fără erori.
- [ ] **Step 2: Verificare prin script** (`debug.paths` nou: tipărește căile, ancorele și poziția instanțelor ancorate
  la timpul curent): o cale desenată cu Pen în Animare, o cutie ancorată (`anchor:` acțiune nouă), `anim:time,2` →
  cutia la mijloc; `edit.undo` / `edit.redo` corecte.
- [ ] **Step 3: Commit** — `"Căile în Document: desen cu Pen în Animare, poza, ancore, timp, undo"`

### Task 8: Căile în viewport, Obiecte și Inspector

**Files:**
- Modify: `render/src/Viewport.cpp` / `.h`, `app/qml/shell/Outliner.qml`, `app/qml/shell/ObjectInfo.qml`,
  `app/qml/shell/EntityInfo.qml`, `app/qml/Main.qml`

**Interfaces:**
- Consumes: Task 7.
- Produces:
  - `appendModelExtras`: fiecare cale = linia eșantionată (lume), cu săgeată la final și puncte la start / final;
    accent când e selectată, estompată când animația ei nu e cea activă; nimic în `cleanView()`.
  - `objectHandleAt`: `"path"` la ≤ 8 px de linia căii; dublu click → `beginPathEdit`.
  - `Viewport::pickingAnchor` (bool, ca `pickingFocus`): următorul click pe un obiect → `anchorToPath(calea selectată,
    obiectul)`.
  - Outliner: rânduri `kind: "path"` (iconița `route`), ca la camere: click selectează, ochiul ascunde.
  - ObjectInfo (cale): `PropertyGroup` „Cale” (nume), „Poziție (mm)”, „Rotație (°)”, „Dimensiuni (mm)”, „Parcurgere”
    (durată, start, curbă — butonul deschide `EasingEditor` cu presetările), „Obiecte ancorate” (rânduri cu
    `RowAction`: ⟳ 90°, × scoate; acțiunea `+` din titlu pornește `pickingAnchor`); un text când animația căii nu e
    cea activă.
  - EntityInfo (instanță ancorată): grupul „Față de cale (mm, °)” (înainte, lateral, sus, rotire) în locul Poziției /
    Rotației, cu butonul „Scoate de pe cale”.
  - Acțiuni de script: `anchor:` (ancorează obiectul selectat la ultima cale), `debug.paths`.

- [ ] **Step 1: Implement**; `make build`.
- [ ] **Step 2: Verificare în aplicație** (capturi arătate utilizatorului): scena din spec (o casă din cutii, o
  „mașină” din cutii, o cale în S spre „garaj”); Inspector-ul căii; Inspector-ul mașinii ancorate; două mașini în
  coloană; redare (calea dispare, mașinile merg); un export PNG fără cale.
- [ ] **Step 3: Commit** — `"Căile în viewport, Obiecte și Inspector"`

### Task 9: Căile în timeline

**Files:**
- Modify: `app/qml/shell/Timeline.qml`, `app/qml/shell/TimelineRow.qml`

**Interfaces:**
- Consumes: Task 7 (`timeline().paths`, `setPathTiming`, `selectObject("path", id)`).
- Produces: rânduri `kind: "path"` (după rândul camerei, înaintea obiectelor) cu o bară de la `start` la
  `start + duration`: click selectează calea, tragerea barei mută `start` (snap la cadre, ⇧ fără), tragerea capătului
  drept schimbă durata (minim 1 cadru); o singură chemare `setPathTiming` la eliberare (un pas de undo); numele
  obiectelor ancorate în tooltip.

- [ ] **Step 1: Implement**; `make build`.
- [ ] **Step 2: Verificare în aplicație**: două căi cu starturi diferite; bara trasă după capătul animației o
  lungește; `edit.undo` readuce bara.
- [ ] **Step 3: Commit** — `"Căile în timeline: bara lor, mutarea și durata"`

### Task 10: Documentație și închidere

**Files:**
- Create: `docs/decisions/0036-pen-si-cai.md` (ADR: curba păstrată lângă muchii, regula de editabilitate, calea ca
  obiect de scenă al unei animații, formula de evaluare, decalajul relativ, v4)
- Modify: `CLAUDE.md` (Stadiu: Pen, căi, acțiunile de script noi), `PLAN.md` (o etapă nouă, bifată),
  `docs/plans/pen-si-cai.md` (stare: implementat), `docs/plans/animatie.md` (P2 „traiectoria” → făcut prin căi),
  `docs/qa-checklist.md` (o secțiune „Pen și căi”)

- [ ] **Step 1:** scrie documentele; **Step 2:** toată suita + `make build`; **Step 3:** commit
  `"Pen și căi: ADR 0036, CLAUDE.md, PLAN.md, checklist"`; merge în `main`, push, CI verde.
