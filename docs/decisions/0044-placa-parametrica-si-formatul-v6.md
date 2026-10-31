# ADR 0044 – Placa parametrică, datele de piesă și formatul v6

- Stare: acceptat (2026-10-30)
- Design: [`docs/plans/mobilier-pe-corp.md`](../plans/mobilier-pe-corp.md), §1–§8. Leagă: ADR 0017, 0025, 0030 (pivot),
  0038, 0039, 0043.
- Context: lista de debitare ghicea plăcile din geometrie (orice grup sub 100 mm era placă, deci și mânerele de oțel);
  Aion nu avea date de mobilier.

## Decizii

- **`PartSpec` pe `Definition`** (placă sau feronerie): tip, grosime nominală, axa grosimii, axa fibrei, cantul pe cele
  4 laturi locale (cheie de cant + decor). **Decorul** stă pe `Instance::material`. **`Instance`** primește `roleKey`,
  `nameKey`, `relativePosition`; **`Material`** primește `category` și `libraryKey`; **`Definition`** primește
  `sourceKey` (UUID-ul componentelor salvate).
- **Placa parametrică** (D12, R-1): `generator = "board"`, `parameters = {length, width}`, geometria în mesh-ul
  definiției, originea în colțul minim. Starea (în acord / modificată manual / desprinsă) se calculează din geometrie,
  nu se salvează. `board` nu e în lista generală de generatoare.
- **Finalizatorul** (D20, R-1): rulat primul în `CommandStack::commit()`, pe definițiile `board` cu mesh schimbat: un
  Push/Pull pe un capăt, după care placa rămâne cutie cu grosimea nominală, trece în parametri. Mutarea originii se
  compensează pe instanțe (`translație += L·d`, pivotul explicit `− d` scris direct, scene și ancore cu L-ul lor,
  ghidaje și curbe `− d`); cheile de animație rămân. O excepție anulează tranzacția, fără să oprească aplicația.
  **Motiv (R-1):** placa își amintește dimensiunea de proiect („Readu la parametri”), preferat de autor față de
  varianta „dimensiunea doar din geometrie” propusă de recenzie.
- **Pivotul** (DS-1): la schimbarea dimensiunilor rămâne pe loc în lume; un pivot explicit își păstrează poziția
  relativă. **Scale** (D24) e permis și marchează placa.
- **Categoria materialului** (D10, D14): decide clasa obiectelor fără `PartSpec`; la încărcarea unui fișier v5,
  materialele cu numele unui material inclus își primesc categoria și cheia.
- **Unirea materialelor la inserare** (R-6): după `libraryKey`, altfel nume + categorie, altfel material nou.
- **Lista de debitare v1.5** (D17, D21, D22): clasificare după `PartSpec`, grupare fără nume, piesele ascunse debifate,
  totaluri în core, CSV cu dimensiunea finită implicită și protecție contra formulelor.
- **Legarea** (D11, DS-2): componentele salvate se leagă la plasare prin `sourceKey`; ⌘V, ⌥Move, ⌥D, Array și Make
  Unique golesc cheile pe copii.
- **Formatul v6** (D13, R-5): schema completă la pasul 1; orice salvare e v6; un build v5 refuză fișierul. Pașii 1–4
  se unesc în `main` în același timp, fiindcă publicarea pe web e ireversibilă.
