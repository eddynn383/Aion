#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/geom/Subdivide.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <algorithm>
#include <set>

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

std::set<geom::FaceId> allFaces(const Mesh& mesh)
{
    return { mesh.faces().keys().begin(), mesh.faces().keys().end() };
}

bool closed(const Mesh& mesh)
{
    return std::all_of(mesh.edges().values().begin(), mesh.edges().values().end(),
        [](const geom::Edge& e) { return e.faces.size() == 2; });
}

double maxDistanceFrom(const Mesh& mesh, const Vec3d& center)
{
    double d = 0.0;
    for (const geom::Vertex& v : mesh.vertices().values()) {
        d = std::max(d, glm::length(v.position - center));
    }
    return d;
}

double minDistanceFrom(const Mesh& mesh, const Vec3d& center)
{
    double d = 1e30;
    for (const geom::Vertex& v : mesh.vertices().values()) {
        d = std::min(d, glm::length(v.position - center));
    }
    return d;
}

} // namespace

TEST_CASE("Subdivide rotunjește o cutie și o păstrează închisă", "[geom][subdivide]")
{
    Mesh mesh = box();
    const Vec3d center { 500, 500, 500 };
    const double cornerBefore = maxDistanceFrom(mesh, center);

    const geom::SubdivideResult r = geom::subdivide(mesh, allFaces(mesh), 2);
    REQUIRE(r.ok());
    CHECK(mesh.validate(geom::ValidationLevel::Topology).ok());
    CHECK(closed(mesh));
    // 6 fețe → 24 → 96 de patrulatere (unele împărțite în triunghiuri).
    CHECK(mesh.faces().size() >= 96);
    CHECK(r.selection.faces.size() == mesh.faces().size());
    // Colțurile se trag spre interior: forma se apropie de o sferă.
    CHECK(maxDistanceFrom(mesh, center) < cornerBefore * 0.8);
    CHECK(minDistanceFrom(mesh, center) > 250.0);
}

TEST_CASE("Subdivide face netede muchiile noi dintre fețe apropiate", "[geom][subdivide]")
{
    Mesh mesh = box();
    REQUIRE(geom::subdivide(mesh, allFaces(mesh), 2).ok());
    const auto soft = std::count_if(mesh.edges().values().begin(), mesh.edges().values().end(),
        [](const geom::Edge& e) { return e.soft && e.smoothCurve; });
    CHECK(soft == static_cast<long>(mesh.edges().size()));
}

TEST_CASE("Subdivide face o sferă mai fină", "[geom][subdivide]")
{
    Mesh mesh;
    geom::buildPrimitive(mesh, geom::Primitive::Sphere, { 1000, 1000, 1000 }, 12);
    const std::size_t before = mesh.faces().size();
    REQUIRE(geom::subdivide(mesh, allFaces(mesh), 1).ok());
    CHECK(mesh.validate(geom::ValidationLevel::Topology).ok());
    CHECK(closed(mesh));
    CHECK(mesh.faces().size() > before * 3);
}

TEST_CASE("Subdivide păstrează materialul feței", "[geom][subdivide]")
{
    Mesh mesh = box();
    const model::MaterialId red { 7 };
    for (geom::FaceId f : allFaces(mesh)) {
        mesh.setFaceMaterial(f, red);
    }
    REQUIRE(geom::subdivide(mesh, allFaces(mesh), 1).ok());
    for (const geom::Face& f : mesh.faces().values()) {
        CHECK(f.material == red);
    }
}

TEST_CASE("Subdivide refuză fețele cu găuri și prea multe fețe", "[geom][subdivide]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    geom::drawPolygon(mesh, { { 400, 400, 0 }, { 600, 400, 0 }, { 600, 600, 0 }, { 400, 600, 0 } });
    geom::FaceId outer;
    for (geom::FaceId f : mesh.faces().keys()) {
        if (!mesh.face(f).inner.empty()) {
            outer = f;
        }
    }
    REQUIRE(outer);
    CHECK_FALSE(geom::subdivide(mesh, { outer }, 1).ok());

    Mesh sphere;
    geom::buildPrimitive(sphere, geom::Primitive::Sphere, { 1000, 1000, 1000 }, 64);
    const std::size_t faces = sphere.faces().size();
    CHECK_FALSE(geom::subdivide(sphere, allFaces(sphere), 4).ok());
    CHECK(sphere.faces().size() == faces); // neatinsă
}

TEST_CASE("Subdivide pe o suprafață deschisă rotunjește marginea în planul ei", "[geom][subdivide]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    REQUIRE(geom::subdivide(mesh, allFaces(mesh), 2).ok());
    CHECK(mesh.validate(geom::ValidationLevel::Topology).ok());
    // Pătratul plan rămâne o singură față (patrulaterele coplanare se unesc la loc), cu colțurile rotunjite.
    CHECK(mesh.faces().size() == 1);
    CHECK_FALSE(mesh.findVertexNear({ 0, 0, 0 }, 1e-6));
    for (const geom::Vertex& v : mesh.vertices().values()) {
        CHECK(v.position.z == Approx(0.0).margin(1e-9));
    }
}
