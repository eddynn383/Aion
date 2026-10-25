# Forma – context pentru Claude

Modelator 3D nativ, inspirat de SketchUp, cu UI modern (stil Claude app / Figma / Vercel)
și grafică mai bună decât SketchUp. Nume de lucru: **Forma** (nu folosim numele, logo-ul
sau formatul `.skp` ale SketchUp).

- **Comunicare cu utilizatorul: în limba română.**
- Planul complet și sursa de adevăr pentru ce urmează: [`PLAN.md`](PLAN.md). Bifează sarcinile în el pe măsură ce le termini.
- Decizii de arhitectură: `docs/decisions/` (ADR). Adaugă un ADR pentru orice decizie importantă nouă.

## Decizii deja luate (nu le redeschide fără motiv)
- Platforme: **macOS întâi**, apoi Windows, apoi Web, din același cod.
- Stack: **C++20, Qt 6 (≥ 6.7) cu Qt Quick/QML** (nu Qt Widgets), randare cu **Qt RHI** (`QQuickRhiItem`, Metal pe macOS),
  **CMake + Ninja + vcpkg**, **Catch2**, glm, earcut, Manifold (mai târziu), nlohmann-json, tinygltf.
- `core/` **nu include niciodată Qt** (ca să poată fi compilat și pentru WebAssembly).
- Geometrie non-manifold de tip radial-edge (muchii libere, muchii cu mai mult de 2 fețe), unitate internă mm, comparații numai prin `core/include/forma/core/math/Tolerance.h`.
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
- **Faza 0** verificată pe Mac (2026-10-26): `make setup`, `make test` (4/4) și `make run` merg. Rămâne doar CI-ul,
  care nu a rulat niciodată (proiectul nu e încă pe GitHub).
- **ASan e blocat pe acest Mac** (Apple clang 17 + macOS 26.6: deadlock în `AsanInitInternal`, chiar și la un `int main(){}`).
  `cmake/FormaOptions.cmake` testează la configurare dacă ASan pornește; dacă nu, folosește doar UBSan (cu avertisment).
- **Atenție la `timeout` din `/usr/local/bin`:** e binar Intel, iar tot ce rulează sub el trece prin Rosetta
  (CMake vede `x86_64` și alege tripletul `x64-osx`). Pentru limite de timp folosește `perl -e 'alarm N; exec @ARGV' ...`.
- Verificare vizuală fără permisiuni de screen recording: `FORMA_SCREENSHOT=/cale.png [FORMA_THEME=light|dark] Forma.app/Contents/MacOS/Forma`
  salvează fereastra după randare și închide aplicația.
- Avertismente cunoscute, inofensive: `FindWrapOpenGL ... /usr/X11R6/lib` (XQuartz), și `ld: ... built for newer macOS version`
  pentru framework-urile Qt din Homebrew (deployment target-ul nostru e 13.0; contează abia la distribuție, Faza 10).

## Următorii pași
Faza 1 (vezi `PLAN.md`).

## Convenții
C++20, `clang-format` (config în repo), fără `new`/`delete` manual, namespace `forma::`,
`PascalCase` pentru tipuri, `camelCase` pentru funcții, `m_` pentru membri privați.
Orice cod din `core/` vine cu teste. Commit-uri mici, cu mesaje clare.
