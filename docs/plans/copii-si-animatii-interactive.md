# Copii unice și animații pornite din interacțiuni (design)

Stare: aprobat (2026-10-29). Cerința utilizatorului, pe fișierul lui `Mobila.aion` (două corpuri de mobilă cu uși).

## De ce

Utilizatorul face mobilier interactiv: un click pe un corp îi deschide ușile, un al doilea click le închide, iar un
alt obiect (un întrerupător) poate deschide sau închide toate ușile deodată. Acum:

- un click pe ușile din Corp A deschide și ușile din Corp B (și invers);
- animația rulează la nesfârșit (bucla animației din timeline), nu o dată;
- nu există închidere (redare în sens invers / comutare);
- animațiile nu se pot redenumi;
- obiectele nu se pot copia cu ⌘C / ⌘V;
- „Fă unic” există, dar doar în palette.

Cauza legăturii dintre corpuri (verificată în fișier): Corp B e o copie făcută cu Move din Corp A. `copyInstance` dă
grupului o definiție nouă, dar `cloneDefinition` copiază doar primul nivel: grupul „Usi” din interior rămâne aceeași
definiție în ambele corpuri („Usi ×2”). „Animația 1” are pistele pe ușile din această definiție (instanțele 30, 31),
deci se mișcă în ambele corpuri. În plus, ambele interacțiuni pornesc „Animația 1”, iar `PlayAnimation` o redă ca
timeline-ul (o singură animație activă, globală, cu bucla ei).

## Modelul, așa cum l-a cerut utilizatorul

- Obiectele pot fi puse în grupuri.
- O animație are keyframe-uri pe unul sau mai multe obiecte / grupuri (ca acum, în timeline). Ea decide ce se
  mișcă: o animație pusă pe uși mișcă ușile, oricine o pornește.
- O interacțiune aparține unui singur obiect sau grup (regulile stau pe instanță, ca acum).
- O interacțiune poate porni una sau mai multe animații deodată. Obiectul care o are poate fi oriunde (în grupul
  corpurilor sau în afara lui), de exemplu un întrerupător.
- Copiile legate (aceeași definiție) se mișcă împreună; „Fă unic” le separă.

## Ce se construiește

### 1. Copierea cu ⌘C / ⌘V (Modelare)

- ⌘C copiază obiectele și grupurile selectate (instanțe din contextul editat) într-un clipboard al aplicației (în
  memorie, rămâne valabil și după deschiderea altui document). ⌘V le lipește în contextul editat.
- **Copia e unică, în adâncime**: fiecare definiție din subarbore primește o copie proprie. Dacă două instanțe din
  subarbore împart o definiție, copiile lor împart tot o definiție (nouă): legăturile interne se păstrează, cele cu
  originalul nu. *Decizia mea.*
- **Doar „Duplicare legată” (⌥D) și comanda Array fac instanțe ale originalului**, pentru orice fel de obiect (și
  grupuri / colecții: aceeași definiție). Acum ⌥D sare peste colecții și Array dă grupurilor copii superficiale; se
  repară. Orice altă copie e unică, în adâncime, ca ⌘V: copia cu Move / Rotate (⌥), inclusiv seriile „×N” / „/N”
  (utilizatorul: „doar când folosești Duplicare legată sau Array se creează o instanță a obiectului original”; seriile
  din Move sunt tot copii, *decizia mea*: o legătură apare doar când o ceri explicit).
- **Plasarea**: după ⌘V, copia apare atașată de cursor, cu unealta Move: se prinde de grid și de punctele de
  prindere ale altor obiecte (colțuri, mijloace de muchii; `tools::Grip`), cu lock-urile și VCB-ul lui Move. Copia se
  ține de colțul minim al cutiei ei (ca să se poată pune colț la colț lângă alt corp). *Decizia mea.* Click o
  așază, Esc renunță (nimic în istoric). Lipirea întreagă e un singur pas de undo. Cu mai multe obiecte copiate,
  pozițiile lor relative se păstrează, iar cutia lor comună dă colțul de prindere.
- În Animare, ⌘C / ⌘V rămân pentru keyframe-uri; în Modul interacțiune nu fac nimic. Comenzile noi se clasifică în
  `commandModes` (doar Modelare). Geometria liberă (fețe, muchii) nu se copiază în prima versiune.

### 2. Animațiile și interacțiunile copiei

La ⌘V, la copia cu Move și la „Fă unic”, copia își primește animațiile și interacțiunile:

- **O animație pornită de o regulă din subarborele copiat** primește o copie proprie, numită „<nume> (copie)” (cu
  număr dacă există deja), care conține **doar** pistele obiectelor copiate, mutate pe obiectele noi. *Decizia mea:*
  pistele altor obiecte nu se copiază, ca o interacțiune a copiei să nu miște originalul.
- **Celelalte animații care mișcă obiecte copiate** (de exemplu un film de construcție din timeline, pornit de nicio
  regulă a copiei) nu se duplică: pistele obiectelor noi intră în aceeași animație, deci copia se mișcă în film la
  fel ca originalul (alegerea utilizatorului; lista de animații nu se umple de copii).
- Pista obiectului de sus (dacă e animat el însuși) își mută cheile de poziție cu deplasarea copiei; pistele
  obiectelor din interior sunt relative la părinte și rămân la fel.
- Regulile obiectelor copiate (inclusiv ale celor din interior) se copiază; acțiunile lor trec pe animațiile
  duplicate și pe obiectele noi (`Action::animation`, `Action::target`, recursiv în `first` / `second`). O acțiune
  care pornește o animație fără piste în subarbore rămâne neschimbată.
- Căile de parcurgere și cheile de cameră nu se copiază (o copie pe o cale ar trebui ancorată separat).
- „Duplicare legată” și Array nu duplică animații: copiile legate împart obiectele din interior, deci și mișcarea.

### 3. „Fă unic”, la vedere

- Buton „Fă unic” în Inspector (lângă „Editează” / „Desface”), vizibil când separarea ar schimba ceva: obiectul sau
  grupul are copii legate (×N) **sau conține** un obiect / grup legat cu ceva din afara lui (ca Corp B din
  `Mobila.aion`, ×1, dar cu „Usi ×2” înăuntru). Plus o acțiune pe aceleași rânduri din lista Obiecte (`RowAction`).
  Comanda din palette rămâne. *Decizia mea.*
- „Fă unic” face aceeași copie în adâncime ca ⌘V (secțiunea 1) și duplică animațiile și interacțiunile ca în
  secțiunea 2, pe instanța aleasă. Fără asta, după separare, interacțiunea lui Corp B ar porni în continuare animația
  ușilor din Corp A.
- Spre deosebire de ⌘V, „Fă unic” păstrează comportamentul de dinainte și în afara obiectului: o regulă de oriunde
  (un întrerupător, un părinte) care pornea animația separată o pornește acum și pe copia ei (un bloc „Pornește
  animația” în plus, cu aceleași setări). Așa, întrerupătorul deschide în continuare ușile ambelor corpuri.
  *Decizia mea.*
- Fișierele existente nu se repară singure la deschidere (o legătură veche nu se poate deosebi de una făcută cu ⌥D):
  în `Mobila.aion`, utilizatorul apasă „Fă unic” pe Corp B. *Decizia mea.*

### 4. Animațiile pornite din interacțiuni

- Blocul „Pornește animația” primește:
  - **sensul**: Înainte, Înapoi sau Comută (deschide dacă e la început, închide dacă e la sfârșit; în mișcare,
    se întoarce din punctul în care a ajuns);
  - **repetarea**: O dată, De N ori (N ≥ 1) sau La infinit. La N repetări sau la infinit, fiecare tură reia de la
    capătul de pornire (o elice, o lumină care pulsează). O buclă infinită se oprește, pe loc, la următoarea
    declanșare a aceleiași acțiuni.
- **O dată, într-un sens**: o animație ajunsă deja la capătul spre care e trimisă nu mai face nimic (Înainte pe o ușă
  deschisă o lasă deschisă); una oprită la mijloc continuă spre capăt.
- **Comută cu mai multe animații în aceeași regulă** (întrerupătorul): regula decide o singură dată, pentru toate
  blocurile ei cu Comută: dacă toate animațiile lor sunt la sfârșit, le întoarce pe toate; altfel le trimite pe toate
  înainte. Exemplu: ușile din Corp A au fost închise cu click pe Corp A, iar ale lui B sunt deschise; click pe
  întrerupător: A se deschide, B rămâne deschis. Următorul click le închide pe amândouă (alegerea utilizatorului:
  „toate în aceeași stare”).
- Mai multe animații deodată: blocurile „Pornește animația” puse unul după altul pornesc toate în același moment
  (acțiunea e instantanee; nu așteaptă terminarea). Fiecare bloc are sensul și repetarea lui.
- **Fiecare animație are ceasul ei** în Modul interacțiune: mai multe rulează în paralel; una nu le oprește și nu le
  resetează pe celelalte. La final, obiectele rămân unde a ajuns animația (ușile rămân deschise).
- **Starea ține de animație**, nu de interacțiunea care a pornit-o: dacă întrerupătorul a deschis „Ușile A” și
  „Ușile B”, un click pe Corp A cu „Comută” închide doar „Ușile A”. Blocurile din reguli diferite se decid separat.
- Același obiect animat de două animații care rulează: câștigă ultima pornită. *Decizia mea.*
- Redarea scrie transformările direct în modelul Modului interacțiune (`m_runtime`), ca acțiunile de mutare din
  `RuleRuntime`, nu prin timeline-ul global: se aplică pistele (poziție, rotație, scară, culoare) și căile de
  parcurgere ale animației, de la poza de bază a documentului. Cheile de cameră: camera vederii le urmează cât rulează
  animația (ca acum); cu mai multe, ultima pornită.
- Bucla animației din timeline (`Animation::loop`) rămâne doar pentru previzualizarea din Animare.
- Fișierele vechi: blocurile existente devin Înainte / O dată. *Decizia mea:* bucla din timeline era o setare de
  previzualizare, iar o buclă infinită pornită din interacțiune nu se putea opri. Formatul `.aion` trece la v5.

### 5. Redenumirea animațiilor

- În lista animațiilor din timeline (alegerea animației active): „Redenumește” pe fiecare rând, cu editare pe loc.
  Un pas de undo. Numele gol nu se acceptă.

## Constrângeri

- `core/` fără Qt; tot codul nou din `core/` vine cu teste (copia în adâncime, duplicarea animațiilor și a
  regulilor, redarea cu sens / repetări / paralel, migrarea din v4).
- Operațiile pe model încep cu `require(Modeling)`; comenzile noi se clasifică în `commandModes`.
- UI: tokenii din `Theme.qml`, `PropertyGroup` în Inspector, `RowAction` pe rândurile listelor; textele în română.
- Probabil util: `model/Transfer.h` (`extractInstance` / `insertModel`) pentru clipboard; unealta Move și
  `instanceGrips` / `nearestGrip` pentru plasare; `model::evaluateInstance`, `pathTransformsAt` pentru redare.

## În afara scopului (acum)

- Restrângerea unei animații la „copia pe care dai click” (o animație pe definiție, refolosită de copii legate).
- Așteptarea terminării unei animații într-o secvență de acțiuni; evenimente „la terminarea animației”.
- Clipboard-ul sistemului (copiere între aplicații) și lipirea în alt proces.
- Copierea geometriei libere; ⌘X (tăiere).
- Copierea căilor de parcurgere și a cheilor de cameră odată cu obiectul.

## Lăsat constructorului

- Forma exactă a câmpurilor noi din `Action` (de exemplu `direction`, `repeat`, cu 0 = infinit) și a stării de
  redare din core (o clasă nouă lângă `RuleRuntime`).
- Cum arată sensul și repetarea în blocul din editorul de reguli (segmente, câmp numeric).
- Ordinea de livrare; propunerea: „Fă unic” vizibil + copia în adâncime → ⌘C / ⌘V cu plasare → duplicarea
  animațiilor și a regulilor → redarea din interacțiuni → redenumirea.
