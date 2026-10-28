#include "aion/core/io/AionFile.h"
#include "aion/core/model/CameraLens.h"
#include "aion/core/view/Camera.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <string>

using namespace aion;
using Catch::Approx;
using math::Vec2d;
using math::Vec3d;

namespace {

// O cameră în origine care privește spre +Y (orizontal), 16:9, 35 mm pe full frame.
model::CameraObject frontCamera()
{
    model::CameraObject c;
    c.position = { 0, 0, 0 };
    c.target = { 0, 10000, 0 };
    return c;
}

} // namespace

TEST_CASE("Camera: focala și senzorul dau unghiul de vedere, pe latura lungă a cadrului", "[core][camera][lens]")
{
    // 35 mm pe 36 mm: 54,43° pe orizontală.
    CHECK(glm::degrees(model::fovFromFocal(35.0, 36.0)) == Approx(54.4322).margin(1e-3));
    CHECK(model::focalFromFov(model::fovFromFocal(50.0, 36.0), 36.0) == Approx(50.0));

    model::CameraObject c = frontCamera();
    const auto [h, v] = model::cameraFov(c);
    CHECK(h == Approx(model::fovFromFocal(35.0, 36.0)));
    CHECK(std::tan(v / 2) == Approx(std::tan(h / 2) * 1080.0 / 1920.0));
    // Un cadru vertical (9:16): senzorul trece pe latura lungă, care acum e înălțimea.
    c.formatWidth = 1080;
    c.formatHeight = 1920;
    const auto [h2, v2] = model::cameraFov(c);
    CHECK(v2 == Approx(model::fovFromFocal(35.0, 36.0)));
    CHECK(h2 < v2);

    CHECK(model::formatPresets().size() >= 8);
    CHECK(model::sensorPresets().front().width == Approx(36.0));
}

TEST_CASE("Camera: cadrul în viewport (letterbox) și vederea prin cameră arată exact cadrul", "[core][camera][lens]")
{
    const model::CameraObject c = frontCamera();
    const Vec2d viewport { 1000, 800 };
    const model::FrameRect frame = model::frameInViewport(c, viewport, 0.9);
    // 16:9 într-un viewport 5:4: lățimea limitează.
    CHECK(frame.max.x - frame.min.x == Approx(900.0));
    CHECK(frame.max.y - frame.min.y == Approx(900.0 * 9.0 / 16.0));
    CHECK((frame.min.x + frame.max.x) / 2 == Approx(500.0));
    CHECK((frame.min.y + frame.max.y) / 2 == Approx(400.0));

    const view::Camera view = model::lookThrough(c, viewport, 0.9);
    CHECK(glm::length(view.eye() - c.position) < 1e-6);
    // Un punct pe marginea dreaptă a câmpului camerei (jumătatea unghiului orizontal) cade pe marginea cadrului.
    const auto [h, v] = model::cameraFov(c);
    const Vec3d right = Vec3d { std::sin(h / 2), std::cos(h / 2), 0.0 } * 5000.0;
    CHECK(view.project(right)->x == Approx(frame.max.x).margin(1e-6));
    CHECK(view.project(right)->y == Approx(400.0).margin(1e-6));
    const Vec3d top = Vec3d { 0.0, std::cos(v / 2), std::sin(v / 2) } * 5000.0;
    CHECK(view.project(top)->y == Approx(frame.min.y).margin(1e-6));
    // Raza prin colțul cadrului trece prin colțul câmpului.
    const math::Ray ray = view.rayAt(frame.max);
    CHECK(ray.direction.x / ray.direction.y == Approx(std::tan(h / 2)));
    CHECK(-ray.direction.z / ray.direction.y == Approx(std::tan(v / 2)));
}

TEST_CASE("Camera: shift (perspectiva cu două puncte de fugă) și roll", "[core][camera][lens]")
{
    model::CameraObject c = frontCamera();
    const Vec2d viewport { 1000, 800 };
    const model::FrameRect frame = model::frameInViewport(c, viewport, 0.9);
    const double frameWidth = frame.max.x - frame.min.x;
    // Shift în sus cu 0,1 din latura lungă: punctul din fața camerei coboară cu 0,1 · lățimea cadrului.
    c.shift = { 0.0, 0.1 };
    view::Camera view = model::lookThrough(c, viewport, 0.9);
    const auto ahead = view.project({ 0, 5000, 0 });
    REQUIRE(ahead);
    CHECK(ahead->y == Approx(400.0 + 0.1 * frameWidth).margin(1e-6));
    CHECK(ahead->x == Approx(500.0).margin(1e-6));
    // Verticalele rămân verticale (fără înclinare): două puncte unul deasupra altuia au același x.
    const auto a = view.project({ 1000, 5000, -2000 });
    const auto b = view.project({ 1000, 5000, 3000 });
    CHECK(a->x == Approx(b->x).margin(1e-6));
    // Raza prin pixelul proiectat se întoarce la punct.
    const math::Ray ray = view.rayAt(*a);
    const Vec3d p = ray.origin + ray.direction * (5000.0 / ray.direction.y);
    CHECK(glm::length(p - Vec3d { 1000, 5000, -2000 }) < 1e-6);

    // Roll 90° (camera se rotește în sens trigonometric, privind prin ea): ce era sus apare în dreapta.
    c.shift = { 0.0, 0.0 };
    c.roll = 90.0;
    view = model::lookThrough(c, viewport, 0.9);
    const auto up = view.project({ 0, 5000, 1000 });
    REQUIRE(up);
    CHECK(up->y == Approx(400.0).margin(1e-6));
    CHECK(up->x > 500.0);
    const math::Ray back = view.rayAt(*up);
    CHECK(glm::length(back.origin + back.direction * (5000.0 / back.direction.y) - Vec3d { 0, 5000, 1000 }) < 1e-6);
}

TEST_CASE("Camera: ortografică, cu scara ei (lățimea cadrului în mm)", "[core][camera][lens]")
{
    model::CameraObject c = frontCamera();
    c.orthographic = true;
    c.orthoScale = 8000.0;
    const Vec2d viewport { 1000, 800 };
    const model::FrameRect frame = model::frameInViewport(c, viewport, 0.9);
    const view::Camera view = model::lookThrough(c, viewport, 0.9);
    CHECK(view.projection() == view::Projection::Orthographic);
    // Marginea dreaptă a cadrului e la 4 m de axă, la orice distanță.
    CHECK(view.project({ 4000, 3000, 0 })->x == Approx(frame.max.x).margin(1e-6));
    CHECK(view.project({ 4000, 9000, 0 })->x == Approx(frame.max.x).margin(1e-6));
}

TEST_CASE("Camera: cercul de confuzie (depth of field)", "[core][camera][lens]")
{
    // În planul de focus: zero; crește cu depărtarea de el și cu diafragma deschisă.
    CHECK(model::circleOfConfusion(5000.0, 5000.0, 50.0, 2.8) == Approx(0.0));
    const double near = model::circleOfConfusion(2000.0, 5000.0, 50.0, 2.8);
    const double far = model::circleOfConfusion(20000.0, 5000.0, 50.0, 2.8);
    CHECK(near > 0.0);
    CHECK(far > 0.0);
    CHECK(model::circleOfConfusion(2000.0, 5000.0, 50.0, 8.0) < near);
    // f / N · f / (S − f) · |S − D| / D (mm pe senzor).
    CHECK(near == Approx(50.0 / 2.8 * 50.0 / (5000.0 - 50.0) * 3000.0 / 2000.0));
}

TEST_CASE("Camera: datele noi se salvează; fișierele vechi (fov) primesc focala echivalentă", "[core][camera][lens][io]")
{
    model::Model m;
    model::CameraObject c = frontCamera();
    c.name = "Intrare";
    c.focalLength = 24.0;
    c.sensorWidth = 23.6;
    c.formatWidth = 1080;
    c.formatHeight = 1350;
    c.shift = { 0.05, -0.1 };
    c.roll = 3.0;
    c.orthoScale = 12000.0;
    c.displaySize = 2.0;
    c.dof.enabled = true;
    c.dof.focusDistance = 3200.0;
    c.dof.fStop = 1.8;
    const model::CameraId id = m.addCamera(c);
    const io::LoadResult loaded = io::fromJson(io::toJson(m));
    REQUIRE(loaded.model);
    CHECK(loaded.model->cameras().at(id) == c);

    // Formatul vechi: „fov” vertical (aici 24°), fără focală → focala care dă același unghi vertical în formatul
    // camerei (aici 4:5, deci latura lungă e înălțimea).
    std::string text = io::toJson(m);
    const auto at = text.find("\"focalLength\"");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("\"focalLength\"").size(), "\"fov\"");
    const io::LoadResult old = io::fromJson(text);
    REQUIRE(old.model);
    const auto [h, v] = model::cameraFov(old.model->cameras().at(id));
    CHECK(glm::degrees(v) == Approx(24.0));
    CHECK(h < v);
}
