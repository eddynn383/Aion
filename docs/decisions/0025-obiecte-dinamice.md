# ADR 0025 – Obiecte dinamice (parametrice)

- Stare: acceptat (2026-10-27)
- Context: utilizatorul vrea obiecte dinamice, de exemplu o ușă cu lățime și înălțime modificabile din proprietăți.

## Decizii

- **Definiția păstrează generatorul și parametrii** (`Definition::generator`, `Definition::parameters`), salvați în
  `.aion`. Generatoarele sunt în core (`model/Parametric.h`), cu parametri numerici, etichete, unități și limite:
  ușă, fereastră, masă, bibliotecă, dulap, scară dreaptă.
- **Părțile sunt subgrupuri** (toc, foaie, blat, picioare, polițe, trepte), fiecare cu cutia și materialul ei din
  bibliotecă. Piesele care se ating nu se lipesc într-o singură geometrie, se pot inspecta în Obiecte și au materiale
  diferite (sticlă, lemn, metal).
- **Un parametru schimbat reface părțile** (le șterge și le reconstruiește) într-un singur pas de undo. Obiectul e o
  componentă: copiile legate se schimbă împreună; „Fă unic” dă o variantă separată.
- Originea fiecărui obiect e colțul stânga-față-jos (X = lățime, Y = adâncime, Z = înălțime).

## Limite

- Editările manuale din interiorul părților se pierd la următoarea schimbare de parametru.
- Generatoarele sunt în cod; generatoare noi vin prin plugin-uri (Faza 27).
