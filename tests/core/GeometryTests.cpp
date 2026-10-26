#include "forma/core/commands/CommandStack.h"
#include "forma/core/geom/Operations.h"
#include "forma/core/geom/Triangulate.h"
#include "forma/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace forma::geom;
using forma::commands::CommandStack;

namespace {

void requireValid(const Mesh& mesh)
{
    const ValidationReport report = mesh.validate(ValidationLevel::Full);
    INFO(report.text());
    REQUIRE(report.ok());
}

std::vector<Vec3d> rect(double x0, double y0, double x1, double y1, double z = 0.0)
{
    return { { x0, y0, z }, { x1, y0, z }, { x1, y1, z }, { x0, y1, z } };
}

FaceId onlyFace(const Mesh& mesh)
{
    REQUIRE(mesh.faces().size() == 1);
    return mesh.faces().keys().front();
}

double faceArea(const Mesh& mesh, FaceId f)
{
    const FaceTriangles t = triangulateFace(mesh, f);
    double area = 0.0;
    for (std::size_t i = 0; i + 2 < t.indices.size(); i += 3) {
        area += 0.5 * glm::length(glm::cross(t.points[t.indices[i + 1]] - t.points[t.indices[i]],
                          t.points[t.indices[i + 2]] - t.points[t.indices[i]]));
    }
    return area;
}

// Rulează operația ca o comandă și verifică undo/redo + validate() la fiecare pas.
template <class Fn>
void checkUndoRedo(Mesh& mesh, CommandStack& stack, Fn&& fn)
{
    const Mesh before = mesh;
    REQUIRE(stack.run("Op", fn));
    requireValid(mesh);
    const Mesh after = mesh;
    REQUIRE(stack.undo());
    requireValid(mesh);
    CHECK(mesh.sameAs(before));
    REQUIRE(stack.redo());
    requireValid(mesh);
    CHECK(mesh.sameAs(after));
}

} // namespace

TEST_CASE("Pătrat → o față", "[core][geom][draw]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    checkUndoRedo(mesh, stack, [](Mesh& m) { drawPolygon(m, rect(0, 0, 1000, 1000)); });

    const FaceId f = onlyFace(mesh);
    CHECK(mesh.edges().size() == 4);
    CHECK(mesh.vertices().size() == 4);
    // Fețele de pe sol privesc în jos, ca în SketchUp.
    CHECK(forma::tol::samePoint(mesh.face(f).plane.normal, { 0, 0, -1 }));
    CHECK(forma::tol::equal(faceArea(mesh, f), 1e6, 1e-6));
}

TEST_CASE("Fața apare abia când ciclul se închide", "[core][geom][draw]")
{
    Mesh mesh;
    drawLine(mesh, { 0, 0, 0 }, { 1000, 0, 0 });
    drawLine(mesh, { 1000, 0, 0 }, { 1000, 1000, 0 });
    drawLine(mesh, { 1000, 1000, 0 }, { 0, 1000, 0 });
    CHECK(mesh.faces().empty());
    const auto r = drawLine(mesh, { 0, 1000, 0 }, { 0, 0, 0 });
    CHECK(r.createdFaces.size() == 1);
    CHECK(mesh.faces().size() == 1);
    requireValid(mesh);
}

TEST_CASE("Pătrat împărțit de diagonală → două fețe", "[core][geom][draw]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    drawPolygon(mesh, rect(0, 0, 1000, 1000));
    checkUndoRedo(mesh, stack, [](Mesh& m) { drawLine(m, { 0, 0, 0 }, { 1000, 1000, 0 }); });

    REQUIRE(mesh.faces().size() == 2);
    for (FaceId f : mesh.faces().keys()) {
        CHECK(mesh.face(f).outer.uses.size() == 3);
        CHECK(forma::tol::equal(faceArea(mesh, f), 5e5, 1e-6));
        // Fețele moștenesc orientarea feței împărțite.
        CHECK(forma::tol::samePoint(mesh.face(f).plane.normal, { 0, 0, -1 }));
    }
    const EdgeId diagonal = mesh.findEdge(mesh.findVertexNear({ 0, 0, 0 }, 1), mesh.findVertexNear({ 1000, 1000, 0 }, 1));
    REQUIRE(diagonal);
    CHECK(mesh.edge(diagonal).faces.size() == 2);
}

TEST_CASE("Pătrat în pătrat → față cu gaură", "[core][geom][draw]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    drawPolygon(mesh, rect(0, 0, 3000, 3000));
    checkUndoRedo(mesh, stack, [](Mesh& m) { drawPolygon(m, rect(1000, 1000, 2000, 2000)); });

    REQUIRE(mesh.faces().size() == 2);
    const auto faces = mesh.faces().keys();
    const auto withHole = std::find_if(faces.begin(), faces.end(), [&](FaceId f) { return !mesh.face(f).inner.empty(); });
    REQUIRE(withHole != faces.end());
    CHECK(mesh.face(*withHole).inner.size() == 1);
    CHECK(forma::tol::equal(faceArea(mesh, *withHole), 9e6 - 1e6, 1e-6));
    CHECK_FALSE(faceContainsPoint(mesh, *withHole, { 1500, 1500, 0 }));
    CHECK(faceContainsPoint(mesh, *withHole, { 500, 500, 0 }));
}

TEST_CASE("Pătrat mare desenat în jurul unei fețe existente", "[core][geom][draw]")
{
    Mesh mesh;
    drawPolygon(mesh, rect(1000, 1000, 2000, 2000));
    const FaceId small = onlyFace(mesh);
    drawPolygon(mesh, rect(0, 0, 3000, 3000));
    requireValid(mesh);

    REQUIRE(mesh.faces().size() == 2);
    CHECK(mesh.faces().contains(small)); // fața existentă rămâne neschimbată
    for (FaceId f : mesh.faces().keys()) {
        if (f != small) {
            CHECK(mesh.face(f).inner.size() == 1);
        }
    }
}

TEST_CASE("Liniile care se intersectează se împart reciproc", "[core][geom][draw]")
{
    Mesh mesh;
    drawLine(mesh, { 0, 0, 0 }, { 1000, 1000, 0 });
    const auto r = drawLine(mesh, { 0, 1000, 0 }, { 1000, 0, 0 });
    requireValid(mesh);
    CHECK(r.edges.size() == 2);
    CHECK(mesh.edges().size() == 4);
    CHECK(mesh.vertices().size() == 5);
    const VertexId center = mesh.findVertexNear({ 500, 500, 0 }, 1e-3);
    REQUIRE(center);
    CHECK(mesh.vertex(center).edges.size() == 4);
}

TEST_CASE("Linia peste o muchie existentă nu o dublează", "[core][geom][draw]")
{
    Mesh mesh;
    drawLine(mesh, { 0, 0, 0 }, { 1000, 0, 0 });
    // Se suprapune pe jumătate și continuă.
    const auto r = drawLine(mesh, { 500, 0, 0 }, { 1500, 0, 0 });
    requireValid(mesh);
    CHECK(r.edges.size() == 2);
    CHECK(mesh.edges().size() == 3); // 0–500, 500–1000, 1000–1500
    // Exact aceeași linie: nicio schimbare.
    const Mesh before = mesh;
    drawLine(mesh, { 0, 0, 0 }, { 1500, 0, 0 });
    CHECK(mesh.sameAs(before));
}

TEST_CASE("Capătul pe o muchie o împarte (T)", "[core][geom][draw]")
{
    Mesh mesh;
    drawLine(mesh, { 0, 0, 0 }, { 1000, 0, 0 });
    drawLine(mesh, { 400, 0, 0 }, { 400, 800, 0 });
    requireValid(mesh);
    CHECK(mesh.edges().size() == 3);
    CHECK(mesh.vertex(mesh.findVertexNear({ 400, 0, 0 }, 1e-3)).edges.size() == 3);
}

TEST_CASE("O linie care traversează fața și iese în afară o împarte", "[core][geom][draw]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    drawPolygon(mesh, rect(0, 0, 1000, 1000));
    checkUndoRedo(mesh, stack, [](Mesh& m) { drawLine(m, { 500, -500, 0 }, { 500, 1500, 0 }); });
    CHECK(mesh.faces().size() == 2);
    // Bucățile din afară rămân muchii libere.
    CHECK(mesh.edge(mesh.findEdge(mesh.findVertexNear({ 500, -500, 0 }, 1), mesh.findVertexNear({ 500, 0, 0 }, 1)))
              .faces.empty());
}

TEST_CASE("O muchie care atârnă în interiorul feței nu o împarte", "[core][geom][draw]")
{
    Mesh mesh;
    drawPolygon(mesh, rect(0, 0, 1000, 1000));
    const FaceId f = onlyFace(mesh);
    drawLine(mesh, { 0, 500, 0 }, { 400, 500, 0 }); // de pe margine spre interior
    drawLine(mesh, { 600, 600, 0 }, { 800, 800, 0 }); // complet în interior
    requireValid(mesh);
    CHECK(mesh.faces().size() == 1);
    CHECK(mesh.faces().contains(f));
}

TEST_CASE("Un cub desenat din 12 muchii are 6 fețe", "[core][geom][draw]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    checkUndoRedo(mesh, stack, [](Mesh& m) {
        const double s = 1000;
        drawPolygon(m, rect(0, 0, s, s, 0));
        drawPolygon(m, rect(0, 0, s, s, s));
        for (auto [x, y] : { std::pair { 0.0, 0.0 }, { s, 0.0 }, { s, s }, { 0.0, s } }) {
            drawLine(m, { x, y, 0 }, { x, y, s });
        }
    });
    CHECK(mesh.faces().size() == 6);
    for (EdgeId e : mesh.edges().keys()) {
        CHECK(mesh.edge(e).faces.size() == 2);
    }
}

TEST_CASE("O muchie poate avea trei fețe (non-manifold)", "[core][geom][draw]")
{
    Mesh mesh;
    drawPolygon(mesh, rect(0, 0, 1000, 1000));
    drawLine(mesh, { 500, 0, 0 }, { 500, 1000, 0 });
    // Un perete vertical pe linia din mijloc.
    drawPolygon(mesh, { { 500, 0, 0 }, { 500, 1000, 0 }, { 500, 1000, 800 }, { 500, 0, 800 } });
    requireValid(mesh);
    CHECK(mesh.faces().size() == 3);
    const EdgeId middle = mesh.findEdge(mesh.findVertexNear({ 500, 0, 0 }, 1), mesh.findVertexNear({ 500, 1000, 0 }, 1));
    REQUIRE(middle);
    CHECK(mesh.edge(middle).faces.size() == 3);
}

TEST_CASE("Ștergerea unei muchii șterge fețele și vârfurile rămase singure", "[core][geom][erase]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    drawPolygon(mesh, rect(0, 0, 1000, 1000));
    drawLine(mesh, { 0, 0, 0 }, { 1000, 1000, 0 });
    const EdgeId bottom = mesh.findEdge(mesh.findVertexNear({ 0, 0, 0 }, 1), mesh.findVertexNear({ 1000, 0, 0 }, 1));
    REQUIRE(bottom);
    checkUndoRedo(mesh, stack, [&](Mesh& m) { eraseEdge(m, bottom); });
    CHECK(mesh.faces().size() == 1);
    CHECK(mesh.edges().size() == 4);

    // O muchie liberă dispare cu tot cu capete.
    Mesh lone;
    drawLine(lone, { 0, 0, 0 }, { 100, 0, 0 });
    eraseEdge(lone, lone.edges().keys().front());
    CHECK(lone.vertices().empty());
}

TEST_CASE("Linia degenerată nu schimbă nimic", "[core][geom][draw]")
{
    Mesh mesh;
    drawLine(mesh, { 0, 0, 0 }, { 0.0001, 0, 0 });
    CHECK(mesh.vertices().empty());
}

TEST_CASE("Triangularea unei fețe cu gaură acoperă exact aria", "[core][geom][triangulate]")
{
    Mesh mesh;
    drawPolygon(mesh, rect(0, 0, 3000, 2000));
    drawPolygon(mesh, rect(500, 500, 1000, 1500));
    for (FaceId f : mesh.faces().keys()) {
        const FaceTriangles t = triangulateFace(mesh, f);
        const Vec3d n = mesh.face(f).plane.normal;
        for (std::size_t i = 0; i + 2 < t.indices.size(); i += 3) {
            const Vec3d c = glm::cross(t.points[t.indices[i + 1]] - t.points[t.indices[i]],
                t.points[t.indices[i + 2]] - t.points[t.indices[i]]);
            CHECK(glm::dot(c, n) > 0.0); // în sens trigonometric față de normală
        }
    }
    double total = 0.0;
    for (FaceId f : mesh.faces().keys()) {
        total += faceArea(mesh, f);
    }
    CHECK(forma::tol::equal(total, 6e6, 1e-6));
}

TEST_CASE("Redesenarea unei muchii șterse reface fața", "[core][geom][draw]")
{
    Mesh mesh;
    drawPolygon(mesh, rect(0, 0, 1000, 1000));
    eraseEdge(mesh, mesh.findEdge(mesh.findVertexNear({ 0, 0, 0 }, 1), mesh.findVertexNear({ 1000, 0, 0 }, 1)));
    CHECK(mesh.faces().empty());
    drawLine(mesh, { 0, 0, 0 }, { 1000, 0, 0 });
    requireValid(mesh);
    CHECK(mesh.faces().size() == 1);
}

TEST_CASE("O gaură rămâne goală când desenezi în altă parte a planului", "[core][geom][draw]")
{
    Mesh mesh;
    drawPolygon(mesh, rect(0, 0, 3000, 3000));
    drawPolygon(mesh, rect(1000, 1000, 2000, 2000));
    // Ștergem fața din mijloc: rămâne o gaură în fața mare.
    for (FaceId f : mesh.faces().keys()) {
        if (mesh.face(f).inner.empty()) {
            eraseFace(mesh, f);
            break;
        }
    }
    REQUIRE(mesh.faces().size() == 1);
    // O linie nouă în fața mare, departe de gaură, o împarte fără să umple gaura.
    drawLine(mesh, { 0, 500, 0 }, { 3000, 500, 0 });
    requireValid(mesh);
    CHECK(mesh.faces().size() == 2);
    for (FaceId f : mesh.faces().keys()) {
        CHECK_FALSE(faceContainsPoint(mesh, f, { 1500, 1500, 0 }));
    }
}
