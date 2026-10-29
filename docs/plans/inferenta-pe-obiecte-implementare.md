# Inferență pe obiecte – plan de implementare

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** inferența (snap) vede colțurile, mijloacele, muchiile, fețele și centrele tuturor obiectelor vizibile, în toate uneltele și la plasarea obiectelor noi; primitivele primesc pasul de înălțime și o previzualizare care arată corpul.

**Architecture:** `core` primește o `inference::SnapScene`: o listă de surse (mesh-ul unei definiții + transformarea lui în coordonatele contextului + instanța). `Engine::infer` interoghează `ctx.mesh` ca acum și fiecare sursă cu `camera.inContext(toContext)`, deci reutilizează indexul de picking (BVH) per definiție, fără copii de geometrie. Rezultatele din surse au ID-urile goale și `origin = Object`. `Document` construiește scena (cache după revizia modelului, context, vizibilitate, excluderi) și o pune în `ToolContext::snap`.

**Tech Stack:** C++20, glm, Catch2 (core); Qt 6 / QML (render, app).

**Spec:** `docs/plans/inferenta-pe-obiecte.md`

## Global Constraints

- `core/` nu include Qt; tot ce e nou în core are teste Catch2 (`tests/core/`, adăugate în `tests/CMakeLists.txt`).
- Comparațiile numai prin `core/include/aion/core/math/Tolerance.h`.
- Buclele pe geometrie la hover trec prin `view::forEachVertexNear` / `forEachEdgeNear` / `forEachFaceAlong` / `view::pick`.
- Un `geom::VertexId` / `EdgeId` / `FaceId` din `inference::Inference` înseamnă mereu „din `ctx.mesh`”; pentru alt obiect sunt goale.
- Starea nouă care schimbă desenul atinge documentul sau intră în `DocumentPassCache::Key` (ADR 0038); `Viewport::cleanView()` nu desenează inferențe.
- Comentarii și texte în română, ca în restul codului; namespace `aion::`, `m_` pentru membri privați.
- `make test` verde după fiecare task; verificările din aplicație prin `AION_STARTUP` / `AION_SCREENSHOT` (CLAUDE.md), cu `/usr/bin/perl -e 'alarm N; exec @ARGV'` pentru limite de timp.

## Review Focus

1. Obiect rotit, scalat neuniform sau oglindit (determinant negativ), imbricat pe 2 niveluri: colțul inferat e exact colțul văzut. Test în Task 1 (`snapScene` cu instanță oglindită și imbricată) și Task 2.
2. Un vârf din `ctx.mesh` aflat în spatele unei fețe a unui obiect nu se prinde (ocluzia trece peste surse). Test în Task 2.
3. Când editezi un grup, instanța editată nu apare și ca sursă străină (altfel fiecare colț are un dublu „din obiect” și ID-urile se pierd). Test în Task 1.
4. Ascunderea unui obiect sau a unui tag fără schimbarea modelului: cache-ul scenei nu are voie să mai prindă de el. Cheia cache-ului din Task 6 include vizibilitatea; verificare în aplicație în Task 6.
5. Undo / ștergere cât e o unealtă activă: scena ține pointeri la mesh-uri; se reface înainte de fiecare eveniment al uneltei dacă s-a schimbat revizia modelului. Verificare în Task 6 (undo în mijlocul unui Move, fără crash sub UBSan).

---

### Task 1: SnapScene și construirea ei din model

**Files:**
- Create: `core/include/aion/core/inference/SnapScene.h`
- Create: `core/include/aion/core/model/SnapSources.h`, `core/src/model/SnapSources.cpp`
- Modify: `core/CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/core/SnapSceneTests.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace aion::inference {
  struct SnapSource {
      const geom::Mesh* mesh = nullptr;
      math::Transform toContext;     // local (definiția sursei) → coordonatele contextului editat
      model::InstanceId instance;    // instanța cea mai de sus din contextul editat care o conține; goală pentru
                                     // geometria liberă a unui context părinte
  };
  struct SnapScene { std::vector<SnapSource> sources; };
  }
  namespace aion::model {
  // Sursele de inferență pentru contextul `path` (calea instanțelor editate, ca la worldTransform; gol = rădăcina):
  // toate instanțele vizibile (Model::isVisible, recursiv) de la rădăcină, plus geometria liberă a contextelor
  // părinte. Fără: mesh-ul contextului însuși (e ctx.mesh), instanțele din `path` (cea editată e ctx.mesh),
  // instanțele din `exclude` cu tot ce conțin, definițiile colecție fără geometrie, mesh-urile goale.
  [[nodiscard]] inference::SnapScene snapScene(const Model& model, const std::vector<InstanceId>& path,
      const std::set<InstanceId>& exclude);
  }
  ```

- [ ] **Step 1: Teste care pică** (`[core][inference][snapscene]`):
  - `snapScene: o cutie în rădăcină dă o sursă cu transformarea instanței` — model cu o cutie (ca `boxObject` din `ModelingTests.cpp`) mutată la (3000,0,0); `sources.size() == 1`, `sources[0].instance == id`, `toContext.applyToPoint(colțul local)` = colțul în model.
  - `snapScene: imbricat și oglindit` — grup G (translație 1000,0,0) care conține cutia C oglindită pe X (scară −1); o singură sursă pentru C, cu `instance == G`, și punctul local (0,0,0) ajunge la (1000,0,0) după transformare; determinantul lui `toContext` e negativ.
  - `snapScene: în contextul unui grup` — rădăcina are o linie liberă și instanța editată I + o altă instanță J; `snapScene(m, {I}, {})` conține J (transformată relativ la I: `toContext = worldTransform(I)^-1 * world(J)`) și geometria liberă a rădăcinii (`instance` gol), dar nicio sursă cu `mesh == &m.mesh(def(I))` venită din I.
  - `snapScene: ascunse și excluse` — instanța ascunsă (`hidden = true`), cea cu tag invizibil și cea din `exclude` lipsesc; copiii unei excluse lipsesc și ei.
- [ ] **Step 2:** `make build && build/mac-debug/tests/aion_tests "[snapscene]"` → pică la compilare (`snapScene` nedefinit).
- [ ] **Step 3:** Implementează `model::snapScene` (parcurgere în adâncime de la rădăcină cu `Model::childrenOf`, transformarea globală acumulată, apoi `contextWorld.inverse() *` ei; `contextWorld = worldTransform(model, path)`).
- [ ] **Step 4:** Rulează testele → PASS; `make test` verde.
- [ ] **Step 5:** Commit `core: SnapScene – sursele de inferență din obiectele modelului`.

### Task 2: Engine vede scena (capăt, mijloc, pe muchie, pe față, intersecție, From Point, ocluzie)

**Files:**
- Modify: `core/include/aion/core/inference/Inference.h`, `core/src/inference/Inference.cpp`
- Test: `tests/core/InferenceTests.cpp`

**Interfaces:**
- Consumes: `inference::SnapScene` (Task 1).
- Produces:
  ```cpp
  enum class Origin { Context, Object };
  struct Inference { ...; Origin origin = Origin::Context; model::InstanceId sourceInstance;
                     std::optional<Vec3d> normal; /* normala feței atinse (OnFace sau punct pe o față), în coord. contextului */ };
  Engine& withScene(const SnapScene* scene);   // nullptr = doar mesh-ul (ca acum); se păstrează până la alt apel
  ```
  Eticheta (`label`) pentru `origin == Object` adaugă numele? Nu: rămâne „Endpoint”, „On Edge” etc. (nu avem numele în core).

- [ ] **Step 1: Teste care pică** (`[core][inference]`), cu `isoCamera()` și un `model::Model` care are o cutie 1000³ ca obiect la (3000,0,0) și `ctx.mesh` = mesh-ul rădăcinii (gol sau cu o linie):
  - `Inferență pe obiecte: capătul unui obiect` — cursor la 4 px de colțul (4000,1000,1000): `kind == Endpoint`, `point` ≈ colț (1e-6), `origin == Origin::Object`, `!vertex`, `sourceInstance == id`.
  - `…: mijloc și pe muchie` — mijlocul muchiei de sus → `Midpoint`, `!edge`; pe muchie → `OnEdge`, punct pe segment.
  - `…: pe față, cu normala` — mijlocul feței de sus → `OnFace`, `!face`, `normal` ≈ (0,0,1).
  - `…: From Point de la colțul unui obiect` — atinge colțul (devine referință), apoi `from = (0,0,0)`, cursorul pe axa X în dreptul lui x=4000 → `kind == FromPoint`, `point.x == Approx(4000)`, ultimul ghidaj `dashed`, `a` ≈ colțul.
  - `…: ocluzie peste surse` — un vârf al `ctx.mesh` aflat exact în spatele feței din față a cutiei (pe aceeași rază) nu iese `Endpoint`.
  - `…: obiectul transformat` — aceeași cutie rotită 30° pe Z și scalată (2,1,1): colțul inferat = `toContext` aplicat colțului local.
  - `…: prioritate față de grid` — `withGridSnap(250)` și cursorul la 4 px de colț → tot `Endpoint` (nu `onGrid`).
  - `…: fără scenă, nimic nou` — `withScene(nullptr)` dă exact rezultatele de azi (testele existente rămân neschimbate).
- [ ] **Step 2:** Rulează `aion_tests "[inference]"` → testele noi pică.
- [ ] **Step 3:** Implementează. Fiecare etapă din `inferFree` care citește geometria (1 capete, 2 mijloace, 4a intersecții, 5 pe muchie, fața liberă, `remember`) rulează pe `ctx.mesh` și pe fiecare sursă cu `camera.inContext(source.toContext)`; punctele trec în context cu `toContext.applyToPoint`, normalele cu inversa transpusă. `facePick` devine cea mai apropiată față dintre toate (adâncimea pe raza contextului) și e folosită de `visible()`. Histerezisul pentru un capăt de obiect ales anterior folosește punctul (nu ID-ul). `inferLocked` proiectează și capetele surselor. La egalitate pe ecran (±0,5 px), câștigă `ctx.mesh`.
- [ ] **Step 4:** `aion_tests "[inference]"` → PASS; `make test` verde.
- [ ] **Step 5:** Commit `inference: capete, muchii și fețe din toate obiectele vizibile`.

### Task 3: Muchiile obiectelor ca referință Parallel / Perpendicular

**Files:**
- Modify: `core/include/aion/core/inference/Inference.h`, `core/src/inference/Inference.cpp`
- Test: `tests/core/InferenceTests.cpp`

**Interfaces:**
- Produces: `struct ReferenceEdge { Vec3d a, b; Vec3d normal; };` (în coordonatele contextului; `normal` = a primei fețe sau (0,0,1)). `Engine::referenceEdge()` întoarce `std::optional<ReferenceEdge>` în loc de `std::optional<geom::EdgeId>`; `m_referenceEdge` / `m_referenceDirection` devin un singur `std::optional<ReferenceEdge>`. Apelanții lui `referenceEdge()` (grep în `core/src` și `render/src`) se adaptează.

- [ ] **Step 1: Test care pică** — `Inferență pe obiecte: paralel cu muchia unui obiect`: obiect cu o muchie oblică (cutie rotită 30°); atinge muchia (OnEdge), apoi `from` liber pe sol și cursorul pe direcția paralelă → `kind == Parallel`, `direction` ∥ muchia; ghidajul magenta e muchia obiectului.
- [ ] **Step 2:** Rulează → pică.
- [ ] **Step 3:** Înlocuiește referința prin segment (în `remember`, `withDirection`, `toggleReferenceLock`, direcțiile din etapa 3); ștergerea referinței când muchia nu mai există (`mesh.edges().contains`) dispare: segmentul rămâne valid până la `reset()`.
- [ ] **Step 4:** `make test` verde (inclusiv testele existente de Parallel / săgeata în jos).
- [ ] **Step 5:** Commit `inference: muchia de referință ca segment (și din obiecte)`.

### Task 4: Centrul cercurilor și alinierea dublă

**Files:**
- Modify: `core/include/aion/core/inference/Inference.h`, `core/src/inference/Inference.cpp`
- Test: `tests/core/InferenceTests.cpp`

**Interfaces:**
- Produces: `Kind::Center` (după `Midpoint` în enum; `isPoint()` îl include; `label` = „Center”); `Kind::FromPoint` poate avea două ghidaje punctate.

- [ ] **Step 1: Teste care pică:**
  - `Inferență: centrul unui cerc după ce cursorul a atins cercul` — `geom::drawCurve` cu un cerc de 24 de laturi, raza 500, centrul (1000,1000,0); cursorul pe o muchie a cercului (OnEdge), apoi la 5 px de centru → `kind == Center`, `point` ≈ (1000,1000,0) (1e-6). Fără atingerea cercului întâi → nu e `Center`.
  - `…: centrul unui cilindru din alt obiect` — același lucru pe capacul unei primitive cilindru-obiect (prin `SnapScene`).
  - `…: o curbă care nu e cerc nu are centru` — o polilinie deschisă cu același `curve` → nu apare `Center`.
  - `Inferență: aliniere dublă` — referințe (0,2000,0) și (3000,0,0) atinse; cursorul lângă (3000,2000,0) pe sol → `kind == FromPoint`, `point` ≈ (3000,2000,0), două ghidaje `dashed` (unul X, unul Y).
- [ ] **Step 2:** Rulează → pică.
- [ ] **Step 3:** Implementează. Centrul: când `remember` vede `OnEdge` / `Midpoint` pe o muchie cu `Edge::curve != 0`, strânge vârfurile curbei (muchiile cu același `curve`, prin vecinătate de la muchia atinsă), ia centrul cercului prin 3 vârfuri; îl acceptă dacă toate vârfurile sunt la aceeași rază (`tol::equal`, eps relativ 1e-6·rază + `kLength`) și coplanare; centrul intră în `m_references` (cu marcaj de centru) și ca țintă de tip punct, cu prioritatea unui capăt. Alinierea dublă: în etapa 6, după cea mai bună linie de referință, caută a doua referință pe o altă axă; dacă intersecția celor două linii e în `lineRadiusPx`, rezultatul e intersecția, cu ambele ghidaje.
- [ ] **Step 4:** `make test` verde.
- [ ] **Step 5:** Commit `inference: centrul cercurilor și alinierea cu două puncte`.

### Task 5: Uneltele folosesc scena

**Files:**
- Modify: `core/include/aion/core/tools/Tool.h` (`ToolContext`), `core/src/tools/ToolHelpers.h/.cpp`, `core/src/tools/Tools.cpp`, `ShapeTools.cpp`, `TransformTools.cpp`, `PenTool.cpp`, `PenEditTool.cpp`, `MeasureTools.cpp`
- Test: `tests/core/ToolTests.cpp`

**Interfaces:**
- Consumes: `Engine::withScene`, `Inference::origin` / `normal` (Task 2).
- Produces: `ToolContext::snap` (`const inference::SnapScene* snap = nullptr;`) și în `ToolHelpers.h`:
  `inference::Inference inferAt(inference::Engine& engine, const ToolContext& ctx, Vec2d screen, std::optional<Vec3d> from, std::optional<math::Plane> plane = std::nullopt);`
  (= `engine.withGuides(ctx.guides).withGridSnap(ctx.gridStep).withScene(ctx.snap).infer(ctx.mesh, ctx.camera, screen, from, plane)`). Toate cele 17 apeluri `m_engine.withGuides(...)...infer(...)` din `core/src/tools` trec prin `inferAt`.
  `Vec3d inferenceNormal(const ToolContext& ctx, const inference::Inference& inf, Vec3d fallback);` — `inf.normal` dacă există, altfel normala feței `inf.face` / `inf.pick.face` din `ctx.mesh`, altfel `fallback`. Locurile care citesc azi `ctx.mesh.face(m_inference.pick.face).plane.normal` (PenTool, Protractor, Section, Rectangle / `drawingNormal` din ShapeTools) folosesc `inferenceNormal`.

- [ ] **Step 1: Teste care pică** (`[core][tools][snap]`), cu un model cu o cutie-obiect și `ctx.snap = &scene` (`model::snapScene(m, {}, {})`):
  - `Line: capătul pe colțul unui obiect nu atinge obiectul` — linie de la (0,0,0) la colțul obiectului; mesh-ul rădăcinii are o muchie care se termină exact în colț; mesh-ul definiției cutiei e `sameAs` copia de dinainte.
  - `Push/Pull: distanța până în dreptul vârfului unui obiect` — dreptunghi pe sol; Push/Pull, cursorul pe colțul de sus (z=1000) al obiectului → fața ajunge la z=1000.
  - `Rectangle pe fața altui obiect` — primul click pe fața de sus a obiectului → dreptunghiul e în planul z=1000, în rădăcină.
  - `Move: colțul obiectului mutat pe colțul altui obiect` — două cutii; scena exclude selecția (`snapScene(m, {}, {A})`); Move apucă colțul lui A, cursorul pe colțul lui B → A ajunge colț pe colț.
- [ ] **Step 2:** `aion_tests "[snap]"` → pică.
- [ ] **Step 3:** Adaugă `ToolContext::snap`, `inferAt`, `inferenceNormal`; înlocuiește apelurile. Uneltele nu schimbă altceva: o inferență `Origin::Object` are ID-urile goale, deci ramurile existente cu `contains(...)` o tratează ca punct liber.
- [ ] **Step 4:** `make test` verde.
- [ ] **Step 5:** Commit `tools: inferența vede obiectele (Line, forme, Pen, Push/Pull, Move, Rotate, Scale, măsurători)`.

### Task 6: Document construiește scena; verificarea în aplicație

**Files:**
- Modify: `render/include/aion/render/Document.h`, `render/src/Document.cpp`

**Interfaces:**
- Consumes: `model::snapScene` (Task 1), `ToolContext::snap` (Task 5).
- Produces: `const inference::SnapScene& Document::snapScene(const std::set<model::InstanceId>& exclude = {}) const;` (public, folosită și de Viewport în Task 9; cache cu cheia `{ modelRevision(), calea contextului, exclude, hiddenTags(), revizia vizibilității obiectelor }` — dacă vizibilitatea unei instanțe nu schimbă `modelRevision`, adaugă un contor `m_visibilityRevision` incrementat la ascundere / arătare / schimbarea vizibilității unui tag).

- [ ] **Step 1:** La fiecare `tools::ToolContext ctx { ... }` din `Document.cpp` (grep: 10 locuri) setează `ctx.snap = &snapScene(exclude)`, cu `exclude = m_selection.instances` când unealta activă e `move`, `rotate`, `scale` sau `place` (și selecția copiei la `place`), altfel gol. Scena se cere din nou la fiecare eveniment (cache-ul o refolosește), deci un undo între evenimente nu lasă pointeri vechi.
- [ ] **Step 2:** `make build && make test` verde.
- [ ] **Step 3: Verificare: stâlpii.** `AION_SCREENSHOT=<scratch>/stalpi.png AION_SCREENSHOT_DELAY_MS=9000 AION_STARTUP="scriptjs:aion.box(0,0,0,300,300,2500);wait:300;view.extents;wait:1500;tool.rectangle;clickw:2000,0,0;clickw:2300,300,0;tool.pushpull;clickw:2150,150,0;movew:300,300,2500;debug.geometry"` → captura arată linia punctată de la colțul primului stâlp și eticheta „Endpoint”; clicul final (`clickw:300,300,2500`) face al doilea stâlp de 2500 (Inspector / `debug.geometry`).
- [ ] **Step 4: Verificare: ascundere și undo.** Ascunde primul stâlp (`org.hide` cu el selectat) și repetă hover-ul: nu mai apare „Endpoint” pe el. Undo în mijlocul unui Move (`tool.move`, `clickw:`, `keys:,ctrl+z`, `movew:`) nu oprește aplicația (rulare debug, UBSan).
- [ ] **Step 5:** Commit `render: scena de inferență pentru unelte (cache, excluderea selecției)`.

### Task 7: Move – puncte reale înaintea punctelor cutiei

**Files:**
- Modify: `core/src/tools/TransformTools.cpp` (`gripObjectNear`, `nearestGrip`, `MoveTool::track`)
- Test: `tests/core/ToolTests.cpp`

- [ ] **Step 1: Test care pică** — `Move: un colț real bate punctul cutiei la aceeași distanță`: un cilindru-obiect B și o cutie A; cursorul la egală distanță pe ecran de un colț al cutiei lui B (punct în aer) și de un vârf real al capacului lui B → ținta e vârful real. `instanceGrips` rămân ținte când nu e niciun vârf real în rază.
- [ ] **Step 2:** Rulează → pică.
- [ ] **Step 3:** Compară distanța pe ecran a celui mai bun `Grip` cu a inferenței (`Endpoint` / `Midpoint` / `Center`); la diferență ≤ 0,5 px câștigă inferența.
- [ ] **Step 4:** `make test` verde.
- [ ] **Step 5:** Commit `move: colțurile reale înaintea punctelor cutiei`.

### Task 8: Primitive – pasul de înălțime, VCB și previzualizarea corpului

**Files:**
- Modify: `core/src/tools/ShapeTools.cpp` (`PrimitiveTool`)
- Test: `tests/core/ToolTests.cpp` (testul „Obiecte primitive: două clicuri…” se rescrie)

**Interfaces:**
- Cub / Cilindru / Con: pas 1 = centrul bazei, pas 2 = mărimea (raza), pas 3 = înălțimea (pe normala bazei, `pointOnLineUnderCursor` din centrul bazei, cu prindere: dacă inferența de sub cursor e un punct (`isPoint()`, `Center`, `OnEdge`), înălțimea = proiecția lui pe normală, cu ghidaj punctat de la punct la vârful înălțimii, ca la Push/Pull). Sfera: doi pași.
- VCB: pas 2: „1000” = lățimea → pasul 3 (sfera: gata); „1000,500” = gata. Pas 3: „500” = înălțimea; „1000,500” = lățimea și înălțimea. Valori ≤ 0 → `false`.
- Status: pas 1 „Alege centrul bazei …”, pas 2 „Alege mărimea bazei sau tastează-o (1000, sau 1000,500 cu înălțimea) · Esc anulează”, pas 3 „Alege înălțimea sau tastează-o · Esc anulează”. Măsurătoarea VCB: „Lățime” la pasul 2, „Înălțime” la pasul 3.
- Previzualizare (`overlay()`): forma reală la mărimea curentă (înălțimea = lățimea la pasul 2), 16 segmente: sferă = 8 meridiane + 7 paralele + conturul văzut din cameră (cercul perpendicular pe direcția camerei prin centrul sferei); cilindru = cercurile de jos și de sus + 8 generatoare + cele 2 generatoare de contur; con = cercul bazei + 8 generatoare spre vârf + cele 2 de contur; cub = 12 muchii. Conturul se calculează din `ctx.camera` la fiecare `track` (unealta păstrează direcția camerei). La pasul 3, cota înălțimii (`o.dimensions`).

- [ ] **Step 1: Teste care pică** (`[core][tools][primitives]`):
  - `Primitive: trei clicuri pentru cub (centru, mărime, înălțime)` — clicuri (1000,1000,0), (1400,1000,0), apoi pe direcția Z la z=1500 → `size.x ≈ 800`, `size.z ≈ 1500`; după două clicuri nu există încă nicio instanță.
  - `Primitive: înălțimea în dreptul colțului altui obiect` — cu `ctx.snap` și o cutie de 2500 la (3000,0,0): al treilea clic pe colțul ei de sus → `size.z ≈ 2500` (margin 1e-6).
  - `Primitive: sfera rămâne cu două clicuri`.
  - `Primitive: VCB pe pași` — pas 2 `applyMeasurement("1000")` → încă nicio instanță, status cu „înălțimea”; apoi `"500"` → `size.x ≈ 1000`, `size.z ≈ 500`; alt cub: pas 2 `"600,1500"` → gata. `"0"` și `"-5"` → `false`.
  - `Primitive: previzualizarea sferei are meridiane și paralele` — după primul clic și o mișcare: `overlay().lines.size() >= 8 * 16` (meridiane + paralele, ca segmente), iar punctele lor sunt la raza curentă de centru (±1e-6).
- [ ] **Step 2:** Rulează → pică.
- [ ] **Step 3:** Implementează pașii, VCB-ul, statusul și previzualizarea.
- [ ] **Step 4:** `make test` verde.
- [ ] **Step 5: Verificare vizuală:** `AION_STARTUP="view.extents;wait:1500;tool.sphere;clickw:0,0,0;movew:600,0,0"` și la fel `tool.cylinder` / `tool.cone` cu `movew:` după al doilea clic; capturile arată corpul (rețeaua sferei, generatoarele cilindrului / conului).
- [ ] **Step 6:** Commit `primitive: pasul de înălțime cu prindere și previzualizarea corpului`.

### Task 9: Plasarea din Bibliotecă cu inferență și centrul bazei

**Files:**
- Modify: `render/include/aion/render/Document.h`, `render/src/Document.cpp` (`placeLibraryItem`, `createParametric(generator, at)`), `render/include/aion/render/Viewport.h`, `render/src/Viewport.cpp` (`dropPointAt`, `setDropPreview`, desenul previzualizării), `app/qml/Main.qml` (DropArea din jurul liniei 1080)

**Interfaces:**
- Consumes: `model::snapScene`, `inference::Engine` (Tasks 1–2).
- Produces:
  - Inferența drop-ului stă în Viewport: `Q_INVOKABLE QVector3D Viewport::dropPointAt(const QPointF&)` folosește un membru nou `inference::Engine m_dropEngine` cu scena documentului (`Document::snapScene()` din Task 6, cu `exclude` gol) și mesh-ul contextului. Ultima inferență (`inference::Inference m_dropInference`) se desenează cât durează drop-ul (marcaj, etichetă, ghidaje), cu aceleași funcții ca overlay-ul uneltelor. `Viewport::clearDropPreview()` (o adaugi dacă nu există; QML o cheamă la ieșirea din DropArea și după drop) golește previzualizarea și face `m_dropEngine.reset()`.
  - `Viewport::setDropPreview(const QVector3D& baseCenter, const QVector3D& size)`: parametrul devine centrul bazei; cutia se desenează de la `baseCenter - (size.x/2, size.y/2, 0)`.
  - `Document::placeLibraryItem(id, at)` și `createParametric(generator, at)`: `at` = centrul bazei. Pentru fișiere: translația = `at - centrulBazei(bounds(modelul încărcat))`; pentru generatoare: `at - (lățime/2, adâncime/2, 0)` (originea lor e colțul stânga-față-jos, ADR 0025).

- [ ] **Step 1:** Implementează schimbările de mai sus (fără teste core noi: logica de inferență e testată în Tasks 1–2; `insertModel` / `createParametric` primesc doar altă translație).
- [ ] **Step 2:** `make build && make test` verde.
- [ ] **Step 3: Verificare:** întâi o captură cu `AION_STARTUP="scriptjs:aion.box(0,0,0,1000,1000,1000);wait:300;view.extents;wait:1500"` ca să afli pe ecran colțul de sus (1000,1000,1000); apoi `…;droppreview:gen:table,<x>,<y>` (acțiunea existentă, coordonate de fereastră) → captura arată cutia de previzualizare cu centrul bazei pe colț și marcajul „Endpoint”. Adaugă acțiunea de script `dropat:gen:table,<x>,<y>` (în `Main.qml`, lângă `droppreview:`: `doc.placeLibraryItem(id, viewport.dropPointAt(...))`) și verifică cu `debug.geometry` / Inspector că masa are centrul bazei în (1000,1000,1000).
- [ ] **Step 4:** Commit `bibliotecă: plasare cu inferență, pe centrul bazei`.

### Task 10: Documentație și performanță

**Files:**
- Create: `docs/decisions/0042-inferenta-pe-obiecte.md`
- Modify: `CLAUDE.md` (secțiune nouă sub „Index de picking”), `PLAN.md` (bifează / adaugă sarcina), `docs/plans/inferenta-pe-obiecte.md` (Stare: implementat)

- [ ] **Step 1: Performanța** — înainte (pe `main`, notat în ADR) și după: `navbench:hover,5` și `fpslog:5` pe un model cu 200 de obiecte (`scriptjs:` cu o buclă de `aion.sphere`, 64 de segmente) cu `make run CONFIG=release`; `aion_tests "[.perf]"`. Hover-ul nu scade sub 90% din fps-ul de dinainte; dacă scade, cache pe sursele din tronsonul razei (`rayHitsBox` pe cutia fiecărei surse înainte de interogare) — e voie în acest task.
- [ ] **Step 2:** ADR 0042 (deciziile din spec + forma aleasă pentru `SnapScene` + rezultatele de performanță); CLAUDE.md (o intrare: `inference/SnapScene.h`, `model/SnapSources.h`, `ToolContext::snap`, `tools::inferAt`, `Kind::Center`, pașii primitivelor, drop-ul pe centrul bazei); PLAN.md.
- [ ] **Step 3:** `make test` verde.
- [ ] **Step 4:** Commit `docs: ADR 0042 – inferența pe obiecte`.
