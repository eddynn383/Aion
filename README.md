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

## Instalare pe macOS

### 1. Unelte de bază
```bash
xcode-select --install          # compilatorul clang
brew install cmake ninja git
```

### 2. Qt 6.8
Variantă recomandată: instalatorul oficial de la <https://www.qt.io/download-qt-installer-oss>.
Alege **Qt 6.8.x → macOS** și modulul **Qt Shader Tools**.

```bash
export QT_ROOT_DIR="$HOME/Qt/6.8.3/macos"
```

Variantă rapidă, cu Homebrew:
```bash
brew install qt
export QT_ROOT_DIR="$(brew --prefix qt)"
```

### 3. vcpkg
```bash
git clone https://github.com/microsoft/vcpkg "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg"
```

Adaugă cele două `export` (`QT_ROOT_DIR` și `VCPKG_ROOT`) în `~/.zshrc`, ca să rămână setate.

## Build și rulare

```bash
cmake --preset mac-debug          # prima dată durează câteva minute (vcpkg compilează dependențele)
cmake --build --preset mac-debug
ctest --preset mac-debug

open build/mac-debug/app/Forma.app
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
