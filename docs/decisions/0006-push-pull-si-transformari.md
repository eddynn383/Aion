# 0006 – Push/Pull, transformări și curbe

**Status:** acceptat
**Data:** 2026-10-26

## Context
Faza 5 aduce operațiile care fac din desen un model 3D: Push/Pull (inclusiv tăieri și găuri), Move/Rotate/Scale
cu deformarea geometriei conectate, curbe (cerc, arc, poligon), Offset și suprafețe netede.

## Decizie

### Push/Pull (`geom::pushPull`)
- Construit peste desenul „lipicios” existent: laturile (un patrulater pe fiecare muchie a fiecărei bucle) și capacul
  se desenează cu `drawPolygon`, deci se unesc, se împart și formează fețe ca orice linie. Apoi fețele din planurile
  operației se clasifică și se orientează.
- **Normala feței arată în afara solidului.** Distanța pozitivă (în sensul normalei) adaugă material; negativă îl scoate.
- **Fața liberă** (nicio muchie nu are alte fețe): rămâne ca bază, rezultă o cutie închisă. Altfel fața se mută
  (dispare de pe loc), dacă nu se cere păstrarea ei (Option).
- **Tăiere:** un patrulater lateral care cade pe material existent (o față coplanară îl conține) e o tăietură și dispare.
  **Anulare:** un capac care cade pe o față existentă o anulează: se face gaură (fereastra prin perete).
- **Curățenie** în jurul operației: muchiile rămase fără fețe se șterg, fețele coplanare cu aceeași orientare se unesc
  (`mergeFacesAcross`), vârfurile cu două muchii coliniare se dizolvă (`dissolveVertex`).
  Așa, fața de sus a unei cutii trasă în sus dă tot o cutie cu 6 fețe.

### Transformări (`geom::transformVertices`, `geom::copySelection`)
- Move/Rotate/Scale transformă **vârfurile** (ale selecției sau ale entității de sub cursor); geometria conectată se
  deformează. Fețele atinse își recalculează planul (Newell, deci orientarea urmează buclele); o față care nu mai e
  plană se **pliază în triunghiuri**. O muchie care s-ar strânge într-un punct anulează operația.
- Copierea (Option) redesenează muchiile transformate prin `drawLine` (se lipesc de geometria existentă) și reorientează
  fețele copiate după normalele originalelor. `x3` / `/3` după o copiere: array (al doilea înlocuiește copia).
- Mesh are primitivele noi `setPosition`, `setFacePlane`, `setEdgeStyle`, înregistrate pentru undo ca celelalte.

### Curbe și muchii soft
- `Edge` are `soft`, `curve` (identificatorul curbei, 0 = niciuna) și `smoothCurve`. Bucățile unei muchii împărțite
  moștenesc stilul. Click (Select) sau gumă pe o muchie a unei curbe = toată curba.
- Push/Pull pe o curbă netedă (cerc, arc) face laturile dintre segmente soft; capacul e o curbă nouă. Poligoanele
  (curbe ne-netede) rămân cu muchii vizibile. Copierea și Offset păstrează curbele (cu identificatori noi).
- Randarea: muchiile soft nu se desenează (decât selectate sau sub cursor); normala unui vârf e media fețelor legate
  prin muchii soft în jurul lui (umbrire netedă).

### Unelte
- `ToolHelpers.h` (intern) are funcțiile comune; uneltele de transformare și cele de curbe stau în fișiere separate.
- `inference::Engine::restartFrom(p)`: după primul punct al unei operații, o săgeată apăsată imediat (fără mișcare)
  blochează axa prin acel punct.

## Consecințe
- Scale e deocamdată uniform (punct fix, referință, factor); mânerele și scalarea pe axă rămân de făcut.
- Follow Me e amânat. Mutarea unor vârfuri peste alte entități nu le unește încă (fără „auto-merge” la Move).
- Testul „gata când” (căsuța, cu undo/redo pe fiecare stare) e în `tests/core/PushPullTests.cpp`.
