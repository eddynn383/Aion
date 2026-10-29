# Inferență pe obiecte – design

- Stare: propus (2026-10-29), de aprobat de utilizator (al treilea clic pentru înălțime: aprobat)
- Legături: ADR 0005 (inferența și VCB), ADR 0032 (indexul de picking), ADR 0030 (pivot), ADR 0026 (biblioteca)

## De ce

Utilizatorul a construit patru stâlpi, fiecare un obiect separat. La al doilea stâlp n-a putut opri înălțimea în
dreptul vârfului primului. La fel, un obiect nou (din Bibliotecă sau cu o unealtă de formă) nu se poate pune pe
colțul sau pe muchia altui obiect: se prinde doar de grid.

Cauza: motorul de inferență (`core/inference/Inference.h`, `Engine::infer`) primește doar `ToolContext::mesh`,
adică geometria liberă a contextului editat. Tot ce e în obiecte (grupuri, componente, primitive, obiecte dinamice,
importuri), adică aproape tot ce face utilizatorul, e invizibil pentru inferență. Tipurile de inferență există deja
(Endpoint, Midpoint, OnEdge, OnFace, FromPoint cu linie punctată în culoarea axei, Parallel, Perpendicular,
Intersection), doar că nu văd obiectele.

Succesul, în cuvintele utilizatorului: „funcția ar trebui să se aplice pe fiecare intersecție cu muchiile sau colțurile
obiectelor din jur; când o intersecție e detectată, apare o linie punctată între ele” (ca „From Point” din SketchUp).

## Obiective

1. **Inferența vede toate obiectele vizibile**, nu doar geometria liberă a contextului: capete (colțuri), mijloace,
   puncte pe muchii, puncte pe fețe, alinierea pe o axă cu un punct atins („From Point”, linie punctată în culoarea
   axei, între punctul de referință și cursor) și muchiile lor ca referință pentru Parallel / Perpendicular
   (decizia mea: ca în SketchUp; referința devine un segment în coordonatele contextului, nu un `EdgeId`). Geometria din obiecte e transformată în coordonatele contextului
   editat; obiectele imbricate se văd la orice adâncime.
2. **Peste tot unde se alege un punct**: Line, Arc, Rectangle, Circle, Polygon, Cub / Sferă / Con / Cilindru,
   Pen (și editarea curbelor), Push/Pull (distanța până în dreptul unui punct), Move, Rotate, Scale, Offset, Follow Me
   (unde alege puncte), Tape Measure, Protractor, Dimension, Text, Section, pivotul (`PivotTool`), lipirea (`PlaceTool`).
3. **Plasarea unui obiect nou** se prinde ca orice punct: tras din Bibliotecă (drop-ul din viewport) și uneltele de formă.
   Punctul obiectului care ajunge pe ținta aleasă e **centrul bazei** (decizia utilizatorului: „punctul din care începe
   desenul e mijlocul feței bazei”, cum e deja la primitive).
4. **Înălțimea primitivelor cu mouse-ul** (aprobat de utilizator): Cub, Cilindru și Con primesc un al treilea pas,
   înălțimea, care se prinde în dreptul punctelor din jur, ca Push/Pull. Sfera rămâne cu doi pași.
6. **Previzualizarea formelor cât se desenează** (cererea utilizatorului: „sfera nu arată a sferă, ci apar doar 2 cercuri
   paralele”): la fiecare pas se vede corpul care va ieși, nu doar contururi. Vezi „Previzualizarea formelor”.
5. **Inferențe în plus** (ca în SketchUp):
   - centrul unui cerc / arc / poligon (`Kind::Center`), al geometriei libere și al celei din obiecte. Decizia mea, ca
     în SketchUp: centrul apare după ce cursorul a atins curba (devine punct de referință, prin mecanismul existent
     `remember` / `m_references`), nu printr-un index separat de centre. „Cerc / arc / poligon” = muchiile cu același
     `Edge::curve`; centrul se calculează din trei puncte ale curbei și se acceptă doar dacă toate punctele ei sunt la
     aceeași distanță de el (`Tolerance.h`) și în același plan;
   - aliniere dublă: punctul aflat simultan în dreptul a două puncte de referință, pe două axe, cu două linii punctate;
   - la Move: colțul (sau punctul de prindere, `tools::Grip`) obiectului mutat se prinde de colțurile altor obiecte
     (o parte există deja: `instanceGrips` / `nearestGrip`). Decizia mea: punctele cutiei (`instanceGrips`) rămân ca
     ținte și ca puncte de apucare (la un cilindru sunt utile), lângă colțurile reale; la aceeași distanță pe ecran,
     un colț real bate un punct al cutiei.

## Non-obiective

- Lipirea geometriei de alt obiect: o inferență pe alt obiect **doar alege punctul**. Geometria nouă rămâne în
  contextul editat, iar obiectul atins nu se schimbă (nicio muchie nu se împarte în el).
- Inferența pe curbele Pen ca Bézier (se folosesc muchiile lor, ca acum).
- Un comutator global de inferență; Shift (lock) și săgețile (axă) rămân ca acum.

## Anti-obiective (ar fi un eșec chiar dacă funcționează)

- Hover-ul devine sacadat pe modele mari: inferența pe obiecte trebuie să treacă prin indexul de picking (BVH,
  ADR 0032), nu prin bucle pe toate vârfurile la fiecare mișcare a mouse-ului.
- Obiectul mutat / rotit / scalat se prinde de el însuși.
- O unealtă care primește ID-uri din alt mesh și le folosește pe `ctx.mesh` (împarte o muchie greșită, pornește
  Push/Pull pe fața altui obiect). Un ID de vârf / muchie / față din `Inference` trebuie să însemne mereu „din
  `ctx.mesh`”.
- Snap-ul la grid care bate un colț de obiect aflat sub cursor.

## Constrângeri

- `core/` fără Qt, cu teste (Catch2) pentru tot ce e nou în core.
- Comparațiile numai prin `math/Tolerance.h`.
- Buclele noi pe geometrie la hover trec prin `view::forEachVertexNear` / `forEachEdgeNear` / `forEachFaceAlong`
  sau prin echivalentele lor noi.
- Starea care schimbă desenul trebuie să intre în `DocumentPassCache::Key` sau să atingă documentul (ADR 0038).
- `Viewport::cleanView()` (redare, export) nu desenează inferențe.

## Comportament

### Ce geometrie se vede

- Obiectele vizibile din model, la orice adâncime, în coordonatele contextului: și cele din afara contextului editat
  (când editezi un grup, colțurile obiectelor din jur se văd), ca în SketchUp. Decizia mea: și geometria liberă a
  contextelor părinte se vede (de aceea originea unei inferențe e „din `ctx.mesh`” / „din afară”, cu instanța sursă
  opțională, goală pentru geometria liberă a unui părinte).
- Nu se văd: obiectele ascunse (proprii sau prin tag), fețele / muchiile ascunse (`view::isHidden`), ce e tăiat de
  un plan de secțiune activ (unde se poate stabili ieftin), și **selecția care se transformă** (Move / Rotate / Scale /
  lipirea / plasarea: obiectul în mișcare și copiile lui).
- Obiectele blocate se văd (blocarea oprește editarea, nu prinderea).
- Muchiile soft (laturile unui cilindru) se văd pentru inferență, ca în contextul propriu.

### Priorități (de la cea mai tare)

1. Capăt / centru (pe ecran, în `pointRadiusPx`), apoi mijloc.
2. Intersecția unei direcții (axă / paralel) cu o muchie; alinierea dublă.
3. Pe muchie.
4. Aliniere pe axă cu un punct de referință („From Point”), pe axă din punctul de pornire.
5. Pe față (cea mai apropiată de cameră, din orice obiect).
6. Grid / sol / planul de lucru.

Între un candidat din `ctx.mesh` și unul dintr-un obiect, la aceeași prioritate, câștigă cel mai apropiat pe ecran;
la egalitate, cel din `ctx.mesh`. Histerezisul existent (`Options::hysteresis`) se aplică la fel.

### Punctele de referință („From Point”)

Ca acum (`Engine::remember`, `m_references`, cel mult 4): un capăt, un mijloc sau un centru pe care cursorul s-a oprit
devine referință, indiferent dacă e din `ctx.mesh` sau dintr-un obiect. Linia punctată merge de la referință la cursor,
în culoarea axei.

### Plasarea unui obiect nou

- Drop din Bibliotecă: cât se trage peste viewport, punctul de plasare (cutia de previzualizare din
  `Viewport::setDropPreview`) urmează inferența, cu marcaj și eticheta inferenței, nu doar `surfacePoint` + grid.
  Centrul bazei obiectului ajunge pe punctul ales. Decizia mea: azi `placeLibraryItem` pune originea fișierului pe
  punct, iar cutia de previzualizare are colțul minim acolo; ambele trec la centrul feței de jos a cutiei obiectului
  (în axele lui, care la drop sunt cele ale lumii), ca la primitive și ca utilizatorul să aibă aceeași regulă peste
  tot. Obiectele din Bibliotecă nu se rotesc după fața pe care cad (rămân drepte, ca acum); asta poate veni separat.
- Uneltele de formă (Cub, Sferă, Con, Cilindru): centrul bazei, mărimea (raza) și înălțimea (propunere) se prind toate.

### Înălțimea primitivelor (propunere)

Cub / Cilindru / Con: clic 1 = centrul bazei, clic 2 = mărimea bazei, clic 3 = înălțimea (de-a lungul normalei bazei,
cu prindere ca la Push/Pull: în dreptul oricărui punct atins, cu linia punctată spre el). Sfera rămâne cu doi pași.
VCB (decizia mea): la pasul 2, „1000” = lățimea și trece la pasul 3, „1000,500” = lățimea și înălțimea, gata; la
pasul 3, „500” = înălțimea, „1000,500” = lățimea și înălțimea. Textul din status bar spune ce se alege la fiecare pas.

### Previzualizarea formelor

Azi (`PrimitiveTool::overlay`) se desenează doar conturul bazei și al capacului (la sferă: baza și ecuatorul, adică
două cercuri paralele), deci forma nu se citește. Decizia mea:

- Se desenează forma reală, cu dimensiunile curente: aceeași geometrie pe care o face `geom::buildPrimitive`, dar
  mai rară (de exemplu 16 segmente), ca linii de previzualizare:
  - sferă: meridiane și paralele (rețeaua din stilul Wireframe), plus conturul ei văzut din cameră;
  - cilindru: cercul de jos și cel de sus, câteva generatoare verticale și cele două generatoare de contur (marginile
    văzute din cameră);
  - con: cercul bazei, câteva generatoare spre vârf și cele două de contur;
  - cub: toate cele 12 muchii (ca acum).
- La pasul 2 (mărimea bazei), corpul se arată cu înălțimea pe care ar avea-o dacă s-ar termina acum (= lățimea); la
  pasul 3 se vede cu înălțimea aleasă, plus cota înălțimii.
- Liniile sunt cele de previzualizare existente (`Overlay::lines`, culoarea `previewColor`); generatoarele de contur
  se calculează din cameră la fiecare cadru (ca profilele). Dacă un corp translucid se poate desena ieftin cu ce are
  deja viewport-ul, e bine-venit, dar nu e obligatoriu.

## Partea tehnică decisă

- Un ID din `Inference` (`vertex`, `edge`, `face`) e mereu din `ctx.mesh`. O inferență pe alt obiect are ID-urile
  goale și spune de unde vine (de exemplu `model::InstanceId source` sau un tip `Origin { Context, Object }`), iar
  uneltele care folosesc ID-urile (Line împarte muchii, Push/Pull alege fața, Rectangle se orientează după față)
  tratează „pe obiect” ca un punct liber, dar cu planul / normala feței atinse (pentru desenul pe fața altui obiect).
- Ce geometrie vede inferența vine din `ToolContext` (ceva nou, de felul `const SnapScene* snap`), construit de
  `Document` și păstrat cât nu se schimbă modelul (`Document::modelRevision()`), contextul editat sau ce se exclude.
  `core/` primește doar date (nu `Model`-ul întreg), ca testele să-l poată construi ușor.

## Lăsat la latitudinea constructorului

- Forma lui `SnapScene`: o geometrie unită, transformată în coordonatele contextului, cu indexul ei de picking
  (reutilizează tot `view::forEach*` și codul existent din `Engine`), sau o listă de (mesh, transformare, index) per
  definiție, cu transformarea aplicată la cerere (mai puțină memorie la multe copii legate). Criteriul: hover-ul
  rămâne fluid în `navbench:hover` pe un model cu ~200 de obiecte și ~100 000 de fețe.
- Excluderea selecției în mișcare: fie o `SnapScene` refăcută fără ea la începutul operației, fie un filtru pe instanțe.
- Cum se desenează a doua linie punctată la alinierea dublă (structura `Guide` o permite deja).
- Etichetele noi (`label()`), de exemplu „Endpoint (în Masă)” / „Center”: textul exact.

## Verificare

- Teste în core: inferență pe un `Model` cu o instanță transformată (rotită, scalată, imbricată): Endpoint, Midpoint,
  OnEdge, OnFace, FromPoint pe colțul ei; excluderea selecției; prioritatea față de grid; ID-urile goale pentru
  obiecte; centrul unui cerc; alinierea dublă; înălțimea unei primitive prinsă în dreptul colțului altui obiect;
  Push/Pull prins de vârful altui obiect.
- În aplicație, prin `AION_STARTUP` (`movew:`, `clickw:`, `debug.geometry`, `png:`): scenariul stâlpilor (al doilea
  stâlp cu înălțimea în dreptul primului), un obiect din Bibliotecă pus pe colțul altuia, Move cu colț pe colț.
- Performanța: `navbench:hover` și `aion_tests "[.perf]"` înainte și după, pe un model mare.

## Decizii luate de mine (de verificat de utilizator)

- Muchiile altor obiecte servesc și ca referință Parallel / Perpendicular.
- Centrul unui cerc / arc apare după ce cursorul a atins curba (ca în SketchUp).
- La Move, punctele cutiei obiectului rămân ca ținte, lângă colțurile reale (un colț real câștigă la egalitate).
- Când editezi un grup, se vede și geometria liberă din afara lui.
- Obiectele din Bibliotecă se pun cu centrul bazei pe punct (azi: originea fișierului) și nu se rotesc după fața pe
  care cad.
- VCB la primitive: pasul 2 „1000” = lățimea (apoi pasul 3), „1000,500” = gata; pasul 3 „500” = înălțimea.
- Previzualizarea ca linii (rețea), nu ca un corp translucid.
