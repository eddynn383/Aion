# ADR 0016 – Orbitare în jurul selecției sau al cursorului; zoom precis la scară mică

- Stare: acceptat (2026-10-26)
- Context: utilizatorul a cerut orbitarea și în jurul obiectului selectat. A observat și probleme de zoom la
  dimensiuni de ordinul milimetrilor și a întrebat de o setare de prag, ca în Blender.

## Cauzele problemelor de zoom

1. Ancora zoom-ului era pe planul de sol sau pe planul țintei, nu pe suprafața de sub cursor. Pasul zoom-ului
   (proporțional cu distanța până la țintă) era prea mare lângă un detaliu mic, iar zoom-ul trecea prin el.
2. Distanța minimă a camerei era de 1 mm.
3. Planul îndepărtat era la 100 × distanța până la țintă: foarte aproape de un detaliu, restul modelului dispărea.

## Decizii

- **Zoom spre suprafață** (ca „Auto Depth” din Blender, activ implicit): ancora e punctul de pe model de sub cursor
  (`Document::surfacePoint`). Înainte de zoom, ținta camerei se mută la adâncimea lui, fără să miște imaginea
  (`Camera::retargetToDepth`). Așa, pasul zoom-ului și planul apropiat urmează adâncimea reală. Fără model sub cursor,
  zoom-ul rămâne cel de până acum.
- **Planuri de tăiere adaptive**:
  - planul îndepărtat cuprinde toată scena (`Camera::setSceneBounds`);
  - planul apropiat e 1% din distanță, dar raportul far/near nu depășește 10⁶ (precizia buffer-ului de adâncime);
  - distanța minimă a camerei e 0,01 mm.
- **Setare manuală** „Clip start / Clip end” în Preferințe → Navigare, ca în Blender (0 = automat). E utilă doar
  în cazuri extreme; automatul ar trebui să ajungă.
- **Centrul orbitei**, în Preferințe și în meniul Vedere:
  - centrul vederii (ca până acum);
  - selecția (centrul cutiei ei);
  - punctul de sub cursor.

  Orbita în jurul unui pivot e o rotație rigidă (`Camera::orbit` cu `setOrbitPivot`): pivotul rămâne pe loc pe
  ecran. În timpul orbitei, pivotul e marcat cu un punct.
- Setările se păstrează între sesiuni (Settings, categoria `ui`).
