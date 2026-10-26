#include "aion/core/geom/Operations.h"
#include "aion/core/inference/Inference.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using Catch::Approx;
using geom::Mesh;
using inference::Axis;
using inference::Kind;
using math::Vec2d;
using math::Vec3d;

namespace {

// Camera de sus peste zona (−1300..3300, −900..2900) a solului; ~4.7 mm pe pixel.
view::Camera topCamera()
{
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Top, { { 1000, 1000, 0 }, 0, 0, 6000 }));
    return camera;
}

view::Camera isoCamera()
{
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1000, 1000, 0 }, 0, 0, 8000 }));
    return camera;
}

Vec2d screen(const view::Camera& camera, Vec3d world)
{
    const auto s = camera.project(world);
    REQUIRE(s.has_value());
    return *s;
}

bool near(Vec3d a, Vec3d b, double eps = 1e-6)
{
    return glm::length(a - b) <= eps;
}

} // namespace

TEST_CASE("Inferență: capăt, mijloc, pe muchie, pe față, pe sol", "[core][inference]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 2000, 0 }, { 0, 2000, 0 } });
    const auto camera = topCamera();
    inference::Engine engine;

    auto r = engine.infer(mesh, camera, screen(camera, { 2000, 2000, 0 }) + Vec2d { 5, -4 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK(near(r.point, { 2000, 2000, 0 }));
    CHECK(inference::label(r) == "Endpoint");

    r = engine.infer(mesh, camera, screen(camera, { 1000, 0, 0 }) + Vec2d { 4, 3 }, std::nullopt);
    CHECK(r.kind == Kind::Midpoint);
    CHECK(near(r.point, { 1000, 0, 0 }));

    r = engine.infer(mesh, camera, screen(camera, { 500, 0, 0 }) + Vec2d { 0, 3 }, std::nullopt);
    CHECK(r.kind == Kind::OnEdge);
    CHECK(r.point.y == Approx(0).margin(1e-6));
    CHECK(r.point.x == Approx(500).margin(10));

    r = engine.infer(mesh, camera, screen(camera, { 700, 1300, 0 }), std::nullopt);
    CHECK(r.kind == Kind::OnFace);
    CHECK(r.point.z == Approx(0).margin(1e-6));

    r = engine.infer(mesh, camera, screen(camera, { -800, 2500, 0 }), std::nullopt);
    CHECK(r.kind == Kind::Ground);
    CHECK(near(r.point, { -800, 2500, 0 }, 1e-3));
    CHECK(inference::label(r).empty());

    // Capetele au prioritate în fața mijloacelor și a muchiilor.
    r = engine.infer(mesh, camera, screen(camera, { 0, 0, 0 }) + Vec2d { 2, 2 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
}

TEST_CASE("Inferență: pe axe din punctul de pornire, cu histerezis", "[core][inference]")
{
    Mesh mesh;
    const auto camera = topCamera();
    const Vec3d from { 0, 0, 0 };

    inference::Engine engine;
    // ~6 px deasupra axei roșii.
    auto r = engine.infer(mesh, camera, screen(camera, { 1500, 30, 0 }), from);
    CHECK(r.kind == Kind::OnAxis);
    CHECK(r.axis == Axis::X);
    CHECK(near(r.point, { 1500, 0, 0 }, 1e-3));
    CHECK(inference::label(r) == "On Red Axis");
    REQUIRE(r.guides.size() == 1);
    CHECK(r.guides[0].tint == inference::Tint::X);

    r = engine.infer(mesh, camera, screen(camera, { 20, 900, 0 }), from);
    CHECK(r.kind == Kind::OnAxis);
    CHECK(r.axis == Axis::Y);
    CHECK(r.point.x == Approx(0).margin(1e-3));

    // La ~15 px de axă: o inferență nouă nu se prinde, dar cea anterioară rămâne (histerezis).
    inference::Engine fresh;
    const Vec2d offAxis = screen(camera, { 1500, 0, 0 }) + Vec2d { 0, -15 };
    CHECK(fresh.infer(mesh, camera, offAxis, from).kind == Kind::Ground);

    inference::Engine sticky;
    CHECK(sticky.infer(mesh, camera, screen(camera, { 1500, 0, 0 }), from).axis == Axis::X);
    r = sticky.infer(mesh, camera, offAxis, from);
    CHECK(r.kind == Kind::OnAxis);
    CHECK(r.axis == Axis::X);

    // Iese din raza mărită: se eliberează.
    r = sticky.infer(mesh, camera, screen(camera, { 1500, 0, 0 }) + Vec2d { 0, -30 }, from);
    CHECK(r.kind == Kind::Ground);
}

TEST_CASE("Inferență: axa albastră în vederea izometrică", "[core][inference]")
{
    Mesh mesh;
    const auto camera = isoCamera();
    inference::Engine engine;
    const auto r = engine.infer(mesh, camera, screen(camera, { 0, 0, 1000 }) + Vec2d { 3, 0 }, Vec3d { 0, 0, 0 });
    CHECK(r.kind == Kind::OnAxis);
    CHECK(r.axis == Axis::Z);
    CHECK(r.point.x == Approx(0).margin(1e-6));
    CHECK(r.point.y == Approx(0).margin(1e-6));
    CHECK(r.point.z == Approx(1000).margin(30));
}

TEST_CASE("Inferență: o axă care taie o muchie dă intersecția", "[core][inference]")
{
    Mesh mesh;
    geom::drawLine(mesh, { 1000, -500, 0 }, { 1000, 1500, 0 });
    const auto camera = topCamera();
    inference::Engine engine;
    const auto r = engine.infer(mesh, camera, screen(camera, { 1012, 15, 0 }), Vec3d { 0, 0, 0 });
    CHECK(r.kind == Kind::Intersection);
    CHECK(near(r.point, { 1000, 0, 0 }, 1e-3));
    CHECK(r.axis == Axis::X);
    CHECK(inference::label(r) == "Intersection");
}

TEST_CASE("Inferență: paralel și perpendicular pe muchia de referință", "[core][inference]")
{
    Mesh mesh;
    geom::drawLine(mesh, { 0, 0, 0 }, { 1000, 1000, 0 });
    const auto camera = topCamera();
    inference::Engine engine;

    // Atingem muchia ca s-o „învețe” ca referință.
    auto r = engine.infer(mesh, camera, screen(camera, { 300, 300, 0 }) + Vec2d { 2, 0 }, std::nullopt);
    REQUIRE(r.kind == Kind::OnEdge);
    REQUIRE(engine.referenceEdge());

    // (Punctul de pornire e ales așa încât perpendiculara să nu coincidă pe ecran cu axa albastră,
    // care în perspectivă e o dreaptă radială din centrul vederii.)
    const Vec3d from { 2000, 500, 0 };
    r = engine.infer(mesh, camera, screen(camera, { 2400, 905, 0 }), from);
    CHECK(r.kind == Kind::Parallel);
    CHECK(r.point.x - from.x == Approx(r.point.y - from.y).margin(1e-3));
    CHECK(inference::label(r) == "Parallel to Edge");
    CHECK(r.guides.size() == 2); // direcția și muchia de referință, magenta

    r = engine.infer(mesh, camera, screen(camera, { 1600, 905, 0 }), from);
    CHECK(r.kind == Kind::Perpendicular);
    CHECK((r.point.x - from.x) == Approx(-(r.point.y - from.y)).margin(1e-3));
}

TEST_CASE("Inferență: aliniere cu un punct de referință (From Point)", "[core][inference]")
{
    Mesh mesh;
    geom::drawLine(mesh, { 1500, 1000, 0 }, { 1500, 1800, 0 });
    const auto camera = topCamera();
    inference::Engine engine;

    // Atingem capătul: devine punct de referință.
    REQUIRE(engine.infer(mesh, camera, screen(camera, { 1500, 1000, 0 }), std::nullopt).kind == Kind::Endpoint);
    REQUIRE(engine.references().size() == 1);

    // Pe axa roșie din origine, în dreptul punctului de referință.
    auto r = engine.infer(mesh, camera, screen(camera, { 1510, 10, 0 }), Vec3d { 0, 0, 0 });
    CHECK(r.kind == Kind::FromPoint);
    CHECK(near(r.point, { 1500, 0, 0 }, 1e-3));
    REQUIRE(r.guides.size() == 2);
    CHECK(r.guides[1].dashed);
    CHECK(r.guides[1].tint == inference::Tint::Y);

    // Fără punct de pornire: pe sol, aliniat pe verticala punctului de referință.
    inference::Engine free;
    free.infer(mesh, camera, screen(camera, { 1500, 1000, 0 }), std::nullopt);
    r = free.infer(mesh, camera, screen(camera, { 1515, 300, 0 }), std::nullopt);
    CHECK(r.kind == Kind::FromPoint);
    CHECK(r.point.x == Approx(1500).margin(1e-3));
    CHECK(r.point.y == Approx(300).margin(10));
}

TEST_CASE("Inferență: lock pe axă (săgeți) și lock pe inferența curentă (Shift)", "[core][inference]")
{
    Mesh mesh;
    const auto camera = topCamera();
    const Vec3d from { 0, 0, 0 };

    inference::Engine engine;
    engine.infer(mesh, camera, screen(camera, { 300, 300, 0 }), from);
    engine.toggleAxisLock(Axis::X);
    REQUIRE(engine.locked());
    auto r = engine.infer(mesh, camera, screen(camera, { 800, 600, 0 }), from);
    CHECK(r.locked);
    CHECK(r.kind == Kind::OnAxis);
    CHECK(r.axis == Axis::X);
    CHECK(near(r.point, { 800, 0, 0 }, 1e-3));

    engine.toggleAxisLock(Axis::X);
    CHECK_FALSE(engine.locked());
    CHECK(engine.infer(mesh, camera, screen(camera, { 800, 600, 0 }), from).kind == Kind::Ground);

    // Shift pe axa verde: cursorul se poate îndepărta, punctul rămâne pe axă.
    inference::Engine shift;
    REQUIRE(shift.infer(mesh, camera, screen(camera, { 10, 900, 0 }), from).axis == Axis::Y);
    shift.lockCurrent();
    r = shift.infer(mesh, camera, screen(camera, { 700, 1200, 0 }), from);
    CHECK(r.locked);
    CHECK(r.axis == Axis::Y);
    CHECK(near(r.point, { 0, 1200, 0 }, 1e-3));

    // Un capăt atins în timpul lock-ului se proiectează pe direcție.
    geom::drawLine(mesh, { 600, 1700, 0 }, { 900, 1700, 0 });
    r = shift.infer(mesh, camera, screen(camera, { 600, 1700, 0 }) + Vec2d { 3, 3 }, from);
    CHECK(near(r.point, { 0, 1700, 0 }, 1e-3));
    shift.unlock();
    CHECK_FALSE(shift.locked());
}
