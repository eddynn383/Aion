# ADR 0033 – Modurile de lucru: Modelare, Animare, Interacțiune

- Stare: acceptat (2026-10-28)
- Context: utilizatorul vrea un toolbar ca în Figma: navigarea lângă Select, iar în dreapta un comutator între trei
  moduri, fiecare permițând doar ce îi aparține. Planul complet și pașii: `docs/plans/moduri-de-lucru.md`.

## Decizii

- **Sursa de adevăr e `Document::workMode`** (0 / 1 / 2), nu QML: regula „ce e permis” se verifică în C++ pentru
  orice cale (palette, scurtături, meniuri, scripturi). Nu e un pas de undo, nu se salvează în fișier; ultimul mod
  se păstrează în preferințe, iar rulările automate pornesc în Modelare. Scurtături: ⌥1 / ⌥2 / ⌥3.
- **O singură clasificare a comenzilor**: `Document::commandModes(id)` / `commandAllowed(id)`. Palette arată doar
  comenzile modului (și pe cele comune), scurtăturile se dezactivează, meniul nativ le lasă inactive, meniul
  contextual le omite. **Gărzi** (`Document::require`) la începutul operațiilor publice, cu mesaj („În modul Animare
  nu se modifică geometria. Treci în Modelare (⌥1).”); scripturile (macro) sunt scutite.
- **Toolbar**: primul grup are Select, Interact (doar în Interacțiune) și secțiunea „Navigare” (Orbit, Pan, Zoom);
  grupurile și uneltele au `modes`; comutatorul `shell/ModeSwitch.qml` stă în dreapta.
- **Animare** — o *copie pozată* a modelului (`Document::pose()`): obiectele animate stau în poza de la timpul curent.
  Uneltele de transformare și Inspector-ul lucrează pe ea (cu un istoric temporar); ce s-a mutat devine keyframe-uri
  (auto-key, un pas de undo, animația se creează la nevoie). Modelul de bază nu se schimbă. Nucleul și uneltele nu
  au avut nevoie de schimbări. Timeline-ul e legat de mod: mereu vizibil în Animare, fără buton de închidere
  (decizia utilizatorului); fețele și muchiile nu se selectează aici.
- **Interacțiune** — fără două stări separate (decizia utilizatorului): cu Select se aleg obiecte și li se adaugă
  interacțiuni; Interact (sub Select, F5) rulează interacțiunile pe o copie a modelului (`interactionMode`), iar
  revenirea la Select o oprește. Un click pe un obiect fără interacțiuni arată un tooltip lângă cursor.
- **Layout pe mod**: secțiunile din stânga (Bibliotecă, Tag-uri, Materiale doar în Modelare; Scene în Modelare și
  Animare; Interacțiuni doar în Interacțiune), grupurile din Inspector și bara de stare (numele modului).

## Limite

- În Animare nu se animă culoarea prin auto-key (doar butonul de keyframe de culoare din timeline). Camerele și
  luminile se animă ca înainte, din timeline.
