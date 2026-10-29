# ADR 0041 – Subdivide, Muchii netede și segmentele primitivelor

- Stare: acceptat (2026-10-29)
- Context: utilizatorul aștepta ca „Soften / Smooth” să rotunjească obiectul (mai multe fețe, o sferă mai fină).
  Soften doar ascunde muchii și netezește umbrirea, iar primitivele se creează deja netezite, deci nu schimba nimic
  vizibil. Am păstrat Soften (necesar pentru Push/Pull pe cercuri, Follow Me, importuri) și am adăugat ce lipsea.

## Decizii

- **Soften se numește „Muchii netede”** în meniu și în palette (comanda rămâne `edit.soften`). Panoul arată câte
  muchii au fost netezite și spune când pragul e prea mic (muchiile unui cub au 90°). Pe obiecte intră și în
  subobiecte (masa: blatul și picioarele).
- **Subdivide** (`geom/Subdivide.h`, comanda `edit.subdivide`, `overlays/SubdividePanel.qml`): Catmull-Clark pe
  o plasă de poligoane după indici, apoi fețele se refac prin `MeshBuilder` (patrulaterele neplane devin două
  triunghiuri, cele coplanare se unesc la loc). Muchiile cu altă număr de fețe decât 2 sunt margini (regula de
  margine; colțurile și răscrucile rămân pe loc). Muchiile noi sub 60° devin soft + smooth. Materialul / tag-ul
  feței originale trec pe fețele din ea. Fețele cu găuri se refuză; limita e `kMaxSubdivideFaces` (60 000).
- **Ce se subdivizează**: obiectele selectate (toată geometria lor și a subobiectelor) sau, în context, fețele
  conectate de selecție (altfel s-ar rupe de vecinele lor). Previzualizarea e prin undo, ca la Array / Soften;
  încercarea pe o copie întâi, ca o eroare să nu lase pas în istoric.
- **Segmentele** sferei / cilindrului / conului (Inspector, grupul „Formă”): în `Definition::parameters["segments"]`
  (fără schimbare de format). Se pot schimba doar cât geometria e încă primitiva (aceleași numere de vârfuri /
  muchii / fețe și aceleași dimensiuni ca o primitivă construită din nou): după Push/Pull sau Subdivide câmpul dispare.
- **O previzualizare care n-a schimbat nimic nu se anulează**: `runOnModel` întoarce dacă a adăugat un pas, iar
  `m_previewApplied` se ia de acolo (înainte, Esc sau slider-ul anulau pasul de dinainte al utilizatorului).

## Limite

- Fără muchii ascuțite păstrate (creases); pentru margini drepte: Bevel înainte de Subdivide.
- Subdivide pe o parte dintr-o suprafață închisă mută și vârfurile de pe marginea comună cu restul.
