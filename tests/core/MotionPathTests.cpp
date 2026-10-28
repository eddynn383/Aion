#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/MotionPaths.h"
#include "aion/core/model/Operations.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

using namespace aion;
using math::Vec3d;

namespace {

geom::BezierPoint corner(Vec3d p)
{
    return { p, p, p, false };
}

geom::BezierPath straight(double length)
{
    return { { corner({ 0, 0, 0 }), corner({ length, 0, 0 }) }, false };
}

// Un sfert de cerc cu raza r, de la (r, 0) la (0, r), pe sol.
geom::BezierPath quarter(double r)
{
    const double k = 0.55228 * r;
    return { { { { r, 0, 0 }, { r, 0, 0 }, { r, k, 0 }, false }, { { 0, r, 0 }, { k, r, 0 }, { 0, r, 0 }, false } }, false };
}

model::Easing linear()
{
    model::Easing e;
    e.name = "linear";
    e.x1 = 0.0;
    e.y1 = 0.0;
    e.x2 = 1.0;
    e.y2 = 1.0;
    return e;
}

// O cutie (grup) cu colțul minim în `lo`, de mărimea `size`.
model::InstanceId box(model::Model& m, commands::CommandStack& stack, Vec3d lo, Vec3d size)
{
    geom::FaceId bottom;
    stack.run("Box", [&](geom::Mesh& mesh) {
        const geom::DrawLineResult r = geom::drawPolygon(mesh,
            { lo, lo + Vec3d { size.x, 0, 0 }, lo + Vec3d { size.x, size.y, 0 }, lo + Vec3d { 0, size.y, 0 } });
        bottom = r.createdFaces.front();
    });
    stack.run("Push", [&](geom::Mesh& mesh) { geom::pushPull(mesh, bottom, -size.z); });
    geom::Selection all;
    const geom::Mesh& mesh = m.mesh(m.root());
    all.faces.insert(mesh.faces().keys().begin(), mesh.faces().keys().end());
    model::InstanceId id;
    stack.run("Group", [&](model::Model& model) { id = model::makeGroup(model, model.root(), all, false); });
    return id;
}

model::MotionPathId addPath(commands::CommandStack& stack, const geom::BezierPath& curve, double duration = 4.0)
{
    model::MotionPathId id;
    stack.run("Path", [&](model::Model& model) {
        model::MotionPath p;
        p.name = "Calea 1";
        p.curve = curve;
        p.duration = duration;
        p.easing = linear();
        p.animation = model.addAnimation({});
        id = model.addMotionPath(p);
    });
    return id;
}

bool attach(commands::CommandStack& stack, model::MotionPathId path, model::InstanceId instance)
{
    bool ok = false;
    stack.run("Ancorează", [&](model::Model& model) { ok = model::attachToPath(model, path, instance); });
    return ok;
}

// Centrul feței de jos a cutiei obiectului, în lume, la transformarea dată.
Vec3d basePoint(const model::Model& m, model::InstanceId id, const math::Transform& t)
{
    const auto [lo, hi] = model::bounds(m, m.instance(id).definition);
    return t.applyToPoint({ (lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, lo.z });
}

math::Transform at(const model::Model& m, model::MotionPathId path, model::InstanceId id, double time)
{
    const model::MotionPath& p = m.motionPaths().at(path);
    for (const model::PathAnchor& a : p.anchors) {
        if (a.instance == id) {
            return model::anchorTransform(m, p, a, time, model::decompose(a.before).scale);
        }
    }
    FAIL("obiectul nu e pe cale");
    return {};
}

// Fața aleasă la ancorare (axa locală Rz(front · 90°) · X), în coordonatele obiectului.
Vec3d frontAxis(const model::Model& m, model::MotionPathId path)
{
    const double a = glm::radians(90.0 * m.motionPaths().at(path).anchors.front().front);
    return { std::cos(a), std::sin(a), 0.0 };
}

bool sameTransform(const math::Transform& a, const math::Transform& b)
{
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            if (std::abs(a.matrix()[c][r] - b.matrix()[c][r]) > 1e-6) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

TEST_CASE("Căi: obiectul merge pe cale, cu fața înainte", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 5000, 5000, 0 }, { 1000, 400, 300 });
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, car));
    // La ancorare obiectul e la start.
    CHECK(glm::length(basePoint(m, car, m.instance(car).transform) - Vec3d { 0, 0, 0 }) < 1e-6);
    const math::Transform t = at(m, path, car, 2.0);
    CHECK(glm::length(basePoint(m, car, t) - Vec3d { 5000, 0, 0 }) < 1e-3);
    CHECK(glm::length(t.applyToVector({ 1, 0, 0 }) - Vec3d { 1, 0, 0 }) < 1e-9); // fața (axa X a cutiei) înainte
    CHECK(glm::length(t.applyToVector({ 0, 0, 1 }) - Vec3d { 0, 0, 1 }) < 1e-9); // în picioare
}

TEST_CASE("Căi: pe o curbă obiectul se întoarce lin", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 1000, 400, 300 });
    const model::MotionPathId path = addPath(stack, quarter(5000));
    REQUIRE(attach(stack, path, car));
    // Calea pornește spre +Y: cu cât mai puțină rotire, fața devine axa Y a cutiei.
    CHECK(m.motionPaths().at(path).anchors.front().front == 1);
    double previous = -1e9;
    for (int i = 0; i <= 10; ++i) {
        const Vec3d forward = at(m, path, car, 0.4 * i).applyToVector(frontAxis(m, path));
        const double angle = glm::degrees(std::atan2(forward.y, forward.x));
        CHECK(angle > previous);
        previous = angle;
        if (i == 0) {
            CHECK(std::abs(angle - 90.0) < 0.5);
        }
        if (i == 10) {
            CHECK(std::abs(angle - 180.0) < 0.5);
        }
    }
}

TEST_CASE("Căi: easing-ul se aplică pe lungime", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 600, 600, 600 });
    const model::MotionPathId path = addPath(stack, straight(10000));
    stack.run("Easing", [&](model::Model& model) {
        model::MotionPath p = model.motionPaths().at(path);
        p.easing = *model::easingPreset("easeInOut");
        model.setMotionPath(path, p);
    });
    REQUIRE(attach(stack, path, car));
    CHECK(glm::length(basePoint(m, car, at(m, path, car, 2.0)) - Vec3d { 5000, 0, 0 }) < 1.0);
    CHECK(basePoint(m, car, at(m, path, car, 1.0)).x < 2500.0); // pornește lin
}

TEST_CASE("Căi: ancorarea și scoaterea", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 1000, 400, 300 });
    stack.run("Rotate", [&](model::Model& model) {
        model::Instance inst = model.instance(car);
        inst.transform = math::Transform::translation({ 7000, 3000, 0 })
            * math::Transform::rotation({ 0, 0, 0 }, { 0, 0, 1 }, glm::radians(30.0)) * inst.transform;
        model.setInstance(car, inst);
    });
    const math::Transform original = m.instance(car).transform;
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, car));
    CHECK_FALSE(sameTransform(m.instance(car).transform, original));
    CHECK_FALSE(attach(stack, path, car)); // deja pe o cale a animației

    SECTION("scoasă direct")
    {
        stack.run("Scoate", [&](model::Model& model) { model::detachFromPath(model, path, car); });
        CHECK(sameTransform(m.instance(car).transform, original));
        CHECK(m.motionPaths().at(path).anchors.empty());
    }
    SECTION("scoasă după o editare a curbei")
    {
        stack.run("Edit", [&](model::Model& model) {
            model::MotionPath p = model.motionPaths().at(path);
            p.curve.points[1].point = { 8000, 3000, 0 };
            p.curve.points[1].in = p.curve.points[1].point;
            p.curve.points[1].out = p.curve.points[1].point;
            model::updatePath(model, path, p);
        });
        // Startul nu s-a mutat, dar direcția da: obiectul ancorat urmează forma nouă.
        const Vec3d dir = glm::normalize(Vec3d { 8000, 3000, 0 });
        CHECK(glm::length(m.instance(car).transform.applyToVector({ 1, 0, 0 }) - dir) < 1e-6);
        stack.run("Scoate", [&](model::Model& model) { model::detachFromPath(model, path, car); });
        CHECK(sameTransform(m.instance(car).transform, original));
    }
}

TEST_CASE("Căi: coloana nu se suprapune", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId a = box(m, stack, { 0, 0, 0 }, { 600, 600, 600 });
    const model::InstanceId b = box(m, stack, { 3000, 3000, 0 }, { 600, 600, 600 });
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, a));
    REQUIRE(attach(stack, path, b));
    const model::MotionPath& p = m.motionPaths().at(path);
    REQUIRE(p.anchors.size() == 2);
    CHECK(p.anchors[0].along == 0.0);
    CHECK(p.anchors[1].along <= -800.0 + 1e-9);
    // La start, al doilea e pe prelungire; la final, în spatele primului, fără să se atingă.
    CHECK(basePoint(m, b, at(m, path, b, 0.0)).x < 0.0);
    const double xa = basePoint(m, a, at(m, path, a, 4.0)).x;
    const double xb = basePoint(m, b, at(m, path, b, 4.0)).x;
    CHECK(std::abs(xa - 10000.0) < 1e-3);
    CHECK(xa - xb >= 600.0);
}

TEST_CASE("Căi: decalajul din Move", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 1000, 400, 300 });
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, car));
    const math::Transform expected = at(m, path, car, 2.0);
    const math::Transform edited = math::Transform::translation({ 0, 500, 0 }) * expected; // 500 mm la stânga
    stack.run("Move", [&](model::Model& model) { model::absorbAnchorEdit(model, path, car, edited, 2.0); });
    const model::PathAnchor& anchor = m.motionPaths().at(path).anchors.front();
    CHECK(std::abs(anchor.lateral - 500.0) < 1e-6);
    CHECK(std::abs(anchor.along) < 1e-6);
    CHECK(std::abs(anchor.up) < 1e-6);
    CHECK(sameTransform(at(m, path, car, 2.0), edited));
    // Rotația în jurul normalei căii devine yaw.
    const math::Transform turned
        = math::Transform::rotation(basePoint(m, car, edited), { 0, 0, 1 }, glm::radians(20.0)) * edited;
    stack.run("Rotate", [&](model::Model& model) { model::absorbAnchorEdit(model, path, car, turned, 2.0); });
    CHECK(std::abs(m.motionPaths().at(path).anchors.front().yaw - 20.0) < 1e-6);
    CHECK(sameTransform(at(m, path, car, 2.0), turned));
}

TEST_CASE("Căi: decalaj lateral mare pe o curbă strânsă", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 1000, 400, 300 });
    const model::MotionPathId path = addPath(stack, quarter(500));
    REQUIRE(attach(stack, path, car));
    stack.run("Lateral", [&](model::Model& model) {
        model::MotionPath p = model.motionPaths().at(path);
        p.anchors.front().lateral = 2000.0;
        model.setMotionPath(path, p);
    });
    const geom::ArcLength arc(quarter(500));
    for (int i = 0; i <= 8; ++i) {
        const double time = 0.5 * i;
        const math::Transform t = at(m, path, car, time);
        const Vec3d p = basePoint(m, car, t);
        CHECK((std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)));
        const Vec3d tangent = arc.at(arc.length() * time / 4.0).tangent;
        CHECK(glm::length(t.applyToVector(frontAxis(m, path)) - tangent) < 1e-6);
    }
}

TEST_CASE("Căi: calea închisă face un tur", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 600, 600, 600 });
    const geom::BezierPath loop { { corner({ 0, 0, 0 }), corner({ 4000, 0, 0 }), corner({ 4000, 4000, 0 }), corner({ 0, 4000, 0 }) },
        true };
    const model::MotionPathId path = addPath(stack, loop);
    REQUIRE(attach(stack, path, car));
    CHECK(glm::length(basePoint(m, car, at(m, path, car, 4.0)) - Vec3d { 0, 0, 0 }) < 1e-3);
    CHECK(glm::length(basePoint(m, car, at(m, path, car, 2.0)) - Vec3d { 4000, 4000, 0 }) < 1e-3);
}

TEST_CASE("Căi: ștergerea căii și undo", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 2000, 2000, 0 }, { 600, 600, 600 });
    const math::Transform original = m.instance(car).transform;
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, car));
    const math::Transform anchored = m.instance(car).transform;
    stack.run("Șterge calea", [&](model::Model& model) { model::removePath(model, path); });
    CHECK_FALSE(m.motionPaths().contains(path));
    CHECK(sameTransform(m.instance(car).transform, original));
    stack.undo();
    REQUIRE(m.motionPaths().contains(path));
    CHECK(m.motionPaths().at(path).anchors.size() == 1);
    CHECK(sameTransform(m.instance(car).transform, anchored));
    stack.redo();
    CHECK_FALSE(m.motionPaths().contains(path));
    CHECK(sameTransform(m.instance(car).transform, original));
}

TEST_CASE("Căi: ștergerea animației le ia și căile", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 2000, 2000, 0 }, { 600, 600, 600 });
    const math::Transform original = m.instance(car).transform;
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, car));
    const model::AnimationId animation = m.motionPaths().at(path).animation;
    stack.run("Șterge animația", [&](model::Model& model) { model::removeAnimationWithPaths(model, animation); });
    CHECK_FALSE(m.animations().contains(animation));
    CHECK(m.motionPaths().empty());
    CHECK(sameTransform(m.instance(car).transform, original));
}

TEST_CASE("Căi: poza la un timp pune obiectele pe cale", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 1000, 400, 300 });
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, car));
    REQUIRE(model::pathOf(m, m.motionPaths().at(path).animation, car) == path);
    model::Model posed = m;
    model::applyPaths(posed, m, m.motionPaths().at(path).animation, 3.0);
    CHECK(sameTransform(posed.instance(car).transform, at(m, path, car, 3.0)));
    model::Model other = m;
    model::applyPaths(other, m, model::AnimationId { 999 }, 3.0); // altă animație: nimic
    CHECK(sameTransform(other.instance(car).transform, m.instance(car).transform));
}

TEST_CASE("Fișier: căile se salvează și se încarcă (v4)", "[core][paths][io]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 2000, 2000, 0 }, { 600, 600, 600 });
    const model::MotionPathId path = addPath(stack, quarter(5000));
    REQUIRE(attach(stack, path, car));
    const io::LoadResult loaded = io::fromJson(io::toJson(m));
    REQUIRE(loaded.model);
    REQUIRE(loaded.model->motionPaths().contains(path));
    const model::MotionPath& a = m.motionPaths().at(path);
    const model::MotionPath& b = loaded.model->motionPaths().at(path);
    CHECK(b.name == a.name);
    CHECK(b.curve == a.curve);
    CHECK(b.animation == a.animation);
    REQUIRE(b.anchors.size() == 1);
    CHECK(b.anchors[0].instance == a.anchors[0].instance);
    CHECK(sameTransform(b.anchors[0].before, a.anchors[0].before));
    CHECK(b.duration == a.duration);
    CHECK(b.easing == a.easing);
}

TEST_CASE("Căi: o mutare în Modelare se păstrează relativ la cale", "[core][paths]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::InstanceId car = box(m, stack, { 0, 0, 0 }, { 1000, 400, 300 });
    const model::MotionPathId path = addPath(stack, straight(10000));
    REQUIRE(attach(stack, path, car));
    // Move (în Modelare) schimbă direct transformarea de bază: 500 mm la stânga de start.
    stack.run("Move", [&](model::Model& model) {
        model::Instance inst = model.instance(car);
        inst.transform = math::Transform::translation({ 0, 500, 0 }) * inst.transform;
        model.setInstance(car, inst);
    });
    CHECK(glm::length(basePoint(m, car, at(m, path, car, 2.0)) - Vec3d { 5000, 500, 0 }) < 1e-6);
    model::Model posed = m;
    model::applyPaths(posed, m, m.motionPaths().at(path).animation, 2.0);
    CHECK(glm::length(basePoint(m, car, posed.instance(car).transform) - Vec3d { 5000, 500, 0 }) < 1e-6);
    const auto single = model::pathTransformAt(m, m.motionPaths().at(path).animation, car, 2.0);
    REQUIRE(single);
    CHECK(sameTransform(*single, posed.instance(car).transform));
    // Calea editată (întoarsă spre +Y): decalajul rămâne la stânga direcției de mers.
    stack.run("Edit", [&](model::Model& model) {
        model::MotionPath p = model.motionPaths().at(path);
        p.curve.points[1] = corner({ 0, 10000, 0 });
        model::updatePath(model, path, p);
    });
    CHECK(glm::length(basePoint(m, car, at(m, path, car, 2.0)) - Vec3d { -500, 5000, 0 }) < 1e-6);
}
