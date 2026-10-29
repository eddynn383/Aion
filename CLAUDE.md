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
  `previewSoften` / `commitOperation` / `cancelOperation`; o previzualizare care n-a schimbat nimic nu se anulează). Comenzi: `edit.mirrorX|Y|Z`, `edit.array`, `edit.soften`; acțiunea de script `soften:unghi` (cu panoul deschis).
  Atenție în QML: un Popover nu poate avea proprietăți proprii numite `opacity` sau `spacing` (FINAL în Popup).
  Tot în QML: nu folosi `long`, `int`, `short`, `byte`, `char`, `float`, `double` etc. ca nume de variabile JS —
  Qt-ul din CI (qmlcachegen) le respinge („Expected token identifier”), deși Qt 6.12 local le acceptă.

- **Faza 24** (ADR 0024): `model::Texture` (octeții imaginii, în arhivă), `Material::texture` / `textureSize`,
  `geom::TextureTransform` pe fețe, `model/Texturing.h` (UV), array de texturi pe binding 2 în `mesh.frag`,
  texturi procedurale în `render/src/ProceduralTextures.cpp`, export glTF / OBJ cu UV și imagini.
  Acțiune de script: `material:front,24` (24–28 = materialele texturate din bibliotecă).

- **Fazele 25–26** (ADR 0025, 0026): `model/Parametric.h` (generatoare, părți ca subgrupuri), `model/Transfer.h`
  (`insertModel` / `extractInstance`), `shell/LibraryPanel.qml`, drop în viewport (`Viewport::dropPointAt`).
  Comenzi: `object.parametric:door|window|table|shelf|cabinet|stairs`, `library.save`.

- **Faza 27** (ADR 0027): `render/ScriptEngine.h` (QJSEngine, API `aion`, plugin-uri din `Plugins/`),
  `overlays/ScriptConsole.qml`, `Document::beginMacro` / `endMacro` (un pas de undo pe rulare). Documentație:
  `docs/scripting.md`; exemplu: `examples/plugins/gard/`. Acțiuni de script: `script:N` (exemplul N),
  `scriptjs:cod` (fără `;` — separă acțiunile), `plugin:dosar/funcție`.

- **Panoul din stânga** e un `Flickable` cu bară de scroll proprie; fiecare `AccordionSection` deschisă are
  înălțimea ei (`bodyHeight`), trasă de marginea de jos (`resizeRequested` / `resetRequested`) și păstrată în
  `preferences.leftSectionHeights` (JSON cheie → px).

- **Etapa 4** (Fazele 28–31, ADR 0028): marcajele luminilor / camerelor se trag (`Viewport::m_objectDrag`), umbre
  pentru un spot (a doua hartă, adâncime liniară), Scale cu mânere (`ScaleTool`, 26 de mânere, VCB pe axe),
  materiale trase pe fețe (Paint Bucket temporar), medii procedurale + SSAO (`gbuffer.frag`, `ssao.frag`) + MSAA 8×
  la cameră oprită, preset-uri de calitate. Acțiuni de script: `view.environment:studio`, `view.ambientOcclusion`
  (comută și salvează în profilul de test!), `draglight:dx,dy`, `debug.materialsOnly`.

- **Acțiunile de pe rândurile listelor** (Obiecte, Tag-uri, Scene, Interacțiuni) folosesc `components/RowAction.qml`:
  locuri fixe (doar se estompează, fără să mute rândul), hover detectat cu `HoverHandler` pe rând; ordinea: ștergere,
  comenzi, stări (lacăt, aprins / stins), ochiul ultimul; o stare neobișnuită (ascuns, blocat, stins) se vede mereu.

- **Inspector-ul** își grupează setările cu `components/PropertyGroup.qml` (titlu + acțiuni + câmpuri): titlu → câmpuri
  `space1 + 2`, între categorii `space4`. Orice categorie nouă folosește PropertyGroup, ca regula să rămână aceeași.

- **Bevel / chamfer** (Faza 32, ADR 0029): `geom/Bevel.h` (`bevelEdges`, `bevelFace`), `overlays/BevelPanel.qml`,
  `Document::previewBevel`. Comenzi: `edit.chamfer`, `edit.bevel`. În scripturi, `clicks:` pe geometrie are nevoie de
  `AION_SCREENSHOT_DELAY_MS` mai mare (altfel captura se face înainte de acțiuni).

- **Subdivide și segmente** (ADR 0041): `geom/Subdivide.h` (Catmull-Clark), `Document::previewSubdivide`,
  `overlays/SubdividePanel.qml` (comanda `edit.subdivide`); Soften apare ca „Muchii netede”; `model::primitiveSegments`
  / `setPrimitiveSegments` (grupul „Formă” din Inspector). Acțiune de script: `subdivide:N` (cu panoul deschis).
  Meniul „+” din Obiecte creează și forme (`Outliner.addObject`, `Document::createPrimitive`).

- **Fețe din muchii**: `geom::createFaces` (comanda `edit.createFaces`; Line o apelează când retrasează o muchie).
  Cursoarele uneltelor se fac în `app/src/bridge/NavigationCursors.cpp` (găleata, guma).

- **Versiunea web publică**: <https://eddynn383.github.io/Aion/> (GitHub Pages). `pages.yml` publică automat
  artefactul `aion-web` după fiecare CI verde pe `main` (sau manual, cu ID-ul unei rulări CI).

- **Pivot** (Faza 33, ADR 0030): `Instance::pivot`, `model::pivotLocal` / `pivotPoint`, `Placement::pivot`;
  `Document::selectionPivot` (marcajul din viewport), cheile `vx/vy/vz` și `pivot: center|base|corner` în
  `setPlacement` / `setGeometryPlacement`. Acțiune de script: `placement:vx=0,vy=0,vz=0`. Rotate pornește din
  pivot (`ToolContext::pivot`; Option = alt centru), Scale cu Option ține pivotul fix. **Poziția din Inspector e a
  pivotului** (ca în timeline); pivotul se schimbă doar din grupul „Pivot (de la colțul minim)” sau trăgând
  marcajul: unealta internă `pivot` (`Document::beginPivotDrag`, `ToolRequests::pivot`) se prinde ca Move, plus
  centrele fețelor și originea. Acțiuni: `debug.pivot`, `pivotpos:x=…,y=…`, `dragpivot:x,y,z[,hold]`.

- **Move cu puncte de prindere**: `tools::Grip` în `Overlay::grips` (desenate în `Viewport::appendOverlay`),
  `instanceGrips` / `nearestGrip` în `TransformTools.cpp` (cutia orientată a obiectului, colțuri + mijloace).

- **Follow Me** (ADR 0031): `geom/Sweep.h` (`followMe`), `tools/FollowMeTool.cpp`; fețele noi (și la bevel) prin
  `core/src/geom/FaceBuilder.h` (`buildFaces`). Acțiune de script: `tool.followme`.

- **Index de picking** (ADR 0032): `view::forEachFaceAlong` / `forEachVertexNear` / `forEachEdgeNear` (BVH din
  `core/src/view/PickIndex.cpp`, după `Mesh::revision()`); buclele noi pe geometrie la hover trebuie să treacă prin ele.
  Testul de performanță: `aion_tests "[.perf]"`.

- **Camera** (plan Animare, secțiunea 9): `CameraObject` cu focală / senzor, format, shift, roll, `orthoScale`, `dof`,
  `displaySize`; `model/CameraLens.h` (`cameraFov`, `frameInViewport`, `lookThrough`, `circleOfConfusion`, presetări).
  Vederea prin cameră: `Viewport::lookThrough` (id; zbor, cadru `lookFrame`, `lookLabel`, `lockCameraToView`,
  `previewLookAspect`), suprapunerea `shell/CameraFrame.qml`; `Document::setCameraProperties` / `cameraInfo`. Acțiuni:
  `camera.look`, `look:lock`, `look:focus` (următorul click alege focusul), `debug.look`,
  `object:focalLength=50,shiftY=0.1,dofEnabled=1`. Move / Rotate / Scale pe camere și lumini (`ToolContext::object`);
  depth of field în `shaders/dof.frag` (ADR 0035). În rulările automate fereastra randează rar: zborurile durează mai
  mult decât 0,7 s — exportul (`png:`) îl termină singur.
- **Scurtături personalizabile**: `preferences.customShortcuts` (JSON id comandă → secvență, "" = fără),
  `window.setShortcut` / `resetShortcuts` în Main.qml; `commands` le aplică (păstrează `defaultShortcut`), iar
  `Toolbar.keyFor(tool)` le arată la unelte. Câmpul: `components/ShortcutRecorder.qml`.

- **Preferințele** sunt un modal (`overlays/PreferencesPage.qml`): categoriile în stânga, paginile din carduri
  `components/SettingsGroup.qml` cu rânduri `components/SettingsRow.qml` (titlu + descriere, controlul în dreapta;
  `stacked` pentru controale late, `compact` pentru liste). Setările noi folosesc aceleași componente.

- **Modurile de lucru** (ADR 0033, `docs/plans/moduri-de-lucru.md`): `Document::workMode` (⌥1 / ⌥2 / ⌥3),
  `commandModes` / `commandAllowed` (o singură clasificare a comenzilor), `require(mode)` (gărzi), `pose()` (Animare:
  auto-key pe o copie pozată), `shell/ModeSwitch.qml`; grupurile toolbar-ului au `modes`. Acțiuni de script:
  `mode:model|animate|interact`, `mode.interaction` (Interact ↔ Select). Comenzile noi trebuie să fie clasificate în
  `commandModes` dacă nu sunt comune; operațiile noi de model încep cu `require(Modeling)`.
- **Tastatura în Animare**: comenzile `anim.*` (Space redare, ← → cadre, ⇧ 10 cadre, ⌥ chei, Home / End, K);
  `window.shortcutOwners` dă fiecare tastă unei singure comenzi pe mod (cea a modului bate una comună; Select are și
  V); săgețile rămân ale uneltei cât `Tool::ownsArrowKeys()`. Acțiunea `keys:,alt+right` simulează taste cu
  modificatori (prin harta scurtăturilor); `debug.anim` afișează timpul și cheile.
- **Timeline-ul** (ADR 0034): `shell/Timeline.qml` (vederea în timp `viewStart` / `pxPerSecond`, `selectedKeys` ca
  `{ instance, property, time }`, tragerea în grup cu snap), `TimelineRow` (obiect ▸ / ▾, canal cu valoarea curentă),
  `TimelineKey`, `TimelineRuler`. Datele vin din `Document::timeline` (pistele cu `times` și `channels`). Acțiune de
  script: `anim:expand` (deschide toate obiectele).
- **Easing** (ADR 0034): `overlays/EasingEditor.qml` (graficul cu mânere, presetări, curbele proprii din
  `preferences.easingPresets`), `components/EasingCurve.qml` (miniaturi), `components/EasingMath.js`.
  `Animation::defaultEasing` pentru cheile noi. În Popover, delegații Repeater-ului nu văd id-ul fișierului la creare
  (conținut amânat): câmpurile editorului sunt scrise explicit. Acțiuni: `anim:savecurve,…`, `anim:curve,N`, `anim:curves`.
- **Auto-key** (`Document::autoKey`, `anim.autoKey`): oprit, schimbările intră în poza de bază, iar canalele animate își
  mută toate cheile (`model::adjustWithoutKeys`). Copiere / lipire de chei: `anim.copyKeys` / `anim.pasteKeys` (⌘C / ⌘V,
  prin meniul nativ „Animare”). În scripturi, o comandă necunoscută ca acțiune rulează prin `runCommand` (`anim.fit`).
  Cât rulează animația (și la export), `Viewport::cleanView()` desenează doar modelul (fără selecție, pivot, hover,
  ce desenează unealta, cutia grupului editat).
  Undo / redo trec prin `Document::stepHistory`: animația activă (una readusă devine activă), apoi timeline-ul,
  valorile de la playhead, Inspector-ul și Outliner-ul primesc semnalele lor (pasul poate schimba orice).

- **Pen și căi de parcurgere** (Etapa 7, ADR 0036): `geom/Bezier.h`, `model::PenCurve` + `model/PenCurves.h` (curbe
  editabile cât muchiile se potrivesc), uneltele `pen` și `penedit` (`Document::beginCurveEdit` / `beginPathEdit`, ca
  `beginPivotDrag`), `model::MotionPath` + `model/MotionPaths.h` (`attachToPath`, `anchorTransform`, `applyPaths`,
  `pathTransformAt`, `absorbAnchorEdit`), `Document::paths` / `pathInfo` / `setPathTiming`, `Viewport::pickingAnchor`.
  Format `.aion` v4. Acțiuni de script: `dragw:x1,y1,z1,x2,y2,z2`, `dblclickw:x,y,z`, `anchor:`, `pathtime:s,d`,
  `debug.paths`. Web: `Main.qml` fără qmlcachegen (`QT_QML_SKIP_CACHEGEN`, altfel runner-ul CI oprea build-ul).
  Pen: Esc / ⌘Z = înapoi un nod (`Tool::undoStep`, apelat de `Document::undo`), săgețile aleg planul, axa Z → plan
  vertical. O cale se editează doar în Animare.
- **Performanța viewport-ului** (ADR 0038): `cameraChanged` o dată pe cadru (`updatePolish`), `projectionChanged`
  separat; `DocumentPass` (desenul documentului păstrat între cadre, profilele alese pe cadru; orice stare nouă care
  schimbă desenul trebuie să atingă documentul sau să intre în `DocumentPassCache::Key`); indexul `Model::childrenOf`,
  `model::BoundsCache`, `Document::modelRevision()`; hover-ul fără schimbări nu atinge documentul (`ToolLook`);
  macro-urile anunță doar la `endMacro`. Măsurare: `navbench:orbit|pan|hover,s` și `fpslog:s` (fps pe secundă; fereastra
  vine în față — una acoperită nu primește cadre), `aion_tests "[.perf][model]"`, `sample <pid> 4` în timpul rulării.
  `make run` e debug (UBSan, fără optimizări); pentru fps reale: `make run CONFIG=release`.
- **Copii și animații din interacțiuni** (ADR 0039): `model/Copying.h` (`copyDeep` unic în adâncime pentru ⌘V și
  Move/Rotate cu Option; `copyLinked` doar pentru ⌥D și Array; `makeUniqueDeep`, `canMakeUnique`, `copyAnimations`,
  `instanceOutline`); unealta internă `place` (`ToolContext::place`, `ToolRequests::placeAt` / `placeCancel`);
  `Document::copySelection` / `pasteClipboard` / `makeUnique`; `model::AnimationPlayer` (redarea din interacțiuni,
  `Action::direction` / `repeat`, format v5). Acțiuni de script: `keys:,ctrl+c`, `keys:,ctrl+v`, `org.makeUnique`;
  în Modul interacțiune: `mode.interaction` (F5) apoi `clickw:`.
- **Ecranul de start și recuperarea** (ADR 0040): `overlays/StartScreen.qml` (la pornire, dacă `UserPrefs.startScreen`
  sau dacă a rămas o copie; niciodată în rulările automate sau când pornește cu un fișier); `core/io/Recovery.h` (copii pe
  sesiune: `Recuperare/<sesiune>.aion|json`), `render/src/RecoveryStore` (QLockFile pe sesiune, migrarea vechiului
  `autosave.aion`, QSettings pe web), `Document::recoveries` / `recoverSession` / `discardRecovery` / `autosave` /
  `recentDocuments`, `text::describeTime`. Acțiuni de script: `app.start`, `debug.autosave`, `debug.crash` (închidere
  bruscă, doar în rulările automate). Clicurile reale date în altă fereastră în timpul unei rulări automate pot ajunge la
  fereastra Aion și închid pop-up-urile (ecranul de start, Despre) — o captură fără el înseamnă de obicei asta.
- **Ștergerea** (ADR 0037): guma și ⌫ păstrează fețele (`geom::eraseEdgesKeepingFaces`), Delete le ia
  (`eraseEdgesWithFaces`); `tools::deleteSelection(ctx, keepFaces)`. În scripturi: `keys:,backspace` / `keys:,delete`;
  `debug.geometry` arată și titlul selecției.

## Următorii pași
0. **Făcut (2026-10-28): Animare, timeline, cameră, rotația față de pivot** — `docs/plans/animatie.md` (și `PLAN.md` →
   Etapa 6). Rămân funcțiile P2 din plan (traiectoria în viewport, faze de construcție, camera pe traseu, însorire,
   vizibilitate / opacitate, editor de curbe, markeri, export video; C7), în ordinea din secțiunea 6, la cerere.
1. Faza 20 (import `.skp`): **suspendată**. Termenii Trimble (developer-terms §2.3, §2.5(d), §2.7) interzic
   redistribuirea SDK-ului și folosirea lui într-o aplicație terță care replică SketchUp, fără aprobare scrisă.
   Alternativa propusă utilizatorului: import COLLADA (.dae), fără SDK. SDK-ul NU se pune în repo.

## Convenții
C++20, `clang-format` (config în repo), fără `new`/`delete` manual, namespace `aion::`,
`PascalCase` pentru tipuri, `camelCase` pentru funcții, `m_` pentru membri privați.
Orice cod din `core/` vine cu teste. Commit-uri mici, cu mesaje clare.
