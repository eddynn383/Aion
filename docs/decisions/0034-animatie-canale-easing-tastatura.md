# ADR 0034 – Animația pe canale, easing Bézier și tastatura timeline-ului

- Stare: acceptat (2026-10-28)
- Context: planul `docs/plans/animatie.md` (aprobat de utilizator): Space / săgeți în Animare, tranziții predefinite
  și proprii (salvate), un timeline cu obiecte expandabile care arată doar proprietățile modificate și valoarea la
  fiecare cheie. Cheile vechi țineau matricea întreagă: nu se putea anima doar X și nici avea easing diferit pe
  rotație și poziție.

## Decizii

- **Canale pe proprietate** (`model::Channel { property, keys }`, `ObjectTrack::channels`): `position.x/y/z`,
  `rotation.x/y/z`, `scale.x/y/z`, `color`. Un canal există doar pentru o proprietate animată; proprietățile fără canal
  rămân cum sunt în model (poza de bază). Evaluarea: fiecare canal se interpolează separat, apoi transformarea se
  recompune din valori (`composeTransform`).
- **Poziția e a pivotului** (în coordonatele părintelui), nu colțul minim din Inspector: doar așa rotația și scara
  animate se fac în jurul pivotului (`Instance::pivot`, ADR 0030). Unghiurile sunt Euler XYZ în grade, ca în Inspector;
  la scriere se alege soluția Euler cea mai apropiată de valoarea de dinainte (fără salturi de 360°).
- **Auto-key scrie doar canalele schimbate** (Move pe X → `position.x`; Rotate pe Z → `rotation.z`). Un canal nou
  pornește cu valoarea de dinainte la începutul pistei (prima cheie a obiectului, sau 0 s), altfel o singură cheie ar
  ține valoarea pe toată animația. Keyframe-ul manual (◆, K) scrie toate cele 9 canale de transformare.
- **Easing = curbă Bézier cubică** ca `cubic-bezier` din CSS (x rezolvat cu Newton + bisecție) sau `step`; presetările
  (linear, ease, easeIn, easeOut, easeInOut, easeInBack, easeOutBack, step) sunt curbe cu nume. **Cheia ține curba
  însăși**, nu o referință: fișierul rămâne la fel pe orice calculator.
- **Fișierul** (`formatVersion` 2): canalele cu chei compacte `[timp, valoare, easing]` (culoarea `[timp, r, g, b, a,
  easing]`); easing-ul e numele presetării sau curba întreagă. **Migrarea** fișierelor vechi: cheile-matrice se
  descompun în canale după ce modelul e construit (față de pivotul și poza fiecărui obiect); se păstrează canalele care
  variază sau diferă de model; easing-ul vechi (enum) devine presetarea cu același nume.
- **Cheile pe timeline** se adresează prin `KeyRef { instance, property, time }` (instanța nulă = camera; fără
  proprietate = rândul sumar al obiectului): ștergere, mutare în grup, easing, copiere / lipire pe aceleași canale
  sau pe alt obiect.
- **Tastatura în Animare**: comenzile `anim.*` (Space redare, ← → cadru, ⇧ 10 cadre, ⌥ cheia anterioară / următoare,
  Home / End, K keyframe). Fiecare tastă are un singur „proprietar” pe mod (`window.shortcutOwners`): comanda modului o
  bate pe cea comună, deci Space nu mai alege Select în Animare (Select are și V). Săgețile rămân ale uneltei cât
  `Tool::ownsArrowKeys()` (Move după primul click, Rotate — săgețile aleg axa, Scale în tragere).

- **Timeline-ul** (`shell/Timeline.qml`, `TimelineRow`, `TimelineKey`, `TimelineRuler`): un rând pe obiect (momentele
  cu chei, bare pe intervale) care se deschide în canalele lui, cu valoarea la fiecare cheie și valoarea curentă
  editabilă; vederea în timp proprie (`viewStart`, `pxPerSecond`: zoom, derulare, bara de jos, „încadrează tot”);
  selecția (`selectedKeys`), tragerea în grup cu snap, dreptunghiul, copierea / lipirea, ceasul editabil.
- **Editorul de curbă** (`overlays/EasingEditor.qml`): graficul cu mânere, valorile, previzualizarea, presetările;
  curbele proprii stau în preferințele aplicației (`preferences.easingPresets`), deci în orice document.
  `Animation::defaultEasing` = easing-ul cheilor noi (în fișier).
- **Auto-key** (`Document::autoKey`, pornit implicit): oprit, o schimbare din Move / Rotate / Scale / Inspector intră în
  poza de bază, iar o proprietate deja animată își mută toate cheile cu diferența (`model::adjustWithoutKeys`).

## Limite

- O transformare cu forfecare (un obiect rotit și scalat neuniform pe axele lumii) se descompune fără forfecare la
  animare.
- Un pivot mutat după animare (`model::setPivot`, `setPlacement`): cheile de poziție se recalculează
  (`model::rebasePivot`), deci la ele obiectul rămâne exact pe loc; rotația animată se face de acum în jurul pivotului
  nou, așa că drumul dintre chei se poate schimba (un pivot mutat ca să repare o rotație o repară). Pivotul nu se
  animă: în Animare, Inspector-ul și marcajul tras îl schimbă în model, raportat la poza de la timpul curent.
- Curbele presetărilor vechi (cubice) diferă puțin de presetările Bézier cu același nume.
- Camera își păstrează cheile ei (țintă, unghiuri, distanță), cu easing-ul nou.
