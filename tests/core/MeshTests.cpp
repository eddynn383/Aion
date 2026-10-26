#include "forma/core/geom/Mesh.h"

#include <catch2/catch_test_macros.hpp>

using namespace forma::geom;
using forma::math::Plane;

namespace {

struct Square {
    VertexId v[4];
    EdgeId e[4];
    FaceId face;
};

// Pătrat de 1000 mm pe sol, cu normala în sus (bucla în sens trigonometric privită de sus).
Square makeSquare(Mesh& mesh)
{
    Square s {};
    const Vec3d p[4] { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } };
    for (int i = 0; i < 4; ++i) {
        s.v[i] = mesh.addVertex(p[i]);
    }
    Loop loop;
    for (int i = 0; i < 4; ++i) {
        s.e[i] = mesh.addEdge(s.v[i], s.v[(i + 1) % 4]);
        loop.uses.push_back({ s.e[i], false });
    }
    s.face = mesh.addFace(Plane::fromPointNormal({ 0, 0, 0 }, { 0, 0, 1 }), loop);
    return s;
}

struct RecordingObserver : MeshObserver {
    int calls = 0;
    ChangeSummary last;
    void meshChanged(const Mesh&, const ChangeSummary& summary) override
    {
        ++calls;
        last = summary;
    }
};

} // namespace

TEST_CASE("Mesh: un pătrat construit din primitive e valid", "[core][geom][mesh]")
{
    Mesh mesh;
    const Square s = makeSquare(mesh);
    INFO(mesh.validate().text());
    CHECK(mesh.validate().ok());
    CHECK(mesh.edge(s.e[0]).faces.size() == 1);
    CHECK(mesh.findEdge(s.v[1], s.v[0]) == s.e[0]);
    CHECK_FALSE(mesh.findEdge(s.v[0], s.v[2]));
    CHECK(mesh.findVertexNear({ 1000.0004, 0, 0 }, 1e-3) == s.v[1]);
    CHECK_FALSE(mesh.findVertexNear({ 1000.01, 0, 0 }, 1e-3));
}

TEST_CASE("Mesh: împărțirea unei muchii păstrează fața validă", "[core][geom][mesh]")
{
    Mesh mesh;
    const Square s = makeSquare(mesh);
    const VertexId mid = mesh.addVertex({ 500, 0, 0 });
    const auto [first, second] = mesh.splitEdge(s.e[0], mid);

    INFO(mesh.validate().text());
    CHECK(mesh.validate().ok());
    CHECK_FALSE(mesh.edges().contains(s.e[0]));
    CHECK(mesh.face(s.face).outer.uses.size() == 5);
    CHECK(mesh.edge(first).faces.size() == 1);
    CHECK(mesh.edge(second).faces.size() == 1);
}

TEST_CASE("Mesh: undo și redo prin înregistrarea stărilor", "[core][geom][mesh][undo]")
{
    Mesh mesh;
    makeSquare(mesh);
    const Mesh initial = mesh;

    mesh.beginRecording();
    // O operație compusă: împarte o muchie, șterge fața, adaugă o muchie liberă.
    const VertexId mid = mesh.addVertex({ 500, 0, 0 });
    const EdgeId bottom = mesh.findEdge(mesh.findVertexNear({ 0, 0, 0 }, 1), mesh.findVertexNear({ 1000, 0, 0 }, 1));
    mesh.splitEdge(bottom, mid);
    mesh.removeFace(mesh.faces().keys().front());
    const VertexId top = mesh.findVertexNear({ 0, 1000, 0 }, 1);
    mesh.addEdge(mid, top);
    const ChangeSet changes = mesh.endRecording();
    const Mesh after = mesh;

    REQUIRE_FALSE(changes.empty());
    CHECK(mesh.validate().ok());

    mesh.apply(changes, ChangeDirection::Backward);
    CHECK(mesh.sameAs(initial));
    CHECK(mesh.validate().ok());

    mesh.apply(changes, ChangeDirection::Forward);
    CHECK(mesh.sameAs(after));
    CHECK(mesh.validate().ok());
}

TEST_CASE("Mesh: entitățile create și șterse în aceeași operație nu apar în schimbări", "[core][geom][mesh][undo]")
{
    Mesh mesh;
    makeSquare(mesh);
    mesh.beginRecording();
    const VertexId a = mesh.addVertex({ 5000, 0, 0 });
    const VertexId b = mesh.addVertex({ 6000, 0, 0 });
    const EdgeId e = mesh.addEdge(a, b);
    mesh.removeEdge(e);
    mesh.removeVertex(a);
    mesh.removeVertex(b);
    const ChangeSet changes = mesh.endRecording();
    CHECK(changes.empty());
}

TEST_CASE("Mesh: observatorii primesc rezumatul schimbărilor", "[core][geom][mesh]")
{
    Mesh mesh;
    RecordingObserver observer;
    mesh.addObserver(&observer);

    mesh.beginRecording();
    makeSquare(mesh);
    const ChangeSet changes = mesh.endRecording();
    CHECK(observer.calls == 1);
    CHECK(observer.last.vertices.added.size() == 4);
    CHECK(observer.last.edges.added.size() == 4);
    CHECK(observer.last.faces.added.size() == 1);

    mesh.apply(changes, ChangeDirection::Backward);
    CHECK(observer.calls == 2);
    CHECK(observer.last.faces.removed.size() == 1);
    CHECK(mesh.vertices().empty());

    // Fără schimbări, fără notificare.
    mesh.beginRecording();
    mesh.endRecording();
    CHECK(observer.calls == 2);
    mesh.removeObserver(&observer);
}

TEST_CASE("Mesh: validate() raportează problemele", "[core][geom][mesh][validate]")
{
    SECTION("vârf izolat")
    {
        Mesh mesh;
        mesh.addVertex({ 0, 0, 0 });
        CHECK_FALSE(mesh.validate().ok());
    }
    SECTION("față cu orientarea greșită")
    {
        Mesh mesh;
        const Square s = makeSquare(mesh);
        Face f = mesh.face(s.face);
        mesh.removeFace(s.face);
        mesh.addFace(f.plane.flipped(), f.outer);
        CHECK_FALSE(mesh.validate().ok());
    }
    SECTION("față care nu e plană")
    {
        Mesh mesh;
        const VertexId a = mesh.addVertex({ 0, 0, 0 });
        const VertexId b = mesh.addVertex({ 1000, 0, 0 });
        const VertexId c = mesh.addVertex({ 1000, 1000, 50 });
        const VertexId d = mesh.addVertex({ 0, 1000, 0 });
        Loop loop;
        for (auto [x, y] : { std::pair { a, b }, { b, c }, { c, d }, { d, a } }) {
            loop.uses.push_back({ mesh.addEdge(x, y), false });
        }
        mesh.addFace(Plane::fromPointNormal({ 0, 0, 0 }, { 0, 0, 1 }), loop);
        CHECK_FALSE(mesh.validate().ok());
    }
    SECTION("muchii care se încrucișează și vârfuri care coincid")
    {
        Mesh mesh;
        mesh.addEdge(mesh.addVertex({ 0, 0, 0 }), mesh.addVertex({ 100, 100, 0 }));
        mesh.addEdge(mesh.addVertex({ 0, 100, 0 }), mesh.addVertex({ 100, 0, 0 }));
        CHECK(mesh.validate(ValidationLevel::Topology).ok());
        CHECK_FALSE(mesh.validate(ValidationLevel::Full).ok());

        Mesh twins;
        twins.addEdge(twins.addVertex({ 0, 0, 0 }), twins.addVertex({ 100, 0, 0 }));
        twins.addEdge(twins.addVertex({ 0, 0, 0.0001 }), twins.addVertex({ 0, 100, 0 }));
        CHECK_FALSE(twins.validate().ok());
    }
}
