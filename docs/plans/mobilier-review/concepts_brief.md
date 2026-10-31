# Aion pentru proiectarea de mobilier pe corp

* **Autor:** Eduard Boboc (cu analiza EGM, patru runde)
* **Data:** 2026-10-30

## 1. Prezentare

Aion e un modelator 3D nativ (C++/Qt), inspirat de SketchUp, făcut de un singur om. Propunerea: Aion se îndreaptă
spre proiectarea de mobilier pe corp (dulapuri, corpuri de bucătărie, rafturi), pentru amatori avansați și ateliere
mici, în România și apoi internațional.

Nișa în care Aion poate câștiga bani:

> Pe macOS sau în browser, construiești un dulap din plăci parametrice și feronerie luate din bibliotecă. Apoi trimiți
> o listă de debitare, cu cant pe fiecare latură și cu direcția fibrei, într-un fișier pe care o firmă de debitare din
> România îl acceptă. Primești și un deviz simplu și o imagine de prezentat clientului.

Concurentul real e **SketchUp + OpenCutList**. OpenCutList e gratuit și open-source, dar rulează doar în SketchUp
desktop, care se plătește cu abonament anual; SketchUp Free, din browser, nu acceptă extensii. PRO100, Polyboard și
Mozaik vând ieșiri de producție (CNC, găurire, cataloage de feronerie) și rulează doar pe Windows. Contextul complet
(inventar, concurenți, surse) e în [`brainstorm_context.md`](brainstorm_context.md).

## 2. Propunerea

### 2.0 Glosar

- **Placă parametrică:** o placă luată din bibliotecă. Lungimea și lățimea ei se schimbă din parametri, în Inspector.
- **Placă brută (format):** foaia din care taie firma de debitare (de exemplu 2800 × 2070 mm). Nu e același lucru cu
  o placă din model.
- **`BoardSpec`:** datele de piesă ale unei plăci, păstrate pe definiția ei: tip, grosime nominală, axa grosimii,
  axa fibrei, cantul pe cele 4 laturi locale. Toate copiile legate le au la fel.
- **Feronerie:** un obiect etichetat ca balama, glisieră sau mâner. Se numără pe bucată.
- **Decor:** materialul de aspect (alb, nuc, stejar). Stă pe instanță, deci copiile legate pot avea decoruri diferite.
- **Grosime nominală / măsurată:** nominală = cea din `BoardSpec` (PAL 18). Măsurată = dimensiunea reală pe axa
  grosimii.
- **Cutie simplă:** o placă cu 6 fețe și 8 vârfuri, fără sub-obiecte, ale cărei dimensiuni se potrivesc cu parametrii.
- **Capăt:** oricare dintre cele 4 fețe înguste ale plăcii. Cele două fețe mari sunt fețele de grosime.
- **Modificată manual:** o placă parametrică ce nu mai e cutie simplă, are o grosime nepermisă sau e scalată.
  Starea se calculează din geometrie de fiecare dată; nu se salvează.
- **Readu la parametri:** reface placa din parametri și pierde modificarea manuală. Un pas de undo.
- **Desprinde de parametri:** placa devine o cutie obișnuită cu `BoardSpec`, măsurată din geometrie.
- **Transformă în corp obișnuit:** un corp generat (dulap, raft) nu se mai regenerează. Plăcile din el rămân
  parametrice.
- **Latură locală:** −X, +X, −Y, +Y în axele plăcii. Cantul se salvează pe ele. Numele L1, L2, W1, W2 sunt doar
  pentru afișare și export (2.4).
- **Dimensiune finită / de tăiere:** finită = cu cant. De tăiere = finită minus cantul marcat „se scade”.
- **Adaos:** milimetrii de cant adăugați pe fiecare muchie la calculul metrilor de cant.
- **Componentă salvată:** un corp salvat cu „Salvează în bibliotecă”. Are o cheie stabilă (`sourceKey`).
- **Lista „În model”:** componentele din documentul deschis, afișate în bibliotecă.
- **Întinde:** taie un corp cu un plan și schimbă parametrii de lungime ai plăcilor tăiate de plan. Celelalte plăci
  se mută.
- **Categoria materialului:** lemn, metal, sticlă, plastic, piatră. Contează doar pentru obiectele fără `BoardSpec`.

### 2.1 Modelul de date

- **`BoardSpec` pe `Definition`**, ca ultim câmp (`Model.h:266-279`), salvat ca bloc JSON opțional. Păstrează: tip,
  grosime nominală, axa grosimii, axa fibrei și cantul pe 4 laturi locale (tip de cant ca cheie text, decorul cantului
  ca cheie sau „ca placa”). `PartLabel` și `BoardSpec` sunt aceeași structură; pentru feronerie ea are tipul
  „feronerie” și cheia variantei.
- **`Definition::parameters`** păstrează doar numerele: lungimea și lățimea. Tipul, grosimea și cantul nu se scriu
  acolo, pentru că un text în `parameters` face versiunile vechi să refuze tot fișierul (`AionFile.cpp:820`).
- **Inspector-ul** arată tipul, grosimea și cantul ca opțiuni, dar le citește și le scrie în `BoardSpec`. Lista de
  grosimi depinde de tip (PAL → 16/18/25, HDF → 3/4).
- **Decorul stă pe `Instance::material`.** Placa are fețe fără material propriu, deci găleata pe placă o colorează.
- **Geometria plăcii stă în definiția ei proprie**, nu într-un subgrup ca la generatoarele de azi
  (`Parts::box`, `Parametric.cpp:78-97`). Modelul de urmat e `setPrimitiveSegments` (`Modeling.cpp:234-269`).
- **Axele plăcii:** X = lungime, Y = lățime, Z = grosime, cu originea în colțul minim. Fibra e pe X implicit; poate
  fi pe Y, iar atunci doar etichetele L și W își schimbă locul.
- **Tabele în core:** `BoardType` (cheie, nume, grosimi permise, dacă merge la firma de debitare, unitate) și tipurile
  de cant (cheie, grosime, tipuri de placă compatibile, „se scade”).
- **Cheile stabile:** `sourceKey` (UUID) pe definițiile componentelor salvate; `libraryKey` pe materiale.
- **Drumuri pe care `BoardSpec` și `sourceKey` trebuie păstrate sau tratate explicit:** salvarea în fișier, „Salvează
  în bibliotecă” (`extractInstance`, `Transfer.cpp:147-160`, azi le pierde), inserarea, ⌘V, Make Unique, booleenele
  (`Boolean.cpp:103`, azi creează un grup nou), Explode (avertizează). ⌘V și Make Unique șterg `sourceKey` pe copie și
  îl păstrează pe original.

### 2.2 Placa parametrică (D12, D20)

- **Se trage din bibliotecă** și se redimensionează din parametri: lungime, lățime (numere), tip și grosime (opțiuni).
- **Câmpurile „Dimensiuni (mm)”** scriu în lungime și lățime; Z e doar citire. Rotația nu contează (dimensiunile sunt în
  axele plăcii). La o placă oglindită se ia valoarea absolută a scării.
- **Push/Pull pe un capăt** cât placa rămâne cutie simplă: noile dimensiuni intră automat în parametri, fără marcaj.
- **Push/Pull pe o față de grosime, Bevel, decupaje, desen în interior:** placa devine „Modificată manual”. Grupul
  „Parametri” arată starea, cu „Readu la parametri” și „Desprinde de parametri”. Lista de debitare o marchează.
- **Scale pe o placă (D24)** funcționează, iar placa devine „Modificată manual”. Consecințe de spus în interfață:
  decorul se întinde odată cu placa, o placă rotită oblic se poate deforma, iar Scale pe un corp întreg schimbă și
  grosimile, deci toate plăcile din el devin „Modificate manual”. Lista de debitare folosește dimensiunile măsurate.
  „Readu la parametri” readuce scara instanței la 1. Calea recomandată rămâne câmpul „Dimensiuni”.
- **Copiile legate:** orice schimbare de parametri, „Readu” și „Desprinde” lucrează pe definiție. Inspector-ul arată
  „Afectează N copii” (există deja în `EntityInfo.qml:145-148`). Pentru o singură copie: Make Unique întâi.
- **Zecimale:** câmpurile din Inspector sunt azi întregi (`EntityInfo.qml:131-142`). Trebuie zecimale, altfel cantul
  ABS 0,4 devine 0.

### 2.3 Cantul (D19, D21)

- **Pe fiecare latură locală:** fără cant sau un tip de cant (ABS 0,4 / 1 / 2 mm, melamină), cu decorul lui („ca
  placa” sau altul).
- **Numele laturilor:** cu fibra pe X, L1 = −Y, L2 = +Y (laturile lungi), W1 = −X, W2 = +X (capetele). Ce latură e
  „fața” se confirmă cu firma de debitare.
- **În Inspector:** un dreptunghi cu 4 laturi pe care dai click, trei presetări incluse („fără”, „toate 2 mm”, „față
  2 mm, rest 0,4”) și presetări salvate de utilizator. Latura peste care treci cu mouse-ul se evidențiază în viewport
  (decizia P-5 din design).
- **Dimensiunea de tăiere:** lungime − cant(W1) − cant(W2) și lățime − cant(L1) − cant(L2), doar pentru tipurile
  marcate „se scade”.
- **Exportul implicit (D21):** dimensiunea finită plus coloanele de cant, cu un rând de antet care spune ce dimensiune
  trimite. Modul „de tăiere” se alege explicit. Motivul: dacă Aion scade și firma scade încă o dată, piesele ies prea
  mici și nu se mai pot repara.
- **Metri de cant pe tip:** suma laturilor cu cant, pe dimensiunea finită, plus un adaos pe muchie (global sau pe tip
  de cant, decizia P-5 din design).
- **Oglindirea nu mută cantul.** Reflexia stă pe instanță (`Modeling.cpp:58-62`). Ce mută cantul e rotirea cu 180°,
  cum e și normal fizic.

### 2.4 Lista de debitare și raportul (D10, D17, D22)

- **Dimensiunile:** o placă în acord cu parametrii → parametrii. O placă modificată manual → dreptunghiul măsurat, cu
  marcaj („L param 600, măsurat 640”). O placă cu decupaj apare cu dreptunghiul exterior.
- **Clasificarea:** `BoardSpec` placă → lista de plăci; feronerie → o bucată, fără sub-piese; fără `BoardSpec` →
  după D10.
- **Gruparea** după tip, grosime, dimensiuni, decor, cant pe cele 4 laturi, decorul cantului și starea ascuns/vizibil.
  Numele apar în coloana „folosită în”.
- **Piesele ascunse (D17):** toate piesele apar, inclusiv cele ascunse și tot ce e în ele (azi o ușă ascunsă își ia cu
  ea și balamalele, `Quantities.cpp:108-110`). Fiecare rând are o bifă: vizibile bifate, ascunse debifate. Piesele
  ascunse au rânduri separate. Bifele se resetează la fiecare deschidere a ferestrei; se păstrează când schimbi
  formatul plăcii în fereastră.
- **Totalurile** (plăci brute, cant, deviz) și CSV-ul se calculează în core, doar din rândurile bifate.
- **Banda D22:** „N piese ascunse nu sunt incluse — Include toate”, plus un avertisment la export.
- **D17 se aplică doar listei de debitare**, nu și raportului general de cantități.

### 2.5 Biblioteca și legarea (D11)

- **Plăcile** sunt intrări `board:<tip>` construite în cod; grosimea și decorul se aleg la plasare sau în Inspector.
  Fiecare drop creează o placă nouă; plăcile se leagă doar cu ⌥D sau Array.
- **Feroneria generică** (balamale aplicate / semi-aplicate / încastrate, la 110° / 165°; glisiere) vine ca fișiere
  incluse, etichetate. Schimbarea variantei înlocuiește definiția (`replaceDefinition`) pe toate copiile legate.
- **Componentele salvate se leagă:** „Salvează în bibliotecă” scrie un `sourceKey` și în fișier, și pe definiția din
  model (un pas de undo), deci corpul original și drop-urile următoare sunt legate. Un drop se leagă de definiția cu
  același `sourceKey` dacă ea are măcar o instanță în arborele modelului (ascunse incluse). Altfel creează o
  definiție nouă. Un corp generat și apoi salvat se leagă și el.
- **Definițiile fără instanțe** rămân în fișier (`Operations.cpp:132`), dar nu se refolosesc.
- **Un drop în interiorul aceluiași corp** (ar crea un ciclu) creează o definiție nouă.
- **Fișierul din bibliotecă schimbat după inserare:** câștigă definiția din model, ca în SketchUp.
- **ADR 0039 rămâne valabil:** ⌘V și Move cu ⌥ fac copii unice, spre deosebire de SketchUp.
- **Accelerări:** comanda „Fă placă” (etichetează o cutie existentă, utilă și pentru fișierele vechi), unealta „Placă
  pe față”, corpuri de pornire incluse.
- **Fără cataloage de mărci** incluse.

### 2.6 Corpurile și „Întinde”

- **Generatoarele de dulap și raft** rămân ca start rapid. Produc plăci parametrice cu geometrie proprie (spate HDF 3,
  mânere ca feronerie). „Transformă în corp obișnuit” oprește regenerarea corpului; plăcile rămân parametrice.
- **Un dulap vechi** (fișier v5) rămâne cum e. Inspector-ul arată „Generator vechi — Actualizează”; abia la apăsare
  e refăcut cu plăci parametrice (D23), într-un pas de undo.
- **„Întinde”** schimbă parametrii de lungime ai plăcilor tăiate de plan și mută restul. Prima versiune: un plan
  aliniat cu axele, refuz dacă planul taie o placă prin grosime, feroneria doar se mută. Pentru corpuri cu N uși:
  „Întinde în N locuri egale”. Depinde de D15.

### 2.7 Limba interfeței (D5, D18)

- Acum nu există niciun `qsTr` în `app/qml`; sunt aproximativ 700–1000 de texte în 46 de fișiere QML, plus câteva
  sute în `core/` și `render/`.
- Se decid întâi limba sursă (engleză sau română), limba de rezervă și cine verifică traducerile.
- Textele din `core/` se marchează cu un macro gol `AION_TR` și se traduc la granița cu `render/`.
- Schimbarea limbii se aplică după repornire. Exportul pentru firmă urmează formatul firmei, nu limba interfeței.
- Ordinea: infrastructura, româna și engleza complete, celelalte 6 traduse automat, marcate „beta”.
- Ecranele înghețate (animație, interacțiuni) se ascund în profilul de mobilier sau rămân doar în română.

## 3. Riscuri

| Risc | Gravitate | Ce facem |
|---|---|---|
| Un singur om, prea multe fronturi | Blocant | Lista înghețată (secțiunea 4) trecută în `PLAN.md` și în ADR 0043 |
| Un singur tester, amator | Blocant înainte de a cere bani | Ruda la pasul 0 și la două puncte de control |
| Formatele firmelor de debitare din RO sunt necunoscute | Blocant pentru „gata de trimis” | O comandă reală de la rudă devine test de referință |
| Firma scade cantul a doua oară | Major | Exportul implicit = dimensiunea finită, cu antet explicit (D21) |
| Formatul: build-urile vechi (și cel web) pierd câmpurile noi la salvare | Major | v6, cu refuz clar în build-urile vechi (D13) |
| `BoardSpec` se pierde pe drumuri ascunse (bibliotecă, booleene, Explode) | Major | Regulă și test pentru fiecare drum (2.1) |
| Identitatea din bibliotecă se strică la redenumirea fișierului | Major | `sourceKey` în fișier, nu calea (2.5) |
| Ușile ascunse ies din deviz | Major | Banda D22 și avertismentul la export |
| Un dulap vechi se schimbă la prima editare | Major | D23 |
| „Întinde” pe corpuri legate schimbă toate copiile | Major | D15, înainte de pasul 11 |
| Aplicația nu e semnată, nu se actualizează singură, nu raportează crash-uri | Major | Înainte de a cere bani |
| Prețul e plafonat de SketchUp + OpenCutList | Major | Se plătește pentru bibliotecă, lista corectă, exportul pentru firmă și deviz |

## 4. Obiective vs. non-obiective

### Obiective, în ordine

| # | Efort | Pas | Acceptare |
|---|---|---|---|
| 0 | S, fără cod | ADR 0043 (domeniul mobilier, lista înghețată) și 0044 (`BoardSpec`, legarea); sesiunea cu ruda | Fișierele ADR există și conțin D10–D24; răspunsurile rudei sunt notate; fiecare decizie trimite la o secțiune |
| 1 | M | Formatul v6; `BoardSpec` și `sourceKey` pe `Definition`; păstrate de extragere și booleene; Explode avertizează | `BoardSpec` supraviețuiește salvării, inserării, extragerii, ⌘V, Make Unique și scăderii; `sourceKey` e șters pe copie și păstrat pe original; un fișier v5 se deschide; un fișier v7 e refuzat clar |
| 2 | S | Tabelele `BoardType` și tipuri de cant; categoria materialului (D14) și `libraryKey` | PAL {16, 18, 25}, HDF {3, 4}; fiecare tip de cant e compatibil cu tipurile lui de placă; „Oțel periat” are categoria metal |
| 3 | S–M | `measurePart`, verificarea „cutie simplă”, comanda „Fă placă” (prima sursă de plăci etichetate) | Cutie 600×400×18 → fără marcaj; fața de grosime trasă la 36 → marcaj; Bevel → „formă neregulată”; rotire 37° în jurul lui Z → tot 600×400×18, fără marcaj |
| 4 | M | Lista de debitare v1.5 cu D17 și D22: vizibilitatea moștenită de la părinți, cheia de grupare din 2.4, excluderile în core | Dulapul de referință (1000×2000×600, 2 uși): lateralele ×2, ușile ×2, spatele HDF 3, mânerele ×2 la feronerie; o laterală ascunsă are rând separat, debifat; balamalele unei uși ascunse apar, debifate; banda „1 piesă ascunsă…”; CSV-ul are doar rândurile bifate |
| 5 | M–L | Placa parametrică (2.2): geometrie proprie, câmpuri zecimale, opțiuni cu listă dependentă de tip, Dimensiuni → parametri, Push/Pull pe capăt → parametri, starea „Modificată manual”, „Readu”, „Desprinde”, Scale refuzat | Găleată, apoi altă lungime → decorul rămâne; Push/Pull pe capăt la 800 → lungime 800, fără marcaj (și pe lățime); fața de grosime la 20 → marcaj; Bevel → marcaj, „Readu” → cutie, undo → bevel; Scale ×1,5 → marcaj, „Readu” → scara 1; placă rotită 90° în jurul lui Z, Dimensiuni Y = 700 → lungime 700; oglindită → fără marcaj; „Readu” pe 4 copii legate → „Afectează 4 copii”; lista arată noua lungime |
| 6 | S–M | Generatoarele produc plăci parametrice cu geometrie proprie; „Transformă în corp obișnuit” | Un dulap v5: deschis → neschimbat, cu „Actualizează”; după apăsare → plăci parametrice, un pas de undo, lista corectă |
| — | — | **Punctul de control 1 cu ruda** | |
| 7 | M | Cantul (2.3) | 2 uși de 498,5 × 2000, ABS 2 mm „se scade” pe toate laturile → tăiere 494,5 × 1996; ABS 0,4 nescăzut → 498,5 × 2000; metri: 4 × 2498,5 + 8 × 50 = 10 394 mm; oglindirea păstrează cantul; rotirea cu 180° în jurul lui Z îl mută în spate; schimbarea fibrei schimbă doar etichetele L/W |
| 8 | S–M (M–L cu lista „În model”) | Legarea D11 (2.5), inclusiv corpurile generate salvate | 4 drop-uri „Corp 600” → o definiție; originalul se leagă după „Salvează în bibliotecă”; toate șterse, apoi drop → definiție nouă; ⌘V, apoi drop → se leagă de original; plăcile nu se leagă niciodată; un drop în sine însuși nu creează ciclu |
| 9 | M | Feronerie generică, deviz pe bucăți, `replaceDefinition` | Corp cu 2 uși × 2 balamale, plus 3 copii ⌥D → 16 balamale; schimbarea variantei pe o balama legată → „Afectează N copii” |
| — | — | **Punctul de control 2 cu ruda** | |
| 10 | M | Unealta „Placă pe față” | Un dreptunghi pe o față de 600 × 720 → placă PAL 18 cu axa grosimii pe normala feței |
| 11 | L | „Întinde” cu parametri, după D15 | Corp 600 → 800 fără fisuri și fără marcaje; două uși cu două planuri → uși egale; plan prin grosime → refuz |
| 12 | M, apoi L | Traducerea (D18) | Infrastructura, apoi RO și EN complete, verificate în CI |
| Apoi | — | Fibra în export, exportul pentru firmele de debitare, prețuri, semnarea și distribuția | — |

Versiunea minimă: pașii 0–6. Primul lucru vizibil pentru utilizator: lista de debitare corectă pentru orice model
(pașii 1–4).

### Non-obiective

- **Înghețate (rămân funcționale și se repară, fără funcții noi și fără traducere):** animația, timeline-ul,
  regulile, Modul Interacțiune, DOF, video, asistentul AI, comentariile, colaborarea, Match Photo, iPad, importul
  `.skp` / COLLADA / IFC / DWG. ADR-uri: 0018, 0020, 0021, 0033, 0034, 0035. ADR 0039 rămâne valabil, iar ADR 0044
  adaugă doar ștergerea `sourceKey` pe copii. ADR 0025 nu e înghețat.
- **Nu se construiesc:** CNC, G-code, nesting pe forma reală, găurire automată, cataloage Blum / Hettich / Häfele
  incluse.
- **Nu acum:** dialogul de grosime, XLSX, licențiere, planșe 2D, schimbarea limbii fără repornire, limbi de la dreapta
  la stânga.

## 5. Alternative cântărite

| Alternativă | Decizie | De ce |
|---|---|---|
| Concurență cu PRO100 / Polyboard / Mozaik pe funcții de producție | Respinsă | Ani de muncă; ei sunt pe Windows |
| Funcțiile de mobilier ca plugin JS | Respinsă pentru partea plătită | Plugin-urile nu pot crea generatoare și nu văd lista |
| Aion pentru arhitecți, designeri și tâmplari deodată | Respinsă | Trei produse pe jumătate |
| Tipul și grosimea pe material, cu dialog la nepotrivire (runda 2) | Înlocuită | „Ajustează” ar fi șters materialul pus; un singur `board` pentru tot corpul; undo imposibil direct |
| Plăcile ca simple cutii etichetate, fără parametri (runda 3) | Înlocuită de D12 | Utilizatorul vrea să schimbe dimensiunile din Inspector, ca la ușă și scară |
| Plăcile ca generatoare cu subgrup (`Parts::box`) | Respinsă (runda 4) | Container gol la dublu-click, alt nume în listă, decorul ascuns |
| Cantul și tipul în `parameters` | Respinsă (runda 4) | Un text acolo face versiunile vechi să refuze fișierul; id-urile de material nu se remapează |
| Câmpuri noi opționale în v5 | Respinsă (runda 4) | Build-urile vechi le pierd la salvare, fără avertisment |
| „Modificată manual” salvată sau după revizia mesh-ului | Respinsă (runda 4) | Revizia nu se salvează și se schimbă la undo |
| Numele L1/L2 salvate în loc de laturi locale | Respinsă (runda 4) | Schimbarea fibrei ar muta cantul pe alte muchii |
| Oglindirea schimbă laturile de cant | Respinsă (runda 4) | Reflexia stă pe instanță; cantul rămâne pe muchiile fizice corecte |
| Legarea tuturor drop-urilor din bibliotecă | Respinsă (runda 4) | Toate plăcile PAL 18 ar deveni aceeași definiție |
| O intrare în bibliotecă pentru fiecare variantă de placă | Respinsă | Sute de fișiere; acceptată doar pentru feronerie |
| Decorul în `BoardSpec` | Respinsă | Copiile legate n-ar putea avea decoruri diferite |
| Redimensionarea plăcii prin scalare | Respinsă | Întinde decorul, deformează plăcile rotite |
| Scalare proporțională a corpului cu grosimea blocată | Respinsă | Lasă fisuri: 600 → 800 mm, baza iese cu 12 mm prea scurtă |
| O cutie cu „Nuc” trimisă automat la lemn masiv | Respinsă | „Nuc” e de obicei decor pe PAL |
| Doar plăci puse una câte una, fără generator | Respinsă | 30–40 de acțiuni pentru un corp simplu |
| Nesting devreme | Amânat | Firmele de debitare au optimizatoarele lor |
| Abonament lunar | Greu de justificat | Concurează cu abonamentul SketchUp |
| Toate cele 8 limbi complete deodată | Respinsă | Româna și engleza complete, restul beta |

## 6. Decizii și întrebări

### Decizii

| # | Decizie | Stare | Secțiune |
|---|---|---|---|
| D1 | Ținta: mobilier pe corp, amatori avansați și ateliere mici; macOS și web întâi | Luată | 1 |
| D2 | Concurentul de referință: SketchUp + OpenCutList | Luată | 1 |
| D3 | ~~Tip și grosime pe material~~ | Înlocuită (runda 3) | 5 |
| D4 | ~~Dialog la nepotrivirea de grosime~~ | Înlocuită (runda 3) | 5 |
| D5 | Limbi: RO, EN, DE, PL, IT, FR, ES, PT; RO și EN complete, restul beta | Luată | 2.7 |
| D6 | „Înghețat” = funcțional și reparat, fără funcții noi și fără traducere | Luată | 4 |
| D7 | Ruda care lucrează în mobilă e validatorul, de la pasul 0 | Luată | 3, 6 |
| D8 | Plăcile și feroneria sunt obiecte în bibliotecă; nepotrivirile doar marcate | Luată | 2.2, 2.5 |
| D9 | O placă se redimensionează din parametri; un corp cu „Întinde” | Luată | 2.2, 2.6 |
| D10 | Obiectele fără `BoardSpec`: categoria metal / sticlă / plastic / piatră → „Alte componente”; lemn → „nedefinit (lemn)”; fără categorie → regula de azi, marcat „tip nedefinit” | Luată (utilizator, 2026-10-30) | 2.4 |
| D11 | Componentele salvate se leagă, ca în SketchUp; plăcile și generatoarele nu | Luată | 2.5 |
| D12 | Placa are parametri în Inspector; modificările manuale o marchează „Modificată manual” | Luată | 2.2 |
| D13 | Formatul: v6; build-urile vechi refuză clar fișierele noi | Luată (utilizator, 2026-10-30) | 2.1 |
| D14 | Categoria materialului intră acum (pasul 2) | Luată (utilizator, 2026-10-30) | 2.0, 2.4 |
| D15 | „Întinde” pe corpuri legate: întreabă „fă unic?” sau schimbă toate copiile | De decis înainte de pasul 11 | 2.6 |
| D16 | Corpurile generate: comanda „Transformă în corp obișnuit” | Luată (utilizator, 2026-10-30) | 2.6 |
| D17 | Raportul arată și piesele ascunse, debifate; bifele se resetează la deschidere | Luată | 2.4 |
| D18 | Traducerea înainte sau după interfața pentru plăci și feronerie | De decis la punctul de control 2 | 2.7 |
| D19 | Cantul pe fiecare latură e parte din placă, editat în Inspector | Luată | 2.3 |
| D20 | Push/Pull pe un capăt al unei cutii simple actualizează parametrii | Luată | 2.2 |
| D21 | Exportul implicit: dimensiunea finită plus coloanele de cant; bifa „se scade” pe tipul de cant | Luată; detaliile firmei se confirmă cu ruda | 2.3 |
| D22 | Banda „N piese ascunse nu sunt incluse — Include toate” | Luată | 2.4 |
| D23 | Un dulap vechi se reface cu plăci noi doar când utilizatorul apasă „Actualizează”; până atunci rămâne cum e | Luată (utilizator, 2026-10-30) | 2.6 |
| D24 | Scale pe o placă funcționează și o marchează „Modificată manual”; „Readu la parametri” readuce scara la 1 | Luată (utilizator, 2026-10-30) | 2.2 |

### Alegeri de amploare propuse de recenzorul de simplitate (de decis în `/egm-draft`)

- Lista „În model”: acum (M) sau mai târziu, cu Outliner-ul ca înlocuitor.
- Pivotul la schimbarea dimensiunilor: păstrat pe loc (cum e în plan) sau placa crește din colțul minim, ca ușa și
  scara de azi.
- Pentru prima versiune: evidențierea laturii în viewport, decorul de cant diferit de placă, presetări proprii și un
  adaos pe tip de cant pot fi amânate.

### Întrebări pentru rudă (pasul 0)

- O comandă reală: lista lui de debitare și fișierul cerut de firma de debitare.
- Ce tipuri și grosimi de plăci folosește? Ce decoruri?
- Ce balamale și glisiere folosește? Cum le trece în deviz?
- Firma vrea dimensiunea finită sau de tăiere? Ce cant folosește, cât adaos pune pe muchie, cum numește laturile și
  care e „fața”?
- Când schimbă lățimea unui corp, cum o face azi?
- Corpurile identice dintr-o bucătărie: vrea să rămână legate sau independente?
- Feroneria din piesele ascunse trebuie să intre în deviz?

## Anexă: istoricul rundelor

- **Runda 1** a stabilit nișa și concurentul și a găsit erorile din lista de debitare de azi: mânerele din oțel intră
  ca plăci (`Parametric.cpp:199`, `Quantities.cpp:362`); piesele identice cu nume diferite nu se adună
  (`Quantities.cpp:364`); fibra se pierde; cantul e doar estimat; spatele de 5 mm e din materialul corpului; materialul
  piesei poate ieși diferit de la o rulare la alta. Primele patru se rezolvă în pașii 1–5; cantul în pasul 7.
- **Runda 2** a respins tipul pe material cu dialog de grosime: „Ajustează” ar fi șters materialul pus
  (`Parametric.cpp:336-366`), `board` e o singură valoare limitată la 10–50 mm, iar un pas de undo nu se poate face
  direct (`PaintTool.cpp:32`, `CommandStack.cpp:56`).
- **Runda 3** a propus plăcile și feroneria ca obiecte etichetate și a găsit că doar ⌥D și Array leagă copiile, că
  „Salvează în bibliotecă” și booleenele pierd datele de piesă și că Scale întinde decorul.
- **Runda 4** a analizat placa parametrică, cantul, legarea și bifele din raport. Sintetizatorul s-a oprit o dată la
  limita de utilizare, apoi a fost reluat; toți cei șase recenzori au terminat. Concluziile sunt integrate în
  secțiunile 2–6.
