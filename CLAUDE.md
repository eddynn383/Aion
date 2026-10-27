# Aion – context pentru Claude

Modelator 3D nativ, inspirat de SketchUp, cu UI modern (stil Claude app / Figma / Vercel)
și grafică mai bună decât SketchUp. Numele: **Aion** (până la 2026-10-26 „Forma”; vezi ADR 0004).
Nu folosim numele sau logo-ul SketchUp; formatul nostru e `.aion`. Fișierele `.skp` se pot doar importa,
prin SDK-ul oficial, într-un modul opțional numai pentru desktop (ADR 0007, Faza 7b).
- Slogan: „Aion — modelează ce rămâne.” / „Aion — shape what lasts.”
- În cod: namespace `aion::`, header-e în `include/aion/`, ținte `aion_*`, module QML `Aion` / `Aion.Render`,
  variabile de mediu `AION_*`, fișiere `.aion`.

- **Comunicare cu utilizatorul: în limba română.**
- Planul complet și sursa de adevăr pentru ce urmează: [`PLAN.md`](PLAN.md). Bifează sarcinile în el pe măsură ce le termini.
- Decizii de arhitectură: `docs/decisions/` (ADR). Adaugă un ADR pentru orice decizie importantă nouă.

## Decizii deja luate (nu le redeschide fără motiv)
- Platforme: **macOS întâi**, apoi Windows, apoi Web, din același cod.
- Stack: **C++20, Qt 6 (≥ 6.9) cu Qt Quick/QML** (nu Qt Widgets), randare cu **Qt RHI** (`QQuickRhiItem`, Metal pe macOS),
  **CMake + Ninja + vcpkg**, **Catch2**, glm, earcut, Manifold (mai târziu), nlohmann-json, tinygltf.
- `core/` **nu include niciodată Qt** (ca să poată fi compilat și pentru WebAssembly).
- Geometrie non-manifold de tip radial-edge (muchii libere, muchii cu mai mult de 2 fețe), unitate internă mm, comparații numai prin `core/include/aion/core/math/Tolerance.h`.
- Undo/redo prin Command pattern, de la început.
- UI: design tokens în `app/qml/theme/Theme.qml`; componentele nu folosesc culori hardcodate. Dark mode implicit.

## Comenzi
```bash
make setup   # instalează uneltele (Xcode CLT, Homebrew, cmake, ninja, qt, vcpkg)
make build   # compilează (preset mac-debug, cu ASan + UBSan)
make test    # compilează + ctest
make run     # compilează + pornește aplicația
make reconfigure   # după schimbări în CMakePresets.json
```
Variabile necesare: `VCPKG_ROOT` (implicit `~/vcpkg`), `QT_ROOT_DIR` (implicit `$(brew --prefix qt)`).
Pe acest Mac, Homebrew e instalat în `~/homebrew`.

## Stadiu
- **Faza 0** verificată pe Mac (2026-10-26): `make setup`, `make test` și `make run` merg.
- Repo: <https://github.com/eddynn383/Aion> (urcat pe 2026-10-26; `main` e ramura principală).
- **ASan e blocat pe acest Mac** (Apple clang 17 + macOS 26.6: deadlock în `AsanInitInternal`, chiar și la un `int main(){}`).
  `cmake/AionOptions.cmake` testează la configurare dacă ASan pornește; dacă nu, folosește doar UBSan (cu avertisment).
- **Atenție la `timeout` și `perl` din `/usr/local/bin`:** sunt binare Intel, iar tot ce rulează sub el trece prin Rosetta
  (CMake vede `x86_64` și alege tripletul `x64-osx`). Pentru limite de timp folosește `/usr/bin/perl -e 'alarm N; exec @ARGV' ...` (calea completă).
- Verificare vizuală fără permisiuni de screen recording: `AION_SCREENSHOT=/cale.png [AION_THEME=light|dark] Aion.app/Contents/MacOS/Aion`
  salvează fereastra după randare și închide aplicația.
- Avertismente cunoscute, inofensive: `FindWrapOpenGL ... /usr/X11R6/lib` (XQuartz), și `ld: ... built for newer macOS version`
  pentru framework-urile Qt din Homebrew (deployment target-ul nostru e 13.0; contează abia la distribuție, Faza 10).

- **Faza 1** făcută și unită în `main` (2026-10-26). Navigarea cu mouse-ul și bara de titlu confirmate de utilizator;
  trackpad-ul (două degete / pinch) încă netestat de el.
- Unelte de verificare (variabile de mediu): `AION_SCREENSHOT`, `AION_SCREENSHOT_DELAY_MS`, `AION_THEME`,
  `AION_STORYBOOK=1`, `AION_STARTUP="view.top;palette:zoom;debug.benchmark"` (afișează fps în consolă).

- **Faza 2** făcută și unită în `main`: `core/math`, `geom::Mesh` (topologie, primitive Euler, înregistrarea
  schimbărilor, `validate()`), `geom::drawLine` / `drawPolygon` / `eraseEdge` cu recalcularea fețelor pe plan,
  triangulare earcut, `commands::CommandStack`. 62 de teste, inclusiv fuzz de 10 000 de operații (~40 s în debug).
  Designul: `docs/decisions/0003-nucleu-geometric.md`. Lipsește doar rularea sub ASan (vezi mai sus).

- Toolbar-ul are unelte grupate ca în Figma (`shell/ToolGroupButton.qml`, `components/Menu.qml`).
- **Faza 3** făcută pe `feature/faza-3`: `geom::Selection`, `view::pick` / `pickInRectangle`, `tools::Tool` +
  Select / Line / Rectangle / Eraser (în core, cu teste), `render::Document` (model + istoric + selecție + unealtă),
  contur de selecție în screen-space. Verificare prin input real: `AION_STARTUP="view.top;wait:700;tool.rectangle;clickw:x,y,z;..."`.
- Prototipul WebAssembly e în PR #1 (`feature/wasm-core`): core + teste compilate cu Emscripten și rulate în Node, în CI.

- **Faza 4** făcută pe `feature/faza-4` (pornită din `feature/faza-3`): `inference::Engine`, `units::parseLength`,
  Line/Rectangle cu inferență, lock-uri (Shift, săgeți), VCB, cote live. Designul: `docs/decisions/0005-inferenta-si-vcb.md`.
  Verificare: `AION_STARTUP="view.top;wait:900;tool.line;clickw:-2000,-1500,0;movew:-500,-1460,0;type:4m;key:enter;..."`
  (`movew:` = doar hover, `type:` = text în VCB, `key:enter|escape|left|right|up|down`). Punctele trebuie să fie în viewport.
- Fazele 3–4 și PR #1 (WebAssembly) unite în `main` pe 2026-10-26.

- **Faza 5** făcută pe `feature/faza-5`: Push/Pull (cu tăieri și găuri), Move/Rotate/Scale (cu copiere și array),
  Circle/Arc/Polygon (curbe, muchii soft, umbrire netedă), Offset. Designul: `docs/decisions/0006-push-pull-si-transformari.md`.
  Scale e doar uniform; Follow Me amânat. Atenție în scripturile `AION_STARTUP`: clicurile pe fețe ascunse lovesc altceva,
  iar după `view.extents` trebuie `wait:1500` (animația camerei).

- **Iconița** (conceptul „Axe”: trei axe RGB dintr-un nod alb, pe fundal închis): SVG-uri în `resources/branding/`
  (`aion-icon.svg` mare, `aion-icon-small.svg` pentru 16/32 px, `aion-mark.svg` fără fundal). `Aion.icns` se generează la
  compilare cu `tools/iconrender` (Qt Svg) + `iconutil`; nu se pun imagini binare în repo.

- **Faza 6** făcută pe `feature/faza-6`: `model::Model` (definiții, instanțe, tag-uri, undo pe structură),
  `Camera::inContext`, Make Group/Component, Explode, Make Unique, Outliner, Tag-uri, Entity Info, context de editare.
  Designul: `docs/decisions/0008-model-grupuri-componente.md`. Comenzi în palette: categoria „Organizare” (⌘G, ⇧⌘G).

- **Faza 7** pe `feature/faza-7`: `.aion` (ZIP + model.json), OBJ/glTF export și import, autosalvare + recuperare,
  documente recente, PNG. ADR 0009. Acțiuni de script: `save:`, `open:`, `export:`, `import:`, `png:`, `file.newNow`.

- **Faza 8** (parțial) pe `feature/faza-8`: materiale + Paint Bucket + panou Materiale, stiluri (clay, linii ascunse,
  wireframe, X-Ray), profile, ceață, ACES, umbre de la soare (oră/lună). ADR 0010. Rămân: SSAO, IBL, TAA, texturi/UV.
  Acțiuni de script: `sun:ora`, `style:nume`.

- **Faza 9** pe `feature/faza-9`: ghidaje, Tape Measure, Protractor, Dimension, Text, Section Plane, Intersect Faces,
  booleene (Manifold), Scene. ADR 0011.

- **Faza 10** pe `feature/faza-10`: meniul nativ (`shell/AppMenuBar.qml`), About, Preferințe (⌘,: unități, precizie,
  temă, latitudine), asocierea `.aion` (`app/macos/Info.plist.in`, QFileOpenEvent, argumente), `make package`
  (DMG în `dist/`), `docs/qa-checklist.md`. ADR 0012. Lipsesc build-ul Universal și semnarea reală (certificat).

- **Ghiduri animate** pe `feature/tur-unelte`: `ToolDemo.qml` (scene Canvas per unealtă), `ToolGuides.js` (texte),
  `RichTooltip` la hover lung (1,3 s), `ToolTour` (meniul Ajutor / palette). ADR 0013. Acțiuni de script:
  `moves:x,y` (hover în fereastră), `debug.demos` (galeria animațiilor).

- **Navigare precisă** (ADR 0016): orbită în jurul selecției / cursorului, zoom spre suprafață, planuri de tăiere
  adaptive + Clip start/end în Preferințe. Acțiune de script: `wheelw:x,y,z,trepte`.

- **Snap la grid**: `Engine::withGridSnap` (doar puncte libere și distanțe pe direcții), `autoGridStep` = regula din
  grid.frag, `ToolContext::gridStep`, Document `snapToGrid`/`gridStep`, pas fix desenat prin `params.z` în grid.frag.

- **Raport de cantități** (ADR 0017): `model/Quantities.h` (general + listă de debitare + CSV), `QuantitiesDialog.qml`,
  `DataTable.qml`; comanda `file.quantities` (Fișier). Acțiune de script: `debug.cutlist`.

- **Animații și interacțiuni** (ADR 0018): `view/SceneAnimation`, `Viewport::playScenes/exportScenes`,
  `AppInfo.encodeVideo` (`platform/VideoMac.mm`, AVFoundation; ffmpeg ca rezervă); `model::Behavior` pe instanțe,
  `model/Interaction`, `view::interactiveInstanceAt`, unealta `interact` (în viewport, ca navigarea).
  Acțiuni de script: `scene.add`, `exportvideo:cale.mp4`, `behavior:rotate,0,90`.

- **Bara de vizualizare** (`shell/ViewportControls.qml`, lângă gizmo, ca în Blender): stilurile ca iconițe
  (`resources/icons/style-*.svg`), umbre, secțiuni, proiecție, grid. Inspector-ul e doar pentru selecție.
  Rulările cu `AION_SCREENSHOT` / `AION_STARTUP` au profilul lor („Aion Test”): nu ating setările utilizatorului.

- **Faza 11** (Windows): CI verde pe MSVC (ADR 0014); installerul Inno Setup e scris, dar netestat pe o mașină reală.
- **Faza 12** (Web, ADR 0015): preset `wasm-app` (Qt 6.10 wasm + Emscripten 4.0.7, excepții JS, triplet
  `wasm32-emscripten-qt`); CI produce artefactul `aion-web`, care pornește în Chromium (verificat cu Playwright).

- **Faza 18** (ADR 0021): `model/Rules.h` (`Rule`, `Action`, `RuleRuntime`), Modul Interacțiune pe o copie a
  modelului (`Document::interactionMode`, F5, Esc), editorul `overlays/RulesEditor.qml` (+ `RulesActionBlock`,
  `RulesActionList`). Acțiuni de script: `rule:click,moveBy,0,0,800`, `rules.edit`, `mode.interaction`,
  `drags:x1,y1,x2,y2` (tragere în coordonatele ferestrei).

- **Faza 19** (ADR 0022): `model::CameraObject` / `model::Light`, până la 8 lumini în `mesh.frag`, gizmo-uri
  (`Viewport::appendModelExtras`, `objectHandleAt`), `shell/ObjectInfo.qml`, secțiunea din Outliner. Acțiuni de
  script: `object.camera`, `object.light.point|spot|directional|area`, `object:intensity=3,color=#EF4444`,
  `camera.look`.

- **Sidebar-ul din stânga** (`components/AccordionSection.qml` cu `count` și `actions`): Obiecte (`Outliner.qml`,
  iconițe după `Definition::shape`), Tag-uri (`TagsPanel.qml`), Materiale (editor de material personalizat:
  `Document::createMaterial` / `updateMaterial`), Scene (tranziția în ⚙), Interacțiuni (`InteractionsPanel.qml`,
  `Document::interactions`). Acțiuni de script: `debug.leftAll`, `material.new`.

- **Fazele 21–23** (ADR 0023): `model/Modeling.h` (mirror, array liniar / polar, soften), panourile
  `overlays/ArrayPanel.qml` și `SoftenPanel.qml` (previzualizare prin undo: `Document::previewArray` /
  `previewSoften` / `commitOperation` / `cancelOperation`). Comenzi: `edit.mirrorX|Y|Z`, `edit.array`, `edit.soften`.
  Atenție în QML: un Popover nu poate avea proprietăți proprii numite `opacity` sau `spacing` (FINAL în Popup).

## Următorii pași
1. Faza 20 (import `.skp`): utilizatorul descarcă SDK-ul SketchUp și îi verifică licența (`AION_SKP_SDK_DIR`).

## Convenții
C++20, `clang-format` (config în repo), fără `new`/`delete` manual, namespace `aion::`,
`PascalCase` pentru tipuri, `camelCase` pentru funcții, `m_` pentru membri privați.
Orice cod din `core/` vine cu teste. Commit-uri mici, cu mesaje clare.
