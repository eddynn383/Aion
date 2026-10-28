#pragma once

#include "aion/core/model/Model.h"
#include "aion/core/view/Camera.h"

#include <string_view>
#include <utility>
#include <vector>

// Lentila și formatul camerelor-obiect (plan Animare, secțiunea 9): focala pe senzor dă unghiul de vedere pe latura
// lungă a cadrului (ca „Sensor Fit: Auto” din Blender), formatul dă raportul, shift-ul mută cadrul fără să încline
// verticalele, roll-ul rotește camera în jurul direcției de privire. Vederea prin cameră încadrează cadrul în viewport.
namespace aion::model {

// Unghiul întreg (radiani) pe o latură a senzorului de `sensorMm`, la focala `focalMm`, și invers.
[[nodiscard]] double fovFromFocal(double focalMm, double sensorMm);
[[nodiscard]] double focalFromFov(double fovRadians, double sensorMm);
// Unghiurile de vedere ale camerei (orizontal, vertical), în radiani.
[[nodiscard]] std::pair<double, double> cameraFov(const CameraObject& camera);
// Focala care dă unghiul vertical `fovY` în formatul și pe senzorul camerei (camerele din fișierele vechi).
[[nodiscard]] double focalForVerticalFov(const CameraObject& camera, double fovY);

struct FormatPreset {
    std::string_view name;
    int width = 0;
    int height = 0;
};
struct SensorPreset {
    std::string_view name;
    double width = 36.0;
};
[[nodiscard]] const std::vector<FormatPreset>& formatPresets();
[[nodiscard]] const std::vector<SensorPreset>& sensorPresets();

// Dreptunghiul cadrului într-un viewport (pixeli, originea stânga-sus): formatul încadrat, centrat, ocupând cel mult
// `margin` din lățime și înălțime (letterbox / pillarbox).
struct FrameRect {
    math::Vec2d min { 0.0 };
    math::Vec2d max { 0.0 };
};
[[nodiscard]] FrameRect frameInViewport(const CameraObject& camera, math::Vec2d viewport, double margin);
// Camera de vedere care arată exact cadrul camerei în dreptunghiul de mai sus: poziția, ținta, unghiul, roll-ul,
// shift-ul, proiecția (ortografică: scara ei) și planurile de tăiere.
[[nodiscard]] view::Camera lookThrough(const CameraObject& camera, math::Vec2d viewport, double margin);

// Cercul de confuzie (mm pe senzor) al unui punct la `depthMm`, cu focusul la `focusMm`, focala și diafragma date:
// f / N · f / (S − f) · |S − D| / D. Zero în planul de focus.
[[nodiscard]] double circleOfConfusion(double depthMm, double focusMm, double focalMm, double fStop);

} // namespace aion::model
