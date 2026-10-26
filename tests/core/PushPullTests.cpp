#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Triangulate.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <algorithm>

using namespace aion;
using Catch::Approx;
using geom::FaceId;
using geom::Mesh;
using math::Vec3d;

namespace {

void requireValid(const Mesh& mesh)
{
    const auto report = mesh.validate(geom::ValidationLevel::Full);
    INFO(report.text());
    REQUIRE(report.ok());
}

Vec3d faceCenter(const Mesh& mesh, FaceId f)
{
    const auto pts = mesh.loopPoints(mesh.face(f).outer);
    Vec3d c { 0.0 };
    for (const Vec3d& p : pts) {
        c += p;
    }
    return c / static_cast<double>(pts.size());
}

// Fața din planul dat (normală și un punct), dacă e una singură.
FaceId faceWithNormal(const Mesh& mesh, const Vec3d& normal, const Vec3d& onPlane)
{
    FaceId found;
    for (FaceId f : mesh.faces().keys()) {
        const auto& plane = mesh.face(f).plane;
        if (glm::length(plane.normal - normal) < 1e-9 && std::abs(plane.signedDistance(onPlane)) < 1e-6) {
            REQUIRE_FALSE(found);
            found = f;
        }
    }
    return found;
}

// Toate fețele unui corp convex au normala spre exterior.
void requireOutward(const Mesh& mesh, const Vec3d& inside)
{
    for (FaceId f : mesh.faces().keys()) {
        INFO("normala: " << mesh.face(f).plane.normal.x << "," << mesh.face(f).plane.normal.y << ","
                         << mesh.face(f).plane.normal.z);
        CHECK(glm::dot(mesh.face(f).plane.normal, faceCenter(mesh, f) - inside) > 0.0);
    }
}

void bounds(const Mesh& mesh, Vec3d& lo, Vec3d& hi)
{
    lo = Vec3d { 1e18 };
    hi = Vec3d { -1e18 };
    for (geom::VertexId v : mesh.vertices().keys()) {
        lo = glm::min(lo, mesh.position(v));
        hi = glm::max(hi, mesh.position(v));
    }
}

// Cutie 2000 × 1000 × 1000 (fața de jos pe sol), făcută prin Push/Pull dintr-un dreptunghi.
FaceId groundRectangle(Mesh& mesh, double w = 2000, double d = 1000)
{
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { w, 0, 0 }, { w, d, 0 }, { 0, d, 0 } });
    REQUIRE(mesh.faces().size() == 1);
    return mesh.faces().keys().front();
}

} // namespace

TEST_CASE("Push/Pull: o față liberă devine o cutie închisă, cu normalele spre exterior", "[core][geom][pushpull]")
{
    Mesh mesh;
    const FaceId base = groundRectangle(mesh);
    // Fața de pe sol are normala −Z (ca în SketchUp): „în sus” înseamnă distanță negativă.
    REQUIRE(mesh.face(base).plane.normal.z == Approx(-1.0));
    const auto r = geom::pushPull(mesh, base, -1000);
    REQUIRE(r.done);
    requireValid(mesh);
    CHECK(mesh.faces().size() == 6);
    CHECK(mesh.edges().size() == 12);
    CHECK(mesh.vertices().size() == 8);
    requireOutward(mesh, { 1000, 500, 500 });
    Vec3d lo;
    Vec3d hi;
    bounds(mesh, lo, hi);
    CHECK(glm::length(hi - Vec3d { 2000, 1000, 1000 }) < 1e-9);
}

TEST_CASE("Push/Pull: fața de sus a cutiei, trasă în sus, extinde cutia", "[core][geom][pushpull]")
{
    Mesh mesh;
    geom::pushPull(mesh, groundRectangle(mesh), -1000);
    const FaceId top = faceWithNormal(mesh, { 0, 0, 1 }, { 0, 0, 1000 });
    REQUIRE(top);
    REQUIRE(geom::pushPull(mesh, top, 500).done);
    requireValid(mesh);
    // Laturile noi s-au unit cu cele vechi: tot o cutie cu 6 fețe.
    CHECK(mesh.faces().size() == 6);
    CHECK(mesh.edges().size() == 12);
    CHECK(mesh.vertices().size() == 8);
    requireOutward(mesh, { 1000, 500, 700 });
    Vec3d lo;
    Vec3d hi;
    bounds(mesh, lo, hi);
    CHECK(hi.z == Approx(1500));
}

TEST_CASE("Push/Pull: fața de sus, împinsă în jos, micșorează cutia", "[core][geom][pushpull]")
{
    Mesh mesh;
    geom::pushPull(mesh, groundRectangle(mesh), -1000);
    const FaceId top = faceWithNormal(mesh, { 0, 0, 1 }, { 0, 0, 1000 });
    REQUIRE(geom::pushPull(mesh, top, -400).done);
    requireValid(mesh);
    CHECK(mesh.faces().size() == 6);
    CHECK(mesh.edges().size() == 12);
    CHECK(mesh.vertices().size() == 8);
    requireOutward(mesh, { 1000, 500, 300 });
    Vec3d lo;
    Vec3d hi;
    bounds(mesh, lo, hi);
    CHECK(hi.z == Approx(600));
}

TEST_CASE("Push/Pull: un dreptunghi desenat pe fața de sus devine un turn", "[core][geom][pushpull]")
{
    Mesh mesh;
    geom::pushPull(mesh, groundRectangle(mesh), -1000);
    geom::drawPolygon(mesh, { { 500, 250, 1000 }, { 1500, 250, 1000 }, { 1500, 750, 1000 }, { 500, 750, 1000 } });
    REQUIRE(mesh.faces().size() == 7);
    FaceId inner;
    for (FaceId f : mesh.faces().keys()) {
        if (mesh.face(f).plane.normal.z > 0.5 && mesh.face(f).inner.empty()) {
            inner = f;
        }
    }
    REQUIRE(inner);
    REQUIRE(geom::pushPull(mesh, inner, 800).done);
    requireValid(mesh);
    CHECK(mesh.faces().size() == 11);
    CHECK(mesh.edges().size() == 24);
    Vec3d lo;
    Vec3d hi;
    bounds(mesh, lo, hi);
    CHECK(hi.z == Approx(1800));
}

TEST_CASE("Push/Pull: o fereastră împinsă prin perete face o gaură", "[core][geom][pushpull]")
{
    // Perete 4000 × 200 × 2500: fața din față în planul Y = 0, cu normala −Y.
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 4000, 0, 0 }, { 4000, 200, 0 }, { 0, 200, 0 } });
    geom::pushPull(mesh, mesh.faces().keys().front(), -2500);
    REQUIRE(mesh.faces().size() == 6);
    geom::drawPolygon(mesh, { { 1000, 0, 800 }, { 2000, 0, 800 }, { 2000, 0, 1800 }, { 1000, 0, 1800 } });
    REQUIRE(mesh.faces().size() == 7);
    FaceId window;
    for (FaceId f : mesh.faces().keys()) {
        if (mesh.face(f).plane.normal.y < -0.5 && mesh.face(f).inner.empty()) {
            window = f;
        }
    }
    REQUIRE(window);

    SECTION("până la fața opusă: gaură")
    {
        REQUIRE(geom::pushPull(mesh, window, -200).done);
        requireValid(mesh);
        CHECK(mesh.faces().size() == 10); // 6 ale peretelui (față și spate cu gaură) + 4 glafuri
        CHECK(mesh.edges().size() == 24);
        const FaceId front = faceWithNormal(mesh, { 0, -1, 0 }, { 0, 0, 0 });
        const FaceId back = faceWithNormal(mesh, { 0, 1, 0 }, { 0, 200, 0 });
        REQUIRE(front);
        REQUIRE(back);
        CHECK(mesh.face(front).inner.size() == 1);
        CHECK(mesh.face(back).inner.size() == 1);
        // Glafurile privesc spre axa golului.
        const Vec3d axis { 1500, 100, 1300 };
        for (FaceId f : mesh.faces().keys()) {
            const Vec3d c = faceCenter(mesh, f);
            if (c.x > 999 && c.x < 2001 && c.z > 799 && c.z < 1801 && c.y > 1 && c.y < 199) {
                CHECK(glm::dot(mesh.face(f).plane.normal, axis - c) > 0.0);
            }
        }
    }
    SECTION("doar o parte din grosime: nișă")
    {
        REQUIRE(geom::pushPull(mesh, window, -80).done);
        requireValid(mesh);
        CHECK(mesh.faces().size() == 11); // peretele (față cu gaură) + fundul nișei + 4 glafuri
        const FaceId bottom = faceWithNormal(mesh, { 0, -1, 0 }, { 0, 80, 0 });
        REQUIRE(bottom);
    }
}

TEST_CASE("Push/Pull: păstrarea feței inițiale (Option) și undo/redo", "[core][geom][pushpull]")
{
    Mesh mesh;
    commands::CommandStack stack { mesh };
    stack.setValidation(true, geom::ValidationLevel::Full);
    stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });

    stack.run("Rectangle", [](Mesh& m) {
        geom::drawPolygon(m, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 1000, 0 }, { 0, 1000, 0 } });
    });
    stack.run("Push/Pull", [](Mesh& m) { geom::pushPull(m, m.faces().keys().front(), -1000); });
    Mesh snapshot = mesh;
    const FaceId top = faceWithNormal(mesh, { 0, 0, 1 }, { 0, 0, 1000 });
    stack.run("Push/Pull", [&](Mesh& m) { geom::pushPull(m, top, 500, true); });
    // Cu fața păstrată: o cutie peste alta, despărțite de fața inițială.
    CHECK(mesh.faces().size() == 11);
    Mesh after = mesh;

    REQUIRE(stack.undo());
    CHECK(mesh.sameAs(snapshot));
    REQUIRE(stack.redo());
    CHECK(mesh.sameAs(after));
}

TEST_CASE("Unirea fețelor coplanare și dizolvarea vârfurilor", "[core][geom][pushpull]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 1000, 0 }, { 0, 1000, 0 } });
    geom::drawLine(mesh, { 1000, 0, 0 }, { 1000, 1000, 0 });
    REQUIRE(mesh.faces().size() == 2);
    const geom::EdgeId middle = mesh.findEdge(mesh.findVertexNear({ 1000, 0, 0 }, 1e-6),
        mesh.findVertexNear({ 1000, 1000, 0 }, 1e-6));
    REQUIRE(middle);
    REQUIRE(geom::mergeFacesAcross(mesh, middle));
    requireValid(mesh);
    CHECK(mesh.faces().size() == 1);
    CHECK(mesh.edges().size() == 6); // laturile lungi au rămas împărțite în (1000,0) și (1000,1000)

    REQUIRE(geom::dissolveVertex(mesh, mesh.findVertexNear({ 1000, 0, 0 }, 1e-6)));
    REQUIRE(geom::dissolveVertex(mesh, mesh.findVertexNear({ 1000, 1000, 0 }, 1e-6)));
    requireValid(mesh);
    CHECK(mesh.edges().size() == 4);
    CHECK(mesh.faces().size() == 1);
    // Un colț nu se dizolvă.
    CHECK_FALSE(geom::dissolveVertex(mesh, mesh.findVertexNear({ 0, 0, 0 }, 1e-6)));
}
