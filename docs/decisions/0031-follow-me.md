# ADR 0031 – Follow Me

- Stare: acceptat (2026-10-28)
- Context: extrudarea unui profil de-a lungul unui traseu (rame, cornișe, țevi, balustrade), amânată din Faza 5.

## Decizii

- **Fluxul din SketchUp, varianta cu selecție:** se selectează traseul (muchii legate sau o față, al cărei contur
  devine traseul), apoi unealta Follow Me și click pe profil. Varianta „trasă cu mouse-ul de-a lungul muchiilor” nu
  e făcută.
- **Secțiunile la coturi se proiectează pe planul bisector** (îmbinare în unghi): fiecare secțiune e cea dinainte
  împinsă de-a lungul segmentului până la planul bisector al cotului, deci benzile sunt plane prin construcție.
  Un traseu deschis pornește de la capătul de lângă profil (profilul devine capacul de început; la final se adaugă
  un capac); un inel nu are capace, iar fața profilului dispare.
- **Muchiile netede** se moștenesc: de-a lungul, la vârfurile profilului dintre două muchii netede (un cerc → un
  cilindru neted), și de-a curmezișul, la vârfurile traseului dintre două muchii netede (un arc).
- **Fețele se construiesc prin `FaceBuilder`** (comun cu bevel-ul, ADR 0029): drawPolygon, orientare, stil și
  scoaterea fețelor necerute. Bucățile unei fețe existente împărțite de o latură nouă rămân (înainte, curățarea le
  putea șterge).
- Unealta încearcă întâi pe o copie: o eroare (profil cu găuri, paralel cu traseul, traseu ramificat, cot prea
  strâns) apare în bara de stare și nu lasă un pas gol în istoric.

## Limite

- Profilul nu are găuri. Traseul nu se ramifică. Secțiunile nu se rotesc de-a lungul unei curbe 3D oarecare (doar
  proiecția pe planul bisector), ceea ce ajunge pentru trasee plane și elicoidale line.
