# Etapa 9 – Pregătirea pentru implementare, reverificare (runda 2)

* **Document:** `docs/plans/mobilier-pe-corp.md`, liniile 805–1335 (după reviziile din 2026-10-30)
* **Recenzori:** egm-implementation-readiness – terminat · egm-codebase-details – terminat · egm-style – terminat
* **Totaluri după deduplicare:** 🔴 2 · 🟡 ~25 · 🟢 ~25
* **Verdict:** pașii 0–5 sunt aproape gata (o corectură: prinderea excepțiilor din finalizator). Pasul 6 (corpurile din
  bibliotecă) nu e încă implementabil fără decizii de design. Limita de o reverificare pe etapă e atinsă.

**Din runda 1:** toate cele 6 blocante sunt rezolvate (IR-B1…B6). Corecturile din runda 1 se potrivesc cu codul
(`modelingPrefixes`, `appendDocument` / `appendModelExtras`, `qtTrId`, `makeTool`, `UserPrefs`, `setPivot` evitat,
`Scene::transforms`, `PathAnchor::before`, `Behavior::open`).

## 🔴 Blocante

**IR2-B1. Corpurile din bibliotecă nu sunt specificate destul ca să fie scrise fără decizii** (§4, intrările 36–38).
Lipsesc:
- **(a)** câmpurile fiecăruia dintre cele 6 corpuri: implicit, minim, maxim, pas (`width`, `height`, `depth`, `doors`,
  `shelves`, `dividers`, `board`, `back`, `nicheHeight`); dacă `board` / `back` urmează grosimile permise; `rack` cu 0 uși
  implicit;
- **(b)** structura `DesiredPart` primită de `reconcile`;
- **(c)** orientarea fiecărei piese (pe ce axă a corpului stă lungimea și fibra lateralei, ușii, poliței, spatelui) –
  decide L1, coloanele listei și fibra;
- **(d)** sensul lui `depth` (cu sau fără uși);
- **(e)** nișa de la „Corp pentru electrocasnice” (jos sau sus, poliță fixă, uși doar pe rest);
- **(f)** polițele: pe spațiu sau în total; cum se combină separatoarele din câmp cu cele puse cu clic;
- **(g)** poziția balamalelor și a mânerului, partea balamalelor la un număr impar de uși, dacă ușa primește
  `Behavior` (deschidere);
- **(h)** decorul implicit al corpului.

**IR2-B2 (stil). Cifrele din testul „Întinde” sunt greșite** (L1263): baza unui corp de 600 cu laterale de 18 mm e
564 → 764, nu 764 → 964. ✔ verificat.

## 🟡 Majore (grupate)

- **Finalizatorul.** `Model::definitionsWithMeshChanges()` are nevoie de `Mesh::hasRecordedChanges()` (`Mesh.h`, lipsă
  din plan). Excepția: `commit()` golește numele tranzacției înainte de `endRecording()`, deci `abort()` ar declanșa
  verificarea internă; nimeni nu prinde excepția (ar ajunge la bucla Qt). Corectură: finalizatorul rulează primul în
  `commit()`; la excepție `abort()`, fără propagare, `lastError()`, mesaj prin `Document::setError`; test.
- **Unealta „Placă pe față”.** `ToolContext` se reconstruiește la fiecare eveniment; tipul folosit ultima dată trebuie
  ținut în `Document`. Nu e spus unde alege utilizatorul tipul înainte să deseneze.
- **„Adaugă separator”.** Mecanismul de intrare (unealtă sau mod intern), găsirea corpului din clic, ⌥, unde apare
  refuzul; fracția măsurată pe centrul separatorului, de la fața interioară a lateralei stângi.
- **Roluri duplicate.** `copyLinked` (⌥D, Array) copiază și `roleKey`; ce se întâmplă cu piesa care „pierde” la rol
  dublat.
- **Tipul și grosimea schimbate de utilizator pe o piesă cu rol** – păstrate sau suprascrise la refacere.
- **Avertismentul înainte de schimbarea unui câmp:** dialog cu Anulează sau bandă; separatorul mutat de mână nu
  declanșează refacerea.
- **Numele pieselor până la pasul 12:** `nameKey` se afișează tradus abia la pasul 12; până atunci Outliner-ul și CSV-ul
  ar arăta cheia brută. Corectură: un tabel cheie → text românesc în `render/` de la pasul 5, sau și `name` în română.
- **Decorurile noi fără textură** din corpurile din bibliotecă (`builtinTexture` acoperă doar indicii 24–28; textura se
  pune doar din `Document`); regula trebuie scrisă.
- **Tabelul materialelor:** cheile celor 30 de materiale, „Sticlă fumurie” → `glass`, „Piatră” → `stone`, culorile și
  texturile celor 7 noi.
- **Setarea „arată cantul”:** proprietar (`Viewport::showBands`), buton în `ViewportControls.qml`, cache, decalaj față de
  muchii, culoarea pentru „ca placa”.
- **Miniaturile „În model”:** cum se izolează definiția la randare; ce intră în listă (fără feronerie și plăci; corpurile
  din bibliotecă fără `sourceKey`?).
- **Traducerea:** `LinguistTools` lipsește din `find_package` (`CMakeLists.txt:21`); `qt_add_translations` trebuie să
  scaneze și `core/` și `render/`; CI trebuie să instaleze `lupdate`; scriptul de extragere și regula cheilor; serviciul
  de traducere automată.
- **Fișiere lipsă din plan:** `Mesh.h/.cpp`; `ScriptEngine.h`; `ToolHelpers.h` (`makeStretchTool`); `Tool.h` (lista de
  id-uri); `CMakeLists.txt` rădăcină; `components/CheckBox.qml` (nu există o bifă); `GeneratorInfo::legacy` filtrat în
  `Document::generators()` și `libraryItems()`; testul din `ParametricTests` trebuie să folosească `generators()` (nu
  `registry()`, care e privat).
- **Stil:** adaosul „pe tip sau global” fără regulă; limba implicită fără `AION_LANG` și fără setare; intrarea 66 sub
  pasul 12 deși e la pașii 4, 5, 7; escaping-ul CSV fără TAB / CR în plan; `Detached` la refuzurile „Întinde”; U5 și U8
  fără rulare în interfață; modelul de stres pentru performanță nedescris; teste prea comprimate (scena oglindită,
  rotirea în context, „același `ChangeSet`”, separatorul la 1/3, refuzul sub 50 mm).

## 🟢 Minore (rezumat)

`PenCurve` folosește `definition`, nu `context`; `geom::transformVertices` pentru mutarea vârfurilor; `sourceKey` golit în
`cloneShallow`; `occurrences()` are nevoie și ea de `includeHidden`; opțiunile se citesc în `applyMaterialProperties` și
`cutListOptions`; categoriile din bibliotecă vin din date (fără QML nou); `io::readSourceKey` inutil (fișierul se încarcă
oricum); butonul Explode din `EntityInfo.qml`; iconița `stretch.svg`; modelul de stres e local în `ModelScaleTests.cpp`;
supraîncărcări explicite pentru `setBoardValue`; dimensiunile implicite ale unei plăci din bibliotecă (600 × 400, culcată);
dimensiunile feroneriei și originea ei; „Toate 2 mm” pe HDF; materialele utilizatorului ca decor de cant; ordinea celor
două confirmări la Explode; `SheetNeed` fără câmpuri noi; formulări („booleean”, unități în teste, „în spate”).
