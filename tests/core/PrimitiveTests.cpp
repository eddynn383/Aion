#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using geom::Primitive;
using math::Vec3d;

TEST_CASE("Primitive: cub, cilindru, con, sferă — volume închise, cu fețele spre exterior", "[core][geom][primitives]")
{
    const Primitive kind = GENERATE(Primitive::Box, Primitive::Cylinder, Primitive::Cone, Primitive::Sphere);
    INFO("primitiva " << static_cast<int>(kind));
    geom::Mesh mesh;
    geom::buildPrimitive(mesh, kind, { 1000, 1000, 1000 }, 24);

    const auto report = mesh.validate(geom::ValidationLevel::Full);
    INFO(report.text());
    REQUIRE(report.ok());
    // Închis: fiecare muchie are exact două fețe.
    for (const geom::Edge& e : mesh.edges().values()) {
        REQUIRE(e.faces.size() == 2);
    }
    // Spre exterior: normala fiecărei fețe se depărtează de centrul volumului.
    const Vec3d center { 0, 0, 500 };
    for (geom::FaceId f : mesh.faces().keys()) {
        const Vec3d p = geom::faceInteriorPoint(mesh, f);
        CHECK(glm::dot(p - center, mesh.face(f).plane.normal) > 0.0);
    }
    // Suprafețele curbe au muchii soft; cubul nu.
    std::size_t soft = 0;
    for (const geom::Edge& e : mesh.edges().values()) {
        soft += e.soft ? 1 : 0;
    }
    if (kind == Primitive::Box) {
        CHECK(mesh.faces().size() == 6);
        CHECK(soft == 0);
    } else {
        CHECK(soft > 0);
    }
}
