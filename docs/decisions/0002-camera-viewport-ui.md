# 0002 – Camera în core, viewport ca modul QML, UI pe Qt Quick Templates

**Status:** acceptat
**Data:** 2026-10-26

## Context
Faza 1 introduce viewport-ul 3D, camera de navigare și design system-ul. PLAN.md plasa camera în `render/`.

## Decizie
- **Camera (`forma::view::Camera`, `CameraMotion`) stă în `core/view`**, nu în `render/`. E matematică pură (glm),
  se testează fără Qt, iar uneltele din `core/tools` (Faza 3) au nevoie de razele din cameră fără să depindă de randare.
  `render/` doar o folosește. Convenție: Z în sus, unități mm, matrici în stil OpenGL; `render/` aplică
  `QRhi::clipSpaceCorrMatrix()` pentru fiecare backend.
- **`render/` este un modul QML static (`Forma.Render`)** cu `Viewport` (`QQuickRhiItem`, MSAA 4x).
  Aplicația îl importă cu `Q_IMPORT_QML_PLUGIN(Forma_RenderPlugin)`. QRhi vine prin `Qt6::GuiPrivate` (API semi-public).
- **Shaderele** sunt GLSL 440 compilate cu `qt_add_shaders` pentru MSL 1.2, HLSL 5.0 și GLSL 330 / ES 300
  (ES 300 pentru WebGL 2 în Faza 12). Toate folosesc un singur uniform buffer (`Frame`).
- **Grid-ul infinit** e un shader full-screen (intersecția razei cu planul Z = 0), cu pas adaptiv în puteri ale lui 10.
  **Liniile** sunt quad-uri instanțiate cu grosime constantă în pixeli și anti-aliasing analitic, tăiate la planul apropiat.
- **Componentele UI** folosesc `QtQuick.Templates` (comportament complet de control, aspect numai din `Theme.qml`),
  nu un stil Qt Quick Controls existent.
- **Iconițele** Lucide se recolorează la cerere printr-un `QQuickImageProvider` (`image://icon/<culoare>/<nume>`),
  randate la rezoluția ecranului.
- **Bara de titlu** folosește `Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint` (Qt ≥ 6.9): conținutul
  continuă sub bară, butoanele native macOS rămân. Fără cod Objective-C pentru poziționarea lor.

## Consecințe
- Qt minim efectiv devine 6.9 (flag-urile ferestrei, `RectangularShadow`). Pe Mac folosim Qt 6.12 din Homebrew.
- Butoanele native („semafoarele”) stau la poziția implicită macOS; dacă alinierea cu bara noastră de 44 px
  deranjează, se pot repoziționa nativ mai târziu.
