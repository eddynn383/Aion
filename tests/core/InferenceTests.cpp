#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/inference/Inference.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/SnapSources.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

using namespace aion;
using Catch::Approx;
using geom::Mesh;
using inference::Axis;
using inference::Kind;
using inference::Origin;
using math::Transform;
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

// O cutie 1000³ ca obiect în rădăcină: definiția are baza centrată în origine, `local` o mută în (3000..4000, 0..1000,
// 0..1000) și apoi aplică `extra` (rotații, scalări).
model::InstanceId placeBox(model::Model& m, const Transform& extra = {})
{
    model::Definition def { "Cutie", true };
    def.shape = "box";
    const model::DefinitionId d = m.addDefinition(def);
    geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, { 1000, 1000, 1000 }, 24);
    model::Instance inst;
    inst.parent = m.root();
    inst.definition = d;
    inst.transform = extra * Transform::translation({ 3500, 500, 0 });
    return m.addInstance(inst);
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

TEST_CASE("Snap la grid: pasul automat urmează grid-ul vizibil", "[core][inference][grid]")
{
    CHECK(inference::autoGridStep(10000) == Approx(100)); // 2% = 200 mm → linii la 100 mm
    CHECK(inference::autoGridStep(40000) == Approx(1000)); // 800 mm → aproape de 1 m: pasul mare
    CHECK(inference::autoGridStep(100) == Approx(1)); // 2 mm → 1 mm
    CHECK(inference::autoGridStep(0.01) == Approx(0.01)); // limita de jos (ca shaderul: distanța minimă 1 mm)
}

TEST_CASE("Snap la grid: punctele libere cad pe grid, capetele au prioritate", "[core][inference][grid]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 2000, 0 }, { 0, 2000, 0 } });
    const auto camera = topCamera();
    inference::Engine engine;
    engine.withGridSnap(250.0);

    // Pe sol, departe de geometrie.
    auto r = engine.infer(mesh, camera, screen(camera, { 2930, -640, 0 }), std::nullopt);
    CHECK(r.kind == Kind::Ground);
    CHECK(r.onGrid);
    CHECK(near(r.point, { 3000, -750, 0 }));
    CHECK(inference::label(r) == "On Grid");

    // Pe o față orizontală: în planul ei.
    r = engine.infer(mesh, camera, screen(camera, { 1180, 830, 0 }), std::nullopt);
    CHECK(r.kind == Kind::OnFace);
    CHECK(near(r.point, { 1250, 750, 0 }));

    // Lângă un capăt: capătul câștigă, fără rotunjire.
    r = engine.infer(mesh, camera, screen(camera, { 2000, 2000, 0 }) + Vec2d { 4, -3 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK_FALSE(r.onGrid);

    // De-a lungul unei axe dintr-un punct de pornire oarecare: distanța e multiplu de pas.
    const Vec3d from { 2000, 2000, 0 };
    r = engine.infer(mesh, camera, screen(camera, { 2000 + 1160, 2000, 0 }) + Vec2d { 0, 1 }, from);
    REQUIRE(r.kind == Kind::OnAxis);
    CHECK(r.onGrid);
    CHECK(near(r.point, { 3250, 2000, 0 }, 1e-6));

    // Oprit: punctul rămâne cel de sub cursor.
    engine.withGridSnap(0.0);
    r = engine.infer(mesh, camera, screen(camera, { 2930, -640, 0 }), std::nullopt);
    CHECK_FALSE(r.onGrid);
    CHECK(near(r.point, { 2930, -640, 0 }, 1e-3));
}

TEST_CASE("Inferență pe obiecte: capătul unui obiect", "[core][inference]")
{
    model::Model m;
    const model::InstanceId id = placeBox(m);
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene);

    const Vec3d corner { 4000, 1000, 1000 };
    auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, corner) + Vec2d { 4, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK(near(r.point, corner));
    CHECK(r.origin == Origin::Object);
    CHECK_FALSE(r.vertex);
    CHECK(r.sourceInstance == id);
    CHECK(inference::label(r) == "Endpoint");

    // Histerezis: capătul rămâne ales puțin în afara razei (după punct, nu după ID).
    r = engine.infer(m.mesh(m.root()), camera, screen(camera, corner) + Vec2d { 13, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK(near(r.point, corner));

    // La egalitate pe ecran câștigă ctx.mesh.
    geom::drawLine(m.mesh(m.root()), corner, corner + Vec3d { 0, 0, 500 });
    inference::Engine both;
    both.withScene(&scene);
    r = both.infer(m.mesh(m.root()), camera, screen(camera, corner) + Vec2d { 4, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK(r.origin == Origin::Context);
    CHECK(r.vertex);
}

TEST_CASE("Inferență pe obiecte: mijloc și pe muchie", "[core][inference]")
{
    model::Model m;
    placeBox(m);
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene);

    auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, { 3500, 0, 1000 }) + Vec2d { 3, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Midpoint);
    CHECK(near(r.point, { 3500, 0, 1000 }));
    CHECK(r.origin == Origin::Object);
    CHECK_FALSE(r.edge);

    r = engine.infer(m.mesh(m.root()), camera, screen(camera, { 3200, 0, 1000 }) + Vec2d { 0, 3 }, std::nullopt);
    CHECK(r.kind == Kind::OnEdge);
    CHECK(r.origin == Origin::Object);
    CHECK_FALSE(r.edge);
    CHECK(math::projectOnSegment(r.point, { { 3000, 0, 1000 }, { 4000, 0, 1000 } }).distance < 1e-6);
    CHECK(r.point.x == Approx(3200).margin(30));
}

TEST_CASE("Inferență pe obiecte: paralel cu muchia unui obiect", "[core][inference]")
{
    model::Model m;
    constexpr double kPi = 3.14159265358979323846;
    const double angle = 30.0 * kPi / 180.0;
    placeBox(m, Transform::rotation({ 3500, 500, 0 }, { 0, 0, 1 }, angle));
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    const auto camera = topCamera();
    inference::Engine engine;
    engine.withScene(&scene);

    // Muchia de sus (oblică) a cutiei rotite; o atingem într-un punct oarecare, departe de capete și de mijloc.
    const Vec3d a = Transform::rotation({ 3500, 500, 0 }, { 0, 0, 1 }, angle).applyToPoint({ 3000, 0, 1000 });
    const Vec3d b = Transform::rotation({ 3500, 500, 0 }, { 0, 0, 1 }, angle).applyToPoint({ 4000, 0, 1000 });
    const Vec3d onEdge = a + (b - a) * 0.25;
    auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, onEdge), std::nullopt);
    REQUIRE(r.kind == Kind::OnEdge);
    REQUIRE(r.origin == Origin::Object);
    REQUIRE(engine.referenceEdge());
    const Vec3d dir = glm::normalize(b - a);
    CHECK(near(engine.referenceEdge()->a, a, 1e-6));
    CHECK(near(engine.referenceEdge()->b, b, 1e-6));
    const Vec3d normal = engine.referenceEdge()->normal; // a primei fețe a muchiei (sus sau lateral)
    CHECK(glm::length(normal) == Approx(1.0).margin(1e-9));
    CHECK(tol::perpendicular(normal, dir, 1e-6));

    const Vec3d from { 500, 1500, 0 };
    r = engine.infer(m.mesh(m.root()), camera, screen(camera, from + dir * 1200.0), from);
    CHECK(r.kind == Kind::Parallel);
    REQUIRE(r.direction);
    CHECK(tol::parallel(*r.direction, dir, 1e-6));
    REQUIRE(r.guides.size() == 2);
    CHECK(r.guides[1].tint == inference::Tint::Magenta);
    CHECK(near(r.guides[1].a, a, 1e-6));
    CHECK(near(r.guides[1].b, b, 1e-6));

    // Perpendicular: în planul primei fețe, normal pe muchie; dacă ar cădea pe o axă, nu se oferă (ca la ctx.mesh).
    const Vec3d perp = glm::normalize(glm::cross(normal, dir));
    if (std::abs(perp.z) < 0.99) {
        r = engine.infer(m.mesh(m.root()), camera, screen(camera, from + perp * 900.0), from);
        CHECK(r.kind == Kind::Perpendicular);
        REQUIRE(r.direction);
        CHECK(tol::parallel(*r.direction, perp, 1e-6));
    }

    engine.reset();
    CHECK_FALSE(engine.referenceEdge());
}

TEST_CASE("Inferență pe obiecte: pe față, cu normala", "[core][inference]")
{
    model::Model m;
    placeBox(m);
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene);

    const auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, { 3500, 500, 1000 }), std::nullopt);
    CHECK(r.kind == Kind::OnFace);
    CHECK(r.origin == Origin::Object);
    CHECK_FALSE(r.face);
    REQUIRE(r.normal);
    CHECK(near(*r.normal, { 0, 0, 1 }));
    CHECK(near(r.point, { 3500, 500, 1000 }, 1e-3));
    CHECK(inference::label(r) == "On Face");
}

TEST_CASE("Inferență pe obiecte: From Point de la colțul unui obiect", "[core][inference]")
{
    model::Model m;
    placeBox(m);
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene);

    // Colțul de jos din față: linia verde prin el taie axa roșie din punctul de pornire. (Pe axa prin origine,
    // x = 4000 ar fi chiar colțul cutiei, deci un capăt; linia prin colțul de sus nu taie axa deloc.)
    const Vec3d corner { 4000, 0, 0 };
    REQUIRE(engine.infer(m.mesh(m.root()), camera, screen(camera, corner), std::nullopt).kind == Kind::Endpoint);
    REQUIRE(engine.references().size() == 1);

    const Vec3d from { 0, -2000, 0 };
    const auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, { 4000, -2000, 0 }) + Vec2d { 2, 2 }, from);
    CHECK(r.kind == Kind::FromPoint);
    CHECK(r.point.x == Approx(4000));
    REQUIRE_FALSE(r.guides.empty());
    CHECK(r.guides.back().dashed);
    CHECK(near(r.guides.back().a, corner));
}

TEST_CASE("Inferență pe obiecte: ocluzie peste surse", "[core][inference]")
{
    model::Model m;
    placeBox(m);
    const auto camera = isoCamera();
    // Un vârf al ctx.mesh exact în spatele feței din față a cutiei, pe aceeași rază.
    const Vec2d s = screen(camera, { 3500, 0, 500 });
    const math::Ray ray = camera.rayAt(s);
    const auto t = math::intersectRayPlane(ray, math::Plane::fromPointNormal({ 3500, 0, 500 }, { 0, -1, 0 }));
    REQUIRE(t);
    const Vec3d behind = ray.at(*t + 2500.0);
    geom::drawLine(m.mesh(m.root()), behind, behind + Vec3d { 0, 0, 300 });
    const inference::SnapScene scene = model::snapScene(m, {}, {});

    inference::Engine alone;
    CHECK(alone.infer(m.mesh(m.root()), camera, s, std::nullopt).kind == Kind::Endpoint);

    inference::Engine engine;
    engine.withScene(&scene);
    const auto r = engine.infer(m.mesh(m.root()), camera, s, std::nullopt);
    CHECK(r.kind == Kind::OnFace);
    CHECK(r.origin == Origin::Object);
    REQUIRE(r.normal);
    CHECK(near(*r.normal, { 0, -1, 0 }));
}

TEST_CASE("Inferență pe obiecte: obiectul transformat", "[core][inference]")
{
    model::Model m;
    placeBox(m,
        Transform::rotation({ 3500, 500, 0 }, { 0, 0, 1 }, glm::radians(30.0))
            * Transform::scaling({ 3500, 500, 0 }, { 2, 1, 1 }));
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    REQUIRE(scene.sources.size() == 1);
    const Vec3d corner = scene.sources[0].toContext.applyToPoint({ 500, 500, 1000 });
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene);

    const auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, corner) + Vec2d { 4, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK(near(r.point, corner));
}

TEST_CASE("Inferență pe obiecte: obiectul oglindit dă normalele corecte", "[core][inference]")
{
    model::Model m;
    placeBox(m, Transform::scaling({ 3500, 500, 0 }, { -1, 1, 1 }));
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    REQUIRE(glm::determinant(glm::dmat3(scene.sources[0].toContext.matrix())) < 0.0);
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene);

    auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, { 3000, 500, 500 }), std::nullopt);
    CHECK(r.kind == Kind::OnFace);
    REQUIRE(r.normal);
    CHECK(near(*r.normal, { -1, 0, 0 }));

    r = engine.infer(m.mesh(m.root()), camera, screen(camera, { 3000, 0, 1000 }) + Vec2d { 4, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK(near(r.point, { 3000, 0, 1000 }));
}

TEST_CASE("Inferență pe obiecte: prioritate față de grid", "[core][inference]")
{
    model::Model m;
    placeBox(m);
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene).withGridSnap(250.0);

    const Vec3d corner { 4000, 1000, 1000 };
    const auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, corner) + Vec2d { 4, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Endpoint);
    CHECK_FALSE(r.onGrid);
    CHECK(near(r.point, corner));
}

TEST_CASE("Inferență pe obiecte: fără scenă, nimic nou", "[core][inference]")
{
    model::Model m;
    placeBox(m);
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    const auto camera = isoCamera();
    inference::Engine engine;
    engine.withScene(&scene);
    const Vec3d corner { 4000, 1000, 1000 };
    REQUIRE(engine.infer(m.mesh(m.root()), camera, screen(camera, corner), std::nullopt).kind == Kind::Endpoint);

    engine.withScene(nullptr);
    engine.reset();
    const auto r = engine.infer(m.mesh(m.root()), camera, screen(camera, corner) + Vec2d { 4, 0 }, std::nullopt);
    CHECK(r.kind == Kind::Ground);
    CHECK(r.origin == Origin::Context);
    CHECK_FALSE(r.normal);
}
