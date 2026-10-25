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

## Stadiu (la predarea din sesiunea cloud)
- **Faza 0** e scrisă. Pe Mac-ul utilizatorului configurarea și build-ul `mac-debug` au reușit.
- De verificat acum, pe Mac:
  1. `./build/mac-debug/tests/forma_tests` trece? Anterior, descoperirea testelor de către Catch2
     (care rulează executabilul de teste) părea să se blocheze. Bănuieli: două build-uri simultane
     în același folder, sau AddressSanitizer pe macOS 26. Descoperirea a fost mutată la `ctest`
     (`DISCOVERY_MODE PRE_TEST`). Dacă testele se blochează cu ASan, încearcă `-DFORMA_ENABLE_SANITIZERS=OFF`.
  2. Aplicația pornește (`make run`) și arată shell-ul: titlebar, panouri laterale, viewport gol cu gradient, toolbar plutitor, status bar.
  3. Tripletele vcpkg din `triplets/` (deployment target 13.0) elimină avertismentele `ld: ... built for newer macOS version`. Netestat încă.
  4. `make setup` / `make run` (scripts/) încă netestate pe Mac.
- Avertisment cunoscut, inofensiv: `FindWrapOpenGL ... /usr/X11R6/lib` (XQuartz e instalat). Nu folosim OpenGL pe macOS.
- Proiectul nu e încă pe GitHub; CI-ul (`.github/workflows/ci.yml`) nu a rulat niciodată.

## Următorii pași
1. Termină verificările de mai sus și bifează Faza 0 în `PLAN.md`.
2. Faza 1: fonturi Inter / Geist Mono, iconițe Lucide, componentele de bază, command palette (⌘K),
   titlebar integrat pe macOS, viewport `QQuickRhiItem` cu grid infinit, axe și camera orbit/pan/zoom.

## Convenții
C++20, `clang-format` (config în repo), fără `new`/`delete` manual, namespace `forma::`,
`PascalCase` pentru tipuri, `camelCase` pentru funcții, `m_` pentru membri privați.
Orice cod din `core/` vine cu teste. Commit-uri mici, cu mesaje clare.
