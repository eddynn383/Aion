# Etapa 7 – Recenzia critică (runda 1)

* **Document:** `docs/plans/mobilier-pe-corp.md`, FeatureRange (liniile 1–569)
* **Recenzori:** egm-design (standard feature-spec) – terminat · egm-structural (standard feature-spec) – terminat ·
  egm-style (regulile proprii) – terminat
* **Data:** 2026-10-30
* **Totaluri după deduplicare:** 🔴 3 · 🟡 22 · 🟢 ~25
* **Runda e curată?** Nu (are blocante și majore nerezolvate).

Constatările marcate „✔” au fost verificate de agentul principal în cod. Constatările care repetă Etapele 5 și 6 sunt
notate cu sursa.

## 🔴 Blocante

**design-1-B1. Placa „Matches” într-un corp scalat dă dimensiuni greșite în listă.** (L253-254, L296, L62, L493)
`boardState` verifică doar scara instanței plăcii. Dacă scalezi corpul (părintele), plăcile rămân „Matches” și lista
scoate dimensiunile nominale, deși geometria din lume e alta. Lista de azi măsoară în lume
(`Quantities.cpp:347`), deci e o regresie. *Corectură:* „Matches” se verifică pe transformarea acumulată până la lume
(partea liniară ortonormală, ±1 permis la oglindire); altfel motiv `scaled-parent` și măsurare. Test: dulapul de
referință scalat ×1,2 nu are dimensiuni nominale în listă.

**design-1-M1 / comprehension-1 (ridicată la blocant). Mânerele dintr-un fișier v5 nu ajung la „Alte componente”.**
(L57-59, L106-109, L218-220, L337-339) Materialele v5 se încarcă cu `category = ""`, deci mânerele trec prin regula de
azi și redevin plăci – exact defectul pe care îl promite să-l repare Obiectivul 1. *Corectură:* la încărcare, un
material v5 al cărui nume se potrivește cu unul din bibliotecă primește `category` și `libraryKey`; locul (AionFile sau
Document) și un test pe un fișier v5 real.

**structural-1. Lipsește secțiunea „Riscuri și măsuri”.** (tot documentul) Standardul cere riscuri, moduri de eșec și
implicații de securitate. Brief-ul avea 12 riscuri (3 blocante: un singur om pe prea multe fronturi, un singur tester
amator, formatele firmelor necunoscute); niciunul nu a trecut în design. *Corectură:* `## Riscuri și măsuri` între
„Alternative considerate” și „Cerințe nefuncționale”, cu tabel Risc | Gravitate | Măsură | Unde; riscurile din brief,
riscul finalizatorului și un rând de securitate (vezi design-1-M12).

## 🟡 Majore

### Mecanisme

- **design-1-M2. Generatorul v2 șterge tot din corp la orice schimbare de parametru.** `regenerate` face
  `removeSubtree` pe toți copiii (`Parametric.cpp:351-365`): se pierd decorul pe placă, cantul, varianta de feronerie,
  modificările și piesele adăugate. Obiectivul 2 promite păstrarea decorului „după orice schimbare de parametru”.
  *Corectură:* (a) reconciliere după o cheie de rol stabilă, fără ștergerea copiilor străini, sau (b) avertisment
  înainte de regenerare când copiii au date de utilizator. (Se suprapune cu zoom-out M2.)
- **design-1-M3. Mutarea originii de către finalizator strică datele salvate în coordonatele definiției:**
  `Instance::pivot` (contrazice P-1), `Scene::transforms`, cheile de animație, `PathAnchor`, instanțele din alți
  părinți. Nu se spune nici ce se întâmplă la `CommandStack::push()` și în timpul unei tranzacții deschise.
  *Corectură:* lista câmpurilor de rescris sau fără mutarea originii (offset în parametri). (Zoom-out M1 propune
  eliminarea finalizatorului cu totul.)
- **design-1-M4. Ierarhia ușă / balama e contradictorie.** P5 presupune balamalele în ușă, dar o placă „nu coboară în
  copii” și „Matches” cere „fără copii”, deci balamalele ar dispărea din listă. *Corectură:* ierarhia explicită (ușa =
  grup cu placa și balamalele, sau balamalele ca frați) și lista coboară în copiii plăcii, marcându-i.
- **design-1-M10. Golurile din tabelul „Întinde”:** plăci Modified / Detached tăiate de plan, plăci cu lungimea pe altă
  axă locală, feronerie al cărei număr depinde de înălțimea ușii; sub-grupuri legate în altă parte se întind și ele.
  *Corectură:* rândurile lipsă; orice definiție partajată care se modifică devine unică, nu doar plăcile.
- **comprehension-8. „Întinde” presupune creștere din colț, P-1 creștere simetrică față de pivot.** *Corectură:* spus
  explicit că „Întinde” scrie parametrul fără compensarea P-1.
- **comprehension-2. Câmpurile „Dimensiuni” sunt descrise în axele plăcii, testul le folosește în axele lumii.**
  *Corectură:* sistemul de axe și maparea pentru o placă rotită și una în picioare.

### Formatul și legarea

- **design-1-M5. Schema v6 crește pe parcursul a mai mulți pași, cu publicare automată pe web.** Un build v6 mai vechi
  (DMG, web din cache, recuperare) pierde cheile adăugate de un build v6 mai nou – exact alternativa G respinsă.
  *Corectură:* schema v6 completă la pasul 1, sau o versiune pe fiecare pas de schemă, sau păstrarea cheilor JSON
  necunoscute pe fiecare entitate.
- **design-1-M6 / comprehension-15 / zoom-out M4. „Până la pasul 1 inclusiv, retragerea e un revert” e fals.** Fiecare
  salvare scrie v6. *Corectură:* `formatVersion 5` când nu există câmpuri v6, sau publicarea web oprită; propoziția
  corectată.
- **design-1-M7. Materialele se unesc tot după nume la copiere** (✔ `Transfer.cpp:48-53`): un „Nuc” din bibliotecă
  (`board-decor`) ia categoria unui „Nuc” al utilizatorului (`wood`). *Corectură:* unire după `libraryKey`, altfel nume
  + categorie; la conflict, material nou. (Zoom-out M9 propunea să nu se schimbe regula; de decis.)
- **design-1-M9. Sensul legăturii bibliotecă ↔ model e nedefinit:** după editare, fișierul din bibliotecă e învechit;
  o nouă salvare scrie alt UUID și dezleagă copiile; între documente nu e definit; Obiectivul 5 depinde de o întrebare
  încă deschisă (L100). *Corectură:* „modelul câștigă”; `sourceKey` păstrat la resalvarea aceleiași definiții;
  Obiectivul 5 condiționat de pasul 0.
- **design-1-M11. `replaceDefinition` nu există în cod; nu se spune dacă feroneria se leagă.** *Corectură:* marcat ca
  API nou; amploarea schimbării de variantă (selecția sau toate copiile).

### Traducerea

- **design-1-M8 / comprehension-9, 10 / zoom-out M7.** Numele generate ca chei în `Instance::name` (fără regulă de
  deosebire); fișierele din pașii 1–11 rămân cu nume românești; zonele înghețate fără traducere contrazic „engleză
  completă”; textele compuse în core și pluralele. *Corectură:* câmp separat `nameKey` de la pasul 1; core întoarce
  cheie + argumente; Obiectivul 9 restrâns sau zonele înghețate incluse.

### Securitate

- **design-1-M12. CSV-ul nu neutralizează celulele care încep cu `=`, `+`, `-`, `@`** (✔ `csvField`,
  `Quantities.cpp:160-175`): un nume de piesă poate deveni formulă în Excel-ul firmei. Separatorul `;` și virgula
  zecimală sunt fixe, deși exportul țintește și alte țări. *Corectură:* escaping pentru formule; separatorul și zecimala
  ca opțiune de profil.

### Structură și consecvență

- **structural-2 / style-M1 / comprehension-13. `BoardSpec` vs `PartSpec`** (L49, L180, L196); `HardwareType` lipsește
  din diagramă. *Corectură:* `PartSpec` peste tot, un singur header.
- **structural-3. „Resurse” și „Îmbunătățiri ulterioare” sunt H3 sub „Cerințe nefuncționale”.** *Corectură:* H2.
- **structural-4 / comprehension-21, 22 / style-N16.** Trei serii „P” (parcursuri P1–P8, decizii P-1…P-6, alternativa P),
  D10–D24 fără tabel, „pasul N” definit doar după L570. *Corectură:* deciziile ca `DS-1…`; un tabel scurt de decizii;
  o listă de o linie a pașilor în „Prezentare”.
- **structural-5. Pașii 3 și 10 („Fă placă”, „Placă pe față”) și punctele de control nu apar în specificație.**
  *Corectură:* subsecțiuni scurte în planul tehnic și obiectiv + parcurs, sau precizarea că sunt doar în plan.
- **structural-6. „Întrebări deschise” are 2 din cele 7 întrebări pentru rudă; P5 decide deja una dintre ele.**
  *Corectură:* toate, fiecare cu secțiunea pe care o poate schimba; P5 marcat provizoriu.
- **structural-7 / comprehension-12. ADR-urile modificate nu sunt numite** (0017 „doar ce e vizibil”, 0025 subgrupuri și
  axe, 0038 cache-ul overlay-ului). *Corectură:* lista „ADR-uri afectate”; Obiectivul 4 = „Lista de debitare arată…”;
  L542 numește overlay-ul uneltei și regula `DocumentPassCache`.
- **comprehension-3. Lipsește glosarul.** *Corectură:* glosarul din brief, restrâns.
- **comprehension-4 / style-M2. „Deviz” promite prețuri care nu există.** *Corectură:* definit ca blocul de totaluri
  (plăci, cant, bucăți de feronerie), fără prețuri; sau „lista de cantități”.
- **comprehension-5 / structural-14. Dulapul de referință nu e definit complet; „3 piese ascunse” nu se potrivește cu
  regula balamalelor.** *Corectură:* dimensiunile în ordine, piesele, balamalele, mânerele; ce numără N.
- **comprehension-6, 7, 11, 16.** Ce generatoare primesc v2; P7 nu spune că un corp generat e refuzat de „Întinde”;
  starea plăcilor din „Fă placă”; salvarea unei singure plăci în bibliotecă.
- **style-M3. „Tâmplarul profesionist din familie” devine „ruda”.** *Corectură:* un singur nume („tâmplarul consultat”).
- **style-M4. Obiectivul 1 promite „corectă pentru orice model”, testul verifică un dulap.** *Corectură:* afirmația
  restrânsă la criteriile testate.
- **style-M5. Obiectivul 8 („toate câmpurile supraviețuiesc… booleenelor”) contrazice tabelul din §7.**
- **style-M6. Judecată subiectivă la alternativa B („cea mai slabă fundație”).**

## 🟢 Minore (rezumat)

- **Stil:** „grupate fără nume” ambiguu; unitățile (mm) nedeclarate; „drop” / „trage” / „hover” amestecate;
  „finalizer” / „finalizator”; „toate 4 corpurile”; ghilimele tipografice în blocul de cod; cheile de categorie în
  engleză vs română; „refuză clar”; declanșatorul informal „după ce cineva arată că plătește”; maparea L/W cu fibra pe
  Y; „efort L” nedefinit; formulări colocviale; cifre fără sursă (30–40%); repetarea deciziilor P-1/P-3.
- **Structură:** diagrama ASCII cu marginea dreaptă inegală; linii orizontale folosite inconsecvent; numerotarea
  subsecțiunilor din planul tehnic; §11 (decizii) în interiorul arhitecturii; blocul de cod etichetat `text`, cu
  `BandSide` folosit înainte de declarare; căi de fișiere amestecate; alternativa „arhitecți, designeri, tâmplari”
  scoasă fără mențiune; `Material` are și `roughness` / `metallic`, nu „doar culoare și textură”.
- **Design:** `buildPrimitive` centrează baza (offset de spus); ținta de 500 de plăci e sub modelul de stres din
  ADR 0038 (~1 400); `bands` fix pe ±X/±Y deși `thicknessAxis` e configurabil; placa scalată salvată în bibliotecă
  trebuie readusă la scara 1; rândul de antet din CSV poate strica importul firmelor (opțional până la pasul 0);
  amendarea ADR 0017 ca livrabil; germana / poloneza care nu încap.
- **Înțelegere:** N din „Afectează N copii” = apariții în model; unitatea „m liniari” la lemnul masiv; efectul
  `toCutter = false`; regula pentru `grainAxis = 255`; materialele utilizatorului ca decor de cant; „EGM”, „operandul
  A”, `ChangeSet::meshes`; alegeri nerezolvate la intrările 10 și 53; P7 „lateralele s-au mutat”.

## Matrice de urmărire (constatări care se repetă între etape)

| Problemă | Etapa 5 (zoom-out) | Etapa 6 (înțelegere) | Etapa 7 |
|---|---|---|---|
| Mânere v5 fără categorie | — | 1 (blocant) | design-M1 |
| Retragerea v6 și publicarea web | M4 | 15 | design-M5, M6 |
| Finalizatorul și sincronizarea L/W | M1 | — | design-M3 |
| Generatorul v2 regenerează și pierde date | M2 | 7 | design-M2 |
| Nume generate ca chei, zone înghețate | M7 | 9, 10 | design-M8 |
| `BoardSpec` / `PartSpec` | N4 | 13 | structural-2, style-M1 |
| Lipsă glosar / tabel de decizii | — | 3, 21, 22 | structural-4 |
| Deviz nedefinit | — | 4 | style-M2 |
| Dulapul de referință | — | 5 | structural-14 |
| Unirea materialelor după nume | M9 | — | design-M7 |
