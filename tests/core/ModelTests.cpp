#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/Operations.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::DefinitionId;
using model::InstanceId;
using model::Model;

namespace {

struct Doc {
    Model model;
    commands::CommandStack stack { model };

    Doc()
    {
        stack.setValidation(true, geom::ValidationLevel::Full);
        stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
    }

    geom::Mesh& root() { return model.mesh(model.root()); }
};

// O cutie w × d × h cu colțul în `origin`.
void box(geom::Mesh& mesh, Vec3d origin, double w, double d, double h)
{
    const Vec3d o = origin;
    geom::drawPolygon(mesh, { o, o + Vec3d { w, 0, 0 }, o + Vec3d { w, d, 0 }, o + Vec3d { 0, d, 0 } });
    geom::FaceId bottom;
    for (geom::FaceId f : mesh.faces().keys()) {
        if (geom::faceContainsPoint(mesh, f, o + Vec3d { w / 2, d / 2, 0 })) {
            bottom = f;
        }
    }
    REQUIRE(bottom);
    geom::pushPull(mesh, bottom, -h);
}

geom::Selection all(const geom::Mesh& mesh)
{
    geom::Selection s;
    s.edges.insert(mesh.edges().keys().begin(), mesh.edges().keys().end());
    s.faces.insert(mesh.faces().keys().begin(), mesh.faces().keys().end());
    return s;
}

} // namespace

TEST_CASE("Model: structura și geometria au undo/redo împreună", "[core][model]")
{
    Doc doc;
    const Model empty = doc.model;
    DefinitionId def;
    REQUIRE(doc.stack.run("Setup", [&](Model& m) {
        def = m.addDefinition({ "Scaun", false });
        box(m.mesh(def), { 0, 0, 0 }, 400, 400, 450);
        m.addInstance({ m.root(), def, Transform::translation({ 1000, 0, 0 }), "", {}, false, false });
        m.addTag({ "Mobilier", { 200, 120, 40, 255 }, true });
    }));
    const Model full = doc.model;
    CHECK(doc.model.instancesOf(def).size() == 1);
    CHECK(doc.model.mesh(def).faces().size() == 6);

    REQUIRE(doc.stack.undo());
    CHECK(doc.model.sameAs(empty));
    REQUIRE(doc.stack.redo());
    CHECK(doc.model.sameAs(full));

    // Ștergerea unei definiții (cu geometria ei) se poate anula; ID-urile noi nu se suprapun cu cele vechi.
    REQUIRE(doc.stack.run("Delete", [&](Model& m) {
        m.removeInstance(m.instancesOf(def).front());
        m.removeDefinition(def);
    }));
    CHECK_FALSE(doc.model.definitions().contains(def));
    REQUIRE(doc.stack.undo());
    CHECK(doc.model.sameAs(full));
    REQUIRE(doc.stack.run("Line", [&](Model& m) { geom::drawLine(m.mesh(def), { 0, 0, 450 }, { 400, 400, 450 }); }));
    REQUIRE(doc.stack.undo());
    REQUIRE(doc.stack.undo()); // înapoi la modelul gol
    REQUIRE(doc.stack.redo());
    CHECK(doc.model.sameAs(full));
}

TEST_CASE("Model: un ciclu (definiție care se conține pe sine) e semnalat", "[core][model]")
{
    Model m;
    const DefinitionId a = m.addDefinition({ "A", false });
    const DefinitionId b = m.addDefinition({ "B", false });
    m.addInstance({ a, b, {}, "", {}, false, false });
    CHECK(m.validate().ok());
    m.addInstance({ b, a, {}, "", {}, false, false });
    CHECK_FALSE(m.validate().ok());
}

TEST_CASE("Make Group: geometria trece în grup, pe loc; undo o readuce", "[core][model]")
{
    Doc doc;
    doc.stack.run("Box", [&](geom::Mesh& mesh) { box(mesh, { 0, 0, 0 }, 1000, 600, 800); });
    const Model before = doc.model;
    InstanceId group;
    REQUIRE(doc.stack.run("Make Group", [&](Model& m) { group = model::makeGroup(m, m.root(), all(doc.root()), false); }));
    CHECK(doc.root().faces().empty());
    CHECK(doc.root().edges().empty());
    const DefinitionId def = doc.model.instance(group).definition;
    CHECK(doc.model.definition(def).group);
    CHECK(doc.model.mesh(def).faces().size() == 6);
    const auto [lo, hi] = model::bounds(doc.model, def);
    CHECK(glm::length(hi - Vec3d { 1000, 600, 800 }) < 1e-9);
    REQUIRE(doc.stack.undo());
    CHECK(doc.model.sameAs(before));
}

TEST_CASE("Make Group dintr-o singură față: muchiile comune rămân și în context", "[core][model]")
{
    Doc doc;
    doc.stack.run("Box", [&](geom::Mesh& mesh) { box(mesh, { 0, 0, 0 }, 1000, 600, 800); });
    geom::FaceId top;
    for (geom::FaceId f : doc.root().faces().keys()) {
        if (doc.root().face(f).plane.normal.z > 0.5) {
            top = f;
        }
    }
    REQUIRE(doc.stack.run("Make Group", [&](Model& m) {
        model::makeGroup(m, m.root(), geom::faceWithEdges(doc.root(), top), false);
    }));
    CHECK(doc.root().faces().size() == 5);
    CHECK(doc.root().edges().size() == 12); // muchiile feței de sus țin și de laturi
}

TEST_CASE("Faza 6, „gata când”: un scaun ca componentă, 4 copii, unul modificat le schimbă pe toate",
    "[core][model]")
{
    Doc doc;
    // Un „scaun” simplu: șezutul și spătarul.
    doc.stack.run("Scaun", [&](geom::Mesh& mesh) {
        box(mesh, { 0, 0, 400 }, 450, 450, 50);
        box(mesh, { 0, 400, 500 }, 450, 50, 400); // spătarul, deasupra șezutului
    });
    InstanceId chair;
    REQUIRE(doc.stack.run("Make Component",
        [&](Model& m) { chair = model::makeGroup(m, m.root(), all(doc.root()), true, "Scaun"); }));
    const DefinitionId def = doc.model.instance(chair).definition;
    CHECK(doc.model.definition(def).name == "Scaun");

    // Patru copii în jurul unei mese (Move + Option, mutate pe X).
    std::vector<InstanceId> copies { chair };
    for (int k = 1; k < 4; ++k) {
        REQUIRE(doc.stack.run("Copy", [&](Model& m) {
            copies.push_back(model::copyInstance(m, chair, Transform::translation({ 600.0 * k, 0, 0 })));
        }));
    }
    REQUIRE(doc.model.instancesOf(def).size() == 4);

    // Editarea unei copii (spătarul mai înalt cu 200) modifică definiția comună: toate patru se schimbă.
    REQUIRE(doc.stack.run("Push/Pull", [&](Model& m) {
        geom::Mesh& mesh = m.mesh(m.instance(copies[2]).definition);
        geom::FaceId top;
        for (geom::FaceId f : mesh.faces().keys()) {
            if (mesh.face(f).plane.normal.z > 0.5 && geom::faceContainsPoint(mesh, f, { 225, 425, 900 })) {
                top = f;
            }
        }
        REQUIRE(top);
        geom::pushPull(mesh, top, 200);
    }));
    for (InstanceId c : copies) {
        const auto [lo, hi] = model::bounds(doc.model, doc.model.instance(c).definition);
        CHECK(hi.z == Approx(1100));
    }

    // Make Unique: o copie se desparte, iar schimbările ei nu le mai afectează pe celelalte.
    DefinitionId unique;
    REQUIRE(doc.stack.run("Make Unique", [&](Model& m) { unique = model::makeUnique(m, copies[3]); }));
    CHECK(unique != def);
    CHECK(doc.model.instancesOf(def).size() == 3);
    REQUIRE(doc.stack.run("Line", [&](Model& m) { geom::drawLine(m.mesh(unique), { 0, 0, 400 }, { 450, 450, 400 }); }));
    CHECK(doc.model.mesh(unique).edges().size() == doc.model.mesh(def).edges().size() + 1);
}

TEST_CASE("Grupurile copiate sunt independente; Explode readuce geometria transformată", "[core][model]")
{
    Doc doc;
    doc.stack.run("Box", [&](geom::Mesh& mesh) { box(mesh, { 0, 0, 0 }, 1000, 600, 800); });
    InstanceId group;
    doc.stack.run("Make Group", [&](Model& m) { group = model::makeGroup(m, m.root(), all(doc.root()), false); });
    InstanceId copy;
    REQUIRE(doc.stack.run("Copy", [&](Model& m) {
        copy = model::copyInstance(m, group, Transform::translation({ 2000, 0, 0 }));
    }));
    CHECK(doc.model.instance(copy).definition != doc.model.instance(group).definition);

    REQUIRE(doc.stack.run("Explode", [&](Model& m) { model::explode(m, copy); }));
    CHECK(doc.root().faces().size() == 6);
    CHECK(doc.root().findVertexNear({ 3000, 600, 800 }, 1e-6));
    CHECK(doc.model.definitions().size() == 2); // rădăcina și grupul rămas
}
