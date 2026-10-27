#include "aion/core/geom/Bevel.h"
#include "aion/core/geom/Operations.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using Catch::Approx;
using geom::Mesh;
using math::Vec3d;

namespace {

// O cutie 1000 × 1000 × 1000 cu colțul în origine.
Mesh box()
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    geom::pushPull(mesh, mesh.faces().keys().front(), -1000);
    return mesh;
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

geom::EdgeId edgeBetween(const Mesh& mesh, Vec3d a, Vec3d b)
{
    return mesh.findEdge(mesh.findVertexNear(a, 1e-6), mesh.findVertexNear(b, 1e-6));
}

// Un volum închis: fiecare muchie are exact două fețe.
bool closed(const Mesh& mesh)
{
    for (const geom::Edge& e : mesh.edges().values()) {
        if (e.faces.size() != 2) {
            return false;
        }
    }
    return true;
}

double area(const Mesh& mesh, geom::FaceId f)
{
    const std::vector<Vec3d> p = mesh.loopPoints(mesh.face(f).outer);
    Vec3d sum { 0.0 };
    for (std::size_t i = 0; i < p.size(); ++i) {
        sum += glm::cross(p[i], p[(i + 1) % p.size()]);
    }
    return glm::length(sum) / 2.0;
}

} // namespace

TEST_CASE("Chamfer pe o muchie: o față nouă, volumul rămâne închis", "[core][geom][bevel]")
{
    Mesh mesh = box();
    REQUIRE(mesh.faces().size() == 6);
    const geom::EdgeId top = edgeBetween(mesh, { 0, 0, 1000 }, { 1000, 0, 1000 });
    REQUIRE(top);
    const geom::BevelResult r = geom::bevelEdges(mesh, { top }, 100, 1);
    INFO(r.error);
    REQUIRE(r.ok());
    CHECK(mesh.faces().size() == 7);
    CHECK(mesh.validate().ok());
    CHECK(closed(mesh));
    // Fața de sus și cea din față s-au micșorat cu 100 mm.
    CHECK(area(mesh, faceAt(mesh, { 500, 500, 1000 })) == Approx(1000.0 * 900.0));
    CHECK(area(mesh, faceAt(mesh, { 500, 0, 500 })) == Approx(1000.0 * 900.0));
    // Teșitura: lățimea 100·√2.
    const geom::FaceId bevel = faceAt(mesh, { 500, 50, 950 });
    REQUIRE(bevel);
    CHECK(area(mesh, bevel) == Approx(1000.0 * 100.0 * std::sqrt(2.0)));
}

TEST_CASE("Bevel pe o muchie: benzi netede tangente la ambele fețe", "[core][geom][bevel]")
{
    Mesh mesh = box();
    const geom::EdgeId top = edgeBetween(mesh, { 0, 0, 1000 }, { 1000, 0, 1000 });
    const geom::BevelResult r = geom::bevelEdges(mesh, { top }, 200, 4);
    INFO(r.error);
    REQUIRE(r.ok());
    CHECK(mesh.faces().size() == 6 + 4);
    CHECK(mesh.validate().ok());
    CHECK(closed(mesh));
    // Mijlocul arcului (raza 200, centrul la (y, z) = (200, 800)) e la 200 de centru.
    const Vec3d mid { 500, 200 - 200 * std::sqrt(0.5), 800 + 200 * std::sqrt(0.5) };
    const geom::VertexId v = mesh.findVertexNear({ 0, mid.y, mid.z }, 1e-3);
    CHECK(v);
    int soft = 0;
    for (const geom::Edge& e : mesh.edges().values()) {
        soft += e.soft ? 1 : 0;
    }
    CHECK(soft == 3); // între cele 4 benzi
}

TEST_CASE("Bevel / chamfer pe o față: toate laturile, colțurile în unghi", "[core][geom][bevel]")
{
    {
        Mesh mesh = box();
        const geom::BevelResult r = geom::bevelFace(mesh, faceAt(mesh, { 500, 500, 1000 }), 100, 1);
        INFO(r.error);
        REQUIRE(r.ok());
        CHECK(mesh.faces().size() == 6 + 4);
        CHECK(mesh.validate().ok());
        CHECK(closed(mesh));
        CHECK(area(mesh, faceAt(mesh, { 500, 500, 1000 })) == Approx(800.0 * 800.0));
        // Laturile coboară cu 100 mm.
        CHECK(area(mesh, faceAt(mesh, { 500, 0, 500 })) == Approx(1000.0 * 900.0));
    }
    {
        Mesh mesh = box();
        const geom::BevelResult r = geom::bevelFace(mesh, faceAt(mesh, { 500, 500, 1000 }), 100, 3);
        INFO(r.error);
        REQUIRE(r.ok());
        CHECK(mesh.faces().size() == 6 + 4 * 3);
        CHECK(mesh.validate().ok());
        CHECK(closed(mesh));
    }
}

TEST_CASE("Bevel: cazurile nepermise nu schimbă nimic", "[core][geom][bevel]")
{
    Mesh mesh = box();
    const std::size_t faces = mesh.faces().size();
    // Două muchii cu un vârf comun.
    const geom::EdgeId a = edgeBetween(mesh, { 0, 0, 1000 }, { 1000, 0, 1000 });
    const geom::EdgeId b = edgeBetween(mesh, { 1000, 0, 1000 }, { 1000, 1000, 1000 });
    CHECK_FALSE(geom::bevelEdges(mesh, { a, b }, 50, 1).ok());
    // O distanță prea mare.
    CHECK_FALSE(geom::bevelFace(mesh, faceAt(mesh, { 500, 500, 1000 }), 600, 1).ok());
    // O față liberă (fără volum).
    Mesh flat;
    geom::drawPolygon(flat, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    CHECK_FALSE(geom::bevelFace(flat, flat.faces().keys().front(), 100, 1).ok());
    CHECK(mesh.faces().size() == faces);
}

TEST_CASE("Bevel cu multe segmente și distanță mică", "[core][geom][bevel]")
{
    for (int segments : { 2, 3, 6, 12 }) {
        for (double d : { 20.0, 50.0, 300.0 }) {
            Mesh mesh = box();
            const geom::EdgeId top = edgeBetween(mesh, { 0, 0, 1000 }, { 1000, 0, 1000 });
            const geom::BevelResult r = geom::bevelEdges(mesh, { top }, d, segments);
            INFO("segments " << segments << " d " << d << " " << r.error);
            REQUIRE(r.ok());
            CHECK(mesh.faces().size() == std::size_t(6 + segments));
            CHECK(closed(mesh));
            CHECK(r.selection.faces.size() == std::size_t(segments)); // doar benzile noi
        }
    }
}
