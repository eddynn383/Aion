# 0001 – Stack tehnic

**Status:** acceptat
**Data:** 2026-10-25

## Context
Vrem un modelator 3D nativ, întâi pe macOS, apoi pe Windows și pe Web, cu un UI modern
(în stilul aplicației Claude, Figma, Vercel) și o grafică mai bună decât SketchUp.

## Decizie
- **C++20** pentru tot codul.
- **Qt 6 (≥ 6.7), Qt Quick / QML** pentru UI, cu design system propriu. Qt Widgets a fost respins pentru că e greu de adus la un aspect modern.
- **Qt RHI** (`QQuickRhiItem`) pentru randare: Metal, Direct3D și WebGL din același cod.
- **Nucleu (`core/`) fără nicio dependență de Qt**, ca să poată fi testat izolat și compilat pentru WebAssembly.
- **CMake + Ninja + vcpkg** pentru build și dependențe, **Catch2** pentru teste.

## Alternative luate în calcul
- **Rust + wgpu:** portare excelentă pe web, dar UI desktop mai puțin matur și mai puține biblioteci de geometrie.
- **Web (TypeScript + Three.js):** cel mai rapid de pornit, dar nu e nativ.
- **Swift/SwiftUI:** foarte nativ pe macOS, dar ar trebui rescris pentru Windows.

## Consecințe
- UI-ul trebuie construit din componente proprii, ceea ce costă timp în Faza 1, dar ne dă control total asupra aspectului.
- API-ul QRhi are garanții de compatibilitate limitate între versiunile Qt, așa că fixăm versiunea Qt și izolăm RHI în `render/`.
