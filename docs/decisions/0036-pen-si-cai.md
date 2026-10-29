# ADR 0036 – Pen-ul (curbe Bézier) și căile de parcurgere

- Stare: acceptat (2026-10-28)
- Context: designul aprobat de utilizator, [`docs/plans/pen-si-cai.md`](../plans/pen-si-cai.md), și planul de
  implementare [`docs/plans/pen-si-cai-implementare.md`](../plans/pen-si-cai-implementare.md). Exemplul utilizatorului:
  o mașină (un grup) care intră în garajul unei case pe o linie trasă cu un Pen ca în Figma; Pen-ul trebuie să meargă
  și în Modelare, pentru forme libere editabile.

## Decizii

- **Matematica Bézier e în `core/geom/Bezier.h`**: puncte cu mânere (absolute), eșantionare după abaterea de la coardă
  (`defaultTolerance` = 0.2% din cutie, între 0.5 și 10 mm), parametrizare după lungime (`ArcLength`, cu tangenta din
  derivată, prelungirea dreaptă dincolo de capete și modulo pe o cale închisă), inserare (de Casteljau) și ștergere de
  puncte.
- **În Modelare, curba Pen e geometrie obișnuită plus o entitate**: muchiile vin din `geom::drawCurve` (eșantionarea,
  același `curve` id, `smoothCurve`), iar `model::PenCurve` păstrează punctele și mânerele. E **editabilă** cât timp
  muchiile ei se potrivesc exact cu eșantionarea și toate fețele lipite de ele stau în planul curbei
  (`penCurveEditable`); după Push/Pull, un vârf mutat sau mutarea în alt grup rămâne o curbă obișnuită (ca arcele).
  `replacePenCurve` șterge muchiile și le redesenează cu același id; fața-gazdă (o curbă desenată pe o față) se reface
  cu `createFaces`, cu materialul ei (`eraseEdge` nu reface fețele).
- **Uneltele**: `pen` (desenul: click = colț, tragere = punct neted, click pe primul punct = închide, Enter / dublu
  click = termină deschisă, Esc / ⌘Z = înapoi un nod (`Tool::undoStep`); planul = fața de sub primul click, a
  geometriei sau a unui obiect, altfel solul; săgețile îl schimbă înainte de al doilea punct, iar un al doilea punct pe
  axa albastră îl face vertical, rotit în jurul segmentului spre al treilea punct) și `penedit`
  (punctele și mânerele, Option rupe / comută, click pe curbă = punct nou, ⌫ șterge). `penedit` nu scrie în model:
  cere prin `ToolRequests::curveEdit`; Document o pune în locul lui Select (dublu click pe o curbă editabilă sau pe o
  cale), ca unealta internă `pivot`. În Animare, Pen-ul cere o cale (`ToolRequests::penPath`).
- **Calea e un obiect de scenă al unei animații** (`model::MotionPath`: transformare, curba în coordonatele ei,
  normala planului, animația, start, durată, easing, ancore). Cu altă animație activă se vede estompată și nu mișcă
  nimic; ștergerea animației îi șterge căile (`removeAnimationWithPaths`). Durata implicită = a animației; bara trasă
  după capătul animației o lungește.
- **Evaluarea** (`model/MotionPaths.h`): distanța = `ease(easing, u) · lungime + along`; obiectul stă cu centrul feței
  de jos a cutiei lui pe cale, sus = normala planului, fața = tangenta (orientarea doar din tangentă, ca un decalaj
  lateral mare pe o curbă strânsă să nu-l întoarcă), plus decalajul lateral / sus / rotirea. La ancorare, fața e axa
  orizontală a obiectului cea mai apropiată de direcția de start; mai multe obiecte merg în coloană (`along` negativ,
  din lungimile lor + un spațiu), cei din spate pe prelungirea dreaptă la start.
- **Ancorarea schimbă transformarea de bază** a obiectului (la startul căii) și o păstrează pe cea veche în ancoră;
  scoaterea (sau ștergerea căii) o pune la loc. Așa, și în Modelare, în scene, în export și în Modul interacțiune
  obiectul e unde îl duce calea.
- **Mutarea obiectelor ancorate e relativă la cale**: în Animare, Move / Rotate devin decalajul explicit al ancorei
  (`absorbAnchorEdit`); în Modelare (unde `CommandStack` nu permite o a doua comandă în aceeași tranzacție), diferența
  dintre transformarea de bază și poza de start a căii e un decalaj relativ, păstrat la fiecare moment și la editarea
  căii (`anchorTransform = pathPose · relativ`).
- **Poza**: `Document::pose()`, `shownTransform` și `displayTransform` aplică `pathTransformAt` / `applyPaths` după
  pistele obișnuite; poziția și rotirea pistei unui obiect ancorat nu se mai aplică, scara da.
- **Fișierul** `.aion` trece la versiunea 4 (`penCurves`, `motionPaths`); fișierele vechi se deschid neschimbate.
- **UI**: grupul Pen în toolbar (Modelare și Animare, ⇧P); calea în viewport (doar în editor: nu în redare sau
  export), în lista Obiecte, în Inspector (poziție, rotație, dimensiuni, parcurgere, obiecte ancorate) și în timeline
  (o bară trasă pentru start și durată); un obiect ancorat arată „Față de cale” în locul poziției și rotației.

## Consecințe

- Un obiect dintr-un grup nu se poate ancora (calea e în coordonatele modelului); se scoate întâi din grup.
- În Modelare, un decalaj „înainte” e o translație pe tangenta de start (nu de-a lungul curbei); în Animare e de-a
  lungul curbei.
- Copierea, gruparea sau Explode lasă curba Pen needitabilă (ca Bézier); editabilitatea se poate extinde mai târziu.
- Camera pe cale, marșarierul și creionul cu mâna liberă rămân pe mai târziu.
- O cale se editează (mutare, proprietăți, timp, ancore, ștergere) doar în Animare; numele și vizibilitatea oriunde.
  Ancorarea refuzată (obiect într-un grup, deja pe o cale a animației) spune de ce.
