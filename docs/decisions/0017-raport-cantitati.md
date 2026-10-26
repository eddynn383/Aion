# ADR 0017 – Raport de cantități și listă de debitare pentru mobilier

- Stare: acceptat (2026-10-26)
- Context: utilizatorul a cerut un raport de cantități (arii, volume) cu export CSV, plus o variantă pentru
  proiectarea de mobilier.

## Decizii

- **Calculul e în core** (`model/Quantities.h`), fără Qt, cu teste. Se socotește doar ce e vizibil (instanțe
  ascunse, tag-uri ascunse). Fiecare apariție a unei definiții (copiile unui obiect) are transformarea ei completă:
  ariile se calculează după transformare, deci și scalările sunt corecte.
- **Raportul general**:
  - pe obiecte, cu calea în ierarhie („Dulap / Ușă”): bucăți, material predominant, dimensiunile cutiei, aria
    fețelor proprii, volumul pentru solide (teorema divergenței);
  - ariile pe materiale și pe tag-uri;
  - totaluri: arie, volum, suprafețe orizontale în sus, lungimea muchiilor.
- **Lista de debitare (mobilier)**: fiecare grup sau obiect cu fețe proprii e o piesă.
  - Dimensiunile se măsoară pe fața cea mai mare: grosimea pe normala ei, lungimea pe muchia ei cea mai lungă,
    lățimea perpendicular. Așa, piesele rotite au dimensiunile corecte, fără să depindă de axele grupului.
  - Piesele identice (aceleași dimensiuni la 0,1 mm, nume și material) se adună.
  - Necesarul de plăci se calculează pe material și grosime: aria pieselor × (1 + pierderi) / aria plăcii,
    rotunjit în sus. Implicit: placă 2800 × 2070 mm, 15% pierderi.
  - Piesele mai groase decât pragul (implicit 100 mm) sunt masive, iar grupurile plate sunt „fără grosime”. Ambele
    apar în listă, dar nu intră în necesarul de plăci.
  - Cantul e estimat ca maxim (perimetrul tuturor pieselor). Muchiile cu cant real nu sunt modelate încă.
- **CSV pentru Excel în română**: separator „;”, virgulă zecimală, BOM UTF-8 (pentru diacritice). Ariile sunt în
  m², volumele în m³, iar dimensiunile pieselor în mm.

## Ce urmează (idei)

- Cantul pe fiecare muchie (care laturi au cant, ce grosime), direcția fibrei, cote pentru găuri și balamale.
- Optimizarea așezării pe plăci (nesting 2D), cu desen exportat (PDF / DXF).
- Prețuri pe material și deviz.
