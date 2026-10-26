# ADR 0013 – Ghiduri animate pentru unelte (hover lung și tur)

- Stare: acceptat (2026-10-26)
- Context: utilizatorul a cerut un „tur al aplicației”, cu popover-uri care arată (animat, ideal video) cum se
  folosește fiecare funcție. Popover-ul trebuie să apară la hover lung, ca „rich tooltips” din Photoshop.

## Decizii

1. **Animații vectoriale în loc de video.** Fiecare unealtă are o scenă izometrică mică, desenată cu `Canvas` în
   `components/ToolDemo.qml` și condusă de un singur parametru de timp `t` (0..1, în buclă). Scena arată:
   - cursorul, cu undă la click;
   - inferențele (pastile colorate);
   - cotele live;
   - valorile tastate în Measurements box.

   Avantaje față de video:
   - nu adaugă fișiere binare în repo (aceeași regulă ca la iconițe);
   - urmează tema (light/dark) și culorile reale ale axelor și inferenței;
   - respectă „Reduce Motion” (un cadru fix);
   - se poate porta pe Web fără codecuri.

   Dezavantaj: nu arată aplicația reală pixel cu pixel. Dacă vrem și video-uri, pot fi adăugate mai târziu ca resurse
   descărcate la cerere.
2. **Conținutul** (titlu, ce face, pași, sfat) stă într-un singur loc, `shell/ToolGuides.js`, folosit și de tooltip,
   și de tur. Lista `tourOrder` stabilește ordinea din tur.
3. **Hover lung**:
   - `IconButton.guideId`: tooltip-ul simplu apare la 450 ms, iar ghidul (`RichTooltip`) la 1,3 s;
   - ghidul se închide când cursorul pleacă de pe buton sau la click;
   - se poate dezactiva din Preferințe (`UserPrefs.hoverGuides`, păstrat în QSettings).
4. **Turul** (`overlays/ToolTour.qml`): toate uneltele pe rând, cu Înapoi/Următorul, săgeți și puncte de progres.
   Butonul „Încearcă” activează unealta. Se deschide din meniul Ajutor sau din command palette („Turul uneltelor”).
5. Verificare: `AION_STARTUP="debug.demos"` deschide o galerie cu toate animațiile oprite în două momente;
   `moves:x,y` simulează hover în coordonatele ferestrei.
