# Plan de implementare – Modelator 3D (clonă inspirată de SketchUp)

> **Nume:** `Aion` — „modelează ce rămâne” (vezi `docs/decisions/0004-nume-aion.md`).
> Nu folosim numele, logo-ul sau iconițele SketchUp, iar formatul nostru e `.aion`, nu `.skp`.
> Fișierele `.skp` pot fi doar **importate**, prin SDK-ul oficial SketchUp (Faza 7b, ADR 0007).

Acest document este planul de referință al proiectului. Îl urmăm fază cu fază.
Fiecare sarcină are o căsuță de bifat, iar fiecare fază are **criterii de „gata”**.
Nu trecem la faza următoare până nu sunt îndeplinite criteriile fazei curente.

**Legendă status:** `[ ]` de făcut · `[~]` în lucru · `[x]` gata

---

## 1. Obiective

1. Modelare 3D directă, rapidă și intuitivă: desenezi linii, se formează fețe, tragi de ele cu Push/Pull.
2. **macOS întâi**, apoi **Windows**, apoi **Web**, cu **același nucleu de cod**.
3. Geometrie robustă: modelul nu se „strică” după operații repetate.
4. Undo/redo pentru orice operație, de la început.

### Ce NU facem (cel puțin în primele versiuni)
- Randare fotorealistă, animații, plugin-uri Ruby, colaborare în timp real.
- Export `.skp` și citirea lui fără SDK-ul oficial (formatul e închis). Importul prin SDK e în Faza 7b.
- Modelare parametrică/NURBS (rămânem la geometrie poligonală, ca SketchUp).

---

## 2. Stack tehnic (confirmat)

| Strat | Tehnologie |
|---|---|
| Limbaj | **C++20** |
| UI | **Qt 6 (≥ 6.7), Qt Quick / QML**, cu design system propriu (vezi secțiunea 5). Nu folosim Qt Widgets: aspectul lor e „clasic” și greu de modernizat |
| Randare | **Qt RHI** (`QQuickRhiItem`, viewport-ul integrat direct în scena QML): Metal pe macOS, Direct3D 11/12 pe Windows, WebGL pe Web |
| Fonturi | **Inter** pentru UI, **Geist Mono** pentru valori numerice (ambele sub licența OFL, incluse în aplicație) |
| Iconițe | **Lucide** (licență ISC), SVG, plus iconițe proprii pentru uneltele 3D în același stil |
| Matematică | **glm** (vectori, matrici); `double` pentru model, `float` doar pentru GPU |
| Triangulare | **earcut.hpp** |
| Booleene (mai târziu) | **Manifold** |
| Fișiere | **nlohmann/json** (format propriu), **tinygltf** (export glTF), OBJ (scris de noi) |
| Build | **CMake + Ninja**, `CMakePresets.json` |
| Dependențe | **vcpkg** (manifest mode, `vcpkg.json`); Qt instalat separat |
| Teste | **Catch2 v3** |
| Calitate | clang-format, clang-tidy, AddressSanitizer + UBSan în build-ul de debug |
| CI | GitHub Actions: macOS (faza 0), Windows (faza 11), Emscripten (faza 12) |

---

## 3. Arhitectură

```
┌──────────────────────────────────────────────────────────┐
│ app/      Qt Quick/QML: shell, panouri, toolbar, VCB     │  depinde de render + core
├──────────────────────────────────────────────────────────┤
│ render/   Qt RHI: viewport, shadere, buffere GPU, picking│  depinde de core
├──────────────────────────────────────────────────────────┤
│ core/     C++ pur – FĂRĂ Qt                              │  nu depinde de nimic
│   math · geom · model · inference · tools · commands · io│
└──────────────────────────────────────────────────────────┘
```

**Reguli de arhitectură (obligatorii):**
1. `core/` nu include niciun header Qt. Se compilează și se testează singur.
2. Uneltele (Line, Push/Pull etc.) trăiesc în `core/tools` și primesc **evenimente abstracte**
   (`PointerEvent`, `KeyEvent`, raza din cameră). Nu primesc evenimente Qt.
   Astfel aceeași logică merge pe orice platformă.
3. Orice modificare a modelului trece printr-o **Comandă** (Command pattern), ca să funcționeze undo/redo.
4. `render/` citește modelul și ascultă notificări de schimbare. Nu modifică niciodată modelul.

### Structura directoarelor

```
aion/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── PLAN.md
├── core/                # headere publice în core/include/aion/core/<modul>/, surse în core/src/
│   ├── math/          # Vec3d, Plane, Ray, Transform, toleranțe
│   ├── geom/          # topologie: Vertex, Edge, Loop, Face; operații Euler
│   ├── model/         # Model, Entity, Group, ComponentDefinition/Instance, Tag, Material
│   ├── inference/     # motorul de snap/inferență
│   ├── tools/         # Select, Line, Rectangle, Circle, PushPull, Move, Rotate, ...
│   ├── commands/      # Command, CommandStack (undo/redo), tranzacții
│   └── io/            # format propriu (.aion), export OBJ/glTF
├── render/
│   ├── Viewport.*     # QQuickRhiItem
│   ├── Camera.*       # orbit/pan/zoom, perspectivă/ortografic
│   ├── SceneBuffers.* # mesh-uri GPU generate din model
│   ├── passes/        # G-buffer, SSAO, umbre, outline selecție, TAA, tonemapping
│   ├── shaders/       # .vert/.frag → compilate cu qsb
│   └── Picking.*      # selecție față/muchie/vârf sub cursor
├── app/
│   ├── main.cpp
│   ├── bridge/        # expunerea core-ului către QML (modele, comenzi, unealta activă)
│   └── qml/
│       ├── theme/     # Theme.qml: tokeni de culoare, spațiere, raze, tipografie, animații
│       ├── components/# Button, IconButton, Input, NumberField, Tooltip, Popover, Panel, Tabs...
│       ├── shell/     # AppWindow, TitleBar, Toolbar, Inspector, LayersPanel, StatusBar
│       ├── overlays/  # CommandPalette (Cmd+K), MeasurementsBox (VCB), context menu
│       └── Main.qml
├── resources/
│   ├── fonts/         # Inter, Geist Mono
│   ├── icons/         # SVG (Lucide + iconițe proprii)
│   └── hdri/          # medii HDR pentru iluminare
├── tests/
│   ├── core/          # teste unitare pentru geometrie, unelte, comenzi, io
│   └── fixtures/      # modele de test
└── docs/
    └── decisions/     # ADR-uri (decizii de arhitectură, câte un fișier)
```

---

## 4. Concepte cheie ale nucleului

### 4.1 Unități și toleranțe
- Unitate internă: **milimetru**, stocat ca `double`.
- Toleranță de coincidență: `1e-3 mm` pentru puncte, `1e-9` pentru unghiuri și paralelism (pe vectori normalizați).
- Toate comparațiile trec prin funcții din `core/math/Tolerance.h`. Nu folosim `==` pe `double`.
- În UI afișăm mm, cm, m sau in/ft, după setările documentului.

### 4.2 Topologie: non-manifold, nu half-edge clasic
Geometria în stil SketchUp poate avea **muchii libere** (fără față) și **muchii cu mai mult de 2 fețe**.
Un half-edge clasic nu le suportă, așa că folosim o structură de tip **radial-edge simplificată**:

```
Vertex   { id, position }
Edge     { id, v0, v1, uses: [EdgeUse] }        // 0..n utilizări
EdgeUse  { edge, loop, reversed }                // o muchie folosită într-o buclă
Loop     { face, uses: [EdgeUse] (ordonate) }    // ciclu închis
Face     { id, outerLoop, innerLoops[], plane, materialFront, materialBack }
```

- Entitățile au **ID-uri stabile** (`uint64_t`) și sunt stocate în pool-uri (`SlotMap`), nu prin pointeri bruti.
  Asta simplifică undo, salvarea și referințele din UI.
- Operațiile de nivel jos sunt **operații Euler** (adaugă/șterge vârf, muchie, față; împarte o muchie; împarte o față).
  Comenzile de nivel înalt se compun numai din ele.
- `Model::validate()` verifică invarianții: bucle închise, fețe planare, fără muchii duplicate,
  vârfuri unice în toleranță. Se rulează în toate testele și în build-ul de debug după fiecare comandă.

### 4.3 Comportamentul „lipicios” al geometriei
- O linie nouă care atinge sau intersectează o muchie existentă **o împarte** în punctul de intersecție.
- Când un ciclu de muchii **coplanare** se închide, se creează automat o față.
- O față desenată în interiorul altei fețe coplanare devine **gaură** (inner loop) sau o împarte.
- Geometria din interiorul unui **Grup/Componentă** nu interacționează cu cea din afară.

### 4.4 Comenzi și undo/redo
```cpp
class Command {
  virtual void apply(Model&) = 0;
  virtual void revert(Model&) = 0;
  virtual std::string name() const = 0;   // "Push/Pull", "Line", ...
};
```
- `CommandStack` cu `undo()`, `redo()`, `beginTransaction()` / `commit()` / `abort()`.
- Implementare inițială: fiecare comandă înregistrează **diff-ul** operațiilor Euler (entități create, șterse, modificate).
  Revert-ul aplică diff-ul invers.
- O unealtă activă (de exemplu Move în timpul tragerii) lucrează într-o tranzacție **deschisă**.
  La Esc se face `abort()`, la click final `commit()`.

---

## 5. Direcție vizuală și UI

**Țintă:** o aplicație care arată ca un instrument modern (Claude app, Figma, Vercel), nu ca un CAD clasic.
Interfața e discretă și lasă modelul în prim-plan, iar viewport-ul arată vizibil mai bine decât SketchUp.

### 5.1 Principii de UI
1. **Viewport pe tot ecranul.** Panourile sunt subțiri, plutitoare sau andocate, cu margini fine și fără rame grele.
2. **Puțin cromatism.** Fundaluri neutre, o singură culoare de accent, iar culoarea apare numai unde contează (selecție, inferență, stare).
3. **Tipografie clară.** Inter 13 px pentru UI, Geist Mono cu cifre tabulare pentru dimensiuni și coordonate.
4. **Ierarhie prin spațiu**, nu prin linii: grilă de 4 px, padding generos, colțuri rotunjite (6–10 px).
5. **Mișcare subtilă.** Tranziții de 120–200 ms cu easing, fără animații decorative. Respectăm „Reduce Motion” din macOS.
6. **Tastatura pe primul loc.** Command palette (Cmd+K) pentru orice comandă, scurtături vizibile în tooltip-uri.
7. **Dark mode implicit**, cu light mode complet. Urmăm automat tema sistemului.

### 5.2 Layout (inspirat de Figma)
```
┌──────────────────────────────────────────────────────────────────┐
│ ● ● ●   Aion · Casa.aion            [⌘K Caută comenzi]   Share │  ← titlebar integrat (fără bară nativă separată)
├────────┬─────────────────────────────────────────────┬───────────┤
│ Layers │                                             │ Inspector │
│ Outline│               VIEWPORT 3D                   │ Transform │
│ Tags   │                                             │ Material  │
│        │        ┌───────────────────────┐            │ Dimensiuni│
│        │        │ Lungime  2 500 mm  ⏎  │  ← VCB     │           │
│        │        └───────────────────────┘   plutitor │           │
│        │      ╭───────────────────────────────╮      │           │
│        │      │ ↖  ✎  ▭  ◯  ⇅  ✥  ⟳  ⤡  ✂  🪣 │      │           │ ← toolbar plutitor, jos (ca în Figma)
│        │      ╰───────────────────────────────╯      │           │
├────────┴─────────────────────────────────────────────┴───────────┤
│ Unități: mm · Snap: Endpoint · 128 fețe selectate                │  ← status bar discret
└──────────────────────────────────────────────────────────────────┘
```
- Panourile laterale se pot ascunde (Cmd+\), pentru un mod „focus” numai cu viewport.
- **Inspector contextual**: arată numai ce e relevant pentru selecție, ca în Figma.
- **Gizmo de navigare** (ViewCube modern și minimalist) în colțul viewport-ului.
- Tooltip-uri de inferență în stil „pill” (fundal semi-transparent, blur, text mic).

### 5.3 Design tokens (`Theme.qml`)
| Token | Dark | Light |
|---|---|---|
| `bg.app` | `#0A0A0B` | `#FAFAFA` |
| `bg.panel` | `#141416` | `#FFFFFF` |
| `bg.elevated` | `#1C1C1F` | `#FFFFFF` + umbră |
| `border.subtle` | `#FFFFFF` 8% | `#000000` 8% |
| `text.primary` | `#EDEDED` | `#171717` |
| `text.secondary` | `#A1A1AA` | `#52525B` |
| `accent` | `#3B82F6` (configurabil) | `#2563EB` |
| `radius` | 6 / 8 / 12 | idem |
| `spacing` | 4, 8, 12, 16, 24, 32 | idem |
| `motion` | 120 ms (hover), 180 ms (panouri) | idem |

Toate componentele folosesc **numai tokeni**. Nu scriem culori „hardcodate”, ca temele să rămână consecvente.

### 5.4 Calitatea grafică a viewport-ului (peste SketchUp)
| Element | SketchUp | Aion |
|---|---|---|
| Iluminare | lumină simplă, „plată” | **iluminare din mediu HDR (IBL)** + lumină solară, **tonemapping ACES**, randare liniară/sRGB corectă |
| Ocluzie | lipsă | **SSAO / GTAO**: colțurile și îmbinările capătă adâncime |
| Umbre | dure, opționale | **umbre moi** (cascaded shadow maps + PCF/PCSS), activate implicit |
| Muchii | linii de 1 px, cu aliasing | linii **anti-aliased, grosime constantă**, profil exterior mai gros, linii ascunse estompate |
| Anti-aliasing | MSAA de bază | **MSAA 4x + TAA** când camera stă pe loc (imagine „curată”) |
| Selecție | colorare albastră a fețelor | **contur luminos** (outline în screen-space) + tentă subtilă |
| Fundal | cer/sol plate | gradient fin sau HDRI estompat, **grid infinit** care se estompează la distanță |
| Materiale | culoare/textură simplă | **PBR** (albedo, roughness, metallic, normal map), cu preview în Inspector |
| Mișcare cameră | tăiată | **inerție lină** la orbit/zoom, tranziții animate între vederi |

**Buget de performanță:** 60 fps (120 fps pe ecranele ProMotion) în timpul navigării.
La modele mari, efectele scumpe (SSAO, TAA) scad automat în calitate cât camera se mișcă și revin când se oprește.
Fiecare efect se poate dezactiva din Preferințe (preset-uri: *Performanță*, *Echilibrat*, *Calitate*).

---

## 6. Faze de implementare

### Faza 0 – Fundația proiectului
- [x] Repository git, `.gitignore`, `.clang-format`, `.clang-tidy`, `.editorconfig`
- [x] `CMakeLists.txt` cu ținte separate: `aion_core` (lib), `aion_render` (lib), `aion_app` (exe), `aion_tests`
- [x] `CMakePresets.json`: `mac-debug` (cu ASan/UBSan), `mac-release`, plus `core-debug` (fără Qt)
- [x] `vcpkg.json` cu glm, nlohmann-json, catch2, earcut-hpp, tinygltf
- [x] Găsirea Qt 6 prin `CMAKE_PREFIX_PATH` = `$QT_ROOT_DIR` (documentată în README)
- [x] Aplicație Qt Quick care deschide o fereastră (shell schițat cu tema dark/light, verificat pe Mac cu Metal)
- [x] Teste Catch2 care trec (versiune, toleranțe)
- [~] GitHub Actions: build + teste pe `macos-latest` (scris; de verificat după primul push – proiectul nu e încă pe GitHub)
- [x] `README.md` cu pașii de instalare pe Mac (Xcode CLT, Homebrew, cmake, ninja, Qt, vcpkg)

**Gata când:** `cmake --preset mac-debug && cmake --build --preset mac-debug && ctest` merge pe un Mac curat urmând README-ul, iar CI e verde.

---

### Faza 1 – Shell UI, design system, viewport și cameră
- [x] `Theme.qml` cu toți tokenii din secțiunea 5.3, dark + light, comutare automată după sistem
- [x] Fonturile Inter și Geist Mono încărcate din resurse
- [x] Componente de bază: Button, IconButton, Input, NumberField, Tooltip, Popover, Panel, Tabs, Separator (+ Icon, Kbd, Toggle, Section)
- [x] Pagină internă de „storybook” (vizibilă numai în debug) care arată toate componentele în ambele teme
- [x] Shell: fereastră cu titlebar integrat (macOS: conținut sub bara de titlu, butoanele native păstrate și centrate), panouri laterale ascunzibile, toolbar plutitor, status bar
- [x] Command palette (Cmd+K) cu căutare fuzzy (inițial doar comenzi de vizualizare)
- [x] `Viewport` bazat pe `QQuickRhiItem` (backend Metal pe macOS), integrat în shell
- [x] Shadere de bază (compilate cu `qsb`): fețe cu iluminare simplă, linii, puncte
- [x] Fundal cu gradient, **grid infinit** care se estompează și cele 3 axe colorate (roșu X, verde Y, albastru Z, cu Z în sus)
- [x] `Camera`: orbit (butonul din mijloc sau Orbit tool), pan (Shift+mijloc), zoom spre cursor (scroll / trackpad pinch); unealta Zoom; cursoare dedicate pentru fiecare acțiune
- [~] Suport trackpad macOS: două degete = orbit/pan, pinch = zoom (scris; de testat manual)
- [x] Proiecție perspectivă și ortografică; vederi standard (Top, Front, Right, Iso) cu tranziții animate; „Zoom Extents”
- [x] Inerție lină la orbit și zoom
- [x] Gizmo de navigare (ViewCube minimalist) în colțul viewport-ului
- [x] Linii cu grosime constantă în pixeli (desenate ca quad-uri în shader, cu anti-aliasing analitic)
- [x] Anti-aliasing (MSAA 4x), suport ecrane Retina (device pixel ratio)

**Gata când:** fereastra arată ca în schița din 5.2 în ambele teme, afișează grid-ul și axele la 60 fps pe Retina, iar navigarea se simte naturală cu mouse-ul și cu trackpad-ul.

---

### Faza 2 – Nucleul geometric și comenzile
- [x] `core/math`: `Vec3d`, `Plane`, `Ray`, `Segment`, `Transform`, `Tolerance`, intersecții (segment-segment 3D, rază-plan, rază-triunghi, punct-în-poligon)
- [x] `SlotMap` și ID-uri stabile
- [x] Topologie: `Vertex`, `Edge`, `EdgeUse`, `Loop`, `Face`
- [x] Operații Euler + `Model::validate()` (în `geom::Mesh`, vezi ADR 0003)
- [x] `addEdge(a, b)` cu: îmbinarea vârfurilor coincidente, împărțirea muchiilor intersectate, detectarea ciclurilor coplanare închise și crearea fețelor (`geom::drawLine`)
- [x] Fețe cu găuri (inner loops), împărțirea unei fețe de o muchie care o traversează
- [x] Triangularea fețelor cu earcut, pentru randare
- [x] `Command`, `CommandStack`, tranzacții, undo/redo
- [x] Sistem de notificări `ModelObserver` (entități adăugate, modificate, șterse) (`geom::MeshObserver`)
- [x] **Teste unitare extinse:** pătrat → 1 față; pătrat împărțit de diagonală → 2 fețe; pătrat în pătrat → față cu gaură; undo/redo pe fiecare caz; `validate()` după fiecare pas
- [x] Teste de „fuzz”: secvențe aleatoare de operații + undo complet → modelul revine la starea inițială (10 000 de operații, `AION_FUZZ_SEED` pentru alte semințe)

**Gata când:** toate testele trec sub ASan/UBSan, inclusiv fuzz-ul cu ≥ 10.000 de iterații.
_(2026-10-26: ✓ local sub UBSan, iar în CI pe GitHub sub ASan + UBSan; pe acest Mac ASan nu pornește cu Apple clang 17 / macOS 26.)_

---

### Faza 3 – Randarea modelului, selecția, primele unelte
- [x] `SceneBuffers`: buffere GPU generate din model și actualizate incremental la notificări (triangularea se reface doar pentru fețele schimbate; buffer-ul GPU se reîncarcă întreg)
- [x] Fețe față/spate în culori diferite (neutru deschis / gri-albastru), muchii închise la culoare, anti-aliased
- [x] Selecție afișată cu **contur luminos** în screen-space + tentă subtilă de accent
- [x] `Picking`: ce față, muchie sau vârf e sub cursor (rază + toleranță în pixeli pentru muchii și vârfuri)
- [x] Interfață `Tool` în `core/tools` (onPointerDown/Move/Up, onKey, onCancel, overlay de desenat)
- [x] **Select:** click, Shift/Cmd pentru adăugare/scădere, dreptunghi stânga→dreapta (complet în interior) și dreapta→stânga (atinge), dublu-click = față + muchii, triplu-click = tot ce e conectat
- [x] **Line:** desenare continuă de segmente, Esc oprește
- [x] **Rectangle:** pe planul de sol sau pe o față existentă
- [x] **Eraser:** șterge muchii (și fețele care depind de ele)
- [x] Tasta Delete pe selecție, Cmd+Z / Cmd+Shift+Z
- [x] Toolbar plutitor cu iconițe în stil Lucide, tooltip cu numele și scurtătura; scurtături de tastatură (L, R, Space = Select, E)
- [x] Toate uneltele disponibile și din command palette
- [x] Highlight la hover și la selecție

**Gata când:** poți desena un dreptunghi, îl împarți cu o linie în două fețe, selectezi una, o ștergi, apoi dai undo, totul fără erori `validate()`.
_(2026-10-26: scenariul trece în `ToolTests` și prin input real în aplicație (`AION_STARTUP` cu `clickw:`), cu `validate()` după fiecare comandă.)_

---

### Faza 4 – Inferența (snap)
- [x] `InferenceEngine` în `core/inference`, care primește raza cursorului și contextul (ultimul punct, entități din apropiere)
- [~] Puncte: Endpoint (verde), Midpoint (cyan), On Edge (roșu), On Face (albastru), Intersection, Center (cerc) — Center vine cu cercurile (Faza 5)
- [x] Direcții: paralel cu axele X/Y/Z (linie colorată), paralel sau perpendicular pe o muchie (magenta)
- [x] Inferențe de tip „from point”: urmărirea unui punct de referință pe o axă (linie punctată)
- [x] Lock: Shift blochează inferența curentă; săgețile ←↑→ blochează pe o axă
- [x] Prioritizare și histerezis, ca snap-ul să nu „sară” între candidați
- [~] Indicii vizuale: marcaj colorat animat discret + tooltip „pill” („Endpoint”, „On Red Axis”) — marcajul nu e încă animat
- [x] **Measurements box (VCB):** câmp plutitor lângă cursor, în Geist Mono. În timpul desenării tastezi o lungime (`2500`, `2.5m`, `3',6"`) și Enter. Pentru dreptunghi: `3000,2000`
- [x] Dimensiuni „live” afișate pe muchie în timpul desenării (ca în Figma)
- [x] Teste unitare pentru fiecare tip de inferență pe scene sintetice

**Gata când:** poți desena exact o cameră de 4 × 3 m cu pereți perpendiculari numai din inferențe și VCB, fără să lovești manual punctele.
✓ Testul `Unelte: o cameră de 4 × 3 m numai din inferențe și Measurements box` și verificarea prin input simulat în aplicație (ADR 0005).

---

### Faza 5 – Push/Pull și transformări
- [x] **Push/Pull:** extrudarea unei fețe pe normală; creează fețele laterale; cu inferență pe adâncime (până la o altă față sau un alt punct); VCB pentru distanță; Ctrl/Option = creează față nouă (păstrează originalul)
- [x] Push/Pull până la anulare: dacă ajunge la fața opusă, materialul dispare și se face gaură (ex.: fereastră în perete)
- [x] Double-click Push/Pull = repetă ultima distanță
- [x] **Move (M):** mută selecția sau o muchie/un vârf (deformează geometria conectată); Option = copiere; `x3` sau `/3` în VCB = array
- [x] **Rotate (Q):** raportor pe plan cu inferență de plan; unghi din VCB; copiere radială
- [~] **Scale (S):** cu mânere, uniform sau pe axă, din colț sau din centru (Option) — deocamdată uniform: punct fix, referință, factor (VCB); din centru cu Option. Mânerele și scalarea pe axă rămân de făcut
- [x] **Offset (F):** offset al conturului unei fețe
- [x] **Circle (C), Arc (A), Polygon:** curbe aproximate cu segmente (24 implicit), marcate ca „curbă” pentru selecție și afișare netedă
- [ ] **Follow Me:** extrudarea unei fețe pe un traseu (opțional în această fază) — amânat
- [x] Fețe netede (smooth/soft edges) pentru suprafețe curbe

**Gata când:** poți modela o căsuță: pereți, acoperiș în două ape, o ușă și o fereastră tăiate prin Push/Pull, cu undo/redo complet pe tot istoricul.
✓ Testul `Faza 5, „gata când”` (fiecare stare revine identic la undo/redo) și aceeași căsuță construită în aplicație numai cu uneltele (ADR 0006).

---

### Faza 6 – Grupuri, componente, organizare
- [x] **Group:** încapsulează geometria, are transformare proprie; dublu-click intră în editare, Esc iese; restul modelului se estompează
- [x] **Component:** definiție + instanțe; editarea unei instanțe le modifică pe toate; „Make Unique”
- [x] Explode (dezgrupare)
- [x] **Tags (layere):** vizibilitate și culoare per tag
- [x] **Outliner:** arborele modelului (grupuri/componente), cu redenumire și vizibilitate
- [x] **Entity Info:** panou cu proprietățile selecției (lungime, arie, tag, material, nume) — materialele vin în Faza 8
- [~] Hide/Unhide, Lock — pentru grupuri și componente; fețele și muchiile simple nu se pot ascunde încă

**Gata când:** poți crea un scaun ca o componentă, îl copiezi de 4 ori, modifici unul și se actualizează toate.
✓ Testul `Faza 6, „gata când”` (plus Make Unique) și, în aplicație, intrarea în componentă cu restul modelului estompat (ADR 0008).

---

### Faza 7 – Fișiere și export
- [x] Format propriu `.aion`: arhivă ZIP cu `model.json` (topologie, entități, tag-uri, materiale), texturi și thumbnail; câmp `formatVersion` pentru migrări — materialele și texturile se adaugă în Faza 8
- [x] Salvare și deschidere, „documente recente”, marcaj „modificat” în titlul ferestrei
- [x] Autosalvare periodică și recuperare după crash
- [x] Export **OBJ** (+ MTL) și **glTF/GLB**
- [x] Import OBJ (ca grup)
- [x] Import **glTF/GLB** (păstrează ierarhia de noduri ca grupuri); calea fără licențe pentru modelele exportate din SketchUp
- [x] Export imagine PNG din viewport
- [x] Teste: round-trip salvare → încărcare → model identic (`validate()` + comparare)

**Gata când:** toate modelele de test trec round-trip-ul, iar exportul glTF se deschide corect într-un viewer extern (de exemplu Blender).
✓ `tests/core/IoTests.cpp` (round-trip exact, OBJ și GLB dus-întors); deschiderea în Blender rămâne de verificat de utilizator (ADR 0009).

---

### Faza 7b – Import `.skp` (opțional, numai desktop)
- [ ] **Verificarea licenței SketchUp C SDK** (developer.sketchup.com): permite folosirea într-un modelator independent?
      Ce atribuire cere? Se poate redistribui biblioteca cu aplicația? Decizia se trece în ADR 0007. Fără acord, faza nu începe.
- [ ] Modul separat `plugins/skp` (țintă `aion_skp`), construit numai dacă SDK-ul e găsit (`AION_SKP_SDK_DIR`); nu intră în `core/` și nici în Web
- [ ] Citire: fețe (cu găuri), muchii (soft/smooth), curbe, grupuri, componente (definiții + instanțe), tag-uri, materiale și texturi, scene (camera)
- [ ] Conversie în modelul nostru, cu `validate()` după import; ce nu se poate converti apare într-un raport („12 fețe neplanare au fost triangulate”)
- [ ] Comanda „Importă .skp…” (ca grup, cu undo), progres pentru fișiere mari
- [ ] Teste pe fișiere `.skp` de probă (create de noi, fără modele cu drepturi de autor)

**Gata când:** un model `.skp` cu grupuri, componente, tag-uri și texturi se importă corect (structură păstrată, `validate()` trece).

---

### Faza 8 – Grafică avansată, materiale și stiluri
- [ ] Pipeline de randare cu pași separați (`render/passes`): adâncime + normale → SSAO/GTAO → iluminare → outline → TAA → tonemapping
- [ ] Randare în spațiu liniar, HDR intern, **tonemapping ACES**, ieșire sRGB corectă (și Display P3 pe Mac)
- [ ] **Iluminare din mediu (IBL)** din HDRI, cu câteva medii incluse (studio, exterior, înnorat)
- [ ] **Umbre moi**: cascaded shadow maps + PCF/PCSS, soarele poziționat după locație, dată și oră
- [ ] **SSAO / GTAO** cu filtrare care păstrează muchiile
- [ ] **TAA** când camera stă pe loc; calitate adaptivă în timpul navigării
- [ ] Materiale **PBR** (albedo, roughness, metallic, normal map), cu bibliotecă de materiale incluse
- [ ] Panou Materials modern: grilă de preview-uri randate (sfere), căutare, drag & drop pe fețe
- [ ] **Paint Bucket (B):** aplicare pe față; Option = eyedropper
- [ ] Coordonate UV pentru texturi pe fețe (proiecție planară), poziționarea texturii
- [ ] Stiluri de afișare: Wireframe, Hidden Line, Shaded, Shaded with Textures, X-Ray, plus un stil **„Clay”** (alb mat cu AO, pentru prezentări)
- [ ] Profiluri (muchii exterioare îngroșate), ceață atmosferică
- [ ] Preset-uri de calitate în Preferințe: *Performanță*, *Echilibrat*, *Calitate*

**Gata când:** casa din faza 5, texturată, arată vizibil mai bine decât același model în SketchUp (comparație de capturi), cu 60 fps în timpul navigării pe un MacBook Air M1.

---

### Faza 9 – Unelte suplimentare
- [ ] **Tape Measure (T):** măsurare + linii și puncte de construcție (guides)
- [ ] **Protractor:** linii de construcție la unghi
- [ ] **Dimensions** și **Text** (adnotări în model)
- [ ] **Section Plane:** secțiuni prin model
- [ ] **Intersect Faces:** cu modelul sau cu selecția
- [ ] Booleene pe solide cu **Manifold**: Union, Subtract, Intersect (numai pentru grupuri și componente „solide”)
- [ ] **Scenes:** salvarea pozițiilor de cameră, cu tranziții animate

---

### Faza 10 – Finisare macOS și distribuție (v1.0 macOS)
- [ ] Meniul nativ macOS (App menu, Preferences cu Cmd+,), Dark Mode, scurtături standard
- [~] Iconița aplicației, ecran „About”, asociere cu fișierele `.aion` — iconița e gata (conceptul „Axe”, `resources/branding/`, `.icns` generat la compilare)
- [ ] Performanță: modele cu ≥ 100.000 de fețe; profilare cu Instruments; BVH pentru picking și inferență
- [ ] Preferințe: unități, precizie, scurtături personalizabile
- [ ] Build **Universal** (arm64 + x86_64)
- [ ] `macdeployqt`, **semnare** (Developer ID), **notarizare** Apple, DMG
- [ ] Raportare crash-uri (opțional)
- [ ] Testare manuală cu o listă de scenarii (`docs/qa-checklist.md`)

**Gata când:** DMG-ul semnat și notarizat se instalează și rulează pe un Mac curat (Apple Silicon și Intel).

---

### Faza 11 – Windows
- [ ] Preset-uri CMake `win-debug` / `win-release` (MSVC, Ninja)
- [ ] Backend RHI Direct3D 11/12; verificarea shaderelor (`qsb` generează HLSL)
- [ ] Mapare scurtături: Cmd → Ctrl, Option → Alt; navigarea cu mouse-ul e prioritară (fără trackpad)
- [ ] High-DPI pe Windows (scalare fracționară 125%, 150%)
- [ ] CI pe `windows-latest`
- [ ] `windeployqt`, installer (WiX sau Inno Setup), semnare cod (certificat Authenticode)

**Gata când:** aceleași teste trec pe Windows, iar installer-ul semnat rulează pe Windows 10 și 11.

---

### Faza 12 – Web
- [ ] Compilarea `aion_core` cu **Emscripten** (ar trebui să meargă fără modificări datorită regulii „zero Qt în core”)
- [ ] Variantă A: **Qt for WebAssembly** pentru întreaga aplicație (Qt Quick + backend WebGL). Cel mai puțin cod nou; efectele grafice scumpe pot fi reduse pe web
- [ ] Variantă B (dacă A e prea grea sau lentă): UI web nativ (TypeScript) + `aion_core` în WASM, cu randare WebGL/WebGPU
- [ ] Decizia A vs. B documentată într-un ADR, după un prototip de măsurare (dimensiune, timp de încărcare, fps)
- [ ] Salvare și încărcare prin File System Access API și download/upload; IndexedDB pentru autosalvare
- [ ] CI cu Emscripten; deploy pe hosting static

**Gata când:** casa din faza 5 se poate modela complet în browser (Chrome, Safari, Firefox).

---

## 7. Riscuri principale și cum le reducem

| Risc | Impact | Măsuri |
|---|---|---|
| Erori numerice în geometrie (fețe neplanare, muchii aproape coincidente) | Modele corupte | Toleranțe centralizate, `validate()` peste tot, teste fuzz, `double` în core |
| Topologia devine prea complexă | Blocaj în fazele 2–5 | Numai operații Euler la bază, teste pentru fiecare caz, ADR-uri |
| Inferența „se simte” greșit | UX slab | Iterații scurte, testare manuală frecventă, histerezis și prioritizare ajustabile |
| API-ul Qt RHI se schimbă între versiuni | Rescriere randare | Fixăm o versiune Qt, izolăm RHI în `render/` |
| Performanță pe modele mari | Aplicație lentă | Buffere incrementale, BVH, profilare din faza 3 |
| Portarea pe web | Întârzieri | Core fără Qt de la început, prototip timpuriu de compilare WASM a core-ului (după faza 2) |
| UI-ul nu arată „modern” în QML | Produs care pare vechi | Design system cu tokeni din faza 1, storybook intern, review vizual la fiecare fază |
| Efectele grafice costă prea mult | fps scăzut pe Mac-uri mai slabe | Calitate adaptivă, preset-uri, testare pe MacBook Air M1 ca minim |

---

## 8. Convenții de lucru

- **Cod:** C++20, `clang-format` obligatoriu, fără `new`/`delete` manual (RAII, `std::unique_ptr`), fără excepții în bucle fierbinți.
- **Denumiri:** `PascalCase` pentru tipuri, `camelCase` pentru funcții și variabile, `m_` pentru membri privați, namespace `aion::`.
- **Teste:** orice funcționalitate din `core/` vine cu teste. Bug-urile de geometrie primesc întâi un test care le reproduce.
- **Commit-uri:** mici, cu mesaj clar, de exemplu `core/geom: split face by crossing edge`.
- **Decizii importante:** câte un ADR scurt în `docs/decisions/NNNN-titlu.md`.
- **Ramuri:** `main` mereu compilabil și cu teste verzi; lucrul pe ramuri `feature/...`.

---

## 9. Ordinea imediată de lucru

1. Faza 0, integral.
2. Faza 1: viewport și cameră.
3. Faza 2: nucleul geometric, cea mai importantă și mai riscantă parte. Îi alocăm timp generos.
4. Mic prototip: `aion_core` compilat cu Emscripten (validează din timp drumul spre web).
5. Fazele 3 → 4 → 5: aici aplicația devine utilizabilă (**MVP**).

**MVP = fazele 0–5:** desenezi, folosești inferența, faci Push/Pull, transformi, ai undo/redo. Totul pe macOS.
