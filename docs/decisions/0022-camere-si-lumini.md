# ADR 0022 – Camere și lumini ca obiecte

- Stare: acceptat (2026-10-27)
- Context: utilizatorul vrea camere și lumini ca obiecte, fiecare cu setările ei, folosibile în scene și pe timeline.

## Decizii

- **Entități separate în model** (`model::CameraObject`, `model::Light`, cu `CameraId` / `LightId`), nu instanțe cu
  geometrie: au undo, se salvează în `.aion` (`cameras`, `lights`) și nu intră în picking-ul geometriei, în
  cantități sau în exporturi.
- **Camera**: poziție, țintă, unghi de vedere, proiecție, planuri de tăiere. „Privește prin cameră” mută vederea
  (animat) și aplică unghiul ei; vederile standard și scenele revin la unghiul implicit (35°). Scenele și cheile de
  cameră din timeline se fac din vederea curentă, deci și dintr-o cameră după „Privește prin cameră”.
- **Lumini**: punctuală, spot (con + margine estompată), direcțională, panou (suprafață, luminează doar în față).
  Culoare, intensitate, rază (atenuare netedă până la zero la rază), aprinsă / stinsă. Shader-ul `mesh.frag`
  adaugă până la 8 lumini peste cer + soare, în spațiu liniar, cu aceeași strălucire Blinn-Phong ca soarele.
- **Gizmo-uri** de mărime constantă pe ecran (frustum pentru cameră, stea / con / săgeți / dreptunghi pentru lumini),
  în culoarea luminii, accent când sunt selectate. Un click pe marcaj (cu Select) selectează obiectul; Inspector-ul
  arată `ObjectInfo.qml`. Pozițiile lor intră în limitele de tăiere ale camerei, ca să nu dispară.
- Selecția de camere / lumini e separată de cea a geometriei (`Document::selectObject`); o selecție de geometrie o
  anulează. Delete șterge obiectul selectat.

## Limite și urmări

- Doar soarele aruncă umbre; umbre pentru un spot (a doua hartă de umbre) rămân de făcut — câmpul `castShadows`
  există deja în model.
- Luminile nu se mută încă trăgând de gizmo; se mută din Inspector sau cu „Pune în vederea curentă”.
- Panoul e aproximat ca o lumină punctuală orientată (fără arii reale / LTC).
