# ADR 0043 – Domeniul mobilier

- Stare: acceptat (2026-10-30)
- Design: [`docs/plans/mobilier-pe-corp.md`](../plans/mobilier-pe-corp.md) (Decizii D10–D24, DS-1…DS-6, R-1…R-10).
  Leagă: ADR 0017 (raport de cantități), 0025 (obiecte dinamice), 0026 (bibliotecă), 0038 (performanța viewport-ului),
  0039 (copii), 0044 (placa parametrică și formatul v6).
- Context: Aion devine o unealtă pentru mobilier pe corp (dulapuri, corpuri de bucătărie, rafturi), pentru amatori
  avansați și ateliere mici. Concurentul direct e SketchUp + OpenCutList; PRO100, Polyboard și Mozaik rulează doar pe
  Windows.

## Decizii

- **Platforme: macOS → web → Windows.** Schimbă ordinea din `CLAUDE.md` (macOS → Windows → Web): concurenții de
  mobilier sunt pe Windows, iar nișa lui Aion e macOS și browserul.
- **Biblioteca vine plină** (R-3, R-7): plăci (`board:<tip>`), feronerie construită în cod (`hardware:<cheie>`) și
  corpuri gata făcute (`base`, `wall`, `tall`, `appliance`, `wardrobe`, `rack`), cu câmpuri proprii în Inspector.
- **Corpurile se refac din câmpuri și își reconciliază piesele după rol** (R-2): decorul, cantul și varianta de
  feronerie se păstrează; piesele adăugate de utilizator nu se ating; separatoarele puse cu clic se țin proporțional
  (R-8). Generatoarele `cabinet` și `shelf` de azi rămân doar pentru fișierele vechi (R-9), fără câmp de versiune.
- **„Întinde”** lățește corpurile făcute din plăci; pe un corp legat schimbă toate instanțele (DS-3).
- **Traducerea cu chei, la final** (DS-4, DS-6): pașii 1–11 scriu texte în română; pasul 12 le transformă în chei.
- **Un singur document de design** (R-4). **Validarea** (R-10): autorul testează întâi; alți utilizatori (un tâmplar)
  vin după pasul 7.
- **Păstrate, deși recenzia de simplitate le-a propus amânarea:** lista „În model” completă (DS-2), cantul vizual
  complet (DS-5), cele 8 limbi (D5). Motiv: autorul le consideră parte din experiența promisă; fiecare are un obiectiv
  care le testează.

## Zone înghețate

Animația, timeline-ul, regulile, Modul Interacțiune, DOF, video, asistentul AI, colaborarea și importurile rămân
funcționale și se repară, dar nu primesc funcții noi; primesc doar traducerea textelor. ADR-uri vizate: 0018, 0020,
0021, 0033, 0034, 0035. ADR 0039 rămâne valabil (⌘V și Move cu ⌥ fac copii unice); ADR 0044 adaugă doar golirea
cheilor pe copii.

## Decizii vechi schimbate

- ADR 0017: „doar ce e vizibil” rămâne pentru raportul general; lista de debitare arată și piesele ascunse (debifate);
  gruparea nu mai include numele; pragul de 100 mm rămâne doar pentru piesele fără date.
- ADR 0025: plăcile își țin geometria în propria definiție (X = lungime, Y = lățime, Z = grosime); corpurile din
  bibliotecă reconciliază în loc să șteargă.
- `PLAN.md`: „Modelare parametrică” iese din „Ce NU facem”.
