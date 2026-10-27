# ADR 0026 – Biblioteca de obiecte

- Stare: acceptat (2026-10-27)
- Context: o bibliotecă de obiecte (uși, ferestre, mobilier) cu căutare și drag & drop în viewport.

## Decizii

- **Secțiune în panoul din stânga** (`shell/LibraryPanel.qml`): căutare, filtru pe categorii, plăcuțe. Conținutul:
  obiectele dinamice (ADR 0025) și fișierele `.aion` ale utilizatorului din dosarul `Biblioteca` al aplicației
  (cu miniatura din arhivă).
- **Drag & drop**: o „fantomă” în fereastră poartă drag-ul; un `DropArea` peste viewport pune obiectul în punctul de
  sub cursor (suprafața atinsă sau solul, `Viewport::dropPointAt`). Click pe plăcuță = în centrul vederii.
- **Transferul între modele** e în core (`model/Transfer.h`): `insertModel` copiază conținutul unui model ca o
  componentă nouă (geometria cu primitivele Euler, deci cu undo; materialele după nume, texturile după sursă și
  conținut; obiectele dinamice își păstrează generatorul), iar `extractInstance` scoate un obiect într-un model
  separat, cu colțul minim în origine. „Salvează în bibliotecă” (+ din antet) scrie un `.aion` cu miniatura vederii.
- Click dreapta pe un obiect propriu îl scoate din bibliotecă.

- **Previzualizare la tragere**: peste viewport, cutia obiectului (la mărimea reală, calculată o dată într-un model
  temporar — `Document::libraryItemSize`) urmează cursorul pe suprafața sau solul de sub el.
- Plante (copac, brad, tufă) și oameni (stilizați, cu înălțime) sunt obiecte dinamice ca celelalte (ADR 0025).

## Limite

- Previzualizarea e cutia obiectului, nu geometria lui; fără inferență la puncte (se așază pe suprafață sau pe sol).
- Interacțiunile (regulile) nu se copiază din bibliotecă (țintesc obiecte din modelul sursă).
