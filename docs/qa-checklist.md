# Aion – listă de testare manuală

Bifează fiecare scenariu în aplicație (`make run`). Notează ce nu merge cu pașii de reproducere.

## Navigare și interfață
- [ ] Orbit (butonul din mijloc / O), Pan (Shift / H), Zoom (rotiță, Z), trackpad (două degete, pinch)
- [ ] Gizmo-ul de axe: click pe o axă → vederea standard; ⌘0–6; Zoom Extents (⇧Z)
- [ ] Command palette (⌘K): căutare, comenzi recente, „Deschide recent: …”
- [ ] Teme: Sistem / Light / Dark; panourile (⌘\); tooltip-urile lizibile
## Navigare precisă (ADR 0016)
- [ ] Preferințe → Navigare → „Selecției”: selectează un obiect departe de centru, orbitează → obiectul rămâne pe loc (punct albastru = pivot)
- [ ] „Punctului de sub cursor”: orbita pornește din punctul de pe model de sub mouse; pe gol → centrul vederii
- [ ] Zoom cu rotița spre un detaliu de 1–5 mm: se apropie lin, nu trece prin el, nu se blochează
- [ ] Foarte aproape de un detaliu mic, restul modelului (zeci de metri) rămâne vizibil
- [ ] Clip start / Clip end manuale schimbă tăierea; 0 revine la automat
## Snap la grid
- [ ] ⌥G / chip-ul „Snap” din bara de jos / meniul Vedere pornesc și opresc snap-ul
- [ ] Pas automat: punctele de pe sol cad pe liniile vizibile ale grid-ului (se schimbă cu zoom-ul)
- [ ] Pas fix (Preferințe: 1 mm … 1 m): grid-ul desenat arată pasul ales; lungimile pe axe sunt multipli de pas
- [ ] Capetele, mijloacele și muchiile câștigă în fața grid-ului; pe fețe orizontale/verticale snap în planul feței
## Desen și inferență
- [ ] Line: snap pe capăt / mijloc / muchie / față / axe; Shift și săgețile blochează; VCB (`4m`, `2500`, `3'6"`)
- [ ] Camera de 4 × 3 m numai din inferențe și VCB se închide într-o față
- [ ] Rectangle (`3000,2000`), Circle (`24s`, rază), Polygon, Arc (săgeată)
- [ ] Push/Pull: cutie, extindere, micșorare, fereastră prin perete, nișă, dublu-click repetă, Option păstrează fața
- [ ] Move / Rotate / Scale cu VCB; Option copiază; `x3` și `/3`; acoperiș în două ape (Move pe coamă, ↑)
- [ ] Offset, Eraser (curbe întregi), Select (click, dublu, triplu, dreptunghi window/crossing)
## Toolbar și obiecte (Faza 13)
- [ ] Toolbar: Select/Interact, Desen (Line, Arc, Rectangle, Circle, Polygon), Obiecte (Cub, Sferă, Con, Cilindru), Push/Pull…, Eraser singur
- [ ] Cub / Sferă / Con / Cilindru: click centru, click mărime; VCB „1000” sau „1000,500”; pe o față: în planul ei; rezultatul e un obiect selectat
- [ ] Materiale: click pe un material → Paint Bucket activ, cursor găleată; grila umple lățimea panoului; tasta B pornește Paint Bucket
## Organizare: obiecte, grupuri, tag-uri (Faza 15, ADR 0019)
- [ ] Forme → ⌘G = obiect; obiecte → ⌥⌘G = grup (folder în Outliner); ⌥D = duplicare legată (×2 în Outliner)
- [ ] Outliner: săgeată strânge/desface grupurile; ochi, lacăt; dublu-click redenumește; drag pe un grup = mută în el; drag pe zona de jos = scoate din grup (poziția rămâne)
- [ ] Tag-uri (sub Outliner): +, redenumire, „Atribuie selecției” (și pentru fețe/muchii), „Selectează”, ochi (ascunde tot ce are tag-ul, din orice grup)
- [ ] Panoul din stânga e acordeon (Outliner, Materiale, Scene); starea se păstrează
## Organizare
- [ ] Creează grup (⌘G) / obiect (⇧⌘G); dublu-click intră, Esc iese; restul se estompează
- [ ] Copii ale unui obiect se modifică împreună; Fă unic; Desface
- [ ] Outliner (selectare, redenumire, ascundere); Tag-uri (vizibilitate); Entity Info (dimensiuni, arie, lungime)
- [ ] Meniul de click dreapta: geometrie, un obiect, două obiecte, spațiu gol, în grup
- [ ] Undo / Redo imediat după Creează grup (grupul selectat), din interiorul grupului, după Desface și după Interact: fără crash
## Fișiere
- [ ] Salvează / Deschide `.aion`, punctul de „modificat” din titlu, întrebarea la închidere
- [ ] Autosalvare: închide forțat aplicația cu modificări, repornește → bannerul de recuperare
- [ ] Export OBJ, GLB, glTF, PNG; deschide GLB-ul în Blender; import OBJ / glTF
## Raport de cantități (Fișier → Raport de cantități)
- [ ] General: arie totală, volumul solidelor, obiectele cu bucăți (copiile unui obiect adunate), materiale, tag-uri
- [ ] Ascunde un obiect sau un tag → dispare din raport după „Actualizează”
- [ ] Mobilier: un corp din panouri grupate separat (laterale, blat, poliță, spate) → dimensiuni L × l × g corecte,
      și pentru panourile rotite; piesele identice adunate; necesarul de plăci se schimbă cu placa și pierderile
- [ ] Exportă CSV (ambele variante) și deschide în Excel / Numbers: coloane corecte, diacritice, virgulă zecimală
## Animații și interacțiuni (ADR 0018)
- [ ] Scene → Redă: camera trece lin prin scene (tranziție, pauză, buclă); fiecare scenă își aplică stilul; click în viewport oprește
- [ ] Bara de redare (sus, în viewport) arată scena curentă; × oprește
- [ ] Scene → Exportă video…: progres în panou, Anulează; MP4-ul se deschide în QuickTime, are durata și fps-ul ales
- [ ] Entity Info → Interacțiune: Balama (stânga / dreapta / sus / jos, unghi, durată) și Glisare (6 direcții, distanță)
- [ ] Unealta Interact (I): click pe ușă → se deschide animat; încă un click → se închide; ⌘Z anulează
- [ ] Ușă într-un dulap grupat: click pe ușă mișcă ușa, nu dulapul; copiile unui obiect au același comportament
- [ ] Salvează și redeschide: ușile rămân deschise / închise, comportamentul rămâne
## Bara de vizualizare (colțul viewport-ului, lângă gizmo)
- [ ] Stilurile ca iconițe (wireframe, linii ascunse, clay, materiale, X-Ray); ▾ = Profile, Ceață
- [ ] Umbre (☀) + ▾ = ora, luna; Secțiuni (apare doar dacă există); Zoom Extents, proiecție, grid
- [ ] Inspector-ul arată doar selecția (fără selecție: datele modelului)
## Inspector (selecție)
- [ ] Grup/obiect: Poziție X/Y/Z, Dimensiuni L/l/h (lacăt = proporții), Rotație, Scară — editabile, cu undo
- [ ] Material: pastila deschide paleta (model + bibliotecă + „Fără material”)
- [ ] Fețe: Față / Verso; Paint Bucket pe o față văzută din spate vopsește versoul; Reverse Faces le schimbă
## Scene ca stări (Faza 16)
- [ ] „Adaugă scena curentă” reține camera, stilul, pozițiile obiectelor, ce e ascuns și tag-urile ascunse
- [ ] Click pe o scenă: camera și obiectele trec lin (durata din slider-ul „Tranziție”); ⌘Z anulează aplicarea
- [ ] ↻ pe o scenă o recapturează; fișierele vechi (scene doar cu cameră) schimbă doar camera
## Timeline (Faza 17, ADR 0020)
- [ ] ⌥T deschide timeline-ul; „Animație nouă” din meniul din stânga sus; redenumire (dublu-click), ștergere
- [ ] Selectează un obiect → ◆ Keyframe la 0 s; mută playhead-ul, mută/rotește/redimensionează obiectul → ◆; Play (Space)
- [ ] Culoare: pune material, ◆ Culoare; alt material, alt moment, ◆ Culoare → culoarea se schimbă lin
- [ ] Cameră: ◆ Cameră în două momente cu vederi diferite → camera se mișcă la redare
- [ ] Romburile: drag = mută în timp; click dreapta = easing / șterge; Delete șterge cheia selectată
- [ ] Click de editare în viewport iese din previzualizare (obiectele revin la model); buclă; Durată / fps (⚙)
- [ ] Exportă video → MP4 cu animația și camera ei
## Grafică
- [ ] Materiale (bibliotecă, în model, pipetă ⌥, ⌘ conectate, ⇧ același material), sticlă transparentă
- [ ] Stiluri: materiale, clay, linii, wireframe, X-Ray; profile; ceață; umbre (oră, lună)
## Măsurare și adnotare
- [ ] Tape Measure (ghidaj paralel / punct), Protractor, cote, texte (editare, ștergere), ghidaje în inferență
- [ ] Secțiuni (activare, mutare ±100 mm, ștergere); Intersect cu modelul; reuniune / scădere / intersecție
- [ ] Scene: adaugă, actualizează, redenumește, tranziție animată
## Preferințe, meniu, sistem (macOS)
- [ ] Meniul de sus: Aion (Despre, Preferințe ⌘,), Fișier (+ Deschise recent), Editare, Vedere, Stil, Organizare, Unelte, Ajutor
- [ ] Scurtăturile ⌘ rulează o singură dată (⌘Z nu face două undo-uri; ⌘G un singur grup)
- [ ] În Measurements box / la redenumire, ⌘A / ⌘Z acționează pe text, nu pe model
- [ ] Preferințe: unitățile (Auto, mm, cm, m, inci, picioare) schimbă cotele, Entity Info, VCB, bara de jos; precizia
- [ ] Tema și latitudinea se păstrează după repornire; latitudinea mută umbrele
- [ ] Despre Aion: versiune, backend grafic
- [ ] Dublu-click pe un `.aion` în Finder deschide Aion (și cu aplicația deja pornită); trage un .aion pe iconița din Dock
- [ ] `make package` → `dist/Aion-0.1.0.dmg`: montează, trage în Applications, pornește (fără Homebrew în PATH)
- [ ] Model mare (mii de fețe): navigare fluentă, picking fără întârzieri
## Pagina de Preferințe (⌘,)
- [ ] Categorii: General, Unități, Salvare, Aspect, Viewport, Navigare, Grid și snap, Scurtături; Esc / × închide
- [ ] Salvare: autosalvare oprită → nu mai apare bannerul de recuperare; interval 1–30 min
- [ ] Aspect: accentul (8 culori) și culorile personalizate (ColorDialog) se aplică imediat, separat pentru light și dark; Resetează
- [ ] Viewport: MSAA (fără / 2× / 4× / 8×) și calitatea umbrelor schimbă imaginea; „Arată fps”
## Ghiduri și tur
- [ ] Hover ~1,3 s pe o unealtă din toolbar → ghidul animat (titlu, scurtătură, animație, pași, sfat); dispare când pleci
- [ ] Animațiile arată bine în light și dark; cu „Reduce Motion” (macOS) apare un cadru fix
- [ ] Preferințe → „Ghiduri animate la hover lung” oprit → doar tooltip-ul simplu
- [ ] Ajutor → Turul uneltelor: Înapoi / Următorul, săgețile ←/→, punctele de progres, „Încearcă” activează unealta
## Windows (Faza 11)
- [ ] `win-debug` compilează din Developer PowerShell; aplicația pornește pe Direct3D 11
- [ ] Textele de ajutor arată Ctrl / Alt (nu ⌘ / Option); Ctrl+Z, Ctrl+G, Ctrl+S etc.
- [ ] Scalare 125% / 150%: interfața clară, picking-ul nimerește
- [ ] Fișiere cu diacritice în nume (ă, î, ș, ț) se salvează, se deschid și se exportă
- [ ] Installer-ul (Inno Setup): scurtături, asocierea .aion, dezinstalare curată
## Web (Faza 12)
- [ ] Pagina din artefactul CI `aion-web` (servită local, de exemplu `python3 -m http.server`) pornește în Chrome, Safari, Firefox
- [ ] Deschide (selectorul browserului), Salvează (download .aion), Exportă (OBJ + MTL, GLB, PNG), Importă
- [ ] Reîncarcă pagina după modificări (> 1 min) → bannerul de recuperare
- [ ] Umbrele: merg sau se dezactivează fără erori; fps pe casa din Faza 5
## Interacțiuni cu blocuri (Faza 18)
- [ ] Inspector → „Interacțiuni cu blocuri…” pe un grup: editorul se deschide cu numele obiectului
- [ ] Trage „Când dau click” în dreapta → regulă nouă; trage „Mută cu” sub ea; editează Z și durata în bloc
- [ ] Bara albastră arată unde intră blocul (înainte / după); „Alternează” primește blocuri în „întâi” și „apoi”
- [ ] Un bloc tras înapoi pe paletă se șterge; ⌘Z anulează fiecare schimbare
- [ ] F5 (sau ▶): bannerul apare, hover pe obiect → cursor mână; click → obiectul se mută; Esc → totul revine
- [ ] În Modul Interacțiune nu se poate selecta, desena sau șterge nimic
- [ ] Salvează / deschide `.aion`: regulile rămân
## Camere și lumini (Faza 19)
- [ ] Outliner → Camere și lumini → + : cameră din vederea curentă, bec, spot, soare, panou; apar în listă și în viewport
- [ ] Click pe marcajul unei lumini (Select) → Inspector-ul ei; culoare, intensitate, rază, con se văd imediat pe model
- [ ] Bec roșu lângă un perete alb → pată roșie care se stinge spre rază; spot → con cu margine estompată
- [ ] Stinge / aprinde din Outliner; ochiul ascunde doar marcajul
- [ ] „Privește prin cameră” (Inspector, dublu-click în Outliner, palette) → vederea și unghiul camerei; o vedere
      standard revine la 35°
- [ ] ⌘Z / ⇧⌘Z pe adăugare, proprietăți, ștergere; salvare și redeschidere `.aion` păstrează tot
## Sidebar-ul din stânga
- [ ] Cinci secțiuni cu același antet (iconiță, TITLU, număr, acțiuni): Obiecte, Tag-uri, Materiale, Scene, Interacțiuni
- [ ] Obiecte: iconița tipului (cub, sferă, con, cilindru, formă liberă, componentă, grup, cameră, lumină); + adaugă camere și lumini
- [ ] Materiale: + deschide editorul (nume, culoare, opacitate, rugozitate, metalic) → „Creează” alege Paint Bucket;
      dublu-click sau click dreapta pe un material din model îl editează sau îl șterge
- [ ] Scene: „Scenă nouă din vederea curentă” după listă; ⚙ din antet → durata tranziției
- [ ] Interacțiuni: obiectele cu reguli și regulile pe scurt; click → editorul cu blocuri; ▶ = Modul interacțiune
## Plasarea în Inspector (12 valori)
- [ ] Poziție, Dimensiuni, Rotație (Euler XYZ), Scară — câte X / Y / Z; valorile încap în câmpuri
- [ ] Lacătul (la hover sau permanent când e blocat): valoarea devine gri, nu se poate edita; Move / Rotate / Scale nu o schimbă
- [ ] 🔗 la Dimensiuni: proporții legate (axele blocate rămân)
- [ ] Timeline deschis + hover pe un câmp: eticheta devine ◇; click → keyframe la momentul curent (creează animația dacă lipsește);
      ◆ plin când obiectul are deja keyframe acolo
- [ ] Salvare / deschidere `.aion` păstrează lacătele
## Oglindire, array, soften (Fazele 21–23)
- [ ] Click dreapta / palette → Oglindește pe X / Y / Z: obiectul se întoarce pe loc; geometria liberă are fețele albe în afară
- [ ] Inspector → butoanele de lângă Scară oglindesc obiectul; ⌘Z revine
- [ ] Array în linie: 3 copii alăturate implicit; schimbă numărul / axa / pasul → previzualizarea se actualizează; Gata = un pas de undo
- [ ] Array în cerc: rază, tot cercul sau unghi, rotirea copiilor (de exemplu scaune în jurul unei mese)
- [ ] Soften pe o cutie cu 95° → muchiile dispar și umbrirea devine netedă; 0° → toate vizibile; Esc anulează
## Texturi (Faza 24)
- [ ] Biblioteca: Cărămidă aparentă, Parchet de stejar, Lemn, Beton turnat, Gresie — miniaturi cu textura; Paint Bucket le aplică
- [ ] Pe un perete: rândurile de cărămizi sunt orizontale; pe pardoseală parchetul e la mărimea reală
- [ ] Material nou → Textură: procedurală sau „Imagine…” (PNG / JPG); L × H în mm schimbă mărimea pe model
- [ ] Selectează o față texturată → Inspector → Poziția texturii (X, Y, rotație, scară), ↺ resetează
- [ ] Salvare / deschidere `.aion`: texturile rămân (și pe alt calculator, fără fișierele originale)
- [ ] Export GLB / glTF: în Blender sau https://gltf-viewer.donmccurdy.com texturile apar; OBJ + MTL + imaginile alături
- [ ] Web: texturile se văd în browser
## Obiecte dinamice și bibliotecă (Fazele 25–26)
- [ ] Bibliotecă → trage Ușă în viewport: apare pe sol sau pe fața de sub cursor; click pe o plăcuță = în centrul vederii
- [ ] Inspector → Parametri: lățimea / înălțimea / numărul de polițe schimbă obiectul; ⌘Z revine; copiile legate se schimbă împreună
- [ ] Obiecte: părțile (Toc, Foaie, Clanță…) apar ca subgrupuri
- [ ] + din antetul Bibliotecii salvează obiectul selectat („Ale mele”, cu miniatură); click dreapta îl scoate
- [ ] Un obiect din „Ale mele” se inserează cu materialele lui (cele cu același nume se refolosesc)
## Scripturi și plugin-uri (Faza 27)
- [ ] ⌘K → Consola de scripturi; fiecare exemplu rulează (⌘↵) și se anulează cu un singur ⌘Z
- [ ] Un script cu eroare (de exemplu `aion.box(0,0,0,100,100,100); nuExista()`) arată linia și nu lasă nimic în model
- [ ] `while (true) {}` se oprește după 15 s, fără să blocheze aplicația definitiv
- [ ] Copiază `examples/plugins/gard` în dosarul plugin-urilor (📁), ↻ → „Gard: Gard de 6 m” apare în palette și merge
- [ ] Web: consola și exemplele merg în browser
## Restanțe din Fazele 24–26
- [ ] Un verso cu material texturat (Paint Bucket pe partea din spate) își arată textura
- [ ] Unealta Poziționează textura (Inspector → Poziția texturii → 🖼 sau palette): trage = mută; Shift = rotește; ⌘ = scalează; ⌘Z
- [ ] Trage un obiect din bibliotecă peste viewport: cutia lui urmărește cursorul; ieși din viewport → dispare
- [ ] Plante (copac, brad, tufă) și Om (cu înălțime) din bibliotecă, cu parametri în Inspector
## Panoul din stânga: înălțimi și scroll
- [ ] Marginea de jos a unei secțiuni deschise (linia scurtă) se trage în sus / jos; cursorul devine ↕; înălțimea rămâne după repornire
- [ ] Dublu-click pe margine = înălțimea implicită
- [ ] Mai multe secțiuni deschise decât încap → bară de scroll în dreapta panoului (trasă sau cu rotița pe antete)
## Etapa 4 (Fazele 28–31)
- [ ] Select: trage marcajul unei lumini / camere → se mută la aceeași înălțime; cu Shift pe verticală; ⌘Z o readuce
- [ ] Spot cu „Aruncă umbre” deasupra unui obiect → umbra lui pe podea; oprit → umbra dispare
- [ ] Scale: cutia cu mânere; colț = uniform, muchie = două axe, față = o axă; VCB „2,1,1.5”; Option = din centru
- [ ] Trage un material din panou pe o față: fața se evidențiază, la drop îl primește (versoul dacă e privit din spate)
- [ ] Meniul soarelui → Mediu: Studio / Exterior / Înnorat schimbă lumina și reflexiile metalelor
- [ ] Preferințe → Viewport: preset-urile Performanță / Echilibrat / Calitate; SSAO întunecă contactele; după repornire setările rămân
- [ ] Orbită apoi oprire: după ~0,4 s marginile devin mai fine (MSAA 8×)
## Forme în Inspector
- [ ] Click pe o linie → „Linie” cu lungimea editabilă (capătul se mută pe direcția ei); comutatorul Netedă (soft)
- [ ] Click pe marginea unui cerc / arc / poligon → raza și segmentele (laturile); arcul arată și unghiul
- [ ] După Push/Pull, segmentele sunt dezactivate (cu explicație); raza se poate schimba
- [ ] Click pe un dreptunghi → Lățime (latura pe X / orizontală) și Înălțime
- [ ] Fiecare schimbare = un pas de undo
## Câmpurile numerice din Inspector
- [ ] Tastează o valoare (de exemplu Rotație Z = 45) și nu apăsa nimic: se aplică după ~0,6 s; Enter o aplică imediat
- [ ] Esc în câmp: revine la valoarea reală, selecția rămâne
- [ ] După o valoare tastată, săgețile ↑/↓ pornesc de la valoarea reală; selectează alt obiect → câmpurile îl urmează
- [ ] Timeline deschis, animație cu keyframe-uri, playhead între ele: o schimbare în Inspector se vede imediat în
      viewport (iese din previzualizare); ◇ pune keyframe-ul fără să iasă
