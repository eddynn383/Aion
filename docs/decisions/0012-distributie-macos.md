# ADR 0012 – Finisare și distribuție pe macOS

- Stare: acceptat (2026-10-26)
- Context: Faza 10. Aplicația trebuie să arate și să se comporte ca o aplicație Mac, să deschidă fișierele `.aion`
  din Finder și să poată fi distribuită ca DMG.

## Decizii

1. **Meniul nativ** se face cu `Qt.labs.platform` (`shell/AppMenuBar.qml`), construit din aceeași listă de comenzi ca
   command palette. „Despre” și „Preferințe” folosesc rolurile native (ajung în meniul „Aion”). Pe macOS, scurtăturile
   cu ⌘ aparțin meniului, iar `Shortcut`-urile QML corespunzătoare se dezactivează (altfel ar rula de două ori).
   Pe Windows și Linux rămân `Shortcut`-urile QML și command palette (Faza 11 decide dacă adăugăm o bară de meniu).
2. **Preferințe**: unitatea și precizia lungimilor sunt în `core` (`units::DisplaySettings`, globale pe proces).
   `Document` le citește și le scrie în `QSettings`. Tema și latitudinea se păstrează prin `Settings` (QtCore) în QML.
3. **Asocierea `.aion`**: un `Info.plist` propriu (`app/macos/Info.plist.in`) declară tipul `dev.aion.model`
   (UTExportedTypeDeclarations, rol Editor, Owner) și OBJ/glTF ca tipuri pe care doar le poate deschide.
   Fișierele vin ca `QFileOpenEvent` (filtru în `main.cpp`) sau ca argumente în linia de comandă. Trec printr-o coadă
   din `AppInfo`, ca să nu se piardă cele sosite înainte ca interfața să fie gata.
4. **Pachetul**: `scripts/package-macos.sh` (`make package`) face build-ul release, rulează `macdeployqt`, șterge
   rpath-urile spre Qt-ul de build și semnează. Fără `AION_SIGN_ID`, semnarea e ad-hoc. Cu `AION_SIGN_ID` se semnează
   cu Developer ID și hardened runtime; cu `AION_NOTARY_PROFILE` se face și notarizarea (`notarytool` + `stapler`).
   Rezultatul e `dist/Aion-<versiune>.dmg`.

## Consecințe și limite

- Qt din Homebrew e compilat pentru macOS 14.4–26, deci pachetul făcut cu el nu rulează pe macOS 13, deși
  deployment target-ul nostru e 13.0. Mai rămân două limite:
  - un build **Universal** (arm64 + x86_64) cere un Qt universal (installer-ul oficial Qt sau un Qt compilat de noi),
    plus tripletul vcpkg `universal-osx`;
  - `macdeployqt` copiază și stiluri Qt Quick Controls nefolosite (~144 MB .app, ~60 MB DMG).
  Ambele se rezolvă cu un Qt oficial în CI. Până atunci, pachetul e pentru Apple Silicon cu macOS recent.
- Raportarea crash-urilor rămâne opțională și neimplementată.
