# Mobilier – de verificat la testare

Valorile implicite alese de autor în [`mobilier-pe-corp.md`](mobilier-pe-corp.md), de verificat la punctele de control
(după pasul 4 și după pasul 7) și, mai târziu, cu alți utilizatori (decizia R-10). Fiecare rând spune unde se schimbă.

| Întrebare | Valoarea aleasă | Unde se schimbă | Notițe după testare |
|---|---|---|---|
| Formatul fișierului pentru firma de debitare (coloane, separator, rândul de antet) | Coloanele din §5, `;`, virgulă zecimală, rând de antet activ | §5 (CSV), pasul 4 | |
| Dimensiunea trimisă: finită sau de tăiere | Finită, plus coloanele de cant | D21, §5 | |
| Numele laturilor de cant și care e „fața” | L1 = latura lungă din față | §5, pasul 7 | |
| Tipurile și grosimile de plăci | PAL 16/18/25, MDF 16/18/19/22, HDF 3/4, placaj 12/15/18, masiv 18/20/26/40 | §2 | |
| Formatul plăcii brute | 2800 × 2070 (PAL, MDF, HDF), 2500 × 1250 (placaj) | §2 | |
| Canturile și dacă „se scad” | ABS 0,4 (nu), ABS 1 / 2 (da), melamină 0,4 (nu) | §2 | |
| Balamale pe ușă | ≤ 900 mm: 2; 901–1600: 3; > 1600: 4 | §4 | |
| Construcția corpurilor | Laterale pe toată înălțimea, spate HDF încastrat, uși aplicate cu 3 mm doar între ele | §4 | |
| Dimensiunile implicite ale corpurilor | Tabelul din §4 | §4 | |
| Adaosul de cant pe muchie | 50 mm | §5 | |
| Pierderile la plăci | 15% | §5 | |
| Feroneria din piesele ascunse intră în deviz? | Nu, până la „Include toate” | D17, D22 | |
| Corpurile identice dintr-o bucătărie: legate sau independente? | Legate (componentă salvată) | Obiectivul 6, pasul 8 | |
| Baza și capacul: pe toată adâncimea sau cât polițele? | Pe toată adâncimea (964 × 600 la dulapul de referință); polițele și separatoarele = adâncimea − spatele | §4 (tabelul de orientare spune altceva; s-a ales dulapul de referință) | |
| Polițele corpului pentru electrocasnice | În spațiul de sub nișă; ușa de jos acoperă și polița de sub nișă | §4, pasul 6 | |
| Balamaua și mânerul pe ușă | Balamaua la 5 mm de latura ei, pe spatele ușii; mânerul vertical, la 40 mm de marginea liberă, centrat | §4, pasul 6 | |
| Cantul în viewport | O fâșie pe toată fața îngustă, cu grosimea cantului spre interiorul plăcii (desen, nu geometrie: dimensiunile și lista nu se schimbă); comutatorul e în Preferințe → Mobilier | §7 al planului, pasul 7 (cerut de autor după testare) | |
| Decorul cantului | Doar materialele incluse de tip decor sau lemn | Pasul 7 | |
| Lista „În model” | Dalele au iconița și „×N”, fără miniatură randată (amânată); doar componentele (Creează componentă sau copiile ⌥D), nu obiectele simple ⌘G | Pasul 8 | |
| Mutarea unei piese cu rol în alt părinte (Outliner) | Fără confirmarea „Corpul devine obișnuit” încă (doar Explode și grupările o cer) | Pasul 6 | |
| „Placă pe față”: ghidul animat | Textul ghidului există; scena animată din ToolDemo lipsește (amânată) | Pasul 10 | |
| Decorul plăcilor desenate cu „Placă pe față” | Fără decor (Implicit), ca plăcile din scripturi; cele din corpuri primesc „Alb (decor)” | Pasul 10 | |
| „Întinde”: planul | Urmează cursorul după primul clic, al doilea clic îl fixează; „200/2” îl înlocuiește cu N plane egale | Pasul 11 | |
| „Întinde”: ghidul animat | Text da, scenă animată nu (amânată) | Pasul 11 | |
| Limba implicită | Româna (nu limba sistemului) cât traducerea acoperă doar lista de debitare și Inspector-ul | Pasul 12a | |
| Propozițiile cu numere în traducere | Lipite din bucăți („Afectează ” + N + „ apariții legate.”), fără plural `%n` încă | Pasul 12a | |
| Ușile corpurilor | Fiecare ușă are o animație de deschidere (0,8 s, 105° spre față) și interacțiunea „Clic → pornește animația (comută)”; copiile legate ale corpului se deschid împreună | Pasul 6 (cerut de autor după testare) | |
| Limitele câmpurilor corpurilor | Fără maxime (până la 100 m, 50 de piese); minimele de construcție: lățime și înălțime 100, adâncime 50; o valoare în afara limitelor se aduce la cea permisă, cu mesaj | Cerut de autor după testare | |
| Afișarea „uși deschise” în Modelare | Butonul cu ușa din bara viewport-ului (sau comanda „Ușile corpurilor”); doar desenul — clicurile nimeresc încă ușile închise | Cerut de autor după testare | |
| Cantul rotunjit | Pe latură: „Drept” / „Rotunjit”; desenat cu muchiile rotunde (raza = grosimea cantului), ieșit în afara feței cât grosimea cantului; în CSV „ABS 2 mm rotunjit” | Cerut de autor după testare | |
| Balamalele | O plăcuță 50 × 12 × 35 pe spatele ușii, dincolo de fața interioară a lateralei (nu mai trec prin laterală) | Cerut de autor după testare | |
| Limitele câmpurilor corpurilor | Fără maxime (până la 100 m, 50 de piese); minimele de construcție: lățime și înălțime 100, adâncime 50; o valoare în afara limitelor se aduce la cea permisă, cu mesaj | Cerut de autor după testare | |
| Afișarea „uși deschise” în Modelare | Butonul cu ușa din bara viewport-ului (sau comanda „Ușile corpurilor”); doar desenul — clicurile nimeresc încă ușile închise | Cerut de autor după testare | |
| Cantul rotunjit | Pe latură: „Drept” / „Rotunjit”; desenat cu muchiile rotunde (raza = grosimea cantului), ieșit în afara feței cât grosimea cantului; în CSV „ABS 2 mm rotunjit” | Cerut de autor după testare | |
| Balamalele | O plăcuță 50 × 12 × 35 pe spatele ușii, dincolo de fața interioară a lateralei (nu mai trec prin laterală) | Cerut de autor după testare | |
