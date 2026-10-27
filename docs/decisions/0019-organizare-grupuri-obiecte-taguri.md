# ADR 0019 – Organizarea: forme, obiecte, grupuri, tag-uri (ca în Blender)

- Stare: acceptat (2026-10-27)
- Context: utilizatorul vrea organizarea ca în Blender. Formele devin obiecte, obiectele se pun în grupuri, iar
  grupurile sunt modul principal de organizare. Tag-urile rămân opționale și transversale: pot marca elemente din
  grupuri diferite (de exemplu pereții din toate camerele).

## Decizii

- **Forme**: geometria liberă (fețe, muchii).
- **Obiect**: o definiție cu geometrie, plasată de o instanță. ⌘G („Creează obiect”) o face din formele selectate.
  Copiile legate („Duplicare legată”, ⌥D) partajează definiția: se editează împreună.
- **Grup**: o definiție cu `collection = true`, care conține doar alte instanțe (folder în Outliner). ⌥⌘G
  („Grupează”) o face din obiectele selectate. Are transformarea ei, deci un grup se mută întreg.
- **Mutarea între grupuri** (drag & drop în Outliner, `model::moveToParent`) păstrează poziția în lume. Nu se poate
  muta un grup în el însuși și nici într-un obiect cu copii, unde destinația ar fi ambiguă.
- **Tag-uri** pe obiecte, grupuri, fețe și muchii (`Face::tag`, `Edge::tag`, păstrate la tăieri și în `.aion`). Un
  tag ascuns face elementele invizibile și imposibil de ales sau de atins prin snap. Filtrul e global, în
  `view::setHiddenTags` (ca setările de unități), și îl actualizează Document.
- **Fișierele vechi** se încarcă fără migrare: câmpul `collection` lipsește, deci totul e obiect. Asta e corect,
  pentru că vechile „grupuri” aveau geometrie.
- **Panoul din stânga** e un acordeon: Outliner (cu tag-urile dedesubt), Materiale, Scene. Spațiul se împarte după
  ponderi între secțiunile deschise, iar starea se păstrează.

## Limite

- „Selectează după tag” alege elementele din contextul editat. Selecția între contexte diferite (de exemplu fețe din
  două grupuri simultan) cere o selecție pe căi de instanțe și rămâne pentru mai târziu. Ascunderea și afișarea după
  tag funcționează însă în tot modelul.
