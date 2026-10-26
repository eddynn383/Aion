# Aion – context pentru Claude

Modelator 3D nativ, inspirat de SketchUp, cu UI modern (stil Claude app / Figma / Vercel)
și grafică mai bună decât SketchUp. Numele: **Aion** (până la 2026-10-26 „Forma”; vezi ADR 0004).
Nu folosim numele, logo-ul sau formatul `.skp` ale SketchUp.
- Slogan: „Aion — modelează ce rămâne.” / „Aion — shape what lasts.”
- În cod: namespace `aion::`, header-e în `include/aion/`, ținte `aion_*`, module QML `Aion` / `Aion.Render`,
  variabile de mediu `AION_*`, fișiere `.aion`.

- **Comunicare cu utilizatorul: în limba română.**
- Planul complet și sursa de adevăr pentru ce urmează: [`PLAN.md`](PLAN.md). Bifează sarcinile în el pe măsură ce le termini.
- Decizii de arhitectură: `docs/decisions/` (ADR). Adaugă un ADR pentru orice decizie importantă nouă.

## Decizii deja luate (nu le redeschide fără motiv)
- Platforme: **macOS întâi**, apoi Windows, apoi Web, din același cod.
- Stack: **C++20, Qt 6 (≥ 6.9) cu Qt Quick/QML** (nu Qt Widgets), randare cu **Qt RHI** (`QQuickRhiItem`, Metal pe macOS),
  **CMake + Ninja + vcpkg**, **Catch2**, glm, earcut, Manifold (mai târziu), nlohmann-json, tinygltf.
- `core/` **nu include niciodată Qt** (ca să poată fi compilat și pentru WebAssembly).
- Geometrie non-manifold de tip radial-edge (muchii libere, muchii cu mai mult de 2 fețe), unitate internă mm, comparații numai prin `core/include/aion/core/math/Tolerance.h`.
- Undo/redo prin Command pattern, de la început.
- UI: design tokens în `app/qml/theme/Theme.qml`; componentele nu folosesc culori hardcodate. Dark mode implicit.

## Comenzi
```bash
make setup   # instalează uneltele (Xcode CLT, Homebrew, cmake, ninja, qt, vcpkg)
make build   # compilează (preset mac-debug, cu ASan + UBSan)
make test    # compilează + ctest
make run     # compilează + pornește aplicația
make reconfigure   # după schimbări în CMakePresets.json
```
Variabile necesare: `VCPKG_ROOT` (implicit `~/vcpkg`), `QT_ROOT_DIR` (implicit `$(brew --prefix qt)`).
Pe acest Mac, Homebrew e instalat în `~/homebrew`.

## Stadiu
- **Faza 0** verificată pe Mac (2026-10-26): `make setup`, `make test` și `make run` merg.
- Repo: <https://github.com/eddynn383/Aion> (urcat pe 2026-10-26; `main` e ramura principală).
- **ASan e blocat pe acest Mac** (Apple clang 17 + macOS 26.6: deadlock în `AsanInitInternal`, chiar și la un `int main(){}`).
  `cmake/AionOptions.cmake` testează la configurare dacă ASan pornește; dacă nu, folosește doar UBSan (cu avertisment).
- **Atenție la `timeout` din `/usr/local/bin`:** e binar Intel, iar tot ce rulează sub el trece prin Rosetta
  (CMake vede `x86_64` și alege tripletul `x64-osx`). Pentru limite de timp folosește `perl -e 'alarm N; exec @ARGV' ...`.
- Verificare vizuală fără permisiuni de screen recording: `AION_SCREENSHOT=/cale.png [AION_THEME=light|dark] Aion.app/Contents/MacOS/Aion`
  salvează fereastra după randare și închide aplicația.
- Avertismente cunoscute, inofensive: `FindWrapOpenGL ... /usr/X11R6/lib` (XQuartz), și `ld: ... built for newer macOS version`
  pentru framework-urile Qt din Homebrew (deployment target-ul nostru e 13.0; contează abia la distribuție, Faza 10).

- **Faza 1** făcută și unită în `main` (2026-10-26). Navigarea cu mouse-ul și bara de titlu confirmate de utilizator;
  trackpad-ul (două degete / pinch) încă netestat de el.
- Unelte de verificare (variabile de mediu): `AION_SCREENSHOT`, `AION_SCREENSHOT_DELAY_MS`, `AION_THEME`,
  `AION_STORYBOOK=1`, `AION_STARTUP="view.top;palette:zoom;debug.benchmark"` (afișează fps în consolă).

- **Faza 2** făcută și unită în `main`: `core/math`, `geom::Mesh` (topologie, primitive Euler, înregistrarea
  schimbărilor, `validate()`), `geom::drawLine` / `drawPolygon` / `eraseEdge` cu recalcularea fețelor pe plan,
  triangulare earcut, `commands::CommandStack`. 62 de teste, inclusiv fuzz de 10 000 de operații (~40 s în debug).
  Designul: `docs/decisions/0003-nucleu-geometric.md`. Lipsește doar rularea sub ASan (vezi mai sus).

- Toolbar-ul are unelte grupate ca în Figma (`shell/ToolGroupButton.qml`, `components/Menu.qml`).

## Următorii pași
1. Verifică primul run al CI-ului pe GitHub (n-a mai rulat niciodată; include ASan pe runner-ul macOS).
2. Prototipul WebAssembly pentru `aion_core` (PLAN, secțiunea 9, pasul 4), apoi Faza 3.

## Convenții
C++20, `clang-format` (config în repo), fără `new`/`delete` manual, namespace `aion::`,
`PascalCase` pentru tipuri, `camelCase` pentru funcții, `m_` pentru membri privați.
Orice cod din `core/` vine cu teste. Commit-uri mici, cu mesaje clare.
