# 0009 – Fișierele `.aion`, schimbul OBJ/glTF și autosalvarea

**Status:** acceptat
**Data:** 2026-10-26

## Context
Faza 7: salvarea și deschiderea modelelor, exportul și importul în formate deschise, protecția împotriva pierderilor.

## Decizie

### `.aion`
- Arhivă ZIP (miniz) cu `model.json` și, opțional, `thumbnail.png` (512 px, randat din viewport fără overlay-uri).
- `model.json` conține **tot** modelul cu ID-urile lui: definiții (cu geometria: vârfuri, muchii cu stil, fețe cu plan
  și bucle prin ID-uri de muchii cu semn = sens), instanțe (matricea completă), tag-uri. Vecinătățile (muchiile unui
  vârf, fețele unei muchii) se refac la încărcare. Încărcarea aplică un `model::ChangeSet` de creare, deci modelul
  rezultat e identic (`Model::sameAs`), iar validarea topologică respinge fișierele deteriorate.
- `formatVersion` (acum 1): un fișier mai nou e refuzat cu un mesaj clar; migrările se fac la încărcare.
- Salvarea scrie un fișier temporar alăturat și îl redenumește (fișierul existent nu rămâne pe jumătate scris).

### Schimb
- **OBJ** (+ MTL): fețe triangulate, câte o normală pe față, un `o` pe grup/obiect, în metri cu Y în sus.
- **glTF/GLB** (tinygltf, fără stb_image): fiecare definiție devine un mesh, instanțele devin noduri cu matricea lor
  (translațiile în metri), rădăcina rotește Z-sus în Y-sus. Copiile unui obiect folosesc același mesh.
- **Import**: OBJ ca un grup; glTF cu nodurile ca grupuri, iar mesh-urile folosite de mai multe noduri ca obiecte.
  Geometria importată se construiește direct (`geom::MeshBuilder`: vârfuri sudate pe o grilă de 0.01 mm), apoi fețele
  coplanare vecine se unesc și muchiile dintre fețe aproape coplanare devin soft (suprafețe curbe netede).

### Documentul în aplicație
- `CommandStack::markClean/isClean` (numere unice pe comenzi): documentul e „modificat” când istoricul nu e în punctul
  salvat (și după undo până acolo devine iar nemodificat). Titlul arată un punct după nume.
- Autosalvare la fiecare minut în `AppData/autosave.aion` (doar cu modificări). La pornire, un fișier rămas înseamnă o
  sesiune neîncheiată normal: un banner oferă recuperarea. Salvarea, deschiderea și „Nu salva” îl șterg.
  **Înlocuit de ADR 0040**: copii pe sesiune în `AppData/Recuperare/`, cu lock, oferite din ecranul de start.
- Documente recente (8) în QSettings; în command palette apar ca „Deschide recent: …”.

## Consecințe
- Texturile și materialele nu se salvează încă (Faza 8 le adaugă în `model.json` și în arhivă).
- Importul unor modele foarte mari e lent la unirea fețelor coplanare (O(n²) în cazul cel mai rău).
