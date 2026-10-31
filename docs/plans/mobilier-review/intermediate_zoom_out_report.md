# Etapa 8 – Zoom Out intermediar

* **Document:** `docs/plans/mobilier-pe-corp.md` (liniile 1–1036, neschimbat față de Etapa 5)
* **Recenzor:** egm-zoom-out, cu rapoartele etapelor 5, 6 și 7
* **Data:** 2026-10-30
* **Totaluri:** 🔴 1 · 🟡 6 · 🟢 3

## Verdict

Contextul ajunge, iar designul atacă problema de fond. Majoritatea constatărilor din etapele 6–7 vin însă din patru
alegeri păstrate după Etapa 5:

1. dimensiunea plăcii salvată de două ori (parametri + geometrie), cu originea în colțul minim și compensarea
   instanțelor;
2. generatorul v2 care continuă să regenereze;
3. v6 de la pasul 1, plus schimbarea sensului lui `Instance::name` la pasul 12;
4. amploarea de 12 pași.

Corecturile din Etapa 7 adaugă mecanisme ca să păstreze aceste alegeri. Simplificările din Etapa 5 închid aceleași
probleme și micșorează designul: elimină aproximativ 15 constatări ulterioare.

## Întrebarea 1: etapele 6–7 întăresc simplificările din Etapa 5?

Da, pe toate patru.

| Simplificare | Ce constatări elimină |
|---|---|
| **A. Geometria e singura sursă pentru dimensiunea plăcii** (fără `parameters`, fără `generator = "board"`, fără finalizator, fără colț minim, fără compensare; redimensionarea reface cutia în coordonatele definiției, în jurul pivotului, care e deja salvat acolo) | design-1-B1 (lista măsoară mereu cu transformarea acumulată, ca azi), design-1-M3, comprehension-2, 8, 11, o parte din design-1-M10, rândul de risc al finalizatorului |
| **B. Generator care construiește o singură dată; „Întinde” e singura cale de lățire** | design-1-M2, comprehension-6, 7, câmpul `generatorVersion`, `makePlainBody`, refuzul `Generator` |
| **C. v6 scris doar când există date v6, schema completă la pasul 1, numele rămân text** | design-1-M5, M6, M8, comprehension-10, 15, M4 din Etapa 5 |
| **D. Amânări** (lista „În model”, extraopțiunile de cant, 6 din 8 limbi; feroneria construită în cod) | comprehension-9, 12, 20, problema germanei / polonezei, greșeala de ordine a pașilor (M3 mai jos) |

**Ce nu elimină** (rămân de corectat, dar sunt ieftine): categoria mânerelor v5 (blocantul din etapele 6–7),
ierarhia ușă / balama (design-1-M4), escaping-ul formulelor în CSV (design-1-M12), definirea dulapului de referință
(comprehension-5), glosarul și numele `PartSpec`.

## Întrebarea 2: corecturi care adaugă complexitate și alegerea mai simplă

| Corectura propusă | Ce adaugă | Alegerea mai simplă |
|---|---|---|
| Reconcilierea copiilor generatorului după o cheie de rol (design-1-M2a) | Id de rol pe fiecare copil, potrivire, reguli pentru copiii străini | Generator o singură dată (B) |
| Rescrierea pivoților, scenelor, cheilor de animație și ancorelor la mutarea originii (design-1-M3) | Atinge zone înghețate dintr-o funcție de mobilier | Redimensionare în coordonatele definiției, fără scrieri pe instanțe (A); convenția `buildPrimitive` + `createParametricAtCorner` |
| Păstrarea cheilor JSON necunoscute pe fiecare entitate (design-1-M5c) | Container generic pe `Definition`, `Material` și altele | Schema v6 înghețată la pasul 1; orice cheie ulterioară = versiune nouă; v6 doar când e nevoie |
| `Instance::nameKey` de la pasul 1 (design-1-M8) | Câmp nou, regulă de afișare, migrare | Numele generate rămân text, în limba activă la creare |
| Verificarea transformării acumulate + motivul `scaled-parent` (design-1-B1) | A doua regulă „Matches”, doar pentru listă | Lista măsoară mereu (A); „Matches” doar pentru insigna din Inspector |
| Unirea materialelor după `libraryKey`, apoi nume + categorie (design-1-M7) | Schimbă o regulă generală de inserare | Decoruri cu nume care nu se pot ciocni („Nuc (decor)”), regula rămâne |
| Separatorul și zecimala ca opțiuni de profil (design-1-M12) | O suprafață nouă de opțiuni înaintea răspunsurilor de la pasul 0 | Doar escaping-ul formulelor acum; profilurile rămân la „Îmbunătățiri ulterioare” |
| `sourceKey` păstrat la resalvare, legături între documente (design-1-M9) | Reguli de viață pentru o funcție a cărei premisă e încă o întrebare deschisă | Legarea mutată într-un al doilea design, după pasul 0 |
| Secțiune de riscuri, glosar, tabel de decizii, toate cele 7 întrebări (structural-1, 4, 6) | Un document de 1036 de linii și mai lung | Necesare, dar mici după împărțirea documentului |

## Întrebarea 3: cei 12 pași sunt realiști pentru un singur om?

Nu, nu într-un singur design:

1. Documentul are deja un punct natural de oprire: planul de verificare are puncte de control cu firma de debitare
   după pasul 4 și după pasul 7.
2. Pașii 8–11 depind de răspunsuri care nu există (legarea depinde de întrebarea de la linia 100; coloanele exportului
   de pasul 0; valoarea lui „Întinde” de legare).
3. Pasul 12 nu ține de mobilier: rescrie toate textele, inclusiv zonele înghețate, și are propria verificare în CI.

**Recomandarea: trei designuri.**

- **Designul 1 – „Plăci și lista de debitare pentru firmă”:** pașii 0–7 cu simplificările A, B și cu cantul redus
  (tipul pe latură, `cutSize`, cele 3 presetări incluse, `BandNeed` pe tip, adaos global), feroneria în cod.
  Obiectivele 1, 2, 3, 4, 8 (fără legare) și partea de generator din P8. Criteriul de ieșire: firma rudei acceptă un CSV
  scos din Aion.
- **Designul 2 – „Corpuri: legare, feronerie, Întinde”:** scris după punctul de control al designului 1 și după
  răspunsurile de la pasul 0. Pașii 8 (doar legarea; „În model” amânată), 9, 10, 11; obiectivele 5, 6, 7. `sourceKey`
  rămâne în schema de la pasul 1, ca să nu fie nevoie de v7. Pașii 10 și 11 pot fi opționali.
- **Designul 3 – „Traducerea”:** pasul 12, doar infrastructura + RO și EN, când devine reală o lansare
  internațională.

## Constatări

### 🔴 Blocant

**B1. Dimensiunea plăcii e salvată de două ori; asta stă la baza majorității constatărilor despre mecanisme.**
*Unde:* §3 (240–271), §9, P-1, alternativele D, H, I, intrările 26–28 și 52. Trei recenzii independente au găsit
defecte care vin din această alegere (design-1-B1, M3, comprehension-2, 8, 11, parte din M10); corecturile propuse
adaugă cod. Pașii 6 și 11 se construiesc pe ea, deci o schimbare după pasul 5 înseamnă trei pași rescriși.
*Corectură:* simplificarea A (Inspector-ul afișează `measurePart`; scrierea L/W reface cutia în coordonatele
definiției, în jurul pivotului; „Readu” reface din dreptunghiul măsurat + grosimea nominală; dispar `generator =
"board"`, `parameters`, `adoptBoardGeometry`, finalizatorul, `detachBoard`, `Detached`; alternativele D și I
rescrise). Blochează pasul 5, nu pașii 1–4.

### 🟡 Majore

**M1. Generatorul v2 regenerează în continuare.** *Corectură:* simplificarea B; fără `generatorVersion` (un
`generator = cabinet|shelf` rămas înseamnă v1). Dacă D16/D23 rămân, ADR 0043 trebuie să justifice două mecanisme de
lățire.

**M2. Inspector-ul are două căi de redimensionare a plăcii** („Dimensiuni”, redirecționat la `setBoardValue`, și
grupul „Placă”), în sisteme de axe diferite – cauza comprehension-2 și a contradicției din testul de la linia 817.
*Corectură:* dimensiunea plăcii se editează doar în grupul „Placă”; „Dimensiuni” se comportă ca azi (scalează, deci
placa devine „Modificată”).

**M3. Pasul 6 depinde de fișiere create la pasul 9** (✔ verificat: intrarea 32 folosește mânerele din
`resources/library/hardware/`, create de intrarea 46). Testul pasului 6 nu poate trece. *Corectură:* feroneria
construită în core din `HardwareType` (intrări `hardware:<cheie>`), disponibilă de la pasul 2; sau intrarea 46 mutată
în pasul 6. Nicio etapă anterioară nu a observat asta.

**M4. Strategia v6 împinge spre păstrarea cheilor pe fiecare entitate.** *Corectură:* v6 doar când există date v6;
schema de la pasul 1 e finală; numele generate rămân text; propoziția de la 547–548 corectată.

**M5. 12 pași într-un design depășesc un singur om și decizii încă nelate.** *Corectură:* împărțirea în designurile 1,
2, 3; ADR 0044 și „Etapa 8” din `PLAN.md` acoperă doar designul 1.

**M6. Mai multe corecturi din Etapa 7 lărgesc reguli generale pentru cazuri înguste.** *Corectură:* decoruri cu nume
care nu se ciocnesc; doar escaping-ul formulelor în `csvField`; „lista măsoară mereu” pentru corpurile scalate.
Pentru blocantul mânerelor v5, cea mai mică soluție: o funcție la încărcare, în core, care dă unui material cu
categoria goală și numele unui material din bibliotecă `category` și `libraryKey`; test pe
`tests/data/v5-dulap.aion`; regulile de unire nu se schimbă.

### 🟢 Minore

- **N1.** „Întinde” = `makeUniqueDeep` pe subarborele corpului, apoi mutarea vârfurilor de partea + în fiecare
  definiție-frunză; refuz dacă se schimbă grosimea unei plăci sau o față devine neplană.
- **N2.** Colțul minim ca origine contrazice convenția `buildPrimitive` (`Modeling.cpp:187-189`); fără rost sub A.
- **N3.** Ierarhia ușă / balama se rezolvă prin structură: ușa e un grup cu placa și balamalele alături; placa rămâne
  frunză.

## Cele mai mari pârghii, în ordine

1. Simplificarea A (B1).
2. Simplificarea B (M1).
3. Împărțirea în trei designuri (M5).
4. Schema înghețată, v6 doar când e nevoie, nume ca text (M4).
5. Feroneria în cod (M3).
