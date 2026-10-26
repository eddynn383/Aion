# ADR 0014 – Windows

- Stare: acceptat (2026-10-26)
- Context: Faza 11. Același cod trebuie să ruleze pe Windows 10 și 11, compilat cu MSVC.

## Decizii

1. **Compilator și build**: MSVC (cl) cu Ninja. Preset-urile sunt `win-debug` și `win-release`, rulate dintr-un
   „Developer PowerShell for VS” și din CI (`ilammy/msvc-dev-cmd`). Pentru vcpkg folosim tripletul `x64-windows`.
   Opțiunile MSVC sunt `/W4 /permissive- /utf-8 /Zc:__cplusplus`, plus `_CRT_SECURE_NO_WARNINGS` și `NOMINMAX`.
2. **Grafică**: Direct3D 11 (`selectPreferredGraphicsApi`), cu shaderele HLSL generate de `qsb` din aceleași surse.
3. **Fereastra**: pe Windows rămâne bara de titlu a sistemului (`ExpandedClientAreaHint` doar pe macOS). TitleBar-ul
   nostru stă sub ea. O bară integrată, ca în Figma, se poate face mai târziu (WM_NCCALCSIZE / Qt 6.9+).
4. **Tastele**: textele de ajutor scrise cu tastele de pe Mac (⌘, ⌥, „Option”, „Cmd”) se traduc la afișare în Ctrl și
   Alt (`AppInfo.platformText`, `Document::toolStatus`). Scurtăturile `Ctrl+…` din QML sunt deja Ctrl pe Windows.
5. **Căi Unicode**: căile trec din Qt în `std::filesystem::path` prin UTF-16 (`toStdU16String`). La tinygltf trec
   prin UTF-8. Altfel, diacriticele din numele fișierelor s-ar pierde în codepage-ul local.
6. **Iconița**: `Aion.ico` (PNG-uri 16–256 px), generat la build din aceleași SVG-uri de `aion_iconrender` și legat
   printr-un `.rc`.
7. **Distribuția**: `scripts/package-windows.ps1` rulează `windeployqt` și semnează opțional (signtool, certificat
   Authenticode din `AION_SIGN_CERT`). Installer-ul Inno Setup (`packaging/windows/aion.iss`) creează scurtături și
   asocierea `.aion` (opțională, în HKCU sau HKLM). Fișierul primit în linia de comandă se deschide (`main.cpp`).

## Limite

- O singură instanță nu e impusă: deschiderea altui `.aion` pornește o fereastră nouă.
- High-DPI fracționar (125%, 150%) e gestionat de Qt; e de verificat manual (lista de testare).
