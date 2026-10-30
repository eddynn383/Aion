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
| Liniile de cant în viewport | Pe mijlocul feței înguste, 0,3 mm în afara plăcii, cu adâncime (nu se văd prin alte piese); comutatorul e în Preferințe → Mobilier (nu în bara viewport-ului) | §7 al planului, pasul 7 | |
| Decorul cantului | Doar materialele incluse de tip decor sau lemn | Pasul 7 | |
| Lista „În model” | Dalele au iconița și „×N”, fără miniatură randată (amânată); doar componentele (Creează componentă sau copiile ⌥D), nu obiectele simple ⌘G | Pasul 8 | |
| Mutarea unei piese cu rol în alt părinte (Outliner) | Fără confirmarea „Corpul devine obișnuit” încă (doar Explode și grupările o cer) | Pasul 6 | |
