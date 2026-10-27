# ADR 0020 – Timeline și keyframe-uri

- Stare: acceptat (2026-10-27)
- Context: utilizatorul vrea un timeline ca în Figma sau Blender, cu keyframe-uri pentru poziția, rotația,
  dimensiunea și culoarea obiectelor. Animațiile se salvează și pot fi exportate sau folosite în interacțiuni.

## Decizii

- **Model** (`model::Animation`, entitate cu undo, salvată în `.aion`): piste pe obiecte (transformarea întreagă,
  deci poziție, rotație și scară; plus culoarea) și o pistă a camerei. Keyframe-urile au valori absolute și un
  easing: liniar, lin (in-out), accelerare, încetinire, salt. Interpolarea transformărilor face rotația pe drumul
  cel mai scurt (quaternion).
- **Previzualizare, nu scriere în model.** Cât rulezi sau muți playhead-ul, Document dă viewport-ului pozele
  evaluate (`displayTransform`, `displayColor`, camera). Un click de editare iese din previzualizare, iar uneltele
  lucrează pe model. Fluxul: așezi obiectul, apeși ◆ Keyframe; muți playhead-ul, schimbi obiectul, apeși din nou ◆.
  Undo-ul rămâne curat, iar editarea vede mereu modelul real.
- **Cheile sunt în aceeași animație pentru toate obiectele.** Mai multe animații se aleg din timeline și pot fi
  pornite din interacțiuni (Faza 18).
- **Exportul video** randează animația cadru cu cadru (pozele și camera ei) și o codează în MP4. Scenele nu mai au
  legătură cu animațiile.

## Limite și urmări

- Fără auto-key, fără curbe Bézier editabile, fără piste pentru alte proprietăți (vizibilitate, materialul întreg).
- ~~La export, obiectele selectate apar cu evidențierea selecției~~ — rezolvat: capturile (PNG, cadrele video)
  se randează fără selecție, hover, overlay-ul uneltei și marcajele camerelor / luminilor.

## Completare (2026-10-27): plasarea în Inspector

- `model::Placement` are rotația completă (Euler XYZ, grade); o rotație cerută egală cu cea curentă păstrează exact
  matricea (fără erori de rotunjire).
- Lacăte pe cele 12 valori (`Instance::placementLocks`, salvate în `.aion`): Inspector-ul nu le editează, iar
  `transformInstances` (Move / Rotate / Scale) le readuce la valorile dinainte (`restoreLockedPlacement`).
- Cu timeline-ul deschis, ◇ din câmpuri pune un keyframe de transformare (toată transformarea, ca butonul din timeline).
