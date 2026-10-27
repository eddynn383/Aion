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
- [~] Pipeline de randare cu pași separați (`render/passes`): adâncime + normale → SSAO/GTAO → iluminare → outline → TAA → tonemapping — deocamdată: umbre → fețe → sol → grid → contur → transparente → linii
- [~] Randare în spațiu liniar, HDR intern, **tonemapping ACES**, ieșire sRGB corectă (și Display P3 pe Mac) — fără Display P3
- [ ] **Iluminare din mediu (IBL)** din HDRI, cu câteva medii incluse (studio, exterior, înnorat)
- [~] **Umbre moi**: cascaded shadow maps + PCF/PCSS, soarele poziționat după locație, dată și oră — o singură hartă + PCF; soarele după oră, lună și latitudine
- [ ] **SSAO / GTAO** cu filtrare care păstrează muchiile
- [ ] **TAA** când camera stă pe loc; calitate adaptivă în timpul navigării
- [~] Materiale **PBR** (albedo, roughness, metallic, normal map), cu bibliotecă de materiale incluse — fără normal map și texturi
- [~] Panou Materials modern: grilă de preview-uri randate (sfere), căutare, drag & drop pe fețe — fără drag & drop
- [x] **Paint Bucket (B):** aplicare pe față; Option = eyedropper
- [x] Coordonate UV pentru texturi pe fețe (proiecție planară), poziționarea texturii _(Faza 24, ADR 0024)_
- [x] Stiluri de afișare: Wireframe, Hidden Line, Shaded, Shaded with Textures, X-Ray, plus un stil **„Clay”** (alb mat cu AO, pentru prezentări) — „with Textures” vine cu texturile
- [x] Profiluri (muchii exterioare îngroșate), ceață atmosferică
- [ ] Preset-uri de calitate în Preferințe: *Performanță*, *Echilibrat*, *Calitate*

**Gata când:** casa din faza 5, texturată, arată vizibil mai bine decât același model în SketchUp (comparație de capturi), cu 60 fps în timpul navigării pe un MacBook Air M1.

---

### Faza 9 – Unelte suplimentare
- [x] **Tape Measure (T):** măsurare + linii și puncte de construcție (guides)
- [x] **Protractor:** linii de construcție la unghi
- [x] **Dimensions** și **Text** (adnotări în model)
- [x] **Section Plane:** secțiuni prin model
- [x] **Intersect Faces:** cu modelul sau cu selecția
- [x] Booleene pe solide cu **Manifold**: Union, Subtract, Intersect (numai pentru grupuri și componente „solide”)
- [x] **Scenes:** salvarea pozițiilor de cameră, cu tranziții animate

---

### Faza 10 – Finisare macOS și distribuție (v1.0 macOS)
- [x] Meniul nativ macOS (App menu, Preferences cu Cmd+,), Dark Mode, scurtături standard — `shell/AppMenuBar.qml`, ADR 0012
- [x] Iconița aplicației, ecran „About”, asociere cu fișierele `.aion` (Info.plist + QFileOpenEvent + linia de comandă)
- [~] Performanță: modele cu ≥ 100.000 de fețe; profilare cu Instruments; BVH pentru picking și inferență — deocamdată test pe cutii la picking
- [~] Preferințe: unități, precizie, temă, latitudine (⌘,) — lipsesc scurtăturile personalizabile
- [ ] Build **Universal** (arm64 + x86_64) — cere un Qt universal (vezi ADR 0012)
- [x] `macdeployqt`, **semnare** (Developer ID), **notarizare** Apple, DMG — `scripts/package-macos.sh` / `make package` (semnarea reală cere certificatul tău)
- [ ] Raportare crash-uri (opțional)
- [x] Ghiduri animate la hover lung pe unelte și turul uneltelor (ADR 0013)
- [x] Testare manuală cu o listă de scenarii (`docs/qa-checklist.md`)

**Gata când:** DMG-ul semnat și notarizat se instalează și rulează pe un Mac curat (Apple Silicon și Intel).

---

### Faza 11 – Windows
- [x] Preset-uri CMake `win-debug` / `win-release` (MSVC, Ninja)
- [ ] Backend RHI Direct3D 11/12; verificarea shaderelor (`qsb` generează HLSL)
- [x] Mapare scurtături: Cmd → Ctrl, Option → Alt (textele de ajutor adaptate)
- [ ] High-DPI pe Windows (scalare fracționară 125%, 150%)
- [x] CI pe `windows-latest` (build + teste verzi)
- [~] `windeployqt`, installer Inno Setup, semnare opțională — `scripts/package-windows.ps1`, netestat încă pe o mașină Windows

**Gata când:** aceleași teste trec pe Windows, iar installer-ul semnat rulează pe Windows 10 și 11.

---

### Faza 12 – Web
- [x] Compilarea `aion_core` cu **Emscripten** (ar trebui să meargă fără modificări datorită regulii „zero Qt în core”)
- [x] Variantă A: **Qt for WebAssembly** pentru întreaga aplicație (Qt Quick + backend WebGL). Cel mai puțin cod nou; efectele grafice scumpe pot fi reduse pe web
- [ ] Variantă B (dacă A e prea grea sau lentă): UI web nativ (TypeScript) + `aion_core` în WASM, cu randare WebGL/WebGPU
- [x] Decizia A vs. B documentată într-un ADR (0015; de reconfirmat cu măsurători), după un prototip de măsurare (dimensiune, timp de încărcare, fps)
- [x] Salvare și încărcare prin selectorul browserului și download; autosalvarea în QSettings (persistent)
- [~] CI cu Emscripten (aplicația pornește în Chromium); deploy pe GitHub Pages pregătit, manual (`pages.yml`)

**Gata când:** casa din faza 5 se poate modela complet în browser (Chrome, Safari, Firefox).

---

## 6b. Etapa 2 – organizare, animații, interacțiuni (cerințele din 2026-10-27)

Ordinea ține cont de dependențe: întâi schimbările mici de UI, apoi modelul de organizare (pe el se sprijină
scenele, animațiile și interacțiunile), apoi animațiile (interacțiunile le folosesc), apoi camerele și luminile.

### Decizii de organizare (valabile pentru toată etapa)
- **Formă** = geometrie liberă (fețe, muchii). **Obiect** = geometrie încapsulată, cu transformare proprie
  (azi „grup” cu geometrie sau „obiect” cu copii); copiile legate rămân („Duplicare legată”).
  **Grup** = colecție de obiecte și alte grupuri, ca în Blender: modul principal de organizare, apare ca folder în
  Outliner și se mută împreună. **Tag** = etichetă opțională, transversală: o față, o muchie, un obiect sau un grup
  poate avea un tag, iar elemente din grupuri diferite pot avea același tag (de exemplu „Pereți” pe pereții din
  toate camerele). Tag-urile ascund, afișează și selectează.
- Comenzi: ⌘G = Creează obiect (din forme), ⌥⌘G = Grupează (obiecte), ⌥D = Duplicare legată.
- Fișierele vechi se migrează: un grup cu geometrie devine obiect; un „obiect” cu copii rămâne obiect cu copii legate.
  Formatul `.aion` trece la versiunea 2 (cu migrare automată și test).

### Faza 13 – Toolbar, obiecte primitive, Materiale
- [x] Toolbar: un grup „Desen” (Line, Arc, Rectangle, Circle, Polygon); Eraser singur; Paint Bucket scos din toolbar
      (îl pornește alegerea unui material)
- [x] Grup nou „Obiecte”: Cub, Sferă, Con, Cilindru — click pe sol (sau pe o față), trage pentru mărime, VCB pentru
      dimensiuni; rezultatul e direct un obiect (core: generatoare de primitive, cu teste)
- [x] Panoul Materiale: click pe un material → unealta Paint Bucket activă, cursor găleată; grila de culori umple
      lățimea panoului

### Faza 14 – Pagina de Preferințe
- [x] Pagină întreagă (nu popover), cu categorii în stânga: General, Unități, Salvare, Aspect, Viewport, Navigare,
      Grid și snap, Ghiduri, Scurtături
- [x] Unități (unitate, precizie, format), autosalvare (pornit/oprit, interval 1–30 min, câte copii de rezervă)
- [x] Temă: Sistem / Light / Dark + temă personalizată (accent, fundaluri, panouri, viewport, grid, muchii, fețe),
      salvată și resetabilă; tokenii din `Theme.qml` devin suprascriibili
- [x] Viewport: anti-aliasing (MSAA), calitatea umbrelor, limită de fps / economisire baterie, afișarea fps
- [x] Scurtături: lista completă (personalizarea vine mai târziu)

### Faza 15 – Panoul din stânga: acordeon; Outliner unit cu Tag-urile
- [x] Secțiuni acordeon (Outliner, Materiale, Scene), redimensionabile, starea păstrată
- [x] Modelul de organizare de mai sus (core + migrare + teste): grupuri ca foldere, obiecte, forme; tag pe fețe,
      muchii, obiecte, grupuri
- [x] Outliner ca în Blender: arbore grupuri → obiecte, ochi (vizibil), lacăt, drag & drop între grupuri,
      redenumire; sub el, lista tag-urilor (vizibil, „Selectează tot ce are tag-ul”, culoare)
- [x] Inspector: tag-ul selecției (inclusiv pentru fețe și muchii)

### Faza 16 – Scene = stări ale modelului
- [x] O scenă reține camera, stilul, tag-urile vizibile, obiectele ascunse și **pozițiile obiectelor**
- [x] Aplicarea unei scene: tranziție a camerei și a obiectelor (cu undo); „Actualizează scena” recapturează tot
- [x] Redarea și exportul animației scenelor se mută în Timeline (Faza 17); scenele nu mai au legătură cu animațiile

### Faza 17 – Timeline și keyframe-uri (animații salvate)
- [x] Core: `Animation` (nume, durată, fps) cu piste pe obiecte (poziție, rotație, scară, culoarea materialului) și
      o pistă a camerei; keyframe-uri cu easing; evaluare la un moment dat (cu teste)
- [x] Bara de timeline (ca în Figma/Blender): riglă, playhead, play/pauză/buclă, pistele selecției cu romburi,
      „◆ Keyframe” și auto-key, mutarea keyframe-urilor prin tragere
- [x] Animații multiple, salvate în `.aion`; exportul video (MP4) din timeline
- [x] O animație poate fi pornită dintr-o interacțiune (Faza 18)

### Faza 18 – Interacțiuni cu blocuri (puzzle) și Modul Interacțiune
- [x] Core: reguli pe obiect: eveniment (click, click dreapta, dublu-click, hover intrare/ieșire, apăsare lungă,
      drag, tastă) → secvență de acțiuni înlănțuite (mută la/cu, rotește, scalează, ascunde/arată/comută,
      schimbă materialul, pornește o animație, mergi la scenă, așteaptă, alternează)
      _(2026-10-27, ADR 0021; „repetă”, condiții pe stări arbitrare și culoarea simplă rămân pentru mai târziu)_
- [x] Editor vizual cu blocuri tip puzzle (ca Sketchware/Scratch): paletă colorată pe categorii, blocuri trase și
      îmbinate vertical, parametri în bloc _(balama și glisare rămân prescurtări în Inspector)_
- [x] Modul Interacțiune: modelul nu mai poate fi editat; la hover pe un obiect cu interacțiuni apare cursorul
      evenimentului disponibil (mână, săgeată de drag etc.); ieșire cu Esc _(F5 sau ▶ din bara viewport-ului)_

### Faza 19 – Camere și lumini ca obiecte
- [x] Camere: transformare, FOV, proiecție, planuri de tăiere; „Privește prin cameră”; pot fi folosite în scene și
      pe timeline _(2026-10-27, ADR 0022: scenele / cheile se iau din vederea prin cameră)_
- [x] Lumini: punctuală, spot, direcțională, de suprafață; culoare, intensitate, rază, con
- [x] Randare: mai multe lumini în shader (până la 8); gizmo-uri în viewport, în Outliner și Inspector, salvate în `.aion`
- [ ] Umbre pentru o lumină spot (a doua hartă de umbre); mutarea luminilor trăgând de gizmo

### Faza 20 – Import `.skp` (Faza 7b)
- [ ] SDK-ul SketchUp (Trimble) se descarcă de pe developer.sketchup.com cu un cont și acceptarea licenței — pasul
      tău; licența se verifică de tine (ADR 0007)
- [ ] Modulul `plugins/skp` se construiește doar cu `AION_SKP_SDK_DIR`; restul ca în Faza 7b

### Setări rămase pentru Inspector, în ordinea priorității
1. Muchii: lungime editabilă, soft / netedă / ascunsă (unghi de netezire)
2. Selecție multiplă: proprietățile comune („mixt” unde diferă), totaluri, aliniere și distribuire
3. Cantitățile selecției: arie, volum, „Solid ✓/✗” cu motivul
4. Mobilier: materialul plăcii, direcția fibrei, cantul pe laturi (alimentează lista de debitare)
5. Atribute personalizate (cheie–valoare), incluse în raportul de cantități
6. Ghidaje, cote, note și planuri de secțiune selectabile, cu proprietăți în Inspector
7. Fără selecție: informațiile modelului (autor, descriere, locație), statistici
8. Arce și cercuri: raza și numărul de segmente
9. ~~Texturi: poziționare~~ — făcut în Faza 24 (Inspector + unealta Poziționează textura)
10. Vizibilitate avansată: umbre proiectate/primite, vizibil doar în anumite scene

---

## 6c. Etapa 3 – modelare avansată, conținut și extensibilitate (cerințele din 2026-10-27)

Din `docs/feature-ideas.md`: array liniar și polar, mirror / flip, soften / smooth, bibliotecă de obiecte, obiecte
dinamice, texturi și UV, plugin-uri / scripturi. Ordinea: întâi operațiile simple pe geometrie (de care depind
celelalte), apoi texturile (le folosesc obiectele din bibliotecă), obiectele dinamice, biblioteca și la final
scripturile, care expun tot ce există deja.

### Faza 21 – Mirror / Flip
- [x] Core: oglindirea selecției (instanțe și geometrie liberă) față de planul care trece prin centrul selecției,
      perpendicular pe axa roșie / verde / albastră; fețele rămân orientate corect (normalele inversate la nevoie)
- [x] Comenzi „Oglindește pe X / Y / Z” (meniul contextual, palette, Inspector), cu undo; teste _(ADR 0023)_

### Faza 22 – Array liniar și polar
- [x] Core: copii ale selecției (instanțe legate sau grupuri) pe o direcție cu pas sau distanță totală, ori în jurul
      unei axe (centru, unghi total sau pas, rotirea copiilor da/nu)
- [x] Panou „Array” cu previzualizare în viewport (număr, distanță / unghi, axă), aplicat ca un singur pas de undo
- [x] `x3` / `/3` după Move rămân ca scurtătură

### Faza 23 – Soften / Smooth
- [x] Core: muchiile dintre fețe cu unghi sub prag devin soft (și netede); invers pentru unghiuri peste prag
- [x] Panou cu slider de unghi (0–180°), „Soft pentru coplanare”, aplicat pe selecție, cu undo _(soft = ascunsă +
      umbrire netedă, un singur flag)_

### Faza 24 – Texturi și UV
- [x] Materialul poate avea o imagine (PNG / JPG, salvată în arhiva `.aion`), cu mărime reală (mm pe repetiție)
- [x] UV implicit: proiecție plană pe fiecare față (după normală), la scara texturii; transformare pe față
      (deplasare, rotație, scară) pentru „Position Texture” _(ADR 0024)_
- [x] Shader: eșantionare din texturi (array de texturi), cu mipmap-uri
- [x] Biblioteca de materiale primește texturi procedurale (lemn, cărămidă, parchet, beton, gresie) — fără imagini
      binare în repo; import de imagine proprie din editorul de material
- [x] Poziția texturii pe fețele selectate (Inspector); export glTF / OBJ cu UV și imagini
- [x] Poziționarea texturii direct pe față, în viewport (unealta „Poziționează textura”: trage = mută, Shift = rotește,
      ⌘ = scalează, în jurul punctului apăsat) și textura pe verso _(2026-10-27)_

### Faza 25 – Obiecte dinamice (parametrice)
- [x] Core: o definiție poate avea un generator (`door`, `window`, `table`, `shelf`, `cabinet`, `stairs`) și parametri
      numerici; schimbarea unui parametru regenerează geometria (undo), copiile legate se schimbă împreună _(ADR 0025)_
- [x] Inspector: secțiunea „Parametri” cu câmpurile generatorului (lățime, înălțime, grosime, număr de trepte…)
- [x] Salvate în `.aion` (generatorul și parametrii); „Desface” le transformă în geometrie obișnuită

### Faza 26 – Bibliotecă de obiecte
- [x] Secțiune nouă în stânga cu categorii, căutare și miniaturi _(ADR 0026; plante: copac, brad, tufă; oameni)_
- [x] Conținut: obiectele dinamice din Faza 25 + fișiere `.aion` din dosarul bibliotecii utilizatorului
- [x] Drag & drop în viewport (se așază pe suprafața de sub cursor sau pe sol) sau click → în centrul vederii
- [x] Previzualizarea obiectului (cutia lui, la mărimea reală) care urmează cursorul în timpul tragerii
- [x] „Salvează în bibliotecă” pentru obiectul selectat

### Faza 27 – Plugin-uri și scripturi
- [x] JavaScript (QJSEngine, merge și pe Web): API peste model, selecție, comenzi, geometrie (`box`, `object`,
      `line`, `face`, `move`, `rotate`, `material`…), mesaje _(ADR 0027; fără dialoguri interactive)_
- [x] Consolă de scripturi (editor + rezultat); fiecare rulare e un singur pas de undo, o eroare anulează tot
- [x] Plugin-uri: dosar cu `manifest.json` + `main.js`; comenzile lor apar în palette
- [x] Exemple incluse (scară în spirală, gard, biblioteci, grilă) și documentația API (`docs/scripting.md`)

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
