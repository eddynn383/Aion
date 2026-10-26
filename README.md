# Forma

Modelator 3D modern și nativ, inspirat de ușurința de lucru a SketchUp.
Prima țintă este macOS; urmează Windows și Web.

Planul complet de implementare se află în [`PLAN.md`](PLAN.md).

## Structura proiectului

| Folder | Conținut |
|---|---|
| `core/` | Nucleul C++ (geometrie, unelte, comenzi, fișiere). **Fără Qt.** |
| `render/` | Randarea 3D cu Qt RHI (Metal / Direct3D / WebGL) |
| `app/` | Aplicația Qt Quick: shell, panouri, tema (`app/qml/theme/Theme.qml`) |
| `tests/` | Teste Catch2 |
| `docs/decisions/` | Decizii de arhitectură (ADR) |

## Pornire rapidă (macOS)

```bash
make setup   # o singură dată: Xcode CLT, Homebrew, CMake, Ninja, Qt, vcpkg
make run     # compilează și pornește aplicația
```

| Comandă | Ce face |
|---|---|
| `make setup` | Instalează uneltele lipsă; se poate rula oricând, sare peste ce e deja instalat |
| `make build` | Compilează (prima dată durează câteva minute: vcpkg compilează dependențele) |
| `make test` | Compilează și rulează testele |
| `make run` | Compilează și pornește aplicația; mesajele ei apar în terminal |
| `make clean` | Șterge folderul de build |
| `make reconfigure` | Reface configurarea CMake (după schimbări în preseturi) |

Implicit se folosește build-ul de debug, cu AddressSanitizer + UBSan. Dacă ASan nu poate porni pe sistemul tău
(de exemplu Apple clang 17 pe macOS 26), CMake detectează asta și folosește doar UBSan.
Pentru build-ul optimizat: `make run CONFIG=release`.

### În VS Code
Instalează extensiile recomandate (VS Code le propune la deschiderea proiectului), apoi:
- **⇧⌘B** compilează;
- **F5** compilează și pornește aplicația cu debugger (configurația „Forma (debug)”).

### Instalare manuală (dacă nu vrei `make setup`)
1. `xcode-select --install`
2. Homebrew de la <https://brew.sh>, apoi `brew install cmake ninja qt`
3. vcpkg:
   ```bash
   git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
   "$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
   ```
4. În `~/.zshrc`:
   ```bash
   export VCPKG_ROOT="$HOME/vcpkg"
   export QT_ROOT_DIR="$(brew --prefix qt)"
   ```

Comenzile CMake de bază, fără `make`:
```bash
cmake --preset mac-debug && cmake --build --preset mac-debug && ctest --preset mac-debug
```

| Preset | Pentru ce |
|---|---|
| `mac-debug` | dezvoltare zilnică, cu AddressSanitizer + UBSan |
| `mac-release` | build optimizat |
| `core-debug` | doar nucleul și testele, fără Qt (orice platformă) |

## Navigare și scurtături

| Acțiune | Mouse | Trackpad |
|---|---|---|
| Orbit | buton din mijloc (sau unealta Orbit, `O`) | două degete |
| Pan | Shift + buton din mijloc (sau unealta Pan, `H`) | Shift + două degete |
| Zoom spre cursor | rotița, Ctrl + buton din mijloc (sau unealta Zoom, `Z`: tragi în sus/jos) | pinch (sau ⌘ + două degete) |
| Centrează pe punct | dublu-click cu butonul din mijloc | — |
| Zoom Extents | `⇧Z` | dublu-tap cu două degete |

| Scurtătură | Acțiune |
|---|---|
| `⌘K` | Command palette (toate comenzile, căutare fuzzy) |
| `⌘0` … `⌘6` | Vederi: Iso, Sus, Față, Dreapta, Spate, Stânga, Jos |
| `⌘ \` | Mod focus: ascunde/arată panourile laterale |

Gizmo-ul din colțul viewport-ului: click pe o axă = vederea dinspre ea, click pe centru = Iso.

### Variabile de mediu pentru dezvoltare
| Variabilă | Efect |
|---|---|
| `FORMA_THEME=light\|dark` | Forțează tema |
| `FORMA_SCREENSHOT=cale.png` | Salvează fereastra după randare și închide aplicația (`FORMA_SCREENSHOT_DELAY_MS` = întârzierea) |
| `FORMA_STORYBOOK=1` | Deschide pagina cu componente (doar debug) |
| `FORMA_STARTUP="view.top;palette:zoom"` | Rulează comenzi la pornire; `debug.benchmark` măsoară fps-ul |

## Convenții
Vezi secțiunea „Convenții de lucru” din [`PLAN.md`](PLAN.md).
Formatarea codului: `clang-format -i` pe fișierele modificate.
