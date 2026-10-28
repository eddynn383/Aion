# ADR 0035 – Camera: lentila, vederea prin cameră, uneltele și depth of field

- Stare: acceptat (2026-10-28)
- Context: planul `docs/plans/animatie.md`, secțiunea 9 (aprobat de utilizator, cu deciziile implicite din 9.5). Camera
  avea doar un unghi vertical; „Privește prin cameră” muta doar ținta și unghiurile, deci vederea nu era a camerei.

## Decizii

- **Datele** (`model::CameraObject`): focala pe un senzor (unghiul se calculează pe latura lungă a cadrului, ca
  „Sensor Fit: Auto” din Blender), formatul (L × H, implicit 1920 × 1080), shift-ul (fracțiuni din latura lungă),
  roll-ul, scara ortografică, depth of field (focus, diafragma), mărimea gizmo-ului. Fișierele vechi: `fov` (vertical)
  devine focala care păstrează unghiul în formatul camerei. Matematica e în `model/CameraLens.h`, cu teste.
- **`view::Camera` are roll și shift**: shift-ul e o proiecție decentrată (direcția de privire nu se schimbă, deci
  verticalele rămân drepte — perspectiva cu două puncte de fugă, cu camera orizontală).
- **Vederea prin cameră e o stare a viewport-ului** (`Viewport::lookThrough`), nu o poză: un zbor de intrare / ieșire
  (poza, unghiul pe tangentă, roll-ul, shift-ul), apoi viewport-ul urmează camera la orice schimbare (`model::
  lookThrough`, aceeași lentilă ca gizmo-ul). Cadrul formatului e încadrat în viewport (marginea 0,9), cu exteriorul
  estompat, eticheta și comenzile (`shell/CameraFrame.qml`); marginile lui schimbă formatul (prindere de presetări).
- **Navigarea**: fără „Blochează camera pe vedere”, orice orbit / pan / zoom iese din cameră (zbor înapoi); cu ea, mută
  camera, un pas de undo pe gest. În Animare scrie o cheie a camerei animației (camerele-obiect nu se animă, decizia 4),
  iar prin cameră pistele camerei animației mută camera, cu lentila ei.
- **Exportul prin cameră** (PNG, video): cadrul randat la marginea 1, decupat și scalat la formatul camerei; un zbor în
  curs se termină înainte.
- **Uneltele** (Move / Rotate / Scale) lucrează pe camera / lumina selectată (`ToolContext::object`, doar în Modelare,
  la nivelul modelului): punctele de prindere (poziția, ținta), Rotate în jurul poziției (roll-ul urmează o rotație în
  jurul privirii), Scale = mărimea gizmo-ului (perspectivă) sau scara (ortografic), laturile panoului, conul spotului.
- **Depth of field** (`shaders/dof.frag`): doar prin cameră (deci și la export), nu pe camerele ortografice. Scena se
  desenează într-o țintă proprie compatibilă cu a viewport-ului, apoi două treceri de estompare separabilă cu greutăți
  după cercul de confuzie (`model::circleOfConfusion`) din adâncimea liniară (`gbuffer.frag`); un punct din spate nu se
  întinde peste unul mai clar. „Alege punctul de focus”: următorul click dă distanța pe direcția privirii.

## Limite

- Exportul prin cameră nu randează offscreen la rezoluția formatului: peste rezoluția ferestrei, imaginea e mărită.
- DoF are raza maximă de 16 px pe trecere și nu are bokeh; dacă ținta proprie nu e compatibilă pe un backend, DoF
  rămâne oprit (mesaj în consolă).
- Ghidurile de compoziție, expunerea și animarea focalei / focusului rămân pentru C7 (P2).
