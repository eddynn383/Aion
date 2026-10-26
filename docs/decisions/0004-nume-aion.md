# 0004 – Numele proiectului: Aion (în loc de Forma)

**Status:** acceptat
**Data:** 2026-10-26

## Context
„Forma” a fost un nume de lucru. Înainte de publicarea pe GitHub am verificat conflictele:
**Autodesk Forma** (2023) e un produs real, tot pentru arhitectură și modelarea volumelor, deci
exact concurentul cu care nu vrem să fim confundați.

## Variante analizate
- **Shaper:** respins, pentru că Shaper Tools (routerul CNC Origin și programul Shaper Studio) e în același domeniu.
- **IShape:** respins: A+W iShape (CAD pentru geam), confuzie cu iShaper, prefixul „i” asociat cu Apple.
- **Lintel, Gable, Plinth, Joist:** fără conflicte majore, dar utilizatorul a preferat Aion.
- **Kerf, Maquette:** deja folosite de aplicații CAD / 3D.

## Decizie
Numele e **Aion** (afișat „Aion”).
- Semnificație: grecescul αἰών, „veac, eternitate”; Aion, zeul timpului fără sfârșit, rotește un inel,
  ca orbita din aplicație. Slogan: „Aion — modelează ce rămâne.” / „Aion — shape what lasts.”
- Easter eggs: **A**xes · **I**ntersections · **O**rbits · **N**odes; „a-Ion”, pentru originea românească.
- În cod: namespace `aion::`, `include/aion/`, ținte `aion_*`, `Aion.app`, bundle id `dev.aion.Aion`,
  module QML `Aion` / `Aion.Render`, resurse `:/aion/...`, variabile `AION_*`, fișiere `.aion`.

## Consecințe
- Numele e purtat și de branduri mari din alte domenii (jocul *Aion* de la NCSoft, mașinile *GAC Aion*),
  deci căutarea pe web va fi aglomerată: folosim forme compuse („Aion 3D”, un domeniu ca `aion3d.app`).
- Înainte de o eventuală comercializare: verificarea mărcii la EUIPO / OSIM, clasa 9 (software).
