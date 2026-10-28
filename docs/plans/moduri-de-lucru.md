# Plan: toolbar nou și cele trei moduri de lucru (Modelare / Animare / Interacțiune)

> Stare: **plan scris pe 2026-10-28; deciziile din secțiunea 5 așteaptă confirmarea utilizatorului** (dacă nu
> răspunde altfel, se implementează cum sunt scrise). Poate fi continuat de oricine (alt cont,
> altă sesiune) fără contextul conversației: citește întâi `CLAUDE.md`, apoi acest fișier, și bifează pașii aici
> pe măsură ce îi termini (și în `PLAN.md`, secțiunea „Etapa 5”). Comunicarea cu utilizatorul: în română.

## 1. Ce vrea utilizatorul (cerința, rezumată fidel)

1. **Toolbar ca în Figma.** Cele trei butoane de navigare din dreapta (Orbit, Pan, Zoom) se mută în **primul
   dropdown din stânga**, lângă Select: când una dintre ele e activă, Select nu e activ, deci au sens împreună.
2. În locul lor, în dreapta toolbar-ului, un **comutator între trei moduri de lucru**: **Modelare**, **Animare**,
   **Interacțiune** (ca Design / Prototype / Dev Mode din Figma).
3. **Fiecare mod permite doar ce îi aparține:**
   - Modelare: doar modelare (desen, modificare, organizare, materiale…).
   - Animare: doar animarea elementelor deja existente (fără modificarea geometriei).
   - Interacțiune: crearea interacțiunilor și interacțiunea cu obiectele existente.
4. Utilizatorul se așteaptă la multe modificări de layout și comportament; planul trebuie să fie detaliat, cu pași
   care se pot preda.

## 2. Starea de acum (ce există deja și unde)

| Ce | Unde | Observații |
|---|---|---|
| Grupurile toolbar-ului | `app/qml/shell/Toolbar.qml` (`groups`, `navigation`, `hiddenTools`, `keyFor`) | `select` = Select + Interact; navigarea e un cluster separat (`navigation`). |
| Butonul unui grup cu meniu | `app/qml/shell/ToolGroupButton.qml`, `app/qml/components/Menu.qml` (acceptă `{ header }`) | |
| Unealta activă, navigarea | `app/qml/Main.qml`: `window.activeTool`, `navigationTools = ["orbit","pan","zoom","interact"]`, `selectTool(id)`, `modelingTools` | Navigarea și Interact sunt tratate de `Viewport` (`navigationTool`), restul de `Document`. |
| Comenzile, scurtăturile | `Main.qml`: `commands` (palette, scurtături, meniul nativ), `runCommand(id)`, `shortcutOverrides` | Scurtăturile personalizabile se aplică peste `commands`. |
| Timeline-ul | `app/qml/shell/Timeline.qml`, `window.timelineVisible` (⌥T), `EntityInfo.timelineActive` | Keyframe-uri: `Document::keySelection` / `keySelectionColor` / `keyCamera`, `editAnimation`, previzualizare `m_previewing` (orice editare iese din ea: `runOnModel` → `endPreview`). |
| Modul interacțiune (rulare) | `Document::interactionMode` (F5, `mode.interaction`), `m_runtime` (o copie a modelului), bannerul din `Main.qml`, butonul ▷ din `ViewportControls.qml` și din secțiunea Interacțiuni | Regulile: `overlays/RulesEditor.qml`, panoul `shell/InteractionsPanel.qml`, comportamentele din `EntityInfo.qml` (`behaviorBox`). |
| Panoul din stânga | `Main.qml` (secțiunile Obiecte, Bibliotecă, Tag-uri, Materiale, Scene, Interacțiuni; `preferences.left*`) | |
| Inspector | `app/qml/shell/EntityInfo.qml` (grupuri `PropertyGroup`), `ObjectInfo.qml` | |
| Meniul contextual | `Document::contextMenuItems()` (C++), `Main.qml` `contextMenu` | |
| Meniul nativ | `app/qml/shell/AppMenuBar.qml` | |
| Bara de vizualizare | `app/qml/shell/ViewportControls.qml` (are ▷ pentru modul interacțiune) | |

## 3. Designul

### 3.1 Toolbar (de la stânga la dreapta)

```
[ Select ▾ ] [ Primitive ▾ ] [ Modify ▾ ] [ Transform ▾ ] [ Measure ▾ ] [ Eraser ]  |  [ Modelare | Animare | Interacțiune ]
   Select V/Space
   Orbit O
   Pan H
   Zoom Z
```

- **Primul grup** („Selecție și navigare”): Select, apoi un titlu de secțiune „Navigare” și Orbit, Pan, Zoom
  (`Menu` acceptă deja `{ header }`). Grupul își amintește ultima unealtă, ca celelalte. **Interact** iese din acest
  grup (devine unealta modului Interacțiune, vezi 3.4).
- **Comutatorul de mod** în dreapta, în locul clusterului de navigare: un control segmentat cu 3 iconițe + text la
  hover (tooltip cu scurtătura): `box` / `pen-tool` → Modelare, `film` → Animare, `hand-pointer` → Interacțiune.
  Modul activ e evidențiat cu accentul. Nou: `shell/ModeSwitch.qml`.
- **Grupurile din mijloc depind de mod** (vezi tabelul din 3.5). Un grup fără unelte în modul curent nu apare;
  toolbar-ul își schimbă lățimea animat (`Behavior on implicitWidth`).
- **Scurtăturile modurilor**: ⌥1 Modelare, ⌥2 Animare, ⌥3 Interacțiune (libere azi; ⌘0–⌘6 sunt vederile). În
  `commands`, categoria „Moduri”, deci apar în palette, meniul nativ (Vizualizare) și Preferințe → Scurtături.

### 3.2 Starea „mod” (sursa de adevăr)

- **`Document::workMode`** (enum `WorkMode { Modeling, Animation, Interaction }`, `Q_PROPERTY` cu notify
  `workModeChanged`). În Document, nu în QML, pentru că regulile („ce e permis”) trebuie verificate în C++
  (comenzi venite din palette, scripturi, meniul contextual, scurtături).
- Schimbarea modului: anulează unealta curentă (`m_tool->cancel`), închide panourile cu previzualizare
  (`cancelOperation`), iese din previzualizarea animației dacă nu intră în Animare, iese din rularea interacțiunilor
  (`setInteractionMode(false)`) dacă nu intră în Interacțiune; alege unealta implicită a modului (Select).
- **Nu e un pas de undo** și nu se salvează în fișier; ultimul mod se păstrează în preferințe (`preferences.workMode`),
  ca aplicația să repornească în el.
- Scripturile (`render/ScriptEngine`) rulează indiferent de mod (sunt explicit cerute de utilizator).

### 3.3 Modul **Modelare** (implicit)

- Tot ce există azi, minus: timeline-ul (ascuns), rularea interacțiunilor, editorul de reguli, comportamentele.
- Toolbar: Select/Navigare, Primitive, Modify (Push/Pull, Offset, Follow Me), Transform (Move, Rotate, Scale),
  Measure, Eraser. Paint Bucket rămâne pornit din panoul Materiale / B.
- Panoul stâng: Obiecte, Bibliotecă, Tag-uri, Materiale, Scene.
- Inspector: tot ce e azi, fără grupurile „Interacțiune” (comportament + reguli) și fără butoanele de keyframe.

### 3.4 Modul **Animare**

- **Timeline-ul se arată automat** (și rămâne vizibil cât timp modul e activ; butonul de închidere al timeline-ului
  întoarce în Modelare sau doar ascunde? → *decizie: îl ascunde, modul rămâne*). Dacă documentul nu are animații,
  timeline-ul oferă „Animație nouă”.
- Toolbar: Select/Navigare și **Transform (Move, Rotate, Scale)**. Fără desen, Push/Pull, Eraser, Measure.
- **Auto-key**: în Animare, Move / Rotate / Scale aplicate pe **obiecte** (instanțe) nu schimbă modelul, ci scriu
  keyframe-uri la timpul curent:
  - implementare: în `Document`, după `pointerUp`/`applyMeasurement` care a produs o transformare de instanțe în
    modul Animare, se ia transformarea rezultată, se face `undo` pe pasul din modelul de bază și se scrie
    `setTransformKey(animation, id, t, transform)` prin `editAnimation` (un singur pas de undo „Keyframe”). Mai
    curat: o funcție `Document::transformForAnimation(ids, delta)` folosită de uneltele de transformare când
    `workMode == Animation` (transformTarget din `core/src/tools/TransformTools.cpp` primește un callback în
    `ToolContext`, de exemplu `ctx.onInstanceTransform`, setat de Document doar în Animare).
  - poza afișată e cea din previzualizare (`m_previewing = true` cât timp modul e Animare), deci obiectul se vede
    unde e la timpul curent, iar mutarea pornește de acolo.
  - geometria liberă (fețe, muchii) **nu se poate selecta pentru transformare** în Animare: Select alege doar
    obiecte (instanțe), camere și lumini (`SelectTool` cu o opțiune „doar obiecte” din `ToolContext`).
- Inspector: Poziție / Rotație / Scară ale obiectului **la timpul curent** (citite din previzualizare), cu butoanele
  de keyframe (◆) mereu vizibile (azi apar la hover cu timeline-ul deschis: `PlacementField.keyMode`); Culoarea (key de
  culoare); Dimensiunile și Pivotul rămân, dar dimensiunile devin doar citire (scara se animă, nu geometria).
- Panoul stâng: Obiecte, Scene (camera e și ea animabilă: `keyCamera`). Fără Bibliotecă, Materiale, Tag-uri
  (tag-urile: doar vizibilitatea, în Obiecte).
- Meniul contextual și palette: doar comenzile de animare (keyframe, șterge keyframe, animație nouă, export video,
  redare) plus vizualizarea și selecția.

### 3.5 Modul **Interacțiune**

- Două stări în același mod:
  - **Editare** (implicit): Select alege obiecte; Inspector arată grupul „Interacțiune” (comportament: balama,
    glisare; „Interacțiuni cu blocuri…” → `RulesEditor`); panoul stâng: Obiecte, Interacțiuni.
  - **Testare** (rularea pe copia modelului, azi „Modul interacțiune” / F5): un buton ▷ „Testează” în toolbar
    (în locul grupurilor de modelare) și F5. În testare, unealta activă e **Interact** (click / trage pe obiecte),
    bannerul de azi rămâne, Esc oprește testarea (rămâi în modul Interacțiune).
- Toolbar: Select/Navigare, **Interact** (unealta, ca grup propriu, activă doar în testare sau și în editare
  pentru încercări rapide — *decizie: doar în testare; în editare clicurile aleg obiecte*), ▷ Testează.
- Fără desen, transformări, materiale.

### 3.6 Ce e permis unde (tabelul de referință)

| Funcție | Modelare | Animare | Interacțiune |
|---|:-:|:-:|:-:|
| Select, Orbit, Pan, Zoom, scene, stiluri de afișare, ⌘0–⌘6 | ✓ | ✓ | ✓ |
| Primitive, Push/Pull, Offset, Follow Me, Eraser, Measure, Paint | ✓ | – | – |
| Move / Rotate / Scale | ✓ (model) | ✓ (keyframe, doar obiecte) | – |
| Bevel, Array, Soften, Mirror, booleene, Intersect, Creează fețe | ✓ | – | – |
| Grupare, componente, explode, tag-uri, materiale, bibliotecă | ✓ | – | – |
| Inspector: plasare, dimensiuni, pivot | ✓ | plasare la timpul curent + ◆ | doar citire |
| Timeline, keyframe-uri, export video | – | ✓ | – |
| Comportamente, reguli (RulesEditor), Interacțiuni (panou) | – | – | ✓ |
| Testarea interacțiunilor (F5) | → trece în Interacțiune | → trece în Interacțiune | ✓ |
| Undo / redo, salvare, export, Preferințe, scripturi | ✓ | ✓ | ✓ |

Regula de implementare: **fiecare comandă și unealtă își declară modurile** (`modes: ["model"]` în `commands` și în
grupurile toolbar-ului; implicit toate). Ce nu e permis:
- nu apare în toolbar / meniul contextual / palette (palette poate arăta comenzile altor moduri estompate, cu
  „(în modul Animare)”, iar Enter pe ele schimbă modul — *opțional, vezi 5*);
- scurtătura e dezactivată (în `Main.qml`, `enabled` al `Shortcut`-ului verifică modul);
- **în C++**, `Document` refuză operațiile de model nepermise (gardă în `runOnModel` cu un nume de categorie sau,
  mai simplu, în fiecare metodă publică de modelare: `if (!allows(WorkMode::Modeling)) return setError(...)`), ca
  scripturile și căile uitate să nu ocolească regula. Scripturile rulează între `beginMacro`/`endMacro` și sunt
  scutite (`m_macro`).

## 4. Pașii (fiecare = un commit mic pe o ramură `feature/...`, unit în `main`, CI urmărit)

Bifează aici și în `PLAN.md` → „Etapa 5”.

### Pasul 1 — Starea modului (C++) ✅ criteriu: teste + palette
- [ ] `render/include/aion/render/Document.h`: `enum class WorkMode`, `Q_PROPERTY(int workMode …)` (0/1/2), setter cu
      efectele din 3.2, `Q_INVOKABLE bool allows(int mode) const`.
- [ ] `Main.qml`: `preferences.workMode` (persistat), sincronizat cu `doc.workMode`; comenzile `mode.model`,
      `mode.animate`, `mode.interact` (⌥1/⌥2/⌥3, categoria „Moduri”), acțiunea de script `mode:model|animate|interact`.
- [ ] Teste: dacă logica de „permis” ajunge în `core/` (de exemplu o funcție pură `allowedInMode(command, mode)`),
      teste Catch2; altfel verificare prin script + captură.

### Pasul 2 — Toolbar: navigarea în primul grup + comutatorul ✅ criteriu: captură
- [ ] `Toolbar.qml`: grupul `select` = Select + secțiunea „Navigare” (Orbit, Pan, Zoom); scoate clusterul `navigation`
      (păstrează `allTools` pentru scurtături: O, H, Z rămân); Interact devine grupul `interact` (vizibil doar în
      modul Interacțiune).
- [ ] `shell/ModeSwitch.qml` (nou, înregistrat în `app/CMakeLists.txt`): control segmentat cu 3 iconițe, tooltip-uri
      cu scurtătura (`Toolbar.keyFor` / `shortcutOverrides`), semnal `modeSelected(int)`.
- [ ] Fiecare grup primește `modes`; toolbar-ul filtrează grupurile după `doc.workMode`.
- [ ] Ghidurile animate (`ToolGuides.js`, `ToolTour`) și `RichTooltip` funcționează pentru uneltele mutate.

### Pasul 3 — Filtrarea comenzilor, scurtăturilor, meniurilor ✅ criteriu: în Animare, ⇧R / R / P nu fac nimic
- [ ] `commands` primesc `modes`; `Shortcut.enabled` verifică modul; palette arată doar comenzile modului (sau
      estompate cu mutarea în modul lor — decizie la pasul 3, preferat: doar ale modului curent + cele comune).
- [ ] `Document::contextMenuItems()` filtrează după mod.
- [ ] `AppMenuBar.qml`: meniul „Vizualizare” (sau unul nou „Mod”) cu cele 3 moduri bifabile.

### Pasul 4 — Gărzile din Document ✅ criteriu: test că o operație de modelare e refuzată în Animare
- [ ] `allows()` în metodele de modelare (desen prin unelte: `setActiveTool` refuză uneltele altui mod; `pointerDown`
      nu ajunge la ele; metodele publice: `deleteSelection`, `makeGroup`, `previewBevel`, `previewArray`,
      `mirrorSelection`, `assignMaterial`, booleene, `createPrimitive`…). Scripturile (`m_macro`) sunt scutite.
- [ ] Mesaj clar: „În modul Animare nu se modifică geometria. Treci în Modelare (⌥1).”

### Pasul 5 — Layout-ul pe mod (panouri, Inspector) ✅ criteriu: capturi în cele 3 moduri
- [ ] Panoul stâng: secțiunile vizibile după mod (tabelul din 3.3–3.5); înălțimile și starea deschis/închis rămân
      pe secțiune.
- [ ] `EntityInfo.qml`: grupurile după mod (Interacțiune doar în modul Interacțiune; ◆ mereu vizibile în Animare;
      plasarea doar-citire în Interacțiune).
- [ ] `ViewportControls.qml`: ▷ (testarea interacțiunilor) doar în modul Interacțiune (sau scos, dacă e în toolbar).
- [ ] Bara de stare: modul curent la stânga indicației (de exemplu „Animare · Click pe un obiect…”).

### Pasul 6 — Animare: timeline automat și auto-key ✅ criteriu: Move în Animare scrie un keyframe, modelul nu se schimbă
- [ ] Intrarea în Animare: timeline vizibil, `m_previewing = true` la timpul curent; ieșirea: timeline ascuns,
      `endPreview`.
- [ ] Select doar pe obiecte în Animare (`ToolContext`: `objectsOnly`).
- [ ] Auto-key pentru Move / Rotate / Scale pe instanțe (callback în `ToolContext`, vezi 3.4), un pas de undo.
- [ ] Inspector: plasarea la timpul curent; modificarea ei în Animare scrie keyframe (nu `setPlacement` pe model).
- [ ] Teste în `tests/core/ToolTests.cpp` pentru callback-ul de transformare (unealta Move cu `onInstanceTransform`
      nu schimbă modelul) și în `AnimationTests.cpp` dacă se adaugă funcții noi în `core/model/Animation*`.

### Pasul 7 — Interacțiune: editare vs testare ✅ criteriu: ▷ / F5 pornește testarea, Esc o oprește, modul rămâne
- [ ] Butonul ▷ „Testează” în toolbar (în modul Interacțiune), F5 îl comută; în testare unealta = Interact.
- [ ] Din alt mod, F5 trece în Interacțiune și pornește testarea.
- [ ] `RulesEditor` și „Interacțiune nouă” doar în acest mod.

### Pasul 8 — Documentație și verificare ✅ criteriu: CI verde, QA bifat
- [ ] ADR nou `docs/decisions/0033-moduri-de-lucru.md` (deciziile din 3.x).
- [ ] `PLAN.md` (Etapa 5 bifată), `CLAUDE.md` (o notă despre `workMode`, `ModeSwitch`, `modes` pe comenzi),
      `docs/qa-checklist.md` (secțiunea „Moduri de lucru”).
- [ ] Capturi în cele 3 moduri (dark + light), web (Playwright pe https://eddynn383.github.io/Aion/ după publicare).

## 5. Decizii luate implicit (de confirmat cu utilizatorul; schimbările sunt mici)

1. Scurtăturile modurilor: **⌥1 / ⌥2 / ⌥3**.
2. În Animare, Move / Rotate / Scale pe obiecte **scriu keyframe-uri automat** (auto-key), nu mută modelul.
3. În Interacțiune, unealta Interact e activă **doar în testare**; în editare, clicurile aleg obiecte.
4. Palette arată **doar comenzile modului curent** (plus cele comune); o comandă din alt mod nu e accesibilă fără
   schimbarea modului.
5. Modul **nu se salvează în fișier**; aplicația repornește în ultimul mod folosit.
6. Închiderea timeline-ului în Animare doar îl ascunde; modul rămâne.
7. Scenele (vederile salvate) sunt disponibile în toate modurile.

## 6. Riscuri

- **Auto-key** atinge uneltele din `core/` (callback în `ToolContext`); trebuie teste ca uneltele să se comporte la fel
  în Modelare.
- **Scurtăturile**: ⇧/litere simple sunt multe; dezactivarea după mod trebuie testată (Preferințe → Scurtături arată
  modul fiecărei comenzi? — de adăugat o coloană „Mod”).
- **Web**: verifică toolbar-ul nou la lățimi mici (viewport-ul din browser e mai îngust).
- Testele prin script (`AION_STARTUP`) care pornesc unelte de modelare trebuie să fie în modul Modelare (implicit);
  profilul de test păstrează `workMode` — scripturile trebuie să pornească cu `mode:model` dacă depind de el.

## 7. Cum se verifică (comenzi)

```bash
make build && ./build/mac-debug/tests/aion_tests          # toate testele
python3 tools/check_shader_blocks.py                       # dacă s-au atins shaderele
S=/tmp/aion-shots; mkdir -p $S
AION_SCREENSHOT_DELAY_MS=4000 AION_SCREENSHOT=$S/model.png AION_STARTUP="mode:model;wait:500" \
  /usr/bin/perl -e 'alarm 60; exec @ARGV' build/mac-debug/app/Aion.app/Contents/MacOS/Aion
# la fel cu mode:animate, mode:interact; deschiderea unui dropdown: clicks:<x>,<y> pe săgeata grupului
```
