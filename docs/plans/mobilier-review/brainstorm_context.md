# Context pentru brainstorm: Aion pentru proiectarea de mobilier

Data: 2026-10-30. Material de intrare pentru analiza Goldfish. Scris de agentul principal din codul și documentele
proiectului (`README.md`, `CLAUDE.md`, `PLAN.md`, `docs/feature-ideas.md`, `docs/decisions/`) și din căutări web.

## 1. Ipoteza ancorată (confirmată de utilizator)

Aion devine o unealtă de proiectare a mobilierului pe corp (dulapuri, bucătării, rafturi) pentru amatori avansați și
ateliere mici, în România și internațional. Dezvoltatorul lucrează singur, e el însuși proiectant amator de mobilier,
vrea un produs din care să poată câștiga bani (modelul de monetizare nu e ales) și testează personal. Dacă aduce
venituri, ar investi într-o echipă. Întrebarea: ce funcții lipsesc ca Aion să fie competitiv, în ordinea priorității,
ținând cont că e un singur om.

## 2. Ce are Aion acum (inventar)

Stack: C++20, Qt 6 Quick/QML, Qt RHI (Metal/D3D/WebGL), nucleu `core/` fără Qt, CMake + vcpkg, Catch2. macOS întâi
(DMG nesemnat, fără certificat), Windows (CI verde, installer Inno Setup netestat pe o mașină reală), Web (WebAssembly,
publicat pe GitHub Pages). Interfața e doar în română. Inspirat de SketchUp (modelare directă poligonală).

- **Modelare directă:** Line, Rectangle, Circle, Arc, Polygon, primitive (cub, cilindru, con, sferă), Push/Pull cu
  tăieri și găuri, Offset, Follow Me, Bevel/Chamfer, Subdivide (Catmull-Clark), Mirror, Array liniar/polar, Soften,
  Intersect Faces, booleene (Manifold), Eraser. Inferență (snap) pe geometrie și pe obiecte, VCB (introducerea
  valorilor), expresii în câmpuri numerice, snap la grid, pivot, Move/Rotate/Scale (și neuniform cu mânere), Pen cu
  curbe Bézier editabile.
- **Măsurare și adnotare:** Tape Measure, Protractor, Dimension, Text, ghidaje, Section Plane.
- **Organizare:** grupuri, componente (instanțe legate), Make Unique, tag-uri, Outliner, Entity Info/Inspector.
- **Obiecte parametrice** (`model/Parametric.h`): ușă, fereastră, masă, raft, dulap (cabinet), scară. Generatoare
  care produc părți ca subgrupuri; parametrii se schimbă din Inspector.
- **Bibliotecă de obiecte** cu drag & drop în viewport și inferență la plasare; salvarea propriilor obiecte.
- **Raport de cantități și listă de debitare** (ADR 0017, `model/Quantities.h`):
  - fiecare grup/obiect cu fețe proprii = o piesă; L × l × grosime măsurate pe fața cea mai mare;
  - piese identice adunate; necesarul de plăci = aria × (1 + pierderi) / aria plăcii (implicit 2800 × 2070, 15%);
  - piese masive (> 100 mm) și plate marcate separat;
  - cantul e doar ESTIMAT ca perimetrul tuturor pieselor (nu se știe pe ce muchii e cant);
  - export CSV (separator „;”, virgulă zecimală, BOM UTF-8).
  - Neimplementat: cant pe muchie, direcția fibrei, găuri/balamale, nesting 2D (așezarea pe plăci), desen
    PDF/DXF al plăcilor, prețuri și deviz.
- **Prezentare:** materiale, texturi și UV, texturi procedurale, stiluri (clay, linii ascunse, wireframe, X-Ray),
  umbre de la soare, SSAO, medii procedurale, MSAA, DOF, ACES, camere și lumini ca obiecte, scene, export PNG și
  video MP4.
- **Animație și interacțiune:** timeline cu keyframe-uri, easing, căi de parcurgere, reguli „la click, fă X”, Modul
  Interacțiune, trei moduri de lucru (Modelare / Animare / Interacțiune).
- **Fișiere:** format `.aion` (ZIP + JSON), import/export OBJ și glTF, autosalvare, recuperare după crash, documente
  recente. Fără DXF/DWG, STL, PDF, IFC. Importul `.skp` e suspendat (licența SDK-ului Trimble îl interzice).
- **Extensibilitate:** scripturi JavaScript (QJSEngine), plugin-uri din `Plugins/`, consolă de scripturi, un pas de
  undo pe rulare.
- **UX:** command palette (⌘K), scurtături personalizabile, preferințe, ecran de start, ghiduri animate per unealtă,
  turul uneltelor.
- **Planificat, neconstruit:** asistent AI (chat cu Claude printr-un bridge local, doar pentru autor deocamdată).
- **Neimplementat la nivel de produs:** semnarea codului și notarizarea macOS, actualizări automate, raportarea
  crash-urilor, licențiere/plată, localizare în engleză.

Mărimea proiectului: 42 de ADR-uri, ~208 sarcini bifate în `PLAN.md`, ~15 deschise. Mare parte a muncii recente a fost
pe animație, interacțiuni și inferență, nu pe mobilier.

## 3. Piața: concurenți pentru proiectarea de mobilier

Notă: prețurile vin din agregatoare și pagini de revânzători; nu sunt liste oficiale verificate.

| Produs | Platformă | Preț (aprox.) | Puncte forte | Relevanță pentru Aion |
|---|---|---|---|---|
| SketchUp + OpenCutList | Win/Mac/Web | SketchUp abonament; OpenCutList gratuit, open-source, finanțat prin crowdfunding | Modelare liberă + listă de debitare, diagrame de tăiere pe plăci, etichete, cant (pe 4 muchii, detectat doar pe materiale de tip placă), vederi explodate, export piese DXF/SVG/STL/OBJ, estimări de cost | Combinația cea mai apropiată de Aion; standardul de facto pentru amatori și ateliere mici |
| Polyboard (Boole & Partners) | Windows (Mac prin emulare) | licență perpetuă ~2 500–5 000 USD, pe module | Parametric pe corp, biblioteci de feronerie Blum/Hettich/Häfele, deviz automat, liste de debitare, planuri, fișiere CNC | Profesionist; scump pentru amatori |
| Mozaik (Cyncly) | Doar Windows | abonament ~95–325 USD/lună pe niveluri | Design, 3D, liste de debitare, fișe de asamblare, deviz; nivelul CNC: nesting true-shape, G-code pentru 175+ mărci de mașini, găurire automată pentru feronerie | Ateliere cu CNC; arată că devizul și producția sunt ce se plătește |
| PRO100 (ECRU, Polonia) | Windows | licență plătită o dată, ~2 500 USD conform unui agregator; există demo | Foarte răspândit în RO și estul Europei; ușor de învățat, biblioteci de corpuri, randare, deviz, listă de debitare exportată spre Cut Manager / CUT100 (optimizare tăiere); partener Blum | Concurentul local direct în RO |
| SketchList 3D | Windows/Mac | abonament | Făcut „de tâmplari pentru tâmplari”, simplu, ~7 000 de utilizatori | Arată că există o nișă pentru „simplu, fără CAD” |
| Woodwork for Inventor | Windows, peste Autodesk Inventor | ~80 USD/lună | Inginerie, CNC | Nu e pentru amatori |
| Cabinet Vision, Microvellum, KCD, Teowin | Windows | scump, enterprise | Producție de fabrică | Nu e țintă |
| Programele furnizorilor (Optimik, Cut Manager, programele de debitare ale magazinelor de PAL) | Win/web | adesea gratuite pentru clienți | Optimizarea tăierii și comanda debitării direct la furnizor | În RO, amatorul trimite lista la firma de debitare; formatul acceptat de ea contează |

Observații din piață:
1. Aproape toți concurenții de mobilier sunt doar pe Windows. macOS și web sunt slab acoperite (SketchUp, SketchList).
2. Banii se fac pe ce urmează după model: listă de debitare corectă cu cant, nesting, deviz, fișe de asamblare, CNC.
   Vizualizarea e necesară, dar nu e ce plătesc atelierele.
3. Bibliotecile de feronerie (Blum, Hettich, Häfele) și sistemul de 32 mm sunt standard în produsele profesionale.
4. OpenCutList arată că un singur proiect mic, open-source și finanțat prin crowdfunding poate concura pe partea de
   debitare, dacă stă peste o aplicație de modelare existentă.
5. În RO, PRO100 și firmele de debitare (care acceptă liste în formatele lor) definesc fluxul real al amatorilor.

## 4. Constrângeri

- Un singur dezvoltator, fără buget, fără echipă.
- Fără SDK-uri cu licențe restrictive (lecția `.skp`).
- `core/` fără Qt (pentru WebAssembly); orice funcție nouă din nucleu vine cu teste.
- Piețe: România și internațional (interfața e doar în română acum).
- Validare: doar dezvoltatorul, ca utilizator amator.

## 5. Surse

- https://wooddesigner.org/product/polyboard/
- https://www.techjockey.com/us/detail/polyboard
- https://forums.sketchup.com/t/opencutlist-a-crowdfunded-extension-8-0-campaign/301089
- https://forums.sketchup.com/t/opencutlist-version-6-0-0-is-out/260630
- https://www.mozaiksoftware.com/products
- https://www.cyncly.com/en-US/mozaik
- https://www.ecru.pl/en/possibilities
- https://www.blum.com/md/ro/services/industrial-production/cad-cam-interface/software-partners/pro100/
- https://forum.softpedia.com/topic/182761-proiectare-mobila/page__st__18
- https://www.sourceforge.net/software/product/SketchList-3D
- https://slashdot.org/software/comparison/CABINET-VISION-vs-Woodwork-for-Inventor/
