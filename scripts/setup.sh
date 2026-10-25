#!/usr/bin/env bash
# Instalează tot ce trebuie pentru build pe macOS. Se poate rula de câte ori vrei:
# ce e deja instalat este sărit.
set -euo pipefail

step() { printf '\n\033[1;34m==>\033[0m \033[1m%s\033[0m\n' "$1"; }
ok()   { printf '    \033[32m✓\033[0m %s\n' "$1"; }

if [[ "$(uname)" != "Darwin" ]]; then
    echo "Acest script este pentru macOS." >&2
    exit 1
fi

step "Xcode Command Line Tools"
if xcode-select -p >/dev/null 2>&1; then
    ok "instalate"
else
    xcode-select --install || true
    echo "Termină instalarea din fereastra deschisă, apoi rulează din nou: make setup"
    exit 1
fi

step "Homebrew"
if ! command -v brew >/dev/null 2>&1; then
    for candidate in /opt/homebrew/bin/brew /usr/local/bin/brew "$HOME/homebrew/bin/brew"; do
        if [[ -x "$candidate" ]]; then
            eval "$("$candidate" shellenv)"
            break
        fi
    done
fi
if command -v brew >/dev/null 2>&1; then
    ok "$(brew --version | head -1)"
else
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    echo "Homebrew a fost instalat. Urmează pașii „Next steps” afișați mai sus, apoi rulează din nou: make setup"
    exit 1
fi

step "CMake, Ninja, Qt"
for pkg in cmake ninja qt; do
    if brew list --formula "$pkg" >/dev/null 2>&1; then
        ok "$pkg"
    else
        brew install "$pkg"
    fi
done

step "vcpkg"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
if [[ -x "$VCPKG_ROOT/vcpkg" ]]; then
    ok "$VCPKG_ROOT"
else
    if [[ ! -d "$VCPKG_ROOT/.git" ]]; then
        git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
    fi
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
fi

step "Variabile de mediu în ~/.zshrc"
ZSHRC="$HOME/.zshrc"
touch "$ZSHRC"
if ! grep -q 'VCPKG_ROOT=' "$ZSHRC"; then
    echo "export VCPKG_ROOT=\"$VCPKG_ROOT\"" >> "$ZSHRC"
fi
if ! grep -q 'QT_ROOT_DIR=' "$ZSHRC"; then
    echo 'export QT_ROOT_DIR="$(brew --prefix qt)"' >> "$ZSHRC"
fi
ok "VCPKG_ROOT și QT_ROOT_DIR"

printf '\n\033[1;32mGata.\033[0m Deschide un terminal nou (sau rulează: source ~/.zshrc), apoi: make run\n'
