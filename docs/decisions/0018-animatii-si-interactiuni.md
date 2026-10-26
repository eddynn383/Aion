# ADR 0018 – Animația scenelor și interacțiuni (ca în SketchUp)

- Stare: acceptat (2026-10-26)
- Context: utilizatorul a cerut un modul de animații și interacțiuni similar cu SketchUp. Acolo sunt două lucruri:
  - animația scenelor (View → Animation, Export → Animation);
  - unealta Interact pe componentele dinamice (o ușă care se deschide la click).

## Animația scenelor

- Cronologia e în core (`view/SceneAnimation`), cu teste: pe fiecare scenă, o pauză (`delay`), apoi o tranziție
  lină (`transition`, ease-in-out) spre următoarea; opțional în buclă. Aceeași cronologie e folosită la redare și
  la export, deci video-ul arată exact ca redarea.
- **Redarea** (`Viewport::playScenes`):
  - comenzi: panoul Scene → Redă, meniul Vedere, palette;
  - fiecare scenă își aplică stilul de afișare;
  - orice click sau scroll în viewport oprește redarea, ca în SketchUp.
- **Exportul**:
  - viewport-ul randează cadrele PNG (24/30/60 fps), fără elementele de interfață;
  - `AppInfo.encodeVideo` le codează în MP4 H.264 cu AVFoundation, inclus în macOS, fără dependențe;
  - fără AVFoundation se folosește ffmpeg, dacă e instalat; altfel cadrele rămân pe disc.

## Interacțiuni (unealta Interact)

- Fiecare instanță are un `Behavior` (în model, cu undo, salvat în `.aion`):
  - **Rotate**: balama la stânga, dreapta, sus sau jos, cu unghiul dat;
  - **Slide**: glisare în față, spate, stânga, dreapta, sus sau jos, cu distanța dată.
- Axele și punctele de balama se calculează din cutia definiției, în coordonatele ei. Comportamentul rămâne corect
  după Move și Rotate și e același pentru toate copiile unui obiect. Implicit, deschiderea e spre fața obiectului (−Y
  local); un unghi negativ inversează sensul.
- Starea `open` și transformarea „deschisă” sunt în model. Comutarea e o operație cu undo („Deschide” /
  „Închide”). În timpul animației, Document dă viewport-ului o transformare intermediară (`displayTransform`), fără
  să modifice modelul la fiecare cadru.
- La click, se alege cea mai adâncă instanță interactivă de sub cursor (`interactiveInstanceAt`): ușa dintr-un dulap
  grupat, nu dulapul.
- Configurarea se face din Entity Info → Interacțiune (Niciuna / Balama / Glisare, latura sau direcția, valoarea,
  durata).

## Limite și idei

- Fără condiții și formule, ca la componentele dinamice complete (atribute, „onClick” scriptat).
- Exportul nu include încă interacțiunile animate. Ar putea fi combinate cu scenele (o scenă = ușile deschise).
