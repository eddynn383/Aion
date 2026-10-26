#!/usr/bin/env bash
# Comenzi de dezvoltare: build, test, run, clean.
# Utilizare: scripts/dev.sh <comandă> [debug|release]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

command="${1:-run}"
config="${2:-debug}"

case "$(uname)" in
    Darwin) preset="mac-$config" ;;
    *)      preset="core-debug" ;; # pe alte platforme deocamdată doar core + teste
esac

# Valori implicite, ca scriptul să meargă și dintr-un terminal fără ~/.zshrc încărcat
# (de exemplu din VS Code).
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
if [[ -z "${QT_ROOT_DIR:-}" ]] && command -v brew >/dev/null 2>&1; then
    QT_ROOT_DIR="$(brew --prefix qt 2>/dev/null || true)"
    export QT_ROOT_DIR
fi

build_dir="build/$preset"
app="$build_dir/app/Aion.app"

configure() {
    if [[ ! -f "$build_dir/build.ninja" ]]; then
        cmake --preset "$preset"
    fi
}

build() {
    configure
    cmake --build --preset "$preset"
}

run_tests() {
    build
    ctest --preset "$preset"
}

case "$command" in
    build)
        build
        ;;
    test)
        run_tests
        ;;
    run)
        build
        if [[ "$(uname)" == "Darwin" ]]; then
            # Rulăm direct executabilul, ca mesajele aplicației să apară în terminal.
            "$app/Contents/MacOS/Aion"
        else
            echo "Aplicația se poate rula deocamdată doar pe macOS."
        fi
        ;;
    clean)
        rm -rf "$build_dir"
        echo "Șters: $build_dir"
        ;;
    reconfigure)
        rm -f "$build_dir/CMakeCache.txt" "$build_dir/build.ninja"
        configure
        ;;
    *)
        echo "Comandă necunoscută: $command"
        echo "Comenzi: build, test, run, clean, reconfigure   (opțional: debug | release)"
        exit 1
        ;;
esac
