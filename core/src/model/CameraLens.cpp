#include "aion/core/model/CameraLens.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace aion::model {

namespace {

double longSide(const CameraObject& c)
{
    return std::max({ c.formatWidth, c.formatHeight, 1 });
}

} // namespace

double fovFromFocal(double focalMm, double sensorMm)
{
    return 2.0 * std::atan(sensorMm / (2.0 * std::max(focalMm, 1e-3)));
}

double focalFromFov(double fovRadians, double sensorMm)
{
    return sensorMm / (2.0 * std::tan(std::clamp(fovRadians, 1e-4, 3.1) * 0.5));
}

std::pair<double, double> cameraFov(const CameraObject& camera)
{
    const double half = camera.sensorWidth / (2.0 * std::max(camera.focalLength, 1e-3)); // tangenta pe latura lungă
    const double w = std::max(camera.formatWidth, 1);
    const double h = std::max(camera.formatHeight, 1);
    const double l = longSide(camera);
    return { 2.0 * std::atan(half * w / l), 2.0 * std::atan(half * h / l) };
}

double focalForVerticalFov(const CameraObject& camera, double fovY)
{
    const double h = std::max(camera.formatHeight, 1);
    return camera.sensorWidth * (h / longSide(camera)) / (2.0 * std::tan(std::clamp(fovY, 1e-4, 3.1) * 0.5));
}

const std::vector<FormatPreset>& formatPresets()
{
    static const std::vector<FormatPreset> presets {
        { "16:9 · Full HD", 1920, 1080 },
        { "16:9 · 4K", 3840, 2160 },
        { "4:3", 1600, 1200 },
        { "3:2", 1800, 1200 },
        { "1:1", 1080, 1080 },
        { "4:5 · portret", 1080, 1350 },
        { "9:16 · vertical", 1080, 1920 },
        { "2.39:1 · cinema", 1920, 804 },
        { "A4 / A3 · peisaj (√2)", 3508, 2480 },
        { "A4 / A3 · portret (√2)", 2480, 3508 },
    };
    return presets;
}

const std::vector<SensorPreset>& sensorPresets()
{
    static const std::vector<SensorPreset> presets {
        { "Full frame (36 mm)", 36.0 },
        { "APS-C (23,6 mm)", 23.6 },
        { "Super 35 (24,89 mm)", 24.89 },
        { "Micro 4/3 (17,3 mm)", 17.3 },
        { "1 inch (13,2 mm)", 13.2 },
    };
    return presets;
}

FrameRect frameInViewport(const CameraObject& camera, math::Vec2d viewport, double margin)
{
    const double aspect = double(std::max(camera.formatWidth, 1)) / double(std::max(camera.formatHeight, 1));
    const math::Vec2d room = viewport * margin;
    math::Vec2d size;
    if (room.x / std::max(room.y, 1e-9) > aspect) {
        size = { room.y * aspect, room.y };
    } else {
        size = { room.x, room.x / aspect };
    }
    const math::Vec2d center = viewport * 0.5;
    return { center - size * 0.5, center + size * 0.5 };
}

view::Camera lookThrough(const CameraObject& camera, math::Vec2d viewport, double margin)
{
    view::Camera view;
    view.setViewportSize(viewport.x, viewport.y);
    view.setPose(view::poseLookingAt(camera.position, camera.target));
    const FrameRect frame = frameInViewport(camera, viewport, margin);
    const math::Vec2d frameSize = frame.max - frame.min;
    const double frameLong = std::max(frameSize.x, frameSize.y);
    if (camera.orthographic) {
        view.setProjection(view::Projection::Orthographic);
        // Latura lungă a cadrului = scara ortografică; jumătatea înălțimii viewport-ului în mm, la distanța țintei.
        const double halfHeight = viewport.y * 0.5 * camera.orthoScale / std::max(frameLong, 1e-9);
        view.setFovY(2.0 * std::atan(halfHeight / view.pose().distance));
    } else {
        const double tanV = std::tan(cameraFov(camera).second * 0.5) * viewport.y / std::max(frameSize.y, 1e-9);
        view.setFovY(2.0 * std::atan(tanV));
    }
    view.setRoll(glm::radians(camera.roll));
    view.setShift({ 2.0 * camera.shift.x * frameLong / viewport.x, 2.0 * camera.shift.y * frameLong / viewport.y });
    view.setClipOverride(camera.nearClip, camera.farClip);
    return view;
}

double circleOfConfusion(double depthMm, double focusMm, double focalMm, double fStop)
{
    const double s = std::max(focusMm, focalMm * 1.001);
    const double d = std::max(depthMm, 1e-6);
    return focalMm / std::max(fStop, 0.1) * focalMm / (s - focalMm) * std::abs(s - d) / d;
}

} // namespace aion::model
