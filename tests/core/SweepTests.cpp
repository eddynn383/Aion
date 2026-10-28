#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Sweep.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using Catch::Approx;
using geom::Mesh;
using math::Vec3d;

namespace {

// Volumul (teorema divergenței) pe fețele triangulate ca evantai (fețele sunt convexe în teste).
double volume(const Mesh& mesh)
{
    double v = 0.0;
    for (geom::FaceId f : mesh.faces().keys()) {
        const std::vector<Vec3d> p = mesh.loopPoints(mesh.face(f).outer);
        for (std::size_t i = 1; i + 1 < p.size(); ++i) {
            v += glm::dot(p[0], glm::cross(p[i], p[i + 1])) / 6.0;
        }
    }
    return v;
}

geom::FaceId faceAt(const Mesh& mesh, Vec3d p)
{
    for (geom::FaceId f : mesh.faces().keys()) {
        if (geom::faceContainsPoint(mesh, f, p)) {
            return f;
        }
    }
    return {};
}

} // namespace

TEST_CASE("Follow Me: un profil pătrat pe un traseu în L dă un volum închis, cu cotul în unghi", "[core][geom][sweep]")
{
    Mesh mesh;
    // Profilul 100 × 100 în planul YZ, la începutul traseului.
    geom::drawPolygon(mesh, { { 0, -50, 0 }, { 0, 50, 0 }, { 0, 50, 100 }, { 0, -50, 100 } });
    const geom::FaceId profile = mesh.faces().keys().front();
    // Traseul: 1000 pe X, apoi 1000 pe Y.
    std::set<geom::EdgeId> path;
    for (geom::EdgeId e : geom::drawLine(mesh, { 0, 0, 0 }, { 1000, 0, 0 }).edges) {
        path.insert(e);
    }
    for (geom::EdgeId e : geom::drawLine(mesh, { 1000, 0, 0 }, { 1000, 1000, 0 }).edges) {
        path.insert(e);
    }
    const geom::SweepResult r = geom::followMe(mesh, profile, path);
    INFO(r.error);
    REQUIRE(r.ok());
    CHECK(mesh.validate().ok());
    // 2 capace + 2 × 4 benzi; muchiile traseului rămân libere (pe fața de jos, ca în SketchUp).
    CHECK(mesh.faces().size() >= 10);
    // Volumul: secțiunea 100 × 100 pe linia mediană (1000 + 1000, cotul în unghi nu schimbă volumul).
    CHECK(std::abs(volume(mesh)) == Approx(100.0 * 100.0 * 2000.0).epsilon(0.02));
    // Cotul: vârful exterior al secțiunii la (1050, −50).
    CHECK(mesh.findVertexNear({ 1050, -50, 0 }, 1e-6));
    CHECK(mesh.findVertexNear({ 950, 50, 100 }, 1e-6));
}

TEST_CASE("Follow Me: un inel (traseu închis) fără capace", "[core][geom][sweep]")
{
    Mesh mesh;
    // Traseul: marginea unui pătrat 1000 × 1000 (doar muchii).
    std::set<geom::EdgeId> path;
    const Vec3d c[4] = { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } };
    // Profilul: un pătrat 100 × 100 în planul XZ, la mijlocul laturii de jos (perpendicular pe ea).
    geom::drawPolygon(mesh, { { 500, -50, 0 }, { 500, 50, 0 }, { 500, 50, 100 }, { 500, -50, 100 } });
    const geom::FaceId profile = mesh.faces().keys().front();
    for (int i = 0; i < 4; ++i) {
        for (geom::EdgeId e : geom::drawLine(mesh, c[i], c[(i + 1) % 4]).edges) {
            path.insert(e);
        }
    }
    // Pătratul traseului e și el o față (s-a închis singur); profilul o atinge și îi împarte latura de jos în două.
    REQUIRE(mesh.faces().size() == 2);
    const geom::SweepResult r = geom::followMe(mesh, profile, path);
    INFO(r.error);
    REQUIRE(r.ok());
    CHECK(mesh.validate().ok());
    // 5 segmente (latura de jos e împărțită) × 4 benzi; benzile de jos sunt tăiate în două de traseu (care stă pe
    // mijlocul lor, în planul pătratului), iar pătratul rămâne, nu dispare.
    CHECK(r.selection.faces.size() >= 5 * 4);
    CHECK(faceAt(mesh, { 500, 500, 0 }));
}

TEST_CASE("Follow Me: un cerc pe un traseu drept dă un cilindru cu laturile netede", "[core][geom][sweep]")
{
    Mesh mesh;
    std::vector<Vec3d> circle;
    for (int i = 0; i < 24; ++i) {
        const double a = i * 2.0 * 3.14159265358979 / 24.0;
        circle.push_back({ 0.0, 100.0 * std::cos(a), 100.0 * std::sin(a) });
    }
    geom::drawCurve(mesh, circle, true, true);
    REQUIRE(mesh.faces().size() == 1);
    const geom::FaceId profile = mesh.faces().keys().front();
    std::set<geom::EdgeId> path;
    for (geom::EdgeId e : geom::drawLine(mesh, { 0, 0, 0 }, { 500, 0, 0 }).edges) {
        path.insert(e);
    }
    const geom::SweepResult r = geom::followMe(mesh, profile, path);
    INFO(r.error);
    REQUIRE(r.ok());
    CHECK(r.selection.faces.size() == 24 + 1);
    int soft = 0;
    for (const geom::Edge& e : mesh.edges().values()) {
        soft += e.soft ? 1 : 0;
    }
    CHECK(soft == 24);
    // Capacul de la capăt privește înainte, profilul înapoi.
    const geom::FaceId end = faceAt(mesh, { 500, 0, 0 });
    const geom::FaceId start = faceAt(mesh, { 0, 0, 0 });
    REQUIRE(end);
    REQUIRE(start);
    CHECK(mesh.face(end).plane.normal.x == Approx(1.0));
    CHECK(mesh.face(start).plane.normal.x == Approx(-1.0));
}

TEST_CASE("Follow Me: cazurile nepermise", "[core][geom][sweep]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, -50, 0 }, { 0, 50, 0 }, { 0, 50, 100 }, { 0, -50, 100 } });
    const geom::FaceId profile = mesh.faces().keys().front();
    // Traseu paralel cu profilul.
    std::set<geom::EdgeId> parallel;
    for (geom::EdgeId e : geom::drawLine(mesh, { 0, -50, -200 }, { 0, 50, -200 }).edges) {
        parallel.insert(e);
    }
    CHECK_FALSE(geom::followMe(mesh, profile, parallel).ok());
    // Traseu ramificat.
    std::set<geom::EdgeId> branched;
    for (const auto& [a, b] : { std::pair<Vec3d, Vec3d> { { 0, 0, 0 }, { 500, 0, 0 } }, { { 500, 0, 0 }, { 900, 0, 0 } },
             { { 500, 0, 0 }, { 500, 400, 0 } } }) {
        for (geom::EdgeId e : geom::drawLine(mesh, a, b).edges) {
            branched.insert(e);
        }
    }
    CHECK_FALSE(geom::followMe(mesh, profile, branched).ok());
}
