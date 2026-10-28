# Plan: modul Animare — funcții noi și un timeline mai bun

> Stare: **plan scris pe 2026-10-28; deciziile din secțiunea 6 așteaptă confirmarea utilizatorului.** Poate fi
> continuat de oricine (alt cont, altă sesiune): citește `CLAUDE.md`, apoi acest fișier; bifează pașii aici și în
> `PLAN.md` → „Etapa 6”. Comunicarea cu utilizatorul: în română. Planul precedent (modurile de lucru) e
> `docs/plans/moduri-de-lucru.md`; acesta îl continuă pe partea de Animare.

## 1. Ce vrea utilizatorul

1. **Tastatură în Animare**: **Space** = play / pauză; **← / →** = navigare în timeline.
2. **Tranziții (easing)**: valori predefinite (linear, ease-in, ease-out, ease-in-out…) **și** curbe proprii, create
   de el și **salvate** ca să le refolosească.
3. **Timeline cu obiecte expandabile**: fiecare obiect se poate deschide și arată **toate proprietățile modificate**
   (doar cele care chiar se schimbă), cu **valoarea fiecărei proprietăți în dreptul fiecărui keyframe**.
4. Un plan cu **funcții din aplicațiile de pe piață** care merită aduse în Aion și cu îmbunătățiri de UX ale
   timeline-ului.

## 2. Ce există acum (și unde)

| Ce | Unde | Observații |
|---|---|---|
| Datele | `core/include/aion/core/model/Model.h`: `Animation { name, duration, fps, loop, tracks, camera }`, `ObjectTrack { instance, transform[], color[] }`, `TransformKey { time, Transform value, Easing }`, `ColorKey`, `CameraKey` | **Cheia de transformare ține matricea întreagă**: nu se poate anima doar X, nici avea easing diferit pe rotație și poziție. |
| Easing | `enum class Easing { Linear, EaseInOut, EaseIn, EaseOut, Step }`, `model::ease()` în `AnimationEval.h/.cpp` | Fără curbe proprii. |
| Evaluare / editare | `core/.../model/AnimationEval.h`: `evaluateTransform/Color/Camera`, `setTransformKey`, `removeKey`, `moveKey` | Teste în `tests/core/AnimationTests.cpp`. |
| Document | `render/Document.h/.cpp`: `timeline` (QVariantMap pentru QML), `keySelection`, `keySelectionColor`, `keyCamera`, `removeKey`, `moveKey`, `setKeyEasing`, `play/pause`, `timelineTime`, `editAnimation`, `pose()` (modul Animare, auto-key), `keyTransforms` | |
| Timeline (QML) | `app/qml/shell/Timeline.qml` (~570 de linii): bara de sus (animația, ⏮ ▶ ⟳, timpul, ◆ keyframe, culoare, cameră, export, setări), rândurile (un rând pe obiect + camera), cheile (trase, selectate, Delete), meniul de easing | Un singur rând pe obiect; fără zoom / scroll în timp; fără selecție multiplă. |
| Fișierul | `core/src/io/AionFile.cpp` (animațiile în `model.json`) | Formatul nou cere o migrare. |
| Modul Animare | `docs/plans/moduri-de-lucru.md`, ADR 0033 | Timeline-ul e mereu vizibil; Move / Rotate / Scale și Inspector-ul pun keyframe-uri (auto-key). |

## 3. Ce oferă aplicațiile de pe piață (și ce propun pentru Aion)

Prioritate: **P1** = în acest plan, **P2** = după, dacă rămâne timp, **P3** = idei pentru mai târziu.

| Funcție | Unde există | Pentru Aion | Prioritate |
|---|---|---|---|
| Canale pe proprietate (poziție X/Y/Z, rotație, scară, culoare separat) | After Effects, Blender, Cinema 4D, Figma Smart Animate (implicit) | Baza pentru restul (cerința 3) | **P1** |
| Easing predefinit + curbe Bézier proprii, biblioteci de curbe | After Effects (Easy Ease), Figma, Principle, CSS | Cerința 2 | **P1** |
| Dope sheet cu pistele expandabile și valorile la chei | After Effects (proprietăți sub strat), Blender (Dope Sheet) | Cerința 3 | **P1** |
| Space play / pauză, ← → cadru cu cadru, Shift = 10 cadre, ⌥ / Alt = cheia următoare / anterioară, Home / End | Toate | Cerința 1 | **P1** |
| Selecție multiplă de chei, tragere în grup, copiere / lipire, ștergere | Toate | UX de bază | **P1** |
| Zoom și scroll în timp (⌘ + rotiță, bara de jos), „încadrează tot” | Toate | UX de bază | **P1** |
| Snap la cadre și la alte chei la tragere | After Effects, Blender | UX de bază | **P1** |
| Auto-key pornit / oprit (comutator, bulina roșie) | Blender, Cinema 4D, Maya | Avem auto-key mereu; comutatorul permite și editarea pozei de bază | **P1** |
| Traiectoria obiectului (motion path) în viewport, cu chei trase direct | After Effects, Blender | Se vede drumul; foarte util în arhitectură (o mașină, o ușă) | **P2** |
| Editor de curbe (graph editor) pe canal | After Effects, Blender, Cinema 4D | Pentru cine vrea control fin; după Bézier pe cheie | **P2** |
| Interval de redare (in / out), markeri cu nume, viteza de redare | Blender, After Effects, Premiere | | **P2** |
| Animarea vizibilității și opacității (apare / dispare, fade) | Twinmotion, Lumion, Keynote | Prezentări: obiecte care apar | **P2** |
| **Faze de construcție** („phasing”): obiectele apar pe rând (cad, cresc din sol, fade), ordinea dintr-o listă | Twinmotion Phasing, Lumion, Synchro | Foarte căutat în arhitectură; se poate genera automat din tag-uri | **P2** |
| Studiul de însorire: ora / ziua animate (umbrele se mișcă) | SketchUp (Shadows + Scenes), Twinmotion | Avem soarele cu oră și lună; trebuie doar canale pe el | **P2** |
| Animarea planului de secțiune (o tăietură care se plimbă prin clădire) | SketchUp (prin scene), Enscape | Avem planurile de secțiune | **P3** |
| Camera pe un traseu (o curbă desenată), cu ținta fixă sau „privește înainte”; turntable automat (rotire 360°) | Lumion, Twinmotion, Enscape, Blender | Plimbări prin clădire | **P2** |
| Presetări de animație pe obiect (rotire continuă, apare din sol, plutește, explodează în piese) | Keynote, Principle, Lumion | Un click → chei generate | **P3** |
| Onion skin (pozele vecine estompate) | Blender (grease pencil), Toon Boom | Mai puțin util în 3D arhitectural | **P3** |
| Export video: calitate, rezoluție, interval, cadru-cheie de copertă | Toate | Avem export MP4 | **P2** |

## 4. Designul

### 4.1 Modelul de date: canale pe proprietate (cerința 3)

```text
Animation { name, duration, fps, loop, tracks[], camera[] (rămâne, sau devine și ea canale — vezi 6.4) }
ObjectTrack { instance, channels[] }
Channel { property, keys[] }                       // property: "position.x" … "color", ordinea fixă din listă
Key { time, value (double sau Color), easing }     // easing = preset sau Bézier propriu (4.2)
```

- **Proprietăți**: `position.x/y/z` (mm, poziția pivotului în coordonatele părintelui), `rotation.x/y/z` (grade,
  Euler XYZ ca în Inspector), `scale.x/y/z`, `color` (RGBA), mai târziu `opacity`, `visible`. Valorile sunt cele din
  Inspector, ca utilizatorul să recunoască numerele.
- **Evaluarea**: fiecare canal se interpolează separat (cu easing-ul cheii de început); transformarea se recompune
  din poziție + rotație + scară, în jurul pivotului (`Instance::pivot`, ADR 0030) — rotația animată se face în jurul
  pivotului, cum se așteaptă utilizatorul.
- **„Proprietăți modificate”** = canalele care există pe pistă (o cheie pe o proprietate creează canalul). Auto-key
  (modul Animare) scrie **doar canalele care s-au schimbat** față de valoarea evaluată (Move pe X → doar
  `position.x`; Rotate pe Z → doar `rotation.z`). Butonul ◆ (keyframe manual) scrie toate canalele de transformare.
- **Migrarea** fișierelor vechi: la încărcare, fiecare `TransformKey` se descompune în 9 canale (doar cele care
  variază între chei se păstrează, plus o cheie de început pentru restul); `formatVersion` crește. Testul de
  rotunjire salvare → încărcare în `IoTests`.
- **API nou în core** (`model/AnimationEval.h`, cu teste): `setChannelKey(animation, instance, property, time, value,
  easing)`, `evaluateChannel`, `evaluateInstance(track, time, base) → Transform`, `changedChannels(base, before,
  after)`, `removeKey/moveKey` pe canal, `copyKeys/pasteKeys`.

### 4.2 Easing: presetări + curbe proprii (cerința 2)

- **Reprezentarea**: o curbă **Bézier cubică** `(x1, y1, x2, y2)` ca în CSS (`cubic-bezier`), plus două cazuri
  speciale: `Step` (salt) și, opțional (P2), `Spring` / `Bounce`. Presetările sunt doar curbe cu nume:

  | Presetare | Curba |
  |---|---|
  | Linear | (0, 0, 1, 1) |
  | Ease | (0.25, 0.1, 0.25, 1) |
  | Ease In | (0.42, 0, 1, 1) |
  | Ease Out | (0, 0, 0.58, 1) |
  | Ease In Out | (0.42, 0, 0.58, 1) |
  | Ease In Back / Out Back | (0.36, 0, 0.66, −0.56) / (0.34, 1.56, 0.64, 1) |
  | Step | salt la final |

- **Cheia ține curba însăși** (cele 4 numere + numele presetării, dacă e una), nu o referință: fișierul rămâne
  portabil, iar o curbă salvată și apoi schimbată în bibliotecă nu strică animațiile vechi.
- **Editorul de curbă** (popover nou `overlays/EasingEditor.qml`): graficul curbei cu cele două mânere trase cu
  mouse-ul, câmpuri pentru cele 4 valori, o previzualizare (un punct care se mișcă după curbă, în buclă), lista de
  presetări (implicite + ale utilizatorului) ca miniaturi, butonul **„Salvează ca presetare…”** (nume).
- **Biblioteca personală**: în preferințele aplicației (`UserPrefs.easingPresets`, JSON: `[{ name, x1, y1, x2, y2 }]`),
  disponibilă în toate documentele; redenumire și ștergere din editor. (Opțional P2: export / import ca fișier.)
- **Unde se aplică**: pe o cheie, pe chei selectate (toate primesc curba), pe tot canalul; implicit pentru cheile noi
  (o setare a animației: „easing implicit”).

### 4.3 Timeline: așezarea (cerința 3 + UX)

```text
┌ Animația 1 ▾ │ ⏮ ◀◆ ▶ ◆▶ ⏭ │ 00:01.20 / 00:05.00 · 36f │ ◆ Keyframe ▾ │ ● Auto-key │ ⤢ │ ⚙ │ ⇩ Video ┐
├──────────────────────────┬──────────────────────────────────────────────────────────────────────┤
│ ▾ Ușa intrare        ◆   │  ◆━━━━━━━━━━━━━━━◆━━━━━━━━━━━━━━━━━━━━━━━━━━━━━◆                    │ ← rând sumar
│     Rotație Z            │  ◆ 0°            ◆ 90°                         ◆ 90°                  │ ← canal + valori
│     Culoare              │  ◆ ■             ◆ ■                                                  │
│ ▸ Mașina             ◆   │        ◆━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━◆                    │
│ ▸ Camera             ◆   │  ◆━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━◆                    │
├──────────────────────────┴──────────────────────────────────────────────────────────────────────┤
│ 0s     0.5s     1s     1.5s     2s  …                                ━━━━━━━━[====]━━━━ (zoom)   │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

- **Rândul unui obiect**: săgeata ▸ / ▾ (deschis / închis, ținut per obiect), numele, ◆ (cheie pe toate canalele la
  timpul curent), pe pista lui: un romb pentru fiecare moment cu cel puțin o cheie, iar între ele o bară (intervalul
  animat). Click pe nume = selectează obiectul în viewport.
- **Deschis**: câte un rând pentru fiecare **canal care există** (cerința: doar proprietățile modificate), cu numele
  în limba Inspector-ului („Poziție X”, „Rotație Z”, „Culoare”) și, **lângă fiecare cheie, valoarea** („2000 mm”,
  „90°”, un pătrat colorat). Valoarea se editează cu dublu click (un `NumberField` mic peste cheie) → schimbă cheia.
  Coloana din stânga arată valoarea canalului la timpul curent, editabilă (scrie o cheie).
- **Cheile**: click = selectează (accent), ⇧ / ⌘ click = adaugă la selecție, tragere pe gol = dreptunghi de selecție,
  tragere = mută toate cheile selectate (snap la cadre și la alte chei; ⇧ = fără snap), ⌫ = șterge, ⌘C / ⌘V =
  copiază / lipește la timpul curent, click dreapta = meniu (easing, copiază, șterge, „mergi la cheie”).
- **Easing între chei**: segmentul dintre două chei are un mic semn al curbei (o miniatură); click pe el deschide
  editorul de curbă pentru acea tranziție.
- **Zoom / scroll**: ⌘ + rotiță = zoom în timp în jurul cursorului, rotița = scroll vertical, ⇧ + rotiță = scroll în
  timp, bara de jos cu mânere; `⤢` = încadrează toată animația. Rigla se adaptează (secunde, jumătăți, cadre).
- **Timpul**: afișat `mm:ss.cc` și cadrul (`36f`); click pe el = tastezi un timp. Playhead-ul se trage și pe riglă.
- **Auto-key** (● roșu când e pornit): pornit = comportamentul de acum; oprit = Move / Inspector schimbă poza de bază
  a obiectului (fără chei), pentru reglaje.

### 4.4 Tastatura în Animare (cerința 1)

| Tastă | Acțiune |
|---|---|
| **Space** | Play / pauză (în Animare înlocuiește „Select” de pe Space; Select rămâne pe **V**, adăugat ca scurtătură, și în toolbar) |
| **← / →** | Un cadru înapoi / înainte |
| **⇧ ← / ⇧ →** | 10 cadre (sau 1 s, după setare) |
| **⌥ ← / ⌥ →** | Cheia anterioară / următoare (a selecției, sau a oricărui obiect dacă nu e nimic selectat) |
| **Home / End** | Începutul / sfârșitul animației |
| **K** | Keyframe pe selecție (toate canalele) |
| **⌫ / Delete** | Șterge cheile selectate (dacă sunt chei selectate; altfel ce e azi) |
| **⌘C / ⌘V** | Copiază / lipește cheile |

Regulă de prioritate: cât timp o unealtă are o operație în curs (de exemplu Move după primul click, sau se tastează o
valoare), săgețile rămân ale ei (blocarea pe axe); altfel merg la timeline. Câmpurile de text își păstrează tastele
(`window.typingInField`). Toate apar în Preferințe → Scurtături (categoria „Animare”, `modes: [Animation]`).

## 5. Pașii (fiecare = commit-uri mici pe o ramură, unite în `main`, CI urmărit)

### Pasul 1 — Tastatura în Animare ✅ criteriu: Space / ← → / ⌥← → / Home / End funcționează; Space nu mai alege Select în Animare
- [ ] Comenzi noi `anim.play`, `anim.prevFrame`, `anim.nextFrame`, `anim.back10`, `anim.forward10`, `anim.prevKey`,
      `anim.nextKey`, `anim.start`, `anim.end` (categoria „Animare”, clasificate în `Document::commandModes`).
- [ ] `Document`: `stepFrames(n)`, `jumpToKey(direction)` (pe cheile selecției sau ale tuturor), `togglePlay()`.
- [ ] Select pe V (păstrează Space în Modelare / Interacțiune); prioritatea săgeților (unealtă activă > timeline).
- [ ] Test în `tests/core/AnimationTests.cpp` pentru „cheia următoare / anterioară” (funcție pură în core).

### Pasul 2 — Canale pe proprietate (core + fișier) ✅ criteriu: testele vechi și noi trec; fișierele vechi se deschid
- [ ] `Channel`, `Key` cu easing Bézier (`Easing { name, x1, y1, x2, y2, step }`), `ObjectTrack::channels`.
- [ ] `evaluateInstance` (recompunere în jurul pivotului), `setChannelKey`, `removeKey/moveKey` pe canal.
- [ ] Migrarea în `AionFile.cpp` (vechi → canale), `formatVersion`++; teste de rotunjire.
- [ ] `Document`: auto-key scrie doar canalele schimbate; `timeline` (QVariantMap) expune canalele și valorile.

### Pasul 3 — Timeline: obiecte expandabile, canale, valori la chei ✅ criteriu: captură cu un obiect deschis
- [ ] Rândul sumar + rândurile canalelor (doar cele existente), nume ca în Inspector, valoarea lângă fiecare cheie.
- [ ] Editarea valorii unei chei (dublu click) și a valorii curente din coloana stângă.
- [ ] Starea deschis / închis ținută per obiect (în sesiune).

### Pasul 4 — Easing: presetări și editorul de curbă ✅ criteriu: o curbă proprie salvată apare în alt document
- [ ] `ease(Easing, t)` pentru Bézier (rezolvarea lui x → t cu Newton + bisecție), teste.
- [ ] `overlays/EasingEditor.qml`: graficul cu mânerele, câmpurile, previzualizarea, presetările ca miniaturi.
- [ ] `UserPrefs.easingPresets` (salvează, redenumește, șterge); aplicarea pe cheie / chei selectate / canal.
- [ ] Miniatura curbei pe segmentul dintre chei; click → editorul.

### Pasul 5 — UX-ul timeline-ului ✅ criteriu: selecție multiplă, zoom, snap, copiere / lipire
- [ ] Selecție multiplă (⇧ / ⌘, dreptunghi), tragere în grup cu snap (cadre, alte chei), ⌫, ⌘C / ⌘V, meniul contextual.
- [ ] Zoom (⌘ + rotiță), scroll, bara de jos, „încadrează tot”, rigla adaptivă.
- [ ] Timpul `mm:ss.cc · cadru`, editabil; playhead tras pe riglă.
- [ ] Comutatorul Auto-key.

### Pasul 6 — Documentație și verificare ✅ criteriu: CI verde, QA bifat
- [ ] ADR (canale + easing Bézier + tastatura), `CLAUDE.md`, `docs/qa-checklist.md` (secțiunea „Animare”), `PLAN.md`.
- [ ] Capturi (dark + light, web după publicare).

### După (P2, câte un pas fiecare, în ordinea utilizatorului)
- [ ] Traiectoria obiectului în viewport · [ ] Editorul de curbe pe canal · [ ] Interval de redare și markeri
- [ ] Vizibilitate / opacitate · [ ] Faze de construcție · [ ] Studiu de însorire · [ ] Camera pe traseu / turntable
- [ ] Setările exportului video

## 6. Decizii de confirmat cu utilizatorul (implicitele, dacă nu răspunde altfel)

1. **Space în Animare = play / pauză**; Select primește și tasta **V** (Space rămâne Select în celelalte moduri).
2. **← / →** = un cadru; **⇧** = 10 cadre; **⌥** = cheia următoare / anterioară.
3. **Canale pe proprietate** (modelul nou), cu migrarea automată a fișierelor vechi.
4. Camera rămâne cu cheile ei (ținta, unghiurile, distanța) în acest plan; devine canale doar dacă cere (P2).
5. Curbele proprii se salvează **în preferințele aplicației** (disponibile în toate documentele); cheile își păstrează
   curba în fișier, deci un fișier trimis altcuiva arată la fel.
6. Auto-key primește un **comutator** (pornit implicit).
7. Ordinea funcțiilor P2 după Pasul 6: traiectoria în viewport, faze de construcție, camera pe traseu, studiu de
   însorire, vizibilitate / opacitate, editor de curbe, markeri, export.

## 7. Riscuri

- **Migrarea datelor** (pasul 2): descompunerea matricelor vechi în Euler poate da salturi de 360° între chei; se
  „despachetează” unghiurile (cea mai scurtă diferență față de cheia anterioară). Teste cu rotații mari.
- **Recompunerea în jurul pivotului** schimbă ușor rezultatul față de interpolarea de matrice de acum (rotația nu mai
  trece prin centrul cutiei, ci prin pivot) — e comportamentul dorit, dar animațiile vechi pot arăta altfel; migrarea
  păstrează pozițiile la chei, doar drumul dintre ele se poate schimba.
- **Tastatura**: Space și săgețile sunt folosite azi (Select, blocarea axelor, câmpurile de text); prioritatea din 4.4
  trebuie testată (scripturile de test nu pot simula modificatori — verificarea manuală e necesară).
- **Timeline.qml** crește mult: se împarte în componente (`TimelineHeader`, `TimelineTrack`, `TimelineChannelRow`,
  `TimelineKey`, `TimelineRuler`).

## 8. Cum se verifică

```bash
make build && ./build/mac-debug/tests/aion_tests "[animation]"     # testele de animație
./build/mac-debug/tests/aion_tests                                   # toate
S=/tmp/aion-shots; mkdir -p $S
AION_SCREENSHOT_DELAY_MS=5000 AION_SCREENSHOT=$S/anim.png \
  AION_STARTUP="scriptjs:aion.select([aion.box(0,0,0,1000,1000,1000)]);mode:animate;wait:300;placement:px=2000" \
  /usr/bin/perl -e 'alarm 60; exec @ARGV' build/mac-debug/app/Aion.app/Contents/MacOS/Aion
```
