# Aion – listă de testare manuală

Bifează fiecare scenariu în aplicație (`make run`). Notează ce nu merge cu pașii de reproducere.

## Navigare și interfață
- [ ] Orbit (butonul din mijloc / O), Pan (Shift / H), Zoom (rotiță, Z), trackpad (două degete, pinch)
- [ ] Gizmo-ul de axe: click pe o axă → vederea standard; ⌘0–6; Zoom Extents (⇧Z)
- [ ] Command palette (⌘K): căutare, comenzi recente, „Deschide recent: …”
- [ ] Teme: Sistem / Light / Dark; panourile (⌘\); tooltip-urile lizibile
## Desen și inferență
- [ ] Line: snap pe capăt / mijloc / muchie / față / axe; Shift și săgețile blochează; VCB (`4m`, `2500`, `3'6"`)
- [ ] Camera de 4 × 3 m numai din inferențe și VCB se închide într-o față
- [ ] Rectangle (`3000,2000`), Circle (`24s`, rază), Polygon, Arc (săgeată)
- [ ] Push/Pull: cutie, extindere, micșorare, fereastră prin perete, nișă, dublu-click repetă, Option păstrează fața
- [ ] Move / Rotate / Scale cu VCB; Option copiază; `x3` și `/3`; acoperiș în două ape (Move pe coamă, ↑)
- [ ] Offset, Eraser (curbe întregi), Select (click, dublu, triplu, dreptunghi window/crossing)
## Organizare
- [ ] Creează grup (⌘G) / obiect (⇧⌘G); dublu-click intră, Esc iese; restul se estompează
- [ ] Copii ale unui obiect se modifică împreună; Fă unic; Desface
- [ ] Outliner (selectare, redenumire, ascundere); Tag-uri (vizibilitate); Entity Info (dimensiuni, arie, lungime)
- [ ] Meniul de click dreapta: geometrie, un obiect, două obiecte, spațiu gol, în grup
## Fișiere
- [ ] Salvează / Deschide `.aion`, punctul de „modificat” din titlu, întrebarea la închidere
- [ ] Autosalvare: închide forțat aplicația cu modificări, repornește → bannerul de recuperare
- [ ] Export OBJ, GLB, glTF, PNG; deschide GLB-ul în Blender; import OBJ / glTF
## Grafică
- [ ] Materiale (bibliotecă, în model, pipetă ⌥, ⌘ conectate, ⇧ același material), sticlă transparentă
- [ ] Stiluri: materiale, clay, linii, wireframe, X-Ray; profile; ceață; umbre (oră, lună)
## Măsurare și adnotare
- [ ] Tape Measure (ghidaj paralel / punct), Protractor, cote, texte (editare, ștergere), ghidaje în inferență
- [ ] Secțiuni (activare, mutare ±100 mm, ștergere); Intersect cu modelul; reuniune / scădere / intersecție
- [ ] Scene: adaugă, actualizează, redenumește, tranziție animată
## Preferințe, meniu, sistem (macOS)
- [ ] Meniul de sus: Aion (Despre, Preferințe ⌘,), Fișier (+ Deschise recent), Editare, Vedere, Stil, Organizare, Unelte, Ajutor
- [ ] Scurtăturile ⌘ rulează o singură dată (⌘Z nu face două undo-uri; ⌘G un singur grup)
- [ ] În Measurements box / la redenumire, ⌘A / ⌘Z acționează pe text, nu pe model
- [ ] Preferințe: unitățile (Auto, mm, cm, m, inci, picioare) schimbă cotele, Entity Info, VCB, bara de jos; precizia
- [ ] Tema și latitudinea se păstrează după repornire; latitudinea mută umbrele
- [ ] Despre Aion: versiune, backend grafic
- [ ] Dublu-click pe un `.aion` în Finder deschide Aion (și cu aplicația deja pornită); trage un .aion pe iconița din Dock
- [ ] `make package` → `dist/Aion-0.1.0.dmg`: montează, trage în Applications, pornește (fără Homebrew în PATH)
- [ ] Model mare (mii de fețe): navigare fluentă, picking fără întârzieri
## Ghiduri și tur
- [ ] Hover ~1,3 s pe o unealtă din toolbar → ghidul animat (titlu, scurtătură, animație, pași, sfat); dispare când pleci
- [ ] Animațiile arată bine în light și dark; cu „Reduce Motion” (macOS) apare un cadru fix
- [ ] Preferințe → „Ghiduri animate la hover lung” oprit → doar tooltip-ul simplu
- [ ] Ajutor → Turul uneltelor: Înapoi / Următorul, săgețile ←/→, punctele de progres, „Încearcă” activează unealta
## Windows (Faza 11)
- [ ] `win-debug` compilează din Developer PowerShell; aplicația pornește pe Direct3D 11
- [ ] Textele de ajutor arată Ctrl / Alt (nu ⌘ / Option); Ctrl+Z, Ctrl+G, Ctrl+S etc.
- [ ] Scalare 125% / 150%: interfața clară, picking-ul nimerește
- [ ] Fișiere cu diacritice în nume (ă, î, ș, ț) se salvează, se deschid și se exportă
- [ ] Installer-ul (Inno Setup): scurtături, asocierea .aion, dezinstalare curată
## Web (Faza 12)
- [ ] Pagina din artefactul CI `aion-web` (servită local, de exemplu `python3 -m http.server`) pornește în Chrome, Safari, Firefox
- [ ] Deschide (selectorul browserului), Salvează (download .aion), Exportă (OBJ + MTL, GLB, PNG), Importă
- [ ] Reîncarcă pagina după modificări (> 1 min) → bannerul de recuperare
- [ ] Umbrele: merg sau se dezactivează fără erori; fps pe casa din Faza 5
