# Copii unice și animații pornite din interacțiuni — plan de implementare

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Copii unice în adâncime (⌘C / ⌘V cu plasare ca Move, copia cu Move), „Fă unic” la vedere, animațiile și
regulile duplicate odată cu copia, animații pornite din interacțiuni (sens, repetări, redare paralelă) și redenumirea
animațiilor.

**Architecture:** Copierea stă în core, într-un modul nou `model/Copying.h` (copie în adâncime, copie legată, „Fă
unic” în adâncime, duplicarea animațiilor și a regulilor). Redarea din interacțiuni e o clasă nouă de core,
`model::AnimationPlayer`, condusă de Document în Modul interacțiune (scrie poza direct în copia `m_runtime`, nu prin
timeline). Plasarea la lipire e o unealtă internă de core, `place`, pusă de Document în locul uneltei curente, ca
`pivot` / `penedit`.

**Tech Stack:** C++20, Qt 6 QML, Catch2, glm; `core/` fără Qt.

**Spec:** [`docs/plans/copii-si-animatii-interactive.md`](copii-si-animatii-interactive.md)

## Global Constraints

- `core/` nu include Qt; tot codul nou din `core/` are teste Catch2 în `tests/core/` (fișierele noi se adaugă în
  `tests/CMakeLists.txt`).
- Operațiile noi pe model din Document încep cu `require(Modeling)`; comenzile noi se clasifică în
  `Document::commandModes` (`edit.copy`, `edit.paste`: doar Modelare).
- Toate textele din UI în română; culorile doar din `Theme.qml`; Inspector-ul cu `PropertyGroup`, rândurile listelor cu
  `RowAction`. În QML nu se folosesc `int`, `long`, `float`, `double` etc. ca nume de variabile.
- Fiecare operație pe model e un singur pas de undo (lipirea, „Fă unic”, copia cu Move, redenumirea).
- Formatul `.aion` trece la versiunea 5; fișierele v4 se deschid: blocurile „Pornește animația” devin Înainte / O dată.
- Comparații de lungimi doar prin `aion/core/math/Tolerance.h`.
- Test: `cmake --build build/mac-debug --target aion_tests && build/mac-debug/tests/aion_tests "<filtru>"`; toată
  suita: `build/mac-debug/tests/aion_tests`. Aplicația: `make build`.

## Review Focus

1. Undo după lipire / „Fă unic” readuce tot: instanțele, definițiile, animațiile noi și regulile (un singur pas).
2. Lipirea după ce originalul a fost șters sau modificat: se lipește starea de la ⌘C (clipboard-ul ține o copie).
3. Regulile care țintesc un obiect frate din aceeași selecție copiată (`Action::target`) trec pe copia fratelui.
4. O animație ștearsă, încă pomenită de o regulă sau de clipboard: nicio cădere; blocul nu pornește nimic.
5. Ieșirea și reintrarea în Modul interacțiune: toate animațiile pornesc din nou de la început (starea se golește).

Fiecare are un test în task-ul care deține codul (marcat „Review Focus N”).

## Rulings (decizii luate la scrierea planului)

- **Clipboard-ul e al documentului curent:** se golește la „Nou” și la „Deschide”. Designul spunea că rămâne și după
  deschiderea altui document; asta ar cere potrivirea materialelor, tag-urilor și animațiilor între fișiere.
  Dacă e greșit: o lipire între documente rămâne de făcut separat.
- **„Fă unic” și regulile din afară** (rafinează secțiunea 3 din design): o regulă din afara obiectului făcut unic,
  care pornea o animație separată, e tratată după ce conține obiectul ei:
  - doar obiectele noi (de exemplu Corp B, părintele ușilor separate) → trece pe copia animației;
  - doar originalele (Corp A) → rămâne neschimbată;
  - ambele sau niciunele (un grup „Bucătărie”, un întrerupător) → primește un bloc în plus pentru copie.
  Altfel, un click pe Corp A ar deschide din nou și ușile lui B, adică exact legătura pe care o rupe „Fă unic”.
  Dacă e greșit: un bloc în plus sau în minus, ușor de corectat în editorul de reguli.
- **Copia cu Rotate a unui obiect animat el însuși:** doar cheile de poziție ale pistei lui se transformă cu copia;
  rotația și scara rămân cum erau. Dacă e greșit: copia rotită a unui obiect animat se rotește în animație ca originalul.
- **Plasarea la lipire (v1):** fără săgeți și fără VCB; prinderea pe grid, pe inferență și pe punctele de prindere ale
  obiectului de sub cursor.

---

### Task 1: Copii în adâncime, copii legate, „Fă unic” în adâncime (core)

**Files:**
- Create: `core/include/aion/core/model/Copying.h`, `core/src/model/Copying.cpp`
- Modify: `core/src/CMakeLists.txt` (sau unde e listată sursa), `core/include/aion/core/model/Operations.h`,
  `core/src/model/Operations.cpp` (scoate `copyInstance`, `makeUnique`), `core/src/model/Modeling.cpp:86-100`
  (`arraySelection` → `copyLinked`), `core/src/tools/TransformTools.cpp:405-428` (`copyTarget` → `copyDeep`),
  `render/src/Document.cpp` (`duplicateLinked` → `copyLinked`; `makeUniqueSelection` → `makeUniqueDeep`; meniul
  contextual: „Fă unic” când `canMakeUnique`)
- Test: `tests/core/CopyingTests.cpp`

**Interfaces:**
- Produces (în `namespace aion::model`, `model/Copying.h`):
  ```cpp
  struct CopyResult {
      std::vector<InstanceId> roots;              // copiile de sus, în ordinea surselor
      std::map<InstanceId, InstanceId> instances; // sursă → copie, pentru tot subarborele (și rădăcinile)
  };
  enum class CopyKind { Copy, Unique };
  // Copii unice, în adâncime: fiecare definiție din subarbore primește o copie; două instanțe care împart o definiție
  // o împart și în copie. Regulile vin odată cu instanțele (țintele din subarbore trec pe copii). `source` poate fi
  // chiar `target`. `delta` e în coordonatele contextului.
  CopyResult copyDeep(Model& target, DefinitionId context, const Model& source,
      const std::vector<InstanceId>& sources, const math::Transform& delta);
  // Copie legată (⌥D, Array): aceeași definiție, pentru orice fel de obiect; o definiție `group` care nu e
  // colecție devine `group = false` (ca ⌥D azi).
  InstanceId copyLinked(Model& model, InstanceId instance, const math::Transform& delta);
  // Fă unic, în adâncime: orice definiție din subarbore folosită și în afara lui primește o copie (definițiile
  // proprii subarborelui și instanțele lor rămân). `roots` = { instance }; `instances` = instanțele vechi din
  // definițiile copiate → cele noi (plus instance → instance).
  CopyResult makeUniqueDeep(Model& model, InstanceId instance);
  [[nodiscard]] bool canMakeUnique(const Model& model, InstanceId instance);
  // Muchiile obiectelor (cu tot ce conțin), în coordonatele părintelui lor: previzualizarea lipirii.
  [[nodiscard]] std::vector<std::pair<math::Vec3d, math::Vec3d>> instanceOutline(
      const Model& model, const std::vector<InstanceId>& instances);
  ```

- [ ] **Step 1: Write the failing tests** (`tests/core/CopyingTests.cpp`, tag `[core][copying]`). Un helper
  `mobila()` construiește structura din `Mobila.aion`: un grup „Usi” (definiție U cu două uși, obiecte cutie) folosit
  de două corpuri (definiții proprii A, B, câte o instanță în rădăcină), plus un grup „Corp” comun.
  - `copyDeep: copia nu împarte nicio definiție cu originalul` — copia lui Corp A: pentru orice definiție din
    subarborele copiei, `instancesOf(def)` conține doar instanțe din copie; `instances.size()` = numărul de instanțe
    din subarborele lui A; poziția copiei = poziția lui A + delta.
  - `copyDeep: legăturile din interior rămân` — un corp cu două rafturi care împart definiția R: în copie, cele două
    rafturi noi împart o definiție R' ≠ R.
  - `copyDeep: regulile vin cu copiile, cu țintele mutate` (Review Focus 3) — două obiecte selectate, unul cu o
    regulă `MoveBy` cu `target` = celălalt: în copie, `target` = copia celuilalt; o țintă din afara selecției rămâne.
  - `copyDeep: sursa poate fi chiar ținta` — `copyDeep(m, root, m, {a}, t)` dă același rezultat ca dintr-o copie a
    modelului.
  - `copyDeep: dintr-o copie a modelului, după ștergerea originalului` (Review Focus 2) — `const Model snap = m;`,
    apoi originalul (cu definițiile lui de grup) se șterge din `m`; `copyDeep(m, root, snap, {a}, t)` refăcută din
    `snap`: aceeași geometrie și aceleași reguli ca originalul.
  - `copyLinked: grupuri și colecții rămân legate` — pe un grup colecție: `copy.definition == original.definition`.
  - `makeUniqueDeep: separă ușile lui Corp B` — pe Corp B: definiția Usi a lui B ≠ U; ușile lui A rămân instanțele
    vechi; instanțele directe ale lui B (definiție proprie) își păstrează id-urile; `canMakeUnique(B)` true înainte,
    false după.
  - `undo readuce totul` (Review Focus 1) — `CommandStack::run` cu `makeUniqueDeep`, apoi `undo()`: modelul == cel
    de dinainte (`Model::operator==` sau comparația pe instanțe și definiții).
- [ ] **Step 2: Run, expect FAIL** — `aion_tests "[copying]"`: eroare de compilare (`Copying.h` lipsește).
- [ ] **Step 3: Implement `Copying.h/.cpp`** — `copyDeep` cu o hartă definiție veche → nouă pe apel (o definiție
  folosită de mai multe instanțe din subarbore se copiază o dată); geometria cu `geom::copyEntities`; valorile din
  `source` se copiază înainte de inserare (sursa poate fi ținta). `makeUniqueDeep`: coboară din instanță; o definiție cu
  instanțe și în afara subarborelui primește o copie (cu aceeași hartă), apoi continuă în copia ei.
- [ ] **Step 4: Callers** — `copyTarget` (Move / Rotate, inclusiv seriile ×N) → `copyDeep`; `arraySelection` și
  `Document::duplicateLinked` → `copyLinked`; `Document::makeUniqueSelection` → `makeUniqueDeep`; meniul contextual
  arată „Fă unic” când `canMakeUnique` (și pentru grupuri). `copyInstance` și `makeUnique` dispar (testele lor vechi
  trec pe funcțiile noi).
- [ ] **Step 5: Run** `aion_tests "[copying]"` → PASS; toată suita → PASS.
- [ ] **Step 6: Commit** — „Copii unice în adâncime; ⌥D și Array leagă orice obiect; Fă unic în adâncime”.

### Task 2: Animațiile și regulile copiilor (core)

**Files:**
- Modify: `core/include/aion/core/model/Copying.h`, `core/src/model/Copying.cpp`,
  `core/src/tools/TransformTools.cpp` (`copyTarget` cheamă și `copyAnimations`)
- Test: `tests/core/CopyingTests.cpp`

**Interfaces:**
- Consumes: `CopyResult`, `CopyKind`, `copyDeep`, `makeUniqueDeep` (Task 1).
- Produces:
  ```cpp
  // Secțiunea 2 din design. `source` = modelul din care s-a copiat (poate fi `target`); `delta` = deplasarea copiei.
  // Copy: o animație pornită de o regulă din subarbore → „<nume> (copie)” cu pistele subarborelui (pe copii), iar
  // regulile copiate trec pe ea; celelalte animații cu piste în subarbore primesc pistele copiilor (aceeași animație).
  // Unique: la fel, plus regulile din afară după rulingul „Fă unic și regulile din afară”.
  void copyAnimations(Model& target, const Model& source, const CopyResult& copy, const math::Transform& delta,
      CopyKind kind);
  ```

- [ ] **Step 1: Write the failing tests** (`[core][copying]`):
  - `copyAnimations: animația pornită de regulă se duplică` — Corp A cu regula „Pornește Animația 1” pe Usi; Animația 1
    are piste pe ușile lui A și pe un obiect X din afară. După copie: există „Animația 1 (copie)” cu exact 2 piste (ușile
    copiei); regula copiată pornește copia; Animația 1 neschimbată.
  - `copyAnimations: numele nu se repetă` — două copii succesive: „Animația 1 (copie)”, „Animația 1 (copie 2)”.
  - `copyAnimations: filmul din timeline primește pistele copiei` — Animația 2 (nicio regulă n-o pornește) cu o pistă pe
    o ușă a lui A: după copie, Animația 2 are o pistă în plus, pe ușa copiei; nicio animație nouă.
  - `copyAnimations: pista obiectului de sus se mută cu copia` — Corp A animat el însuși (poziție x: 0 → 1000):
    după copia cu delta (5000, 0, 0), pista copiei are cheile x 5000 → 6000; pistele ușilor (relative) neschimbate.
  - `copyAnimations: căile și camera nu se copiază` — copia unei animații cu chei de cameră și o cale: copia nu are
    nici camera, nici calea.
  - `Fă unic: regulile din afară` — după `makeUniqueDeep(B)` + `copyAnimations(..., Unique)`: regula de pe Usi lui A →
    Animația 1; regula de pe Usi lui B → copia; o regulă pe Corp B (părinte) → copia; un întrerupător din rădăcină →
    pornește ambele (două blocuri, aceleași setări).
  - `regulă cu animație ștearsă` (Review Focus 4) — o regulă pomenește o animație inexistentă: copierea nu cade, blocul
    rămâne cum era.
- [ ] **Step 2: Run, expect FAIL** — `copyAnimations` nedefinit.
- [ ] **Step 3: Implement `copyAnimations`** — „pornită de regulă” = un bloc PlayAnimation (recursiv în `first` /
  `second`) în regulile instanțelor sursă din `copy.instances`; pentru delta, cheile de poziție ale pistelor rădăcinilor
  trec prin `delta.applyToPoint` (cele trei canale de poziție împreună, la fiecare moment de cheie).
- [ ] **Step 4: Run** `aion_tests "[copying]"` → PASS; toată suita → PASS.
- [ ] **Step 5: Commit** — „Copiile își primesc animațiile și regulile”.

### Task 3: Redarea animațiilor din interacțiuni (core)

**Files:**
- Create: `core/include/aion/core/model/AnimationPlayer.h`, `core/src/model/AnimationPlayer.cpp`
- Modify: `core/include/aion/core/model/Model.h` (`PlayDirection`, câmpuri noi în `Action`),
  `core/include/aion/core/model/Rules.h`, `core/src/model/Rules.cpp` (`HostRequest` cu datele blocului)
- Test: `tests/core/AnimationPlayerTests.cpp`, `tests/core/RuleTests.cpp`

**Interfaces:**
- Produces:
  ```cpp
  // Model.h
  enum class PlayDirection { Forward, Reverse, Toggle };
  struct Action { /* ... */ PlayDirection direction = PlayDirection::Forward; int repeat = 1; /* 0 = la infinit */ };
  // Rules.h
  struct HostRequest {
      AnimationId playAnimation;
      SceneId goToScene;
      PlayDirection direction = PlayDirection::Forward;
      int repeat = 1;
      std::string action;      // instanța + calea blocului (oprește bucla infinită a aceluiași bloc)
      std::uint64_t firing = 0; // aceeași declanșare de regulă = același număr
  };
  // AnimationPlayer.h
  struct PlayRequest { AnimationId animation; PlayDirection direction = PlayDirection::Forward; int repeat = 1;
      std::string action; };
  class AnimationPlayer {
  public:
      // Blocurile unei declanșări: cele cu Comută se decid împreună (toate la sfârșit → înapoi, altfel înainte).
      void start(const Model& base, const std::vector<PlayRequest>& requests);
      // Avansează; scrie în `runtime` poza animațiilor care rulează (de la `base`, ultima pornită câștigă) și ultima
      // poză a celor care tocmai s-au oprit. True dacă a scris ceva.
      bool update(Model& runtime, const Model& base, double dt);
      [[nodiscard]] bool active() const;                      // rulează ceva
      [[nodiscard]] double position(AnimationId animation) const; // secunde în animație (0 dacă n-a rulat)
      [[nodiscard]] bool playing(AnimationId animation) const;
      [[nodiscard]] std::optional<Color> color(const Model& base, InstanceId instance) const;
      [[nodiscard]] std::optional<CameraKey> camera(const Model& base) const; // ultima pornită care rulează, cu cameră
      void reset();
  };
  ```

- [ ] **Step 1: Write the failing tests** (`[core][player]`, pe un model cu o ușă și Animația „Ușa” de 1 s, rotație
  0 → 90°):
  - `O dată înainte: rulează o dată și rămâne deschisă` — `start({Ușa, Forward, 1})`, `update(…, 0.5)` → rotația ≈ 45°;
    `update(…, 1.0)` → 90°, `active()` false; încă un `start(Forward)` → rămâne 90° (nimic nu pornește).
  - `Înapoi o închide; la început nu face nimic`.
  - `Comută: deschide, apoi închide; în mișcare se întoarce` — la 0.4 s, Comută → se întoarce din 0.4 spre 0.
  - `De N ori reia de la început` — repeat 3: după 3 s, `active()` false; la 1.5 s rotația ≈ 45°.
  - `La infinit: același bloc o oprește pe loc` — repeat 0: după 10 s încă rulează; încă un `start` cu același
    `action` → `active()` false, poza rămâne unde era.
  - `Comută cu mai multe animații: aceeași decizie` — Ușile A la sfârșit, Ușile B la început; `start({A Toggle, B
    Toggle})` → ambele înainte (A rămâne deschisă, B se deschide); încă o dată → ambele înapoi.
  - `Animații în paralel, fără resetări` — A pornește, la 0.5 s pornește B: A continuă până la 1 s.
  - `Ultima pornită câștigă` — două animații pe același obiect.
  - `reset golește starea` (Review Focus 5) — după `reset()`, `position(Ușa)` = 0 și `active()` false.
  - `Animație ștearsă sau goală: nimic nu cade` (Review Focus 4) — id inexistent, durată 0, fără piste.
  - în `RuleTests.cpp`: `PlayAnimation trimite sensul, repetarea, blocul și declanșarea` — o regulă cu două blocuri
    PlayAnimation: două `HostRequest` cu același `firing`, `action` diferit, câmpurile copiate din `Action`.
- [ ] **Step 2: Run, expect FAIL** — `aion_tests "[player]"`: `AnimationPlayer.h` lipsește.
- [ ] **Step 3: Implement** — fiecare animație are poziția (s), sensul, turele rămase și numărul pornirii; poza:
  `evaluateInstance(track, t, base.instance(id).transform, pivotLocal(base, id))` și `pathTransformsAt(base, anim, t)`
  pentru obiectele ancorate; „la sfârșit” = poziția ≥ durata − 1e-6.
- [ ] **Step 4: Run** `aion_tests "[player]"` și `"[rules]"` → PASS; toată suita → PASS.
- [ ] **Step 5: Commit** — „AnimationPlayer: animațiile din interacțiuni (sens, repetări, în paralel)”.

### Task 4: Formatul `.aion` v5

**Files:**
- Modify: `core/include/aion/core/io/AionFile.h` (`kFormatVersion = 5`), `core/src/io/AionFile.cpp:587-620`
- Test: `tests/core/IoTests.cpp`

- [ ] **Step 1: Write the failing tests** (`[core][io]`): `Acțiunile își păstrează sensul și repetarea` (salvare →
  încărcare, Toggle / 0 și Reverse / 3); `Un fișier v4 dă Înainte / O dată` (JSON fără câmpurile noi);
  `formatVersion e 5`.
- [ ] **Step 2: Run, expect FAIL.**
- [ ] **Step 3: Implement** — câmpurile `"direction"` (0 Forward, 1 Reverse, 2 Toggle) și `"repeat"` în
  `actionToJson` / `actionFromJson`, cu valorile implicite 0 și 1.
- [ ] **Step 4: Run** `aion_tests "[io]"` → PASS; toată suita → PASS.
- [ ] **Step 5: Commit** — „Fișierul .aion v5: sensul și repetarea blocurilor de animație”.

### Task 5: Interacțiunile în aplicație (Document + editorul de reguli)

**Files:**
- Modify: `render/include/aion/render/Document.h`, `render/src/Document.cpp` (`handleRuleRequests`,
  `advanceInteractions`, `setInteractionMode`, `displayColor`, `animationCamera`, `actionToMap` / `actionFromMap`),
  `render/src/Viewport.cpp` (`applyAnimationCamera` și apelul din `onAfterAnimating`),
  `app/qml/overlays/RulesEditor.qml`, `app/qml/overlays/RulesActionBlock.qml`

**Interfaces:**
- Consumes: `AnimationPlayer`, `PlayRequest`, `HostRequest` (Task 3).
- Produces: `model::AnimationPlayer m_player;` în Document; `bool Document::animationCameraActive() const` (timeline-ul
  în previzualizare sau o animație cu cameră care rulează în Modul interacțiune); în hărțile acțiunilor:
  `"direction": "forward" | "reverse" | "toggle"`, `"repeat": număr` (0 = la infinit).

- [ ] **Step 1: Document** — `handleRuleRequests` grupează cererile de animație după `firing` și cheamă
  `m_player.start(m_model, …)` (nu mai pornește timeline-ul); `advanceInteractions` cheamă
  `m_player.update(*m_runtime, m_model, dt)` (crește `m_interactionRevision` când scrie) și ține bucla activă cât
  `m_player.active()`; `setInteractionMode` face `m_player.reset()` la intrare și la ieșire; `displayColor` în Modul
  interacțiune = `m_player.color`; `animationCamera` / `animationCameraActive` folosesc `m_player.camera` în Modul
  interacțiune. `Viewport` aplică camera animației când `animationCameraActive()`.
- [ ] **Step 2: Editorul de reguli** — blocul „Pornește animația” arată, sub animație, sensul (Înainte / Înapoi /
  Comută) și repetarea (O dată / De N ori + câmpul N, minim 1 / La infinit). Blocurile noi: Înainte / O dată.
- [ ] **Step 3: Build** — `make build` fără erori; toată suita → PASS.
- [ ] **Step 4: Verificare în aplicație** — pe `Mobila.aion` (copia din scratchpad), după „Fă unic” pe Corp B
  (Task 7) sau pe un model construit prin `scriptjs:` + `rule:`: Modul interacțiune, click pe ușile lui A → doar ele se
  deschid, o dată; al doilea click (Comută) le închide; ușile lui B rămân. Capturi cu `AION_SCREENSHOT`.
- [ ] **Step 5: Commit** — „Interacțiunile pornesc animații independente (sens, repetări)”.

### Task 6: Unealta de plasare (core)

**Files:**
- Modify: `core/include/aion/core/tools/Tool.h` (`ToolContext::place`, `ToolRequests::placeAt` / `placeCancel`),
  `core/src/tools/TransformTools.cpp` (clasa `PlaceTool`), `core/src/tools/Tools.cpp` (`makeTool("place")`)
- Test: `tests/core/ToolTests.cpp`

**Interfaces:**
- Produces:
  ```cpp
  // ToolContext
  struct PlaceSource { Vec3d base; std::vector<std::pair<Vec3d, Vec3d>> edges; }; // în coordonatele contextului
  std::optional<PlaceSource> place;
  // ToolRequests
  std::optional<Vec3d> placeAt; // punctul ales pentru `base` (click)
  bool placeCancel = false;     // Esc
  ```

- [ ] **Step 1: Write the failing tests** (`[core][tools]`):
  - `Place: urmează cursorul și desenează copia` — `pointerMove` la un punct de pe sol: overlay-ul are muchiile din
    `place->edges` mutate cu (punct − base).
  - `Place: se prinde de colțul altui obiect` — un obiect în scenă; cursorul la 3 px de colțul lui: punctul curent =
    colțul (marcaj „Colț”).
  - `Place: click cere plasarea, Esc o anulează` — `pointerDown` → `requests->placeAt` = punctul curent; `key(Escape)`
    → `requests->placeCancel`.
  - `Place: snap la grid` — `ctx.gridStep = 500`: punctul liber se rotunjește la 500.
- [ ] **Step 2: Run, expect FAIL.**
- [ ] **Step 3: Implement `PlaceTool`** — inferența lui Move (motorul cu grid), punctele de prindere ale obiectului de
  sub cursor (`instanceGrips` / `nearestGrip`, ca Move fără selecție); nu scrie în model.
- [ ] **Step 4: Run** `aion_tests "[tools]"` → PASS; toată suita → PASS.
- [ ] **Step 5: Commit** — „Unealta internă place: plasarea unei copii cu prindere”.

### Task 7: ⌘C / ⌘V, „Fă unic” la vedere (aplicație)

**Files:**
- Modify: `render/include/aion/render/Document.h`, `render/src/Document.cpp`, `app/qml/Main.qml` (comenzile
  `edit.copy` Ctrl+C și `edit.paste` Ctrl+V, categoria „Editare”), `app/qml/shell/AppMenuBar.qml` (meniul Editare),
  `app/qml/shell/EntityInfo.qml` (butonul „Fă unic”), `app/qml/shell/Outliner.qml` (`RowAction` „Fă unic”)

**Interfaces:**
- Consumes: `copyDeep`, `copyAnimations`, `makeUniqueDeep`, `canMakeUnique`, `instanceOutline` (Task 1–2),
  `PlaceSource` / `placeAt` / `placeCancel` (Task 6).
- Produces:
  ```cpp
  Q_INVOKABLE void copySelection();   // ⌘C: obiectele selectate din context
  Q_INVOKABLE void pasteClipboard();  // ⌘V: unealta internă „place”, apoi copia la click
  Q_INVOKABLE void makeUnique(double instanceId);
  Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
  // entityInfo: "canMakeUnique" (bool); rândurile din outline: "canMakeUnique" (bool)
  ```

- [ ] **Step 1: Clipboard** — `copySelection` păstrează o copie a modelului (`std::shared_ptr<const model::Model>`),
  contextul și instanțele (Review Focus 2); `newDocument` și `openFile` golesc clipboard-ul. `pasteClipboard`:
  `require(Modeling)`; `base` = colțul minim al cutiei comune a instanțelor; `ctx.place` = { base,
  `instanceOutline` }; unealta curentă se suspendă (ca `beginPivotDrag`). La `placeAt`: un singur
  `runOnModel("Lipește", …)` cu `copyDeep` + `copyAnimations(…, CopyKind::Copy)`, copiile selectate, unealta veche
  revine; la `placeCancel`: revine fără nimic în istoric.
- [ ] **Step 2: Comenzile** — `edit.copy` / `edit.paste` doar în Modelare (`commandModes`); în Animare tastele rămân
  ale lui `anim.copyKeys` / `anim.pasteKeys` (`shortcutOwners`); în meniul Editare după „Șterge”.
- [ ] **Step 3: „Fă unic”** — `makeUnique(id)` și `makeUniqueSelection`: `runOnModel("Fă unic", …)` cu
  `makeUniqueDeep` + `copyAnimations(…, CopyKind::Unique)`. Butonul din Inspector (lângă „Editează” / „Desface”) și
  `RowAction` din lista Obiecte apar când `canMakeUnique`.
- [ ] **Step 4: Build** — `make build`; toată suita → PASS.
- [ ] **Step 5: Verificare în aplicație** (capturi): ⌘C pe un corp, ⌘V, mutare până la colțul altui corp, click: copia
  stă colț la colț, „×” nu apare în listă, copia are „Animația 1 (copie)”; ⌘Z o scoate cu tot cu animație. Pe
  `Mobila.aion`: „Fă unic” pe Corp B → „Usi” nu mai e „×2”; interacțiunea lui B pornește copia animației.
- [ ] **Step 6: Commit** — „⌘C / ⌘V cu plasare; Fă unic la vedere”.

### Task 8: Redenumirea animațiilor și documentația

**Files:**
- Modify: `render/include/aion/render/Document.h`, `render/src/Document.cpp`, `app/qml/shell/Timeline.qml`
  (rândurile din lista animațiilor); `docs/decisions/0039-copii-si-animatii-interactive.md` (nou), `CLAUDE.md`,
  `docs/qa-checklist.md`, `PLAN.md`, `docs/plans/copii-si-animatii-interactive.md` (starea: implementat)

**Interfaces:**
- Produces: `Q_INVOKABLE void renameAnimation(double id, const QString& name);` (numele fără spații la capete; gol =
  ignorat; un pas de undo).

- [ ] **Step 1: Document** — `renameAnimation` prin `runOnModel("Redenumește animația", …)`; emite `timelineChanged`.
- [ ] **Step 2: Timeline** — pe fiecare rând al listei animațiilor, un buton creion (înaintea ștergerii) schimbă numele
  într-un câmp; Enter salvează, Esc renunță.
- [ ] **Step 3: Build** și verificare cu o captură (lista cu un nume schimbat).
- [ ] **Step 4: Documentația** — ADR 0039 (deciziile din design + rulingurile de mai sus), `CLAUDE.md` (modulele noi,
  acțiunile de verificare folosite), `docs/qa-checklist.md` (secțiune nouă), `PLAN.md` (bifat).
- [ ] **Step 5: Run** toată suita → PASS; **Commit** — „Redenumirea animațiilor; documentație (ADR 0039)”.
