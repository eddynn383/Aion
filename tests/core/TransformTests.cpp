#include "aion/core/geom/Operations.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <numbers>

using namespace aion;
using Catch::Approx;
using geom::FaceId;
using geom::Mesh;
using math::Transform;
using math::Vec3d;

namespace {

void requireValid(const Mesh& mesh)
{
    const auto report = mesh.validate(geom::ValidationLevel::Full);
    INFO(report.text());
    REQUIRE(report.ok());
}

// Cutie w × d × h cu colțul în origine, făcută prin Push/Pull.
void box(Mesh& mesh, double w, double d, double h)
{
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { w, 0, 0 }, { w, d, 0 }, { 0, d, 0 } });
    geom::pushPull(mesh, mesh.faces().keys().front(), -h);
    REQUIRE(mesh.faces().size() == 6);
}

FaceId topFace(const Mesh& mesh)
{
    for (FaceId f : mesh.faces().keys()) {
        if (mesh.face(f).plane.normal.z > 0.99) {
            return f;
        }
    }
    return {};
}

geom::VertexId vertexAt(const Mesh& mesh, Vec3d p)
{
    const auto v = mesh.findVertexNear(p, 1e-6);
    REQUIRE(v);
    return v;
}

} // namespace

TEST_CASE("Move: fața de sus a unei cutii, mutată în sus, întinde laturile", "[core][geom][transform]")
{
    Mesh mesh;
    box(mesh, 2000, 1000, 1000);
    const auto verts = geom::verticesOf(mesh, { {}, { topFace(mesh) } });
    REQUIRE(verts.size() == 4);
    REQUIRE(geom::transformVertices(mesh, verts, Transform::translation({ 0, 0, 500 })));
    requireValid(mesh);
    CHECK(mesh.faces().size() == 6);
    for (FaceId f : mesh.faces().keys()) {
        const Vec3d n = mesh.face(f).plane.normal;
        CHECK(glm::length(n) == Approx(1.0));
    }
    CHECK(mesh.position(vertexAt(mesh, { 0, 0, 1500 })).z == Approx(1500));
}

TEST_CASE("Move: acoperiș în două ape din linia de coamă mutată în sus", "[core][geom][transform]")
{
    Mesh mesh;
    box(mesh, 4000, 3000, 2500);
    geom::drawLine(mesh, { 0, 1500, 2500 }, { 4000, 1500, 2500 });
    REQUIRE(mesh.faces().size() == 7);
    const geom::EdgeId ridge = mesh.findEdge(vertexAt(mesh, { 0, 1500, 2500 }), vertexAt(mesh, { 4000, 1500, 2500 }));
    REQUIRE(ridge);
    REQUIRE(geom::transformVertices(mesh, geom::verticesOf(mesh, { { ridge }, {} }), Transform::translation({ 0, 0, 1500 })));
    requireValid(mesh);
    // Cele două ape și frontoanele (pereții de capăt devin pentagoane) rămân plane: nimic nu s-a pliat.
    CHECK(mesh.faces().size() == 7);
    int pentagons = 0;
    int slopes = 0;
    for (FaceId f : mesh.faces().keys()) {
        const auto& face = mesh.face(f);
        pentagons += face.outer.uses.size() == 5 ? 1 : 0;
        const double nz = face.plane.normal.z;
        slopes += (nz > 0.1 && nz < 0.99) ? 1 : 0;
    }
    CHECK(pentagons == 2);
    CHECK(slopes == 2);
}

TEST_CASE("Move: un colț ridicat pliază fața de sus în triunghiuri", "[core][geom][transform]")
{
    Mesh mesh;
    box(mesh, 2000, 1000, 1000);
    REQUIRE(geom::transformVertices(mesh, { vertexAt(mesh, { 2000, 1000, 1000 }) }, Transform::translation({ 0, 0, 400 })));
    requireValid(mesh);
    CHECK(mesh.faces().size() == 7); // fața de sus devine două triunghiuri
    CHECK(mesh.edges().size() == 13); // plus diagonala
}

TEST_CASE("Move: o muchie strânsă într-un punct e refuzată", "[core][geom][transform]")
{
    Mesh mesh;
    box(mesh, 2000, 1000, 1000);
    const Mesh before = mesh;
    CHECK_FALSE(geom::transformVertices(mesh, { vertexAt(mesh, { 2000, 0, 0 }) }, Transform::translation({ -2000, 0, 0 })));
    CHECK(mesh.sameAs(before));
}

TEST_CASE("Rotate și Scale pe toată cutia", "[core][geom][transform]")
{
    Mesh mesh;
    box(mesh, 2000, 1000, 1000);
    std::set<geom::VertexId> all(mesh.vertices().keys().begin(), mesh.vertices().keys().end());
    REQUIRE(geom::transformVertices(mesh, all, Transform::rotation({ 0, 0, 0 }, { 0, 0, 1 }, std::numbers::pi / 2)));
    requireValid(mesh);
    CHECK(glm::length(mesh.position(vertexAt(mesh, { 0, 2000, 0 }))) == Approx(2000));
    REQUIRE(geom::transformVertices(mesh, all, Transform::scaling({ 0, 0, 0 }, { 2, 2, 2 })));
    requireValid(mesh);
    vertexAt(mesh, { -2000, 4000, 2000 });
    // Normalele rămân spre exterior: centrul cutiei e în spatele fiecărei fețe.
    const Vec3d center { -1000, 2000, 1000 };
    for (FaceId f : mesh.faces().keys()) {
        CHECK(glm::dot(mesh.face(f).plane.normal, geom::faceInteriorPoint(mesh, f) - center) > 0.0);
    }
}

TEST_CASE("Copiere: o cutie copiată alături", "[core][geom][transform]")
{
    Mesh mesh;
    box(mesh, 2000, 1000, 1000);
    geom::Selection all;
    for (FaceId f : mesh.faces().keys()) {
        all.faces.insert(f);
    }
    const geom::Selection copy = geom::copySelection(mesh, all, Transform::translation({ 3000, 0, 0 }));
    requireValid(mesh);
    CHECK(mesh.faces().size() == 12);
    CHECK(copy.faces.size() == 6);
    CHECK(copy.edges.size() == 12);
    const Vec3d center { 4000, 500, 500 };
    for (FaceId f : copy.faces) {
        CHECK(glm::dot(mesh.face(f).plane.normal, geom::faceInteriorPoint(mesh, f) - center) > 0.0);
    }
}
