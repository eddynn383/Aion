# Pachetul de distribuție pentru Windows: build release, windeployqt, semnare (opțional), installer Inno Setup.
# Rulează din „Developer PowerShell for VS 2022” (cl și Ninja în PATH), cu QT_ROOT_DIR și VCPKG_ROOT setate.
#
#   scripts\package-windows.ps1
#   $env:AION_SIGN_CERT = "C:\cale\cert.pfx"; $env:AION_SIGN_PASSWORD = "..."; scripts\package-windows.ps1
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$preset = "win-release"
$buildDir = "build\$preset"
$version = (Select-String -Path CMakeLists.txt -Pattern '^\s*VERSION ([0-9.]+)').Matches[0].Groups[1].Value

Write-Host "==> Build $preset"
if (-not (Test-Path "$buildDir\build.ninja")) { cmake --preset $preset }
cmake --build --preset $preset

$stage = "dist\Aion"
Remove-Item -Recurse -Force dist -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item "$buildDir\app\Aion.exe" $stage
# DLL-urile din vcpkg (manifold etc.) copiate lângă executabil la build.
Get-ChildItem "$buildDir\app\*.dll" | Copy-Item -Destination $stage

Write-Host "==> windeployqt"
& "$env:QT_ROOT_DIR\bin\windeployqt.exe" --release --qmldir app\qml --no-translations --no-system-d3d-compiler "$stage\Aion.exe"

function Sign-File($path) {
    if ($env:AION_SIGN_CERT) {
        signtool sign /f $env:AION_SIGN_CERT /p $env:AION_SIGN_PASSWORD /fd SHA256 `
            /tr http://timestamp.digicert.com /td SHA256 $path
    }
}
Sign-File "$stage\Aion.exe"

Write-Host "==> Installer (Inno Setup)"
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) { throw "Lipsește Inno Setup 6 (winget install JRSoftware.InnoSetup)" }
& $iscc "/DAppVersion=$version" "/DSourceDir=$((Resolve-Path $stage).Path)" "/DOutputDir=$((Resolve-Path dist).Path)" `
    packaging\windows\aion.iss
Sign-File "dist\Aion-$version-setup.exe"

Write-Host "Gata: dist\Aion-$version-setup.exe"
