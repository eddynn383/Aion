#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Shapes.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <cmath>
#include <numbers>

using namespace aion;
using Catch::Approx;
using geom::Mesh;
using geom::ShapeInfo;
using math::Vec3d;

namespace {

std::vector<Vec3d> circlePoints(Vec3d c, double r, int n, double sweep = 2.0 * std::numbers::pi, bool closed = true)
{
    std::vector<Vec3d> out;
    const int count = closed ? n : n + 1;
    for (int k = 0; k < count; ++k) {
        const double a = sweep * k / n;
        out.push_back(c + Vec3d { r * std::cos(a), r * std::sin(a), 0.0 });
    }
    return out;
}

geom::Selection edgesOf(const geom::DrawLineResult& r)
{
    geom::Selection s;
    s.edges.insert(r.edges.begin(), r.edges.end());
    return s;
}

} // namespace

TEST_CASE("Forme: linia își schimbă lungimea pe direcția ei", "[core][geom][shapes]")
{
    Mesh mesh;
    const auto line = geom::drawLine(mesh, { 0, 0, 0 }, { 1000, 0, 0 });
    const auto shape = geom::recognizeShape(mesh, edgesOf(line));
    REQUIRE(shape);
    CHECK(shape->kind == ShapeInfo::Kind::Line);
    CHECK(shape->length == Approx(1000.0));
    REQUIRE(geom::setLineLength(mesh, *shape, 2500.0));
    const auto again = geom::recognizeShape(mesh, edgesOf(line));
    REQUIRE(again);
    CHECK(again->length == Approx(2500.0));
    CHECK(mesh.validate().ok());
}

TEST_CASE("Forme: cercul (cu față) își schimbă raza și segmentele; după Push/Pull doar raza", "[core][geom][shapes]")
{
    Mesh mesh;
    const auto circle = geom::drawCurve(mesh, circlePoints({ 500, 500, 0 }, 400, 24), true, true);
    REQUIRE(mesh.faces().size() == 1);
    const auto shape = geom::recognizeShape(mesh, edgesOf(circle));
    REQUIRE(shape);
    CHECK(shape->kind == ShapeInfo::Kind::Circle);
    CHECK(shape->radius == Approx(400.0));
    CHECK(shape->segments == 24);
    CHECK(shape->segmentsEditable);
    CHECK(glm::length(shape->center - Vec3d { 500, 500, 0 }) < 1e-6);

    // Segmente: 12, cu fața refăcută.
    const auto changed = geom::setShapeSegments(mesh, *shape, 12);
    REQUIRE(changed);
    CHECK(changed->edges.size() == 12);
    CHECK(mesh.faces().size() == 1);
    const auto twelve = geom::recognizeShape(mesh, *changed);
    REQUIRE(twelve);
    CHECK(twelve->segments == 12);
    CHECK(twelve->radius == Approx(400.0));
    CHECK(mesh.validate().ok());

    // Raza (selectând fața).
    REQUIRE(geom::setShapeRadius(mesh, *twelve, 600.0));
    geom::Selection face;
    face.faces.insert(mesh.faces().keys().front());
    const auto bigger = geom::recognizeShape(mesh, face);
    REQUIRE(bigger);
    CHECK(bigger->radius == Approx(600.0));

    // După Push/Pull, segmentele nu se mai pot schimba (ar strica pereții), raza da.
    geom::pushPull(mesh, mesh.faces().keys().front(), -300);
    std::vector<geom::EdgeId> base;
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const geom::Edge& e = mesh.edges().values()[i];
        if (std::abs(mesh.position(e.v0).z) < 1e-9 && std::abs(mesh.position(e.v1).z) < 1e-9) {
            base.push_back(mesh.edges().keys()[i]);
        }
    }
    geom::Selection ring;
    ring.edges.insert(base.begin(), base.end());
    const auto extruded = geom::recognizeShape(mesh, ring);
    REQUIRE(extruded);
    CHECK_FALSE(extruded->segmentsEditable);
}

TEST_CASE("Forme: arcul, poligonul și dreptunghiul", "[core][geom][shapes]")
{
    Mesh mesh;
    const auto arc = geom::drawCurve(mesh, circlePoints({ 0, 0, 0 }, 1000, 8, std::numbers::pi / 2, false), false, true);
    const auto a = geom::recognizeShape(mesh, edgesOf(arc));
    REQUIRE(a);
    CHECK(a->kind == ShapeInfo::Kind::Arc);
    CHECK(a->radius == Approx(1000.0));
    CHECK(a->angle == Approx(90.0));
    const auto arc16 = geom::setShapeSegments(mesh, *a, 16);
    REQUIRE(arc16);
    const auto a2 = geom::recognizeShape(mesh, *arc16);
    REQUIRE(a2);
    CHECK(a2->segments == 16);
    CHECK(a2->angle == Approx(90.0));

    const auto hex = geom::drawCurve(mesh, circlePoints({ 5000, 0, 0 }, 300, 6), true, false);
    const auto p = geom::recognizeShape(mesh, edgesOf(hex));
    REQUIRE(p);
    CHECK(p->kind == ShapeInfo::Kind::Polygon);
    CHECK(p->segments == 6);

    geom::drawPolygon(mesh, { { 0, 3000, 0 }, { 2000, 3000, 0 }, { 2000, 4000, 0 }, { 0, 4000, 0 } });
    geom::Selection rect;
    for (geom::FaceId f : mesh.faces().keys()) {
        if (geom::faceContainsPoint(mesh, f, { 1000, 3500, 0 })) {
            rect.faces.insert(f);
        }
    }
    const auto r = geom::recognizeShape(mesh, rect);
    REQUIRE(r);
    CHECK(r->kind == ShapeInfo::Kind::Rectangle);
    CHECK(r->width == Approx(2000.0)); // latura pe X
    CHECK(r->height == Approx(1000.0));
    REQUIRE(geom::setRectangleSize(mesh, *r, 1500.0, 500.0));
    const auto r2 = geom::recognizeShape(mesh, rect);
    REQUIRE(r2);
    CHECK(r2->width == Approx(1500.0));
    CHECK(r2->height == Approx(500.0));
    CHECK(mesh.validate().ok());
}
