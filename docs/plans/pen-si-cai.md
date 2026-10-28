# Design: Pen-ul (curbe Bézier) și căile de parcurgere

> Stare: **design de aprobat** (2026-10-28), din discuția cu utilizatorul. Nimic nu e implementat încă. După
> aprobare urmează planul de implementare (pașii, testele). Comunicarea cu utilizatorul: în română. Contextul general:
> `CLAUDE.md`; animația de până acum: `docs/plans/animatie.md` și ADR 0034. Pe acolo, „traiectoria obiectului” și
> „camera pe un traseu” erau P2; acest design le înlocuiește pe prima (pentru obiecte) și pregătește terenul pentru a
> doua.
>
> Marcaje: **(utilizator)** = decis de utilizator; **(decizia mea)** = ales de asistent, cu acordul general al
> utilizatorului („continuă”), dar fără să fi fost discutat punct cu punct.

## 1. De ce

Utilizatorul vrea să anime, de exemplu, **o mașină care intră în garajul unei case**: casa (un grup mare, complex) stă
pe loc, mașina (alt grup) urmează o linie trasă pe alee. Cu keyframe-uri de poziție și rotație, o mișcare pe o curbă
lină cere multe chei și iese colțuroasă; o cale desenată o dată, pe care obiectul merge singur, cu fața în direcția
de mers, e felul în care fac asta Lumion, Twinmotion, Blender (Follow Path) și After Effects.

Pentru desenarea căii, utilizatorul vrea **un Pen ca în Figma / Illustrator**, și îl vrea **și în modul Modelare**,
ca să poată desena forme libere (nu doar linii, arce, cercuri), editabile ulterior.

Sunt deci **două lucruri legate**, în această ordine (decizia mea, acceptată de utilizator):

1. **Pen-ul**: curbe Bézier, în Modelare (geometrie) și în Animare (căi).
2. **Căile de parcurgere**: obiecte ancorate pe o curbă desenată cu Pen-ul, cu durată, easing și loc în timeline.

## 2. Ce există acum (și unde)

| Ce | Unde | Observații |
|---|---|---|
| Linii, arce, cercuri, poligoane ca muchii (curbele = mai multe segmente, muchii soft, `curve` id pe muchie) | `core/geom/Mesh.h` (`setEdgeStyle`, `newCurveId`), `tools/ShapeTools.cpp` | Forma se **recunoaște** din geometrie (`geom::recognizeShape`), nu se păstrează parametri |
| Editarea formelor din Inspector (rază, segmente), doar înainte de Push/Pull | `geom/Shapes.h` (`segmentsEditable`) | Aceeași regulă pentru curbele Pen-ului |
| Fețe din muchii închise coplanare | `geom::drawLine` / `drawPolygon`, `geom::createFaces` | Curba închisă a Pen-ului devine față prin ele |
| Inferența (capete, mijloace, axe, grid, ghidaje), lock-uri (Shift, săgeți), VCB | `core/inference`, `tools/*` | Pen-ul o folosește ca Line |
| Obiecte de scenă cu transformare proprie (nu geometrie): camere, lumini | `model::CameraObject`, `model::Light` în `Model.h`; gizmo-uri în `Viewport::appendModelExtras`; secțiune în Outliner | Calea seamănă cu ele |
| Animația: canale pe proprietăți, easing CSS (`cubic-bezier`), chei pe cameră | `model::Animation`, `model/AnimationEval.h`, ADR 0034 | Calea e o pistă nouă în animație |
| Timeline-ul (rânduri pe obiecte, chei, tragere în grup, easing) | `shell/Timeline.qml`, `TimelineRow.qml`, `overlays/EasingEditor.qml` | Rândul căii și bara ei se adaugă aici |
| Poza la un timp (`pose()`, `shownTransform`) și redarea | `render/Document.cpp` | Calea intră în evaluarea pozei |
| Redarea / exportul „curat” (fără selecție, unealtă, pivot) | `Viewport::cleanView()` | Calea nu se vede în redare / export |
| Undo / redo pe tot modelul, inclusiv animațiile | `commands::CommandStack`, `Document::stepHistory` | Tot ce e nou trece pe aici |
| Formatul `.aion` | `core/src/io/AionFile.cpp`, `kFormatVersion = 3` | Crește la 4 |

## 3. Obiective

### 3.1 Pen-ul (P1)

- **Unealtă nouă în toolbar**, „Pen”, în Modelare și în Animare **(utilizator)**. În Modelare desenează geometrie; în
  Animare desenează o cale (secțiunea 3.2).
- **Desen ca în Figma** **(utilizator)**:
  - click = punct cu colț (fără mânere);
  - click și tragi = punct neted, cu două mânere simetrice (direcția și lungimea din tragere);
  - click pe primul punct = curba se închide;
  - Enter, Esc sau dublu click = curba se termină deschisă (Esc fără niciun segment = anulează) (decizia mea, ca în
    Figma).
  - Previzualizare: segmentul de la ultimul punct la cursor, cu mânerele (decizia mea).
- **Pe un singur plan** **(utilizator)**: planul unde dai primul click — solul (Z = 0) sau fața unui obiect / a
  geometriei. Punctele se prind de inferențe ca la Line (capete, mijloace, muchii, axe, ghidaje, grid), proiectate pe
  acest plan (decizia mea).
- **Curba închisă devine o față** (coplanară), pe care se poate face Push/Pull **(utilizator)**; deschisă rămâne o linie
  (utilă, de exemplu, pentru Follow Me).
- **Editabilă după desen** **(utilizator)**, ca în Figma:
  - dublu click pe curbă (cu Select) = editarea ei: apar punctele și mânerele;
  - tragi un punct sau un mâner (cu inferență pe planul curbei);
  - Option pe un mâner = îl rupe de pereche (punctul devine colț); Option + click pe un punct = trece între neted și
    colț (decizia mea, convenția Figma);
  - click pe curbă = adaugă un punct acolo (fără să schimbe forma); Delete / ⌫ = șterge punctul selectat;
  - Esc sau click în afară = ieși din editare.
  - Fiecare editare e un pas de undo.
- **În geometrie, curba e făcută din segmente mici, cu muchii soft / netede**, ca arcele de acum (decizia mea). Numărul
  de segmente se alege automat după lungime și curbură (de stabilit la implementare: de exemplu, o abatere maximă de
  la curbă, în mm, raportată la mărimea ei).
- **Editabilă cât e o formă plată**; după Push/Pull (fața ei face parte dintr-un volum), mânerele nu mai apar (decizia
  mea). O curbă desenată **pe fața unui obiect** (care împarte fața) rămâne și ea editabilă până la Push/Pull — deci
  regula e mai largă decât `segmentsEditable` de acum (care refuză orice muchie lipită de o a doua față): o editare
  șterge muchiile vechi ale curbei și le desenează pe cele noi, refăcând împărțirea feței-gazdă (decizia mea; costă
  geometrie și teste în plus, dar fără ea o formă desenată pe un perete n-ar fi editabilă deloc).
- **Editabilitatea rămâne doar în definiția în care s-a desenat** (decizia mea): Make Group / Component, Explode,
  copierea (Move cu Option, array, mirror), lipirea, biblioteca păstrează geometria, dar curba devine una obișnuită
  (muchii soft, ca arcele), fără mânere. Se poate extinde mai târziu.

### 3.2 Căile de parcurgere (P1)

- **Calea se desenează cu Pen-ul în Animare** **(utilizator)**: aceleași gesturi ca în Modelare; rezultatul nu e
  geometrie, ci un **obiect de scenă „Cale”** (ca o cameră sau o lumină), cu curba în coordonatele lui (decizia mea).
- **Direcția**: de la primul punct la ultimul **(utilizator)**. Fără marșarier deocamdată **(utilizator)**.
- **Inspector-ul căii** **(utilizator)**:
  - dimensiune, poziție, rotație (transformarea obiectului cale);
  - **durata** (cât durează parcurgerea toată);
  - **curba de tranziție** (easing-ul parcurgerii: lin, accelerează, frânează, curbe proprii — aceeași listă și
    același editor ca la chei, `overlays/EasingEditor.qml`);
  - **obiectele ancorate**: lista lor, cu „adaugă” și „scoate”.
- **Timeline-ul** **(utilizator)**: la desenare, calea apare singură în timeline, ca un rând cu **o bară** care începe la
  **secunda 0** și ține cât durata. Bara se trage ca să pornească mai târziu; mai multe căi pot rula în paralel, cu
  starturi diferite. Trăgând capătul barei se schimbă durata (aceeași valoare ca în Inspector) (decizia mea).
- **Calea aparține animației în care a fost desenată** (cea activă); dacă nu există nicio animație, prima cale
  desenată o creează (decizia mea). Cu altă animație activă, calea se vede în viewport și în Obiecte (estompată, cu
  numele animației ei), dar obiectele ancorate nu o urmează — în acea animație au keyframe-urile lor. Ștergerea
  animației șterge și căile ei (cu undo), iar obiectele lor revin unde erau înainte de ancorare. Un obiect stă pe cel
  mult o cale per animație.
- **Durata implicită** a unei căi noi = durata animației (decizia mea). Bara trasă sau întinsă dincolo de capătul
  animației **lungește animația** (un singur pas de undo); nu se taie.
- **La redare**, fiecare obiect ancorat merge **exact pe curbă**, **cu fața în direcția de mers** (se rotește pe
  curbe) **(utilizator)**.
- **Ancorarea** **(utilizator)**: un obiect ancorat sare pe cale (coordonatele lui vin de acolo); **scos de pe cale,
  revine exact unde era** înainte de ancorare. Concret (decizia mea, din cerința utilizatorului): ancorarea schimbă
  transformarea de bază a obiectului (`Instance::transform`) în cea de la startul căii și păstrează transformarea de
  dinainte în ancoră; scoaterea (sau ștergerea căii) o pune la loc. Așa, și în Modelare, în scene, în export și în
  Modul interacțiune obiectul e la startul căii.
- **Fiecare obiect ancorat are o poziție și o rotație proprii față de cale** **(utilizator)**: se poate muta (Move,
  Inspector), dar **relativ la cale**, ca obiectele de pe aceeași cale să aibă fiecare locul lui (de exemplu două
  mașini una lângă alta, pe benzi diferite, sau una în urma celeilalte). Concret (decizia mea):
  - decalajul se exprimă pe axele căii, la locul obiectului: **înainte / înapoi** (de-a lungul curbei), **lateral**
    (stânga / dreapta, pe planul căii) și **sus** (pe normala planului), plus o **rotire** în jurul normalei;
  - Move pe un obiect ancorat (în Modelare și în Animare) schimbă acest decalaj: mutarea se descompune pe axele căii
    de la poziția lui curentă; Rotate (în jurul normalei căii) schimbă rotirea; în Inspector, obiectul ancorat arată
    „Față de cale” (înainte, lateral, sus, rotire) în locul Poziției / Rotației obișnuite;
  - auto-key nu pune chei de poziție / rotație pe un obiect ancorat (decalajul nu se animă deocamdată);
  - transformarea de dinainte de ancorare (pentru scoatere) nu se schimbă prin aceste mutări.
- **Mai multe obiecte pe aceeași cale** nu se suprapun la ancorare (utilizatorul a lăsat decizia; decizia mea):
  - ordinea din listă = ordinea pe cale; primul are decalajul înainte 0 și merge cu durata și easing-ul căii;
  - fiecare obiect nou ancorat primește automat un decalaj **înapoi** față de ultimul din coloană (lungimile lor pe
    direcția de mers plus un spațiu mic); apoi îl poți schimba (Move sau Inspector), inclusiv lateral, ca să stea
    alături;
  - un obiect cu decalaj înapoi stă la start pe **prelungirea dreaptă** a curbei, înapoia primului punct; la final se
    oprește cu decalajul lui în spatele ultimului punct (deci în coloană, nu peste cel din față).
- **Fața obiectului** (decizia mea): la ancorare, obiectul se întoarce cât mai puțin — latura lui orizontală cea mai
  apropiată de direcția de start a căii devine fața (rotirea față de cale pornește din ea). Din listă, un buton îl
  rotește cu câte 90°.
- **Punctul obiectului care stă pe curbă** = **centrul bazei** lui (centrul feței de jos a cutiei, pe axele lui), ca o
  mașină să stea pe roți pe sol (decizia mea; pivotul e implicit în centrul cutiei și ar băga mașina în pământ).
- **Rotirea**: obiectul stă cu baza pe planul căii (sus = normala planului; pe sol: vertical) și se rotește doar în
  jurul acestei normale, fără înclinare laterală (decizia mea). Pe o cale de pe o față înclinată (o rampă), obiectul se
  înclină odată cu fața.
- **Căile închise** (decizia mea): sunt permise (un sens giratoriu, un platou rotativ). Primul obiect face exact un tur
  în durata căii; cei din coloană stau pe curbă, în spatele lui (nu pe o prelungire); cu animația în buclă, coloana se
  rotește fără oprire.
- **Ce controlează calea** (decizia mea): cât e ancorat, obiectul primește **poziția și rotirea de la cale** (plus
  decalajul lui) pe tot timeline-ul animației căii (înainte de start stă la locul de start, după final la locul de
  final); keyframe-urile lui de poziție și rotație nu se aplică; celelalte canale (scara, culoarea) rămân.
- **Vizibilitatea** (decizia mea): calea se vede în viewport cât lucrezi (o linie colorată, cu săgeata direcției și
  punctele de start / final), **nu** se vede în redare și nici în export (`cleanView`).
- **Lista Obiecte** (Outliner): căile apar acolo, cu o iconiță proprie, și se selectează ca orice obiect (decizia mea).

## 4. Non-obiective (acum)

- Marșarier / sens invers pe cale **(utilizator)**.
- Creion cu mâna liberă (Pencil din Figma, care urmează mouse-ul) (decizia mea; utilizatorul a acceptat).
- Curbe Pen în 3D (care urcă / coboară) — doar plane **(utilizator)**.
- Transformarea unei curbe din Modelare în cale (decizia mea; poate veni mai târziu).
- Camera pe cale (rămâne P2 în `animatie.md`; designul căii nu trebuie să o împiedice).
- Înclinarea pe curbe (banking), viteză constantă vs. easing pe porțiuni, opriri pe drum.

## 5. Anti-obiective (ar fi un eșec chiar dacă „merge”)

- Obiectul **alunecă** pe cale fără să se întoarcă, sau se întoarce cu smucituri (tangenta trebuie să fie netedă;
  viteza să urmeze easing-ul pe lungimea reală a curbei, nu pe parametrul Bézier, altfel accelerează pe curbe).
- Obiectul **sare** la ancorare în alt loc decât începutul căii, sau, scos, nu revine exact unde era.
- Două obiecte pe aceeași cale **se suprapun**.
- Mașina **intră în sol** sau plutește când calea e pe sol.
- Calea **apare în videoclipul exportat**.
- Pen-ul **nu se simte ca în Figma** (click / tragere / Option / închidere pe primul punct).
- Un undo care lasă timeline-ul, Inspector-ul sau viewport-ul în urmă (ce s-a reparat în `stepHistory` trebuie să
  rămână adevărat).

## 6. Constrângeri

- `core/` fără Qt; orice cod nou din `core/` vine cu teste (Catch2) — curba Bézier (eșantionare, lungime, punctul și
  tangenta la o distanță), evaluarea căii la un timp (inclusiv coloana și prelungirea), ancorarea / scoaterea,
  editarea punctelor, migrarea fișierelor.
- Comparațiile numai prin `core/math/Tolerance.h`; unitatea internă mm.
- Undo / redo pentru tot: desenul, fiecare editare, ancorarea, setările căii, tragerea barei în timeline.
- Formatul `.aion` crește la **4**; fișierele vechi se deschid neschimbate.
- UI: componentele existente (`PropertyGroup`, `NumberField`, `RowAction`, `EasingEditor`), culori din `Theme.qml`.
  Comenzile noi clasificate în `commandModes` (Pen e în ambele moduri; editarea căii doar în Animare).
- Verificare vizuală prin `AION_STARTUP` / `AION_SCREENSHOT`, cu acțiuni de script noi pentru Pen (puncte, tragere de
  mânere) și pentru căi.
- ADR nou pentru deciziile de mai sus (curba păstrată, obiectul cale, evaluarea).

## 7. Ce rămâne la constructor

- **Unde se păstrează curba Bézier în Modelare** (puncte + mânere + planul), legată de muchiile ei (de exemplu prin
  `curve` id-ul de pe muchii), și cum se detectează că geometria a fost schimbată altfel (Move pe un vârf, Push/Pull),
  ca să nu mai fie editabilă ca Bézier.
- Structura obiectului cale (`model::MotionPath`?) și a pistei din animație (start, durată, easing, ancore cu decalaj
  înainte / lateral / sus, rotire, transformarea de dinainte de ancorare).
- Curba paralelă pentru decalajul lateral pe curbe strânse (unde decalajul e mai mare decât raza curburii).
- Eșantionarea curbei, parametrizarea după lungime, prelungirea dreaptă pentru coloană.
- Gesturile exacte de editare (praguri de tragere, ce se întâmplă cu dublu click pe o curbă după Push/Pull).
- Cum arată rândul căii în timeline (bara, culoarea, numele, obiectele ancorate sub ea).
- Ordinea pașilor de implementare (Pen în Modelare → editare → Pen în Animare / căi → timeline → fișier).
