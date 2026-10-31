# Etapa 7 – Recenzia critică, reverificare (runda 2)

* **Document:** `docs/plans/mobilier-pe-corp.md`, liniile 1–753 (după revizia din 2026-10-30)
* **Recenzori:** egm-design – terminat · egm-structural – terminat · egm-style – terminat
* **Totaluri după deduplicare:** 🔴 1 · 🟡 11 · 🟢 ~30
* **Runda e curată?** Nu. Limita de o reverificare pe etapă e atinsă: constatările rămase se rezolvă cu agentul sau se
  acceptă ca riscuri.

Din runda 1 s-au rezolvat: [1], [2], [3], [4], [6], [7], [11]–[16], [22], [23], [27]. Parțial: [8], [10], [17], [19].
Nerezolvat (corectura era greșită): [5]. Marcajul ✔ = verificat de agentul principal în cod.

## 🔴 Blocant

**R2-B1. Compensarea la mutarea originii e greșită matematic** (§3, L402–405; intrarea 27; testul de la L1035).
1. Vectorul d e în coordonatele plăcii, dar se adaugă la o transformare în coordonatele părintelui. Corect:
   `translație += L·d`, unde L e partea liniară a transformării instanței. Plăcile dintr-un corp sunt aproape toate
   rotite, deci cu „+d” fiecare Push/Pull pe capătul −X mută placa în lume. La fel pentru `Scene::transforms` și
   `PathAnchor`, fiecare cu L-ul lui.
2. ✔ `setPivot` → `rebasePivot` (`AnimationEval.cpp:493-523`) presupune că mesh-ul stă pe loc și mută doar pivotul,
   deci rescrie cheile de poziție. La mutarea originii, mesh-ul se mută odată cu pivotul, așa că cheile sunt deja
   corecte: trecerea prin `setPivot` ar strica animația.

*Corectura:* `translație += L_inst·d`; `pivot = pivot − d`, scris direct, fără `setPivot`; `Scene::transforms[i]` și
`PathAnchor::before` cu `+= L_salvat·d`; cheile de animație și ancorele relative rămân neschimbate. Teste: placă rotită
90° în jurul lui Y, cu pivot explicit și chei de poziție; scenă salvată cu placa oglindită.

## 🟡 Majore

**R2-M1. Regula ușilor din §4 contrazice dulapul de referință.** (L430 vs L122–124, L238, L947, L1073) Cu „rost de 3 mm
între uși și la margini”, ușile ies 495,5 × 1994, nu 498,5 × 2000; „spate aplicat” dă un spate de ~1000 × 2000, nu
964 × 1964. Testul pasului 6 nu poate trece. *Corectură:* o singură construcție, iar dulapul de referință definit ca
ieșirea generatorului „Dulap cu uși”, în design (nu doar în plan).

**R2-M2. `generatorVersion` și id-urile noi se contrazic.** (L326, L424, L452–454, alternativa L, L1047–1054) Corpurile
noi au id-uri noi (`base`, `wall`, `tall`, `shelf2`), adică exact modelul respins la alternativa L; iar
„`createParametric` pune 2” dă versiunea 2 și ușilor, ferestrelor, meselor, care nu au builder v2. *De ales:* (a) id-uri
noi, iar `generatorVersion` marchează doar generatoarele v2; (b) `cabinet` / `shelf` cu versiunea 2, fără
`wardrobe` / `shelf2`.

**R2-M3. Reconcilierea nu are reguli pentru editările de structură din corp.** ⌘V pe o poliță → două piese cu
`shelf.1`; Explode pe grupul ușii → balamale orfane dublate la refacere; Make Group pe două polițe → polițe dublate.
*Corectură:* unde se caută rolurile; copia unei singure piese pierde `roleKey`; Explode / Make Group / mutarea în alt
părinte într-un corp v2 se refuză sau cer întâi „Transformă în corp obișnuit”; rând nou în tabelul §7 și la riscuri.

**R2-M4. Reconcilierea suprascrie fără avertisment editări nemenționate.** Un Push/Pull pe capătul unei polițe (care
rămâne „în acord”) e anulat la următoarea schimbare de câmp, fără avertisment; piesele mutate de mână revin; o ușă
deschisă (`Behavior::open = true`) primește poza închisă, dar rămâne marcată deschisă; cheile de animație ale ușilor
se învechesc. *Corectură:* spus explicit ce se suprascrie; avertismentul extins la plăcile ale căror parametri diferă de
ce ar produce generatorul; reconcilierea scrie poza închisă și resetează `open`; animațiile pieselor se rebazează sau se
avertizează.

**R2-M5. Separatoarele puse cu clic sunt incomplet specificate.** `relativePosition` e `Vec3d`, textul vorbește de o
fracție; nu e clar față de ce se măsoară; dacă un separator pus cu clic împarte polițele; ce înălțime păstrează; ce se
întâmplă dacă se suprapune cu un separator din câmp; o mutare de mână e anulată la refacere; grosimea. *Corectură:*
o fracție scalară din lățimea interioară; separatorul ia toată înălțimea și adâncimea interioară și împarte polițele ca
separatoarele din câmp; rolurile polițelor includ spațiul (`space.2.shelf.1`); fracția se recalculează din poziția
curentă înainte de refacere; grosimea urmează câmpul `board`; regulă de distanță minimă.

**R2-M6. Finalizatorul lasă „Modificată” o placă doar mutată în contextul ei.** Ieșirea „nimic de făcut dacă
dimensiunile sunt egale” ignoră colțul minim. O rotire cu 90° în context ar schimba lungimea cu lățimea și ar muta
cantul. *Corectură:* nimic de făcut doar dacă dimensiunile sunt egale **și** colțul minim e în 0; preluare doar dacă
grosimea e pe `thicknessAxis` și normalele fețelor își păstrează axele.

**R2-M7. Compensarea uită datele salvate în coordonatele plăcii:** sub-piesele, ghidajele, adnotările și curbele Pen
cu contextul în definiția plăcii. *Corectură:* finalizatorul sare plăcile cu sub-piese sau le compensează; ghidajele,
adnotările și curbele mutate cu −d; teste.

**R2-M8. Liniile de cant desenate la fiecare cadru readuc costul eliminat de ADR 0038.** Pe modelul de stres (~1 400 de
plăci) ar însemna parcurgerea întregului model la fiecare cadru în orbită. *Corectură:* liniile persistente în
`DocumentPass` (se schimbă doar cu modelul), setarea lor în `DocumentPassCache::Key`; doar evidențierea laturii de sub
cursor în `appendModelExtras`; un test de fps în orbită pe modelul de stres.

**R2-M9. Dulapul de referință e trimis la „§5”, dar e definit doar în planul de implementare.** (L112, L236, L243)
*Corectură:* un paragraf „Dulapul de referință” în design (împreună cu R2-M1).

**R2-M10. Glosarul contrazice §5 la L1 / L2.** „L1, L2 (laturile lungi)” vs „paralele cu fibra”. *Corectură:* „laturile
paralele cu fibra; fără fibră, cele lungi”.

**R2-M11. „Instanță” și „apariție” sunt folosite diferit, dar doar una e definită.** „Afectează N instanțe” numără de
fapt apariții. *Corectură:* ambele în glosar; folosite consecvent.

## 🟢 Minore (rezumat)

*   **Tabelul de decizii:** R-4 lipsește; D14 → „§1, §8”; D17 – resetarea bifelor spusă și în §5; întrebarea despre
    feroneria ascunsă → D17.
*   **Prefixe care se ciocnesc:** alternativele A–W vs D/R/U → „Alt-A…”.
*   **Glosar:** „Desprinsă”, „În acord (Matches)”, „decor”, „finalizator”, „rol” lipsesc; PAL / MDF / HDF scris neparalel;
    „L1 = −V (fața)” → „latura din față”.
*   **Nume diferite:** „corp pentru electrocasnice” / „Corp pentru cuptor / frigider”; „Dulap cu uși” / „Dulap cu 2 uși”;
    „Modificată” / „Modificată manual” / `Modified`; „Actualizează” (D23) vs butonul „Actualizează” din fereastră.
*   **Obiective:** „plus 3 instanțe legate” ambiguu (corect: „4 instanțe legate”); „fără fisuri” → „fără goluri sau
    suprapuneri”.
*   **Tabelul de riscuri:** gravitatea doar Blocant / Major / Minor; riscul „fișier malformat” trimite la §6, unde nu e
    descris.
*   **Detalii de design:** „scara la 1” trebuie să păstreze semnul (oglindirea); materialul efectiv pe drumul apariției
    (ușa e vopsită ca grup); pierderile și lemnul masiv în deviz; glisierele pe pereche; feroneria pusă în interiorul
    unei plăci; „Dimensiuni” X/Y scriu parametrii (spus doar în plan); `typeKey` necunoscut în listă; escaping și pentru
    TAB / CR; ✔ „Aluminiu”, „Cupru”, „Alamă” trebuie să primească și ele `metal`; scripturile văd parametrii vechi până
    la `endMacro`.
*   **Stil:** formulări nemăsurabile („listă corectă”, „ani de muncă”, „mai puțin matur”); pragul de performanță
    provizoriu (de exemplu < 200 ms); regula „pașii 1–4 împreună” spusă de trei ori; ADR 0044 lipsește din Resurse;
    alternativele fără același șablon.
