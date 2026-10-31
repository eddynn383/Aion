# Goldfish Annotated Review Report – runda 2

**Status: resolved** (2026-10-30: auto accept pentru punctele 1, 2, 4–12 și formulările de la 13; punctul 3 decis de proprietar ca R-9 = varianta (a), id-uri noi fără `generatorVersion`, ceea ce înlocuiește contestarea acceptată în runda 1)

**Target:** `docs/plans/mobilier-pe-corp.md` (1–753, cu efectele în planul detaliat 754–1263)
**Sursa:** `critic_report_round2.md`

## Prioritized Findings & AI Suggestions

### [1] Compensarea la mutarea originii – Severity: High (R2-B1)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** §3 „Compensarea”: `translație += L_inst·d` (L = partea liniară a transformării instanței);
    pivotul explicit `pivot −= d`, scris direct (fără `setPivot`, fără rebazarea animațiilor); `Scene::transforms[i]`
    și `PathAnchor::before` cu `+= L_salvat·d`; cheile de animație și ancorele relative neschimbate. Intrarea 27
    (`shiftBoardOrigin`) și testele de la intrarea 35: placă rotită 90° în jurul lui Y, cu pivot explicit și chei de
    poziție; scenă salvată cu placa oglindită.

### [2] Construcția corpurilor și dulapul de referință – Severity: High (R2-M1, R2-M9)
*   ***AI Suggestion:*** PROPOSE FIX (provizoriu până la pasul 0)
*   ***Proposed Edit:*** §4: rost de 3 mm **doar între uși**; ușile aplicate pe toată înălțimea și lățimea corpului;
    spatele HDF **încastrat** între laterale, bază și capac (cum face generatorul de azi). Un paragraf nou „Dulapul de
    referință” în §4: ieșirea generatorului „Dulap cu uși” cu lățime 1000, înălțime 2000, adâncime 600, 2 uși → laterale
    2000×600, bază și capac 964×600, spate 964×1964, uși 498,5×2000 cu 4 balamale și un mâner. Obiectivul 1 trimite la
    §4; fixture-ul de la intrarea 26 = ieșirea generatorului.

### [3] `generatorVersion` și id-urile noi – Severity: High (R2-M2)
*   ***AI Suggestion:*** ESCALATE (alegere a proprietarului)
*   ***Justification:*** contestarea acceptată la runda 1 a păstrat `generatorVersion`, dar §4 și pasul 6 introduc id-uri
    noi. Ambele variante sunt apărabile:
    *   **(a)** id-uri noi (`base`, `wall`, `tall`, `appliance`, `wardrobe`, `shelf2`); `generatorVersion` dispare;
        „Actualizează” transformă `cabinet` → `wardrobe`, `shelf` → `shelf2`; alternativa L rescrisă.
    *   **(b)** `generatorVersion` rămâne și marchează doar generatoarele de corp; `createParametric` pune 2 doar pentru
        ele; corpurile noi au id-uri noi, iar `cabinet` / `shelf` v1 rămân doar pentru fișierele vechi.

### [4] Reguli pentru editările de structură din corp – Severity: Medium (R2-M3)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** §4: rolurile se caută printre sub-piesele directe ale corpului și în grupurile cu rol (ușa);
    copia unei singure piese (⌘V, ⌥Move) sau o piesă mutată în alt părinte pierde `roleKey` și `relativePosition`;
    copia unui corp întreg le păstrează; Explode, Make Group și mutarea în alt părinte pe piese dintr-un corp generat cer
    confirmarea „Corpul devine obișnuit (nu se mai reface)” și apoi `makePlainBody`; la mai multe piese cu același rol,
    id-ul cel mai mic. Rânduri noi în tabelul §7 și la riscuri. Teste la intrarea 38.

### [5] Ce suprascrie reconcilierea – Severity: Medium (R2-M4)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** §4: reconcilierea scrie transformarea și parametrii pieselor cu rol (poza închisă pentru uși,
    cu `Behavior::open = false`); avertismentul din Inspector apare dacă vreo piesă cu rol diferă de ce ar produce
    generatorul (formă, dimensiuni sau poziție); cheile de animație ale pieselor cu rol se rebazează cu diferența
    transformării de bază. La riscuri: „modificări făcute de mână pieselor cu rol se pierd”, cu avertismentul ca măsură.

### [6] Separatoarele puse cu clic – Severity: Medium (R2-M5)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `Instance::relativePosition` devine `std::optional<double>` (fracția din lățimea interioară).
    Separatorul pus cu clic: toată înălțimea și adâncimea interioară, grosimea câmpului `board`; împarte polițele ca
    separatoarele din câmp; rolurile polițelor includ spațiul (`space.2.shelf.1`); înainte de refacere, fracția se
    recalculează din poziția curentă (o mutare de mână se păstrează); la clic: la mijlocul spațiului, iar cu ⌥ la
    poziția clicului; distanța minimă față de alt separator = 50 mm, altfel refuz.

### [7] Finalizatorul și plăcile doar mutate – Severity: Medium (R2-M6)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** §3: nimic de făcut doar dacă dimensiunile sunt egale **și** colțul minim e în 0; preluare doar
    dacă dimensiunea pe `thicknessAxis` = grosimea nominală și fiecare față își păstrează axa normalei.

### [8] Datele în coordonatele plăcii – Severity: Medium (R2-M7)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** finalizatorul sare plăcile cu sub-piese; ghidajele, adnotările și curbele Pen cu contextul în
    definiția plăcii se mută cu −d; teste.

### [9] Liniile de cant și ADR 0038 – Severity: Medium (R2-M8)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** liniile persistente în `DocumentPass`, setarea „arată cantul” în `DocumentPassCache::Key`; doar
    evidențierea laturii de sub cursor în `appendModelExtras`; cerința de performanță: fps în orbită pe modelul de stres
    neschimbat față de azi (± 5%).

### [10] Glosar și termeni – Severity: Medium (R2-M10, R2-M11 + minore)
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** L1/L2 „paralele cu fibra; fără fibră, cele lungi”; „instanță” și „apariție” definite, „Afectează N
    apariții”; intrări noi: „Desprinsă”, „În acord cu parametrii”, „decor”, „finalizator”, „rol”; PAL / MDF / HDF
    paralel; „latura din față”; un singur nume pentru fiecare corp și pentru starea „Modificată manual”; butonul ferestrei
    redenumit „Recalculează”.

### [11] Tabelul de decizii și trimiterile – Severity: Low
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** R-4 adăugat; D14 → §1, §8; D17 în §5; întrebarea despre feroneria ascunsă → D17; alternativele
    „Alt-A…Alt-W”; ADR 0044 în Resurse; gravitatea riscurilor doar Blocant / Major / Minor; riscul „fișier malformat”
    descris în §6.

### [12] Detalii de design minore – Severity: Low
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** „|scara| → 1, cu semnul păstrat”; materialul efectiv pe drumul apariției; pierderile (implicit
    15%) și lemnul masiv (metri liniari, fără plăci brute) în deviz; glisierele pe pereche; feroneria din interiorul unei
    plăci numărată și marcată; „Dimensiuni” X/Y scriu parametrii (în §3); `typeKey` necunoscut → „tip nedefinit”;
    escaping și pentru TAB / CR; „Aluminiu”, „Cupru”, „Alamă” → `metal`; scripturile citesc parametrii noi doar după
    `endMacro` (în `docs/scripting.md`); obiectivul 7 „4 instanțe legate”; „fără goluri sau suprapuneri”; prag de
    performanță provizoriu < 200 ms; regula „pașii 1–4 împreună” într-un singur loc.

### [13] Formulări nemăsurabile și șablonul alternativelor – Severity: Low
*   ***AI Suggestion:*** DECLINE pentru șablonul alternativelor (standardul e respectat); PROPOSE FIX pentru formulările
    nemăsurabile („listă corectă” → trimitere la obiective; „ani de muncă”, „mai puțin matur” → lipsuri concrete).
