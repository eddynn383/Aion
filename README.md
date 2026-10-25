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

Implicit se folosește build-ul de debug, cu AddressSanitizer + UBSan.
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

## Scurtături disponibile acum

| Scurtătură | Acțiune |
|---|---|
| `⌘ \` | Ascunde/arată panourile laterale |
| butonul ☾/☀ din dreapta sus | Comută tema: Sistem → Light → Dark |

## Convenții
Vezi secțiunea „Convenții de lucru” din [`PLAN.md`](PLAN.md).
Formatarea codului: `clang-format -i` pe fișierele modificate.
