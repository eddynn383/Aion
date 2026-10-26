#!/usr/bin/env bash
# Pachetul de distribuție pentru macOS: build release, macdeployqt (Qt inclus în .app), semnare, notarizare, DMG.
#
# Utilizare:
#   scripts/package-macos.sh                      # nesemnat (doar pentru test local; Gatekeeper îl va bloca altundeva)
#   AION_SIGN_ID="Developer ID Application: Nume (TEAMID)" scripts/package-macos.sh
#   AION_SIGN_ID=... AION_NOTARY_PROFILE=aion-notary scripts/package-macos.sh
#
# Profilul de notarizare se creează o singură dată:
#   xcrun notarytool store-credentials aion-notary --apple-id <email> --team-id <TEAMID> --password <app-specific>
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
if [[ -z "${QT_ROOT_DIR:-}" ]] && command -v brew >/dev/null 2>&1; then
    QT_ROOT_DIR="$(brew --prefix qt 2>/dev/null || true)"
    export QT_ROOT_DIR
fi
preset="${AION_PRESET:-mac-release}"
build_dir="build/$preset"
dist="dist"
version="$(sed -n 's/^ *VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
version="${version:-0.0.0}"

echo "==> Build $preset"
[[ -f "$build_dir/build.ninja" ]] || cmake --preset "$preset"
cmake --build --preset "$preset"

rm -rf "$dist"
mkdir -p "$dist"
cp -R "$build_dir/app/Aion.app" "$dist/"
app="$dist/Aion.app"

echo "==> macdeployqt (framework-uri Qt, plugin-uri, module QML)"
deploy="$QT_ROOT_DIR/bin/macdeployqt"
[[ -x "$deploy" ]] || deploy="$(command -v macdeployqt)"
"$deploy" "$app" -qmldir=app/qml -verbose=1
# rpath-urile spre Qt-ul de build (Homebrew) ar face ca un plugin fără framework în bundle să încarce
# un al doilea QtCore din sistem; pachetul trebuie să depindă numai de ce are înăuntru.
otool -l "$app/Contents/MacOS/Aion" | awk '/LC_RPATH/{getline; getline; print $2}' | grep -v '^@' |
    while read -r rpath; do install_name_tool -delete_rpath "$rpath" "$app/Contents/MacOS/Aion"; done

if [[ -n "${AION_SIGN_ID:-}" ]]; then
    echo "==> Semnare cu „$AION_SIGN_ID” (hardened runtime)"
    codesign --force --deep --options runtime --timestamp \
        --entitlements app/macos/Aion.entitlements --sign "$AION_SIGN_ID" "$app"
    codesign --verify --deep --strict --verbose=2 "$app"
else
    echo "==> Semnare ad-hoc (AION_SIGN_ID nesetat)"
    codesign --force --deep --sign - "$app"
fi

dmg="$dist/Aion-$version.dmg"
echo "==> DMG: $dmg"
staging="$(mktemp -d)"
cp -R "$app" "$staging/"
ln -s /Applications "$staging/Applications"
hdiutil create -volname "Aion $version" -srcfolder "$staging" -ov -format UDZO "$dmg" >/dev/null
rm -rf "$staging"

if [[ -n "${AION_SIGN_ID:-}" ]]; then
    codesign --force --timestamp --sign "$AION_SIGN_ID" "$dmg"
    if [[ -n "${AION_NOTARY_PROFILE:-}" ]]; then
        echo "==> Notarizare (poate dura câteva minute)"
        xcrun notarytool submit "$dmg" --keychain-profile "$AION_NOTARY_PROFILE" --wait
        xcrun stapler staple "$dmg"
    fi
fi

echo "Gata: $dmg"
