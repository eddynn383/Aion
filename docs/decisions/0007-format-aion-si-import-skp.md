# 0007 – Formatul `.aion` și importul `.skp`

**Status:** acceptat (importul `.skp` e condiționat de verificarea licenței SDK-ului)
**Data:** 2026-10-26

## Context
Utilizatorii care vin din SketchUp au modele în `.skp`. Formatul e închis și nedocumentat. Trimble oferă un
SDK oficial (SketchUp C API) care citește și scrie fișiere `.skp` și pe care îl folosesc multe aplicații.
Planul inițial excludea complet `.skp`.

## Decizie
1. **Formatul nostru e `.aion`** (Faza 7): arhivă ZIP cu `model.json` (topologie, grupuri și componente, tag-uri,
   materiale, stilul muchiilor), texturi, thumbnail și `formatVersion` pentru migrări. Salvează tot ce are nucleul.
2. **`.skp` numai la import, numai prin SDK-ul oficial.** Nu scriem un cititor propriu (fragil și riscant juridic)
   și nu exportăm în `.skp`.
3. Importul stă într-un **modul opțional** (`plugins/skp`, țintă `aion_skp`), construit numai când SDK-ul e prezent:
   - SDK-ul există doar pentru Windows și macOS, deci `core/` rămâne fără el (WebAssembly nu e afectat);
   - aplicația funcționează complet și fără modul.
4. **Licența se verifică înainte de orice cod** (prima sarcină a Fazei 7b): dacă permite folosirea într-un modelator
   independent, ce atribuire cere și dacă biblioteca se poate distribui cu aplicația. Concluzia se adaugă aici.
5. Calea fără licențe, disponibilă oricum: importul **glTF/GLB** (păstrează ierarhia) și **OBJ**. SketchUp poate exporta
   în aceste formate, cu pierderi de structură (componentele devin geometrie, tag-urile se pierd parțial).

## Consecințe
- „Import/export `.skp`” iese din lista „Ce NU facem”; rămân excluse exportul `.skp` și citirea fără SDK.
- Faza 7b e opțională și nu blochează Fazele 8–12.
- Fișierele de test `.skp` le creăm noi (fără modele cu drepturi de autor).
