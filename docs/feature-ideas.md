# Aion – idei de funcționalități noi

Grupate după impact. (★ = recomandare pentru următoarea etapă)

## Modelare
- ★ **Follow Me**: profil extrudat de-a lungul unui drum (muluri, țevi, borduri). Butonul există deja în toolbar.
- ★ **Scale neuniform** (pe axe, cu mânere pe fețe și muchii) și Scale numai pe o axă cu VCB (`2,1,1`).
- **Mâner de rotire direct pe obiect** (gizmo ca în Figma / Blender), pe lângă raportor.
- **Fillet / Chamfer** pe muchii (rotunjiri și teșituri), cu VCB pentru rază.
- **Shell / grosime**: un volum devine perete cu grosime dată.
- **Array liniar și polar** cu panou (număr, distanță, unghi), nu doar `x3` după Move.
- **Mirror / Flip** pe axele roșie, verde, albastră.
- **Soften / Smooth** cu slider de unghi (muchii soft automat).
- **Sandbox / teren**: grilă deformabilă, „From Contours”, Smoove.
- **Desen pe imagine**: „Match Photo” (calibrarea camerei după o fotografie).
- **Snap la grid** opțional și grilă configurabilă (pas, subdiviziuni).

## Organizare și date
- ★ **Bibliotecă de obiecte** (uși, ferestre, mobilier) cu căutare și drag & drop în viewport.
- **Obiecte dinamice / parametrice**: o ușă cu lățime și înălțime modificabile, din proprietăți.
- **Straturi de etape** (existent / demolat / nou) cu stiluri diferite.
- **Raport de cantități**: arie de pardoseală, volum, număr de obiecte, export CSV.
- **Atribute personalizate** pe obiecte (preț, furnizor, cod), vizibile în Entity Info.
- **Outliner cu drag & drop** (mutare între grupuri) și filtrare.

## Grafică și prezentare
- ★ **Texturi și UV** (lemn, cărămidă, parchet), cu poziționare (Position Texture). Rămas din Faza 8.
- **SSAO, IBL, TAA** (umbrire ambientală, lumină din mediu, antialiasing temporal). Tot din Faza 8.
- **Randare fotorealistă** (path tracing progresiv în viewport, când stai pe loc).
- **Stiluri schiță** (linii de mână, acuarelă), pentru prezentări conceptuale.
- **Animație între scene** exportată ca video (MP4) sau GIF.
- **Planșe 2D (Layout)**: vederi ortografice cu cote, titlu și scară, export PDF / DXF.
- **Export imagine la rezoluție mare** (4K, 8K) cu fundal transparent.

## Colaborare și cloud
- ★ **Comentarii în model** (pin pe un punct, discuție în fir), ca în Figma.
- **Partajare prin link** (vizualizare în browser, cu versiunea web din Faza 12).
- **Istoric de versiuni** al fișierului, cu comparare vizuală.
- **Editare în timp real** cu alți utilizatori (CRDT pe operațiile din ChangeSet).

## Import / export
- **Import .skp** prin SDK-ul oficial (Faza 7b, după verificarea licenței).
- **DXF / DWG** 2D (import ca muchii; planuri de arhitect).
- **IFC** (BIM), **STL / 3MF** pentru imprimare 3D, cu verificarea solidității.
- **USDZ** pentru vizualizare AR pe iPhone și iPad.

## Experiență de lucru
- ★ **Asistent AI în command palette**: „fă o cameră de 4×3 m cu o ușă de 90 cm” → pașii executați ca undo-uri
  separate. Comenzile există deja; trebuie doar orchestrate.
- **Scurtături personalizabile** (din Preferințe), importabile ca listă.
- **Teme de culoare personalizabile** și o temă cu contrast ridicat.
- **Mod prezentare**: ascunde UI-ul, navigare cu săgeți între scene.
- **Localizare**: engleză pe lângă română (texte în fișiere de traducere Qt).
- **Măsurarea distanței până la cursor** (Tape Measure fără click), afișată permanent în bara de jos.
- **Tutoriale interactive**: turul uneltelor, extins cu exerciții ghidate („desenează o cutie de 2 m”) și verificare
  automată.

## Platformă
- **iPad** (Qt pe iOS, Apple Pencil pentru desen, gesturi).
- **Plugin-uri / scripting** (JavaScript prin QJSEngine sau Python), cu API peste comenzi și model.
- **Raportare crash-uri** (Sentry / Crashpad) și actualizare automată (Sparkle pe macOS, WinSparkle pe Windows).
