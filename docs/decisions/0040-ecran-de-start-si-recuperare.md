# ADR 0040 – Ecranul de start și recuperarea pe sesiuni

- Stare: acceptat (2026-10-29)
- Context: cerința din 2026-10-29: la pornire, o fereastră ca splash-ul din Blender, din care faci un document nou,
  deschizi un fișier sau redeschizi ultimul (cu lista recentelor); după o închidere bruscă, opțiunea de a recupera
  documentul la care lucrai. Utilizatorul a sărit peste întrebările de design; alegerile de mai jos sunt ale
  implementării. Până acum (ADR 0009) exista o singură copie, `AppData/autosave.aion`, și un banner mic la pornire.
  Copia unică avea trei probleme: o recuperare ignorată era suprascrisă de prima autosalvare a sesiunii noi (bannerul
  rămânea, dar oferea acum documentul curent), două instanțe ale aplicației scriau în același fișier, iar o a doua
  instanță pornită lângă una care rula vedea copia celeilalte ca „sesiune neîncheiată”.

## Decizii

- **Copii pe sesiune** (`core/io/Recovery.h`, cu teste): fiecare pornire are un id (`yyyyMMdd-HHmmss-<pid>`) și își
  scrie copia în `AppData/Recuperare/<sesiune>.aion` + `<sesiune>.json` (`source` = fișierul documentului, `savedAt` =
  secunde Unix). `listRecoveries` le dă pe cele rămase, cele mai noi întâi (fără `.json`, sursa e necunoscută și timpul
  e al fișierului; fără `.aion`, nimic de recuperat); `recoveriesToPrune` păstrează cel mult 5 copii (2 pe web), niciuna
  mai veche de 30 de zile.
- **Cine rulează încă: un lock** (`render/src/RecoveryStore`): sesiunea ține `QLockFile` pe `<sesiune>.lock`. La
  pornire, o copie se oferă doar dacă lock-ul ei se poate lua (`staleLockTime 0`: numai un proces mort îl lasă liber,
  nu vârsta); cât e oferită, lock-ul e al nostru, ca o altă instanță să n-o ofere și ea. Pe web nu există procese:
  copiile stau și în QSettings (`recovery/sessions/<sesiune>/data|info`), iar lock-urile lipsesc. Lock-urile rămase
  de la sesiuni moarte care n-au apucat să scrie o copie se șterg la pornire.
- **Închiderea normală nu lasă copie**: `RecoveryStore` își șterge copia la distrugere (utilizatorul a ales deja la
  „Salvezi modificările?”), la Salvează, Deschide și Nou. „Nu salva” nu mai șterge nimic singur.
- **Recuperarea** (`Document::recoverSession(session)`, gol = cea mai nouă): modelul copiei devine documentul, cu
  fișierul de origine și marcat ca modificat; copia trece întâi în sesiunea curentă (`autosave()`), apoi originalul se
  șterge, ca o nouă închidere bruscă să nu piardă nimic. `discardRecovery(session)` renunță. Copia veche
  `autosave.aion` (și cheile `recovery/*` din QSettings) se mută la prima pornire ca sesiunea „anterioara”.
- **Ecranul de start** (`overlays/StartScreen.qml`, un Popover modal): banda de sus (grid-ul în perspectivă cu axele
  RGB ale iconiței, numele, sloganul, versiunea); copiile de recuperare deasupra, într-un card cu accent („Aion s-a
  închis neașteptat”, Recuperează / Renunță, cel mult trei); în stânga Document nou (⌘N), Deschide… (⌘O), Redeschide
  ultimul fișier (↩) și un sfat; în dreapta recentele cu miniatura din `.aion`, dosarul (cu `~`) și momentul
  (`text::describeTime`: „acum 5 min”, „azi, 14:32”, „ieri”, „12 oct.”); jos „Arată la pornire” și Turul uneltelor /
  Preferințe. Săgețile aleg un recent, Enter îl deschide (fără alegere: ultimul). Esc sau un click în afară îl închid
  (rămâne documentul gol); orice document deschis între timp (Finder, meniu, recuperare) îl închide.
- **Când apare**: la pornire, dacă `UserPrefs.startScreen` (Preferințe → General → Ecranul de start, sau comutatorul din
  ecran) sau dacă a rămas o copie de recuperare (atunci apare oricum). Nu apare în rulările automate (`AppInfo.automated`:
  `AION_SCREENSHOT` / `AION_STARTUP`), nici când aplicația a pornit cu un fișier. În browser recentele nu persistă: acolo
  apare doar pentru recuperare. Oricând: Ajutor → Ecranul de start, sau comanda `app.start`.
- **Bannerul** rămâne pentru o copie neprelucrată după ce ecranul s-a închis („Copie de recuperare: …”).
- **Verificarea din scripturi**: `debug.autosave` scrie copia acum, `debug.crash` (`AppInfo.simulateCrash`, doar în
  rulările automate) oprește procesul fără curățenie; rularea următoare cu `app.start` arată copia.

## Consecințe

- O recuperare ignorată rămâne disponibilă la pornirile următoare (până la Recuperează / Renunță sau 30 de zile).
- Autosalvarea nu are miniatură (ar cere o randare la fiecare minut); cardul de recuperare arată o iconiță.
- Pe web, o filă închisă fără salvare lasă o copie oferită la următoarea încărcare (ca înainte); două file deschise
  deodată nu se văd una pe alta ca „în rulare”.
- Mai puțin de un minut de lucru înainte de o închidere bruscă (intervalul autosalvării) nu se poate recupera.
