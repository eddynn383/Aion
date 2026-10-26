#include "aion/core/geom/Operations.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <cmath>
#include <numbers>

using namespace aion;
using geom::Mesh;
using math::Vec3d;

namespace {

void requireValid(const Mesh& mesh)
{
    const auto report = mesh.validate(geom::ValidationLevel::Full);
    INFO(report.text());
    REQUIRE(report.ok());
}

bool hasVertex(const Mesh& mesh, Vec3d p)
{
    return static_cast<bool>(mesh.findVertexNear(p, 1e-6));
}

} // namespace

TEST_CASE("Offset: dreptunghi spre interior, spre exterior și prea mult", "[core][geom][offset]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 1000, 0 }, { 0, 1000, 0 } });
    const geom::FaceId face = mesh.faces().keys().front();

    CHECK_FALSE(geom::offsetOutline(mesh, face, 600)); // ar trece de jumătatea înălțimii

    SECTION("spre interior")
    {
        REQUIRE(geom::offsetFace(mesh, face, 100));
        requireValid(mesh);
        CHECK(mesh.faces().size() == 2);
        CHECK(hasVertex(mesh, { 100, 100, 0 }));
        CHECK(hasVertex(mesh, { 1900, 900, 0 }));
    }
    SECTION("spre exterior")
    {
        REQUIRE(geom::offsetFace(mesh, face, -200));
        requireValid(mesh);
        CHECK(mesh.faces().size() == 2);
        CHECK(hasVertex(mesh, { -200, -200, 0 }));
        CHECK(hasVertex(mesh, { 2200, 1200, 0 }));
    }
}

TEST_CASE("Offset: un contur concav (L) păstrează colțul intrând", "[core][geom][offset]")
{
    Mesh mesh;
    geom::drawPolygon(mesh,
        { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 1000, 0 }, { 1000, 1000, 0 }, { 1000, 2000, 0 }, { 0, 2000, 0 } });
    REQUIRE(mesh.faces().size() == 1);
    REQUIRE(geom::offsetFace(mesh, mesh.faces().keys().front(), 100));
    requireValid(mesh);
    CHECK(hasVertex(mesh, { 900, 900, 0 })); // colțul intrând se mută în afara lui
    CHECK(hasVertex(mesh, { 100, 1900, 0 }));
}

TEST_CASE("Offset: cercul rămâne o curbă", "[core][geom][offset]")
{
    Mesh mesh;
    std::vector<Vec3d> circle;
    for (int k = 0; k < 24; ++k) {
        const double a = 2.0 * std::numbers::pi * k / 24;
        circle.push_back({ 1000 * std::cos(a), 1000 * std::sin(a), 0 });
    }
    geom::drawCurve(mesh, circle, true, true);
    REQUIRE(geom::offsetFace(mesh, mesh.faces().keys().front(), 200));
    requireValid(mesh);
    std::set<std::uint32_t> curves;
    for (const geom::Edge& e : mesh.edges().values()) {
        CHECK(e.curve != 0);
        curves.insert(e.curve);
    }
    CHECK(curves.size() == 2);
}
