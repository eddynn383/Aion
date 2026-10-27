# ADR 0029 – Bevel și chamfer

- Stare: acceptat (2026-10-27)
- Context: utilizatorul vrea să teșească (chamfer) sau să rotunjească (bevel) o muchie sau toate laturile unei fețe.

## Decizii

- **O singură operație, două moduri.** `geom::bevelEdges` / `geom::bevelFace` primesc `distance` (retragerea pe
  fiecare față vecină, de la muchie) și `segments`: 1 = chamfer (o teșitură plată), mai multe = un arc tangent la ambele
  fețe (raza `distance · tan(φ/2)`, cu centrul pe bisectoare). Panoul (`overlays/BevelPanel.qml`) are tab-uri
  Chamfer / Bevel; comenzile `edit.chamfer` și `edit.bevel` sunt în palette și în meniul contextual.
- **Refacere prin fețe noi, nu prin operații Euler dedicate.** Fețele atinse (cele două vecine, capetele sau laturile)
  se șterg și se redesenează cu `drawPolygon`, aceeași cale ca desenul obișnuit, cu materialele, tag-ul și textura lor.
  `drawPolygon` închide singur orice buclă plană; fețele pe care nu le-am cerut (de exemplu inelul de muchii coplanare
  de sub o față teșită) se scot imediat. Codul e scurt și topologia rămâne validă prin construcție.
- **Pe o față, colțurile se îmbină „în unghi”** (ca la o ramă): punctul de colț al fiecărui segment e intersecția
  profilurilor celor două laturi.
- **Muchiile netede doar între benzi.** Muchiile tangente (spre fețele plane) rămân normale: netezite, ar întinde
  normala medie peste toată fața plană și ar umbri-o în gradient.
- **Previzualizare prin undo**, ca la Array / Soften (`Document::previewBevel` + `commitOperation` / `cancelOperation`).
  Operația se încearcă întâi pe o copie a mesh-ului: un caz nepermis arată motivul și nu lasă un pas gol în istoric.

## Limite

- Muchii convexe între exact două fețe plane fără găuri; vârfurile de la capete au exact trei muchii (cutii, forme
  extrudate). Muchiile selectate nu au voie să aibă vârfuri comune (nu se racordează colțurile între ele).
- Pe o față: toate colțurile cu trei muchii și laturi convexe; distanța nu poate întoarce fața pe dos.
- Geometria din interiorul obiectelor se teșește din contextul lor (dublu click pe obiect).
