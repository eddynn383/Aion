#include "aion/core/math/Tolerance.h"
#include "aion/core/view/Camera.h"
#include "aion/core/view/CameraMotion.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace aion::view;
using aion::tol::kAngle;
using aion::tol::samePoint;

namespace {

constexpr double kPixel = 1e-6; // toleranță pentru poziții pe ecran

bool sameDirection(const Vec3d& a, const Vec3d& b)
{
    return samePoint(a, b, 1e-9);
}

bool sameScreen(const Vec2d& a, const Vec2d& b)
{
    return glm::length(a - b) <= kPixel * 1000.0;
}

Camera makeCamera(Projection projection = Projection::Perspective)
{
    Camera c;
    c.setViewportSize(1200.0, 800.0);
    c.setProjection(projection);
    c.setPose({ { 1000.0, -500.0, 200.0 }, 0.7, -0.4, 8000.0 });
    return c;
}

} // namespace

TEST_CASE("Vederile standard au axele așteptate pe ecran", "[core][view][camera]")
{
    Camera c;
    const auto pose = [&](StandardView v) { c.setPose(standardViewPose(v, c.pose())); };

    pose(StandardView::Top);
    CHECK(sameDirection(c.forward(), { 0.0, 0.0, -1.0 }));
    CHECK(sameDirection(c.right(), { 1.0, 0.0, 0.0 }));
    CHECK(sameDirection(c.up(), { 0.0, 1.0, 0.0 }));

    pose(StandardView::Front);
    CHECK(sameDirection(c.forward(), { 0.0, 1.0, 0.0 }));
    CHECK(sameDirection(c.right(), { 1.0, 0.0, 0.0 }));
    CHECK(sameDirection(c.up(), { 0.0, 0.0, 1.0 }));

    pose(StandardView::Right);
    CHECK(sameDirection(c.forward(), { -1.0, 0.0, 0.0 }));
    CHECK(sameDirection(c.right(), { 0.0, 1.0, 0.0 }));
    CHECK(sameDirection(c.up(), { 0.0, 0.0, 1.0 }));

    pose(StandardView::Iso);
    CHECK(sameDirection(c.forward(), glm::normalize(Vec3d { 1.0, 1.0, -1.0 })));
}

TEST_CASE("Baza camerei e ortonormală", "[core][view][camera]")
{
    Camera c = makeCamera();
    const double pitch = GENERATE(-glm::half_pi<double>(), -0.3, 0.0, 0.9, glm::half_pi<double>());
    c.setPose({ {}, 2.1, pitch, 100.0 });

    CHECK(aion::tol::equal(glm::length(c.forward()), 1.0, kAngle));
    CHECK(aion::tol::equal(glm::length(c.right()), 1.0, kAngle));
    CHECK(aion::tol::equal(glm::length(c.up()), 1.0, kAngle));
    CHECK(aion::tol::isZero(glm::dot(c.forward(), c.right()), kAngle));
    CHECK(aion::tol::isZero(glm::dot(c.forward(), c.up()), kAngle));
    CHECK(aion::tol::isZero(c.right().z, kAngle));
}

TEST_CASE("Orbit limitează pitch-ul și păstrează ținta", "[core][view][camera]")
{
    Camera c = makeCamera();
    const Vec3d target = c.pose().target;

    c.orbit(0.0, 10.0);
    CHECK(aion::tol::equal(c.pose().pitch, glm::half_pi<double>(), kAngle));
    c.orbit(0.0, -20.0);
    CHECK(aion::tol::equal(c.pose().pitch, -glm::half_pi<double>(), kAngle));

    c.orbit(7.0, 0.0);
    CHECK(c.pose().yaw > -glm::pi<double>());
    CHECK(c.pose().yaw <= glm::pi<double>());
    CHECK(samePoint(c.pose().target, target));
    CHECK(samePoint(c.eye(), target - c.forward() * c.pose().distance));
}

TEST_CASE("Proiecția și raza sunt consecvente", "[core][view][camera]")
{
    const auto projection = GENERATE(Projection::Perspective, Projection::Orthographic);
    Camera c = makeCamera(projection);

    // Ținta e în centrul ecranului.
    const auto center = c.project(c.pose().target);
    REQUIRE(center.has_value());
    CHECK(sameScreen(*center, { 600.0, 400.0 }));

    // Un punct de pe raza unui pixel se proiectează înapoi pe același pixel.
    const Vec2d pixel { 137.0, 702.0 };
    const Ray ray = c.rayAt(pixel);
    const auto back = c.project(ray.origin + ray.direction * (c.pose().distance * 1.3));
    REQUIRE(back.has_value());
    CHECK(sameScreen(*back, pixel));
}

TEST_CASE("Punctele din spatele camerei nu se proiectează", "[core][view][camera]")
{
    Camera c = makeCamera();
    CHECK_FALSE(c.project(c.eye() - c.forward() * 100.0).has_value());
}

TEST_CASE("Zoom spre cursor ține pe loc punctul de sub cursor", "[core][view][camera]")
{
    const auto projection = GENERATE(Projection::Perspective, Projection::Orthographic);
    Camera c = makeCamera(projection);
    const Vec2d cursor { 250.0, 600.0 };
    const Vec3d anchor = c.zoomAnchorAt(cursor);

    c.zoomAbout(0.5, anchor);
    CHECK(aion::tol::equal(c.pose().distance, 4000.0));
    const auto after = c.project(anchor);
    REQUIRE(after.has_value());
    CHECK(sameScreen(*after, cursor));
}

TEST_CASE("Zoom-ul respectă limitele de distanță", "[core][view][camera]")
{
    Camera c = makeCamera();
    c.zoomAbout(1e-9, c.pose().target);
    CHECK(aion::tol::equal(c.pose().distance, Camera::kMinDistance));
    c.zoomAbout(1e20, c.pose().target);
    CHECK(aion::tol::equal(c.pose().distance, Camera::kMaxDistance));
}

TEST_CASE("Pan mută scena odată cu cursorul", "[core][view][camera]")
{
    Camera c = makeCamera();
    const Vec3d target = c.pose().target;
    c.pan({ 40.0, -25.0 });
    const auto moved = c.project(target);
    REQUIRE(moved.has_value());
    CHECK(sameScreen(*moved, { 640.0, 375.0 }));
}

TEST_CASE("Încadrarea pune toată cutia în cadru", "[core][view][camera]")
{
    const auto projection = GENERATE(Projection::Perspective, Projection::Orthographic);
    Camera c = makeCamera(projection);
    const Vec3d lo { -3000.0, 2000.0, 0.0 };
    const Vec3d hi { 5000.0, 4000.0, 2500.0 };
    c.setPose(c.framing(lo, hi));

    for (int i = 0; i < 8; ++i) {
        const Vec3d corner { (i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z };
        const auto p = c.project(corner);
        REQUIRE(p.has_value());
        CHECK(p->x >= 0.0);
        CHECK(p->x <= 1200.0);
        CHECK(p->y >= 0.0);
        CHECK(p->y <= 800.0);
    }
}

TEST_CASE("Tranziția ajunge la destinație pe drumul cel mai scurt", "[core][view][motion]")
{
    Camera c = makeCamera();
    c.setPose({ {}, 3.0, 0.0, 1000.0 });
    CameraPose target { { 500.0, 0.0, 0.0 }, -3.0, -0.5, 4000.0 };

    CameraMotion motion;
    motion.animateTo(c, target, 0.3);
    REQUIRE(motion.isActive());

    // La jumătate, yaw-ul trece prin ±π (drumul scurt), nu prin 0.
    motion.update(c, 0.15);
    CHECK(std::abs(c.pose().yaw) > 3.0);

    for (int i = 0; i < 10; ++i) {
        motion.update(c, 1.0 / 60.0);
    }
    CHECK_FALSE(motion.isActive());
    CHECK(samePoint(c.pose().target, target.target));
    CHECK(aion::tol::equal(c.pose().yaw, target.yaw, kAngle));
    CHECK(aion::tol::equal(c.pose().distance, target.distance));
}

TEST_CASE("Inerția orbitei se stinge", "[core][view][motion]")
{
    Camera c = makeCamera();
    const double yaw0 = c.pose().yaw;
    CameraMotion motion;
    motion.setOrbitVelocity({ 2.0, 0.0 });

    int frames = 0;
    while (motion.isActive() && frames < 600) {
        motion.update(c, 1.0 / 60.0);
        ++frames;
    }
    CHECK_FALSE(motion.isActive());
    CHECK(frames < 120); // sub 2 secunde
    CHECK(c.pose().yaw > yaw0 + 0.2);
}

TEST_CASE("Zoom-ul lin ajunge exact la factorul cerut", "[core][view][motion]")
{
    Camera c = makeCamera();
    const Vec3d anchor = c.pose().target;
    CameraMotion motion;
    motion.zoomSmoothly(0.5, anchor);
    motion.zoomSmoothly(0.5, anchor);

    for (int i = 0; i < 200 && motion.isActive(); ++i) {
        motion.update(c, 1.0 / 60.0);
    }
    CHECK_FALSE(motion.isActive());
    CHECK(aion::tol::equal(c.pose().distance, 2000.0, 2.0));
}

TEST_CASE("Reduce Motion face tranzițiile instantanee", "[core][view][motion]")
{
    Camera c = makeCamera();
    CameraMotion motion;
    motion.setReducedMotion(true);
    motion.animateTo(c, standardViewPose(StandardView::Top, c.pose()));
    motion.update(c, 0.001);
    CHECK_FALSE(motion.isActive());
    CHECK(sameDirection(c.forward(), { 0.0, 0.0, -1.0 }));

    motion.setOrbitVelocity({ 3.0, 0.0 });
    CHECK_FALSE(motion.isActive());
}

TEST_CASE("Camera: în contextul unui grup, razele și proiecțiile sunt în coordonate locale", "[core][camera]")
{
    using aion::math::Transform;
    aion::view::Camera world;
    world.setViewportSize(1200, 800);
    world.setPose(aion::view::standardViewPose(aion::view::StandardView::Iso, { { 500, 300, 0 }, 0, 0, 9000 }));
    const Transform toWorld = Transform::translation({ 2000, 0, 0 })
        * Transform::rotation({ 0, 0, 0 }, { 0, 0, 1 }, 0.7) * Transform::scaling({ 0, 0, 0 }, { 2, 2, 2 });
    const aion::view::Camera local = world.inContext(toWorld);
    CHECK(local.hasContext());

    const aion::math::Vec3d p { 120, -340, 560 };
    const auto a = local.project(p);
    const auto b = world.project(toWorld.applyToPoint(p));
    REQUIRE(a);
    REQUIRE(b);
    CHECK(glm::length(*a - *b) < 1e-6);

    // Raza locală prin pixelul punctului trece prin punct.
    const aion::math::Ray ray = local.rayAt(*a);
    const aion::math::Vec3d toP = p - ray.origin;
    const aion::math::Vec3d along = ray.direction * glm::dot(toP, ray.direction);
    CHECK(glm::length(toP - along) < 1e-6);
    CHECK(local.worldPerPixelAtTarget() * 2.0 == Catch::Approx(world.worldPerPixelAtTarget()));
}

TEST_CASE("Orbit în jurul unui pivot îl ține pe loc pe ecran", "[core][view][camera]")
{
    Camera c = makeCamera();
    const Vec3d pivot { 2500.0, 1200.0, 300.0 };
    const auto before = c.project(pivot);
    REQUIRE(before);
    c.setOrbitPivot(pivot);
    c.orbit(0.6, -0.3);
    c.orbit(-0.2, 0.15);
    const auto after = c.project(pivot);
    REQUIRE(after);
    CHECK(sameScreen(*before, *after));
    // Distanța de la cameră la pivot nu se schimbă (rotație rigidă).
    Camera reference = makeCamera();
    CHECK(glm::length(c.eye() - pivot) == Catch::Approx(glm::length(reference.eye() - pivot)).epsilon(1e-9));

    // Fără pivot: orbita clasică, în jurul țintei.
    c.setOrbitPivot(std::nullopt);
    const Vec3d target = c.pose().target;
    c.orbit(0.3, 0.1);
    CHECK(samePoint(c.pose().target, target, 1e-9));
}

TEST_CASE("Reîncadrarea la adâncimea unui punct nu mișcă imaginea", "[core][view][camera]")
{
    Camera c = makeCamera();
    const Vec3d point { 1300.0, -350.0, 150.0 };
    const auto before = c.project(point);
    const Vec3d eye = c.eye();
    c.retargetToDepth(point);
    CHECK(samePoint(c.eye(), eye, 1e-6));
    const auto after = c.project(point);
    REQUIRE(before);
    REQUIRE(after);
    CHECK(sameScreen(*before, *after));
    CHECK(c.pose().distance == Catch::Approx(glm::dot(point - eye, c.forward())));
}

TEST_CASE("Aproape de un detaliu mic, planurile de tăiere păstrează și detaliul, și restul scenei", "[core][view][camera]")
{
    Camera c = makeCamera();
    c.setSceneBounds({ -20000.0, -20000.0, 0.0 }, { 20000.0, 20000.0, 5000.0 });
    // Detaliu de 2 mm, privit de la 5 mm.
    CameraPose pose = c.pose();
    pose.target = { 100.0, 100.0, 0.0 };
    pose.distance = 5.0;
    c.setPose(pose);
    CHECK(c.nearPlane() < 1.0);
    const double farthest = glm::length(Vec3d { -20000.0, -20000.0, 5000.0 } - c.eye());
    CHECK(c.farPlane() >= farthest);
    CHECK(c.farPlane() / c.nearPlane() <= 1.0e6 * 1.0001);

    // Valorile manuale (Preferințe) au prioritate.
    c.setClipOverride(0.5, 300000.0);
    CHECK(c.nearPlane() == Catch::Approx(0.5));
    CHECK(c.farPlane() == Catch::Approx(300000.0));
}
